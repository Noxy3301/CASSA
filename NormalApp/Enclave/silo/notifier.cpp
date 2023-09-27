#include <iostream>
#include <fcntl.h>      // open用
#include <unistd.h>     // write用
#include <sys/mman.h>   // mmap用

#include "include/notifier.h"
// #include "include/debug.h"
#include "include/logger.h"
#include "../App/main.h"

void PepochFile::open() {
    fd_ = ::open(file_name_.c_str(), O_CREAT|O_TRUNC|O_RDWR, 0644);
    if (fd_ == -1) {    // fd_は失敗したら-1になる(初期値も-1だけど何かしらの要因で事故ったらここに落ち着く)
        std::cerr << "open failed: " << file_name_ << std::endl;
        printf("ERR!");    //TODO: ERRの対処
    }
    std::uint64_t zero = 0;
    auto sz = ::write(fd_, &zero, sizeof(std::uint64_t));
    if (sz == -1) {
        std::cerr << "write failed";
        printf("ERR!");    //TODO: ERRの対処
    }
    addr_ = (std::uint64_t*)::mmap(NULL, sizeof(std::uint64_t), PROT_WRITE, MAP_SHARED, fd_, 0);
    if (addr_ == MAP_FAILED) {
        std::cerr << "mmap failed";
        printf("ERR!");    //TODO: ERRの対処
    }
}

void PepochFile::write(std::uint64_t epoch) {
    *addr_ = epoch;
    ::msync(addr_, sizeof(std::uint64_t), MS_SYNC);
}

void PepochFile::close() {
    ::munmap(addr_, sizeof(std::uint64_t));
    ::close(fd_);
    fd_ = -1;
}

void Notifier::add_logger(Logger *logger) {
    std::unique_lock<std::mutex> lock(mutex_);
    logger_set_.emplace(logger);
}

// TODO:コピペなので要理解
uint64_t Notifier::check_durable() {
    // calculate min(d_l)
    uint64_t min_dl = __atomic_load_n(&(ThLocalDurableEpoch[0]), __ATOMIC_ACQUIRE);
    for (unsigned int i=1; i < LOGGER_NUM; ++i) {
        uint64_t dl = __atomic_load_n(&(ThLocalDurableEpoch[i]), __ATOMIC_ACQUIRE);
        if (dl < min_dl) {
            min_dl = dl;
        }
    }
    uint64_t d = __atomic_load_n(&(DurableEpoch), __ATOMIC_ACQUIRE);
    if (d < min_dl) {
        bool b = __atomic_compare_exchange_n(&(DurableEpoch), &d, min_dl, false, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE);
        if (b) {
            // store Durable Epoch
            pepoch_file_.write(min_dl);
            int expected = ocall_count.load();
            while (!ocall_count.compare_exchange_weak(expected, expected + 1));
        }
    }
    return min_dl;
}

// // TODO:コピペなので要理解
void Notifier::make_durable(NidBuffer &nid_buffer, bool quit) {
    __atomic_fetch_add(&try_count_, 1, __ATOMIC_ACQ_REL);
    auto min_dl = check_durable();
    if (nid_buffer.min_epoch() > min_dl) return;
    // notify client
    // NotifyStatsを使っていないので省略
}

