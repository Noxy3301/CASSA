#include <mutex>
#include <sys/stat.h>   // fileの状態を確認できるすごいやつ

#include "include/logger.h"

void Logger::add_tx_executor(TxExecutor &trans) {
    trans.logger_thid_ = thid_;
    LogBufferPool &pool = std::ref(trans.log_buffer_pool_);
    pool.queue_ = &queue_;
    std::lock_guard<std::mutex> lock(mutex_);
    thid_vec_.emplace_back(trans.thid_);
    thid_set_.emplace(trans.thid_);
}

void Logger::logging(bool quit) {
    if (queue_.empty()) {
        if (quit) {
            notifier_.make_durable(nid_buffer_, quit);
        }
        return;
    }

    // find min_epoch
    std::uint64_t min_epoch = find_min_epoch();
    if (min_epoch == 0) return;
    // compare with durable epoch, やらないかも
    std::uint64_t d = __atomic_load_n(&(DurableEpoch), __ATOMIC_ACQUIRE);
    // depoch_diff_.sample(min_epoch - d);

    // write log
    std::uint64_t max_epoch = 0;
    std::uint64_t t = rdtscp();
    if (write_start_ == 0) write_start_ == t;
    std::vector<LogBuffer*> log_buffer_vec = queue_.deq();
    size_t buffer_num = log_buffer_vec.size();
    for (LogBuffer *log_buffer : log_buffer_vec) {
        #if 0
            std::uint64_t deq_time = rdtscp();
        #endif
        if (log_buffer->max_epoch_ > max_epoch) {
            max_epoch = log_buffer->max_epoch_;
        }
        log_buffer->write(logfile_, byte_count_);
        // log_buffer->pass_nid(nid_buffer_, nid_stats_, deq_time);
        // log_buffer->pass_nidを実装していないのでここでmin_epochとmax_epochを初期化する
        log_buffer->min_epoch_ = ~(uint64_t)0;
        log_buffer->max_epoch_ = 0;
        log_buffer->return_buffer();
    }
    logfile_.sync();
    write_end_ = rdtscp();
    write_latency_ += write_end_ - t;
    write_count_++;
    buffer_count_ += buffer_num;

    // notify client
    send_nid_to_notifier(min_epoch, quit);
}

std::uint64_t Logger::find_min_epoch() {
    // min_epoch of worker threads
    std::uint64_t min_ctid = ~(uint64_t)0;
    for (auto itr : thid_vec_) {
        auto ctid = __atomic_load_n(&(CTIDW[itr]), __ATOMIC_ACQUIRE);
        if (ctid > 0 && ctid < min_ctid) {
            min_ctid = ctid;
        }
    }
    TIDword tid;
    tid.obj_ = min_ctid;
    std::uint64_t min_epoch = tid.epoch;
    // check
    if (tid.epoch == 0 || min_ctid == ~(uint64_t)0) return 0;
    // min_epoch of queue
    std::uint64_t qe = queue_.min_epoch();
    if (min_epoch > qe) min_epoch = qe;
    return min_epoch;
}


// Durable Epochを見たいから実装するかも
void Logger::send_nid_to_notifier(std::uint64_t min_epoch, bool quit) {
    if (!quit && (min_epoch == 0 || min_epoch == ~(uint64_t)0)) return;
    auto new_dl = min_epoch - 1;    // new durable_epoch
    auto old_dl = __atomic_load_n(&(ThLocalDurableEpoch[thid_]), __ATOMIC_ACQUIRE);
    if (quit || old_dl < new_dl) {
        notifier_.make_durable(nid_buffer_, quit);
        asm volatile("":: : "memory");  // fence
        __atomic_store_n(&(ThLocalDurableEpoch[thid_]), new_dl, __ATOMIC_RELEASE);
        asm volatile("":: : "memory");  // fence
    }
}

void Logger::wait_deq() {
    while (!queue_.wait_deq()) {
        uint64_t min_epoch = find_min_epoch();
        send_nid_to_notifier(min_epoch, false);
    }
}

void Logger::worker() {
    // logging機構
    logdir_ = "log" + std::to_string(thid_);
    struct stat statbuf;
    if (::stat(logdir_.c_str(), &statbuf)) {    // std::string -> const char*, exist => 0, non-exist => -1
        if (::mkdir(logdir_.c_str(), 0755)) printf("ERR!"); //TODO: ERRの対処   // exist => 0, non-exist => -1
    } else {
        if ((statbuf.st_mode & S_IFMT) != S_IFDIR) printf("ERR!"); //TODO: ERRの対処 // (アクセス保護)&(ファイル種別を表すBitMask) != (ディレクトリ)
    }
    logpath_ = logdir_ + "/data.log";
    logfile_.open(logpath_);

    for (;;) {
        std::uint64_t t = rdtscp();
        wait_deq();
        if (queue_.quit()) break;
        wait_latency_ += rdtscp() - t;
        logging(false);
    }
    logging(true);  // Q:Logger::loggingにfalse投げたりtrue投げたり何してんねんまじで

    // Logger終わったンゴ連絡
    notifier_stats_.logger_end(this);
    logger_end();
    // show_result();
    store_result();
}

void Logger::worker_end(int thid) {
    std::unique_lock<std::mutex> lock(mutex_);
    thid_set_.erase(thid);
    if (thid_set_.empty()) {
        queue_.terminate();
    }
    cv_finish_.wait(lock, [this]{return joined_;});     // Q:?
}

void Logger::logger_end() {
    logfile_.close();
    std::lock_guard<std::mutex> lock(mutex_);
    joined_ = true;
    cv_finish_.notify_all();
}

void Logger::store_result() {
    logger_result_.byte_count_ = byte_count_;
    logger_result_.write_latency_ = write_latency_;
    logger_result_.wait_latency_ = wait_latency_;
}