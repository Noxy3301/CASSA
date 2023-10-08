#pragma once

#include <atomic>
#include <map>

#include "../../../Include/consts.h"
#include "silo_util.h"
#include "silo_log_buffer.h"

class LogBuffer;

class LogQueue {
public:
    LogQueue();
    void enq(LogBuffer* x);
    bool wait_deq();
    bool quit();
    std::vector<LogBuffer*> deq();
    bool empty();
    uint64_t min_epoch();
    void terminate();

private:
    std::atomic<unsigned int> my_mutex_;
    std::atomic<unsigned int> logging_status_mutex_;    // condition_variableの代用
    std::mutex mutex_;
    std::condition_variable cv_deq_;
    std::map<uint64_t, std::vector<LogBuffer*>> queue_; // epoch : [log_buffer1, log_buffer2, ...]
    std::size_t capacity_ = 1000;
    std::atomic<bool> quit_;
    // std::chrono::microseconds timeout_;
    int timeout_us_;

    void my_lock();
    void my_unlock();

    // TODO: SGXSDK v2.17からcondition_variable::notifity_one()が使えるようになったので消す
    void logging_lock();
    void logging_unlock();
};