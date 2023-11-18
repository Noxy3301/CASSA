#pragma once

#include <atomic>
#include <map>

#include "../../cassa_common/consts.h"
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
    std::atomic<bool> data_added_; // condition_variableの代用
    std::mutex mutex_;
    std::map<uint64_t, std::vector<LogBuffer*>> queue_; // epoch : [log_buffer1, log_buffer2, ...]
    std::size_t capacity_ = 1000;
    std::atomic<bool> quit_;
    // std::chrono::microseconds timeout_;
    int timeout_us_;
};