// // TODO:コピペなので要理解
// void NidBuffer::notify(std::uint64_t min_dl, NotifyStats &stats) {
//   if (front_ == NULL) return;
//   stats.notify_count_ ++;
//   NidBufferItem *orig_front = front_;
//   while (front_->epoch_ <= min_dl) {
//     //printf("front_->epoch_=%lu min_dl=%lu\n",front_->epoch_,min_dl);
//     std::uint64_t ltc = 0;
//     std::uint64_t ntf_ltc = 0;
//     std::uint64_t min_ltc = ~(uint64_t)0;
//     std::uint64_t max_ltc = 0;
//     std::uint64_t t = rdtscp();
//     for (auto &nid : front_->buffer_) {
//       // notify client here
//       std::uint64_t dt = t - nid.tx_start_;
//       stats.sample_hist(dt);
//       ltc += dt;
//       if (dt < min_ltc) min_ltc = dt;
//       if (dt > max_ltc) max_ltc = dt;
//       ntf_ltc += t - nid.t_mid_;
//     }
//     stats.latency_ += ltc;
//     stats.notify_latency_ += ntf_ltc;
//     if (min_ltc < stats.min_latency_) stats.min_latency_ = min_ltc;
//     if (max_ltc > stats.max_latency_) stats.max_latency_ = max_ltc;
//     if (min_dl != ~(uint64_t)0)
//       stats.epoch_diff_ += min_dl - front_->epoch_;
//     stats.epoch_count_ ++;
//     std::size_t n = front_->buffer_.size();
//     stats.count_ += n;
//     // clear buffer
//     size_ -= n;
//     front_->buffer_.clear();
//     if (front_->next_ == NULL) break;
//     // recycle buffer
//     front_->epoch_ = end_->epoch_ + 1;
//     end_->next_ = front_;
//     end_ = front_;
//     front_ = front_->next_;
//     end_->next_ = NULL;
//     if (front_ == orig_front) break;
//   }
// }

// // TODO:コピペなので要理解
// void NidBuffer::store(std::vector<NotificationId> &nid_buffer, std::uint64_t epoch) {
//   NidBufferItem *itr = front_;
//   while (itr->epoch_ < epoch) {
//     if (itr->next_ == NULL) {
//       // create buffer
//       itr->next_ = end_ = new NidBufferItem(epoch);
//       if (max_epoch_ < epoch) max_epoch_ = epoch;
//       //printf("create end_=%lx end_->next_=%lx end_->epoch_=%lu epoch=%lu max_epoch_=%lu\n",(uint64_t)end_,(uint64_t)end_->next_, end_->epoch_, epoch, max_epoch_);
//     }
//     itr = itr->next_;
//   }
//   // assert(itr->epoch == epoch);
//   std::copy(nid_buffer.begin(), nid_buffer.end(), std::back_inserter(itr->buffer_));
//   size_ += nid_buffer.size();
// }




void Notifier::logger_end(Logger *logger) {
    std::unique_lock<std::mutex> lock(mutex_);

    byte_count_ += logger->byte_count_;
    write_count_ += logger->write_count_;
    buffer_count_ += logger->buffer_count_;
    write_latency_ += logger->write_latency_;
    wait_latency_ += logger->wait_latency_;
    throughput_ += (double)logger->byte_count_/logger->write_latency_;

    logger_set_.erase(logger);
}

void Notifier::display() {
    double cps = CLOCKS_PER_US*1e6;
    size_t n = LOGGER_NUM;
    std::cout << "wait_time[s]:\t"  << wait_latency_/cps  << std::endl;
    std::cout << "write_time[s]:\t" << write_latency_/cps << std::endl;
    std::cout << "write_count:\t"   << write_count_       << std::endl;
    std::cout << "byte_count[B]:\t" << byte_count_        << std::endl;
    std::cout << "buffer_count:\t"  << buffer_count_      << std::endl;

    std::cout << "throughput(thread_sum)[B/s]:\t" << throughput_*cps                           << std::endl;
    std::cout << "throughput(byte_sum)[B/s]:\t"   << cps*byte_count_/write_latency_*n          << std::endl;
    std::cout << "throughput(elap)[B/s]:\t"       << cps*byte_count_/(write_end_-write_start_) << std::endl;

    std::cout << "try_count:\t"     << try_count_ << std::endl; // make_durable未実装なので使えないけどこれ何意味している？
    uint64_t d = __atomic_load_n(&(DurableEpoch), __ATOMIC_ACQUIRE);
    std::cout << "durable_epoch:\t" << d << std::endl;          // これもcheck_durable未実装なので使えません
}