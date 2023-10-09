#include "include/silo_logger.h"
#include <sys/stat.h>   // stat, mkdir

/**
 * @brief Adds a transaction executor to the logger and configures it.
 * 
 * @param trans Reference to the transaction executor to be added.
 */
void Logger::add_tx_executor(TxExecutor &trans) {
    trans.logger_thid_ = thid_;
    LogBufferPool &pool = std::ref(trans.log_buffer_pool_);
    pool.queue_ = &queue_;
    std::lock_guard<std::mutex> lock(mutex_);
    thid_vec_.emplace_back(trans.worker_thid_);
    thid_set_.emplace(trans.worker_thid_);
}

/**
 * @brief Handles the logging of transactions and manages the associated epochs.
 * 
 * @details 
 * This function carries out several tasks related to transaction logging, 
 * including checking if the log queue is empty, determining the minimum epoch, 
 * writing logs, and notifying clients. If the log queue is not empty, it calculates 
 * the minimum epoch and possibly compares it with the durable epoch. It then iterates 
 * through the dequeued log buffers, writing them to the log file and updating byte count.
 * After logs are written, it syncs the log file and notifies the clients, providing 
 * them with information about the minimum epoch and whether the logger is quitting.
 * 
 * @param quit Boolean flag indicating whether the logger should quit.
 */
void Logger::logging(bool quit) {
    // queue_が空で、quitがtrueなら、notifierに通知して終了
    if (queue_.empty()) {
        if (quit) {
            notifier_.make_durable(nid_buffer_, quit);
        }
        return;
    }

    // find min_epoch from queue_ and worker thread
    std::uint64_t min_epoch = find_min_epoch();
    if (min_epoch == 0) return;

    // for write latency
    std::uint64_t t = rdtscp();
    if (write_start_ == 0) write_start_ == t;

    // write log
    std::uint64_t max_epoch = 0;
    std::vector<LogBuffer*> log_buffer_vec = queue_.deq();
    size_t buffer_num = log_buffer_vec.size();
    
    for (LogBuffer *log_buffer : log_buffer_vec) {
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

/**
 * @brief Finds the minimum epoch among worker threads and the queue.
 * 
 * @details
 * - Iterates through thread IDs, finding and returning the smallest epoch value.
 * - Compares the found minimum epoch with the one in the queue, returning the smaller one.
 * 
 * @return The smallest epoch value found, or 0 if no valid epoch is found.
 */
std::uint64_t Logger::find_min_epoch() {
    // find min_epoch from thid_vec_(worker thread)
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

    // find min_epoch from queue_
    std::uint64_t queue_epoch = queue_.min_epoch();
    if (min_epoch > queue_epoch) min_epoch = queue_epoch;
    return min_epoch;
}


/**
 * @brief Sends a notification ID to the notifier, potentially making it durable.
 * 
 * @details If conditions related to the passed epoch and `quit` flag are met, 
 * it interacts with the notifier and updates the thread's local durable epoch.
 * 
 * @param min_epoch The minimum epoch to check and potentially send.
 * @param quit A flag indicating whether the logger is in the process of quitting.
 */
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

/**
 * @brief Waits for a dequeue operation, sending notification IDs in the meantime.
 * 
 * @details Repeatedly attempts to dequeue from the queue, finding and sending 
 * notification IDs while waiting.
 */
void Logger::wait_deq() {
    while (!queue_.wait_deq()) {
        uint64_t min_epoch = find_min_epoch();
        send_nid_to_notifier(min_epoch, false);
    }
}

/**
 * @brief Manages the logging mechanism and interacts with the log directory.
 * 
 * @details
 * - Initializes the log directory and file.
 * - Enters a loop, waiting for, and performing, log operations until quitting is signaled.
 * - Finalizes logging and notifies of the logger's end, storing results afterward.
 */
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
    logging(true);

    // Logger終わったンゴ連絡
    notifier_stats_.logger_end(this);
    logger_end();
    // show_result();
    store_result();
}

/**
 * @brief Handles the termination of a worker thread.
 * 
 * @details Erases the thread ID from the set and checks if it's empty to potentially terminate the queue.
 * 
 * @param thid The thread ID of the worker that is ending.
 */
void Logger::worker_end(int thid) {
    std::unique_lock<std::mutex> lock(mutex_);
    thid_set_.erase(thid);
    if (thid_set_.empty()) {
        queue_.terminate();
    }
    cv_finish_.wait(lock, [this]{return joined_;});     // Q:?
}

/**
 * @brief Finalizes the logger, closing files and notifying of its end.
 * 
 * @details Closes the log file, sets the joined flag, and notifies any waiting threads.
 */
void Logger::logger_end() {
    logfile_.close();
    std::lock_guard<std::mutex> lock(mutex_);
    joined_ = true;
    cv_finish_.notify_all();
}

/**
 * @brief Stores logging results in the `logger_result_` member.
 * 
 * @details Stores several metrics, including byte count and latencies, for later use or analysis.
 */
void Logger::store_result() {
    logger_result_.byte_count_ = byte_count_;
    logger_result_.write_latency_ = write_latency_;
    logger_result_.wait_latency_ = wait_latency_;
}