#include <cassert>

#include "include/log_buffer.h"
#include "include/log_queue.h"
#include "include/notifier.h"
#include "../Include/result.h"
#include "include/util.h"

void LogBufferPool::push(std::uint64_t tid, NotificationId &nid, std::vector<WriteElement> &write_set, uint32_t val, bool new_epoch_begins) {
    nid.tid_ = tid;
    nid.t_mid_ = rdtscp();
    assert(current_buffer_ != NULL);
    // check buffer capa
    if (current_buffer_->log_set_size_ + write_set.size() > LOG_BUFFER_SIZE || new_epoch_begins) {
        publish();
    }
    while (!is_ready()) {
        waitTime_ns(10*1000);
    }
    if (quit_) return;
    current_buffer_->push(tid, nid, write_set, val);
    // better full (?)
    if (current_buffer_->log_set_size_ + MAX_OPE > LOG_BUFFER_SIZE) {
      publish();
    }
    #if 0
        auto t = rdtscp();
        txn_latency_ += t - nid.tx_start_;
        bkpr_latency_ += t - nid.t_mid_;
    #endif
};

void LogBuffer::push(std::uint64_t tid, NotificationId &nid, std::vector<WriteElement> &write_set, uint32_t val) {
    // Q:read_only txもあるからwrite_set.size() == 0もあり得てassertで落ちるけど、NDEBUGがONになっている、じゃあAssert置く意味なくない？
    // assert(write_set.size() > 0);   // A:本当に置く意味がないのでこれでOK Q:というかやっぱりここが釈然としない
    // buffering
    for (auto &itr : write_set) {
        log_set_[log_set_size_++] = LogRecord(tid, itr.key_, val);
    }
    nid_set_.emplace_back(nid);
    // epoch
    TIDword tidw;
    tidw.obj_ = tid;
    std::uint64_t epoch = tidw.epoch;
    // A:1つのLogbufferに違うEpochのログが入らないかチェックしているらしい
    if (epoch < min_epoch_) min_epoch_ = epoch;
    if (epoch > max_epoch_) max_epoch_ = epoch;
    assert(min_epoch_ == max_epoch_); // A:NDEBUGがONになっているのでassert検証はoffだよ
};


bool LogBufferPool::is_ready() {
    if (current_buffer_ != NULL || quit_) {
        return true;
    }
    bool r = false;
    my_lock();
    // pool_から一個取り出してcurrent_buffer_にセットする
    if (!pool_.empty()) {
        current_buffer_ = pool_.back();
        pool_.pop_back();
        r = true;
    }
    my_unlock();
    return r;
}


void LogBufferPool::publish() {
    #if 0
        uint64_t t = rdtscp();
    #endif
    while (!is_ready()) waitTime_ns(5*1000);
    assert(current_buffer_ != NULL);
    // enqueue
    if (!current_buffer_->empty()) {
        LogBuffer *p = current_buffer_;
        current_buffer_ = NULL;
        queue_->enq(p);
    }
    // analyzeは省略
}

void LogBufferPool::return_buffer(LogBuffer *lb) {
    my_lock();
    pool_.emplace_back(lb);
    my_unlock();
    cv_deq_.notify_one();
}

// ResultLog &myresを受け取るけどanalyzeしないから受け取らない方向で

void LogBufferPool::terminate() {
    quit_ = true;
    if (current_buffer_ != NULL) {  // current_buffer_がNULL->bufferの中身全部吐き出してる
        if (!current_buffer_->empty()) {
            publish();    
        }
    }
    // analyzeは省略
}

// TODO:コピペなので要確認
// void LogBuffer::pass_nid(NidBuffer &nid_buffer, NidStats &stats, std::uint64_t deq_time) {
//     std::size_t n = nid_set_.size();
//     if (n==0) return;
//     std::uint64_t t = rdtscp();
//     for (auto &nid : nid_set_) {
//         stats.txn_latency_ += nid.t_mid_ - nid.tx_start_;
//         stats.log_queue_latency_ += deq_time - nid.t_mid_;
//         nid.t_mid_ = t;
//     }
//     // copy NotificationID
//     nid_buffer.store(nid_set_, min_epoch_);
//     stats.write_latency_ += (t - deq_time) * n;
//     stats.count_ += n;
//     // clear nid_set_
//     nid_set_.clear();
//     // init epoch
//     min_epoch_ = ~(uint64_t)0;
//     max_epoch_ = 0;
// }

void LogBuffer::return_buffer() {
    pool_.return_buffer(this);
}

bool LogBuffer::empty() {
    return nid_set_.empty();
}