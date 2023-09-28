#pragma once

#include <vector>
#include <condition_variable>
#include <cstdint>
#include <atomic>
#include <memory>   // for std::align

#include "../../../Include/consts.h"
#include "silo_log.h"
#include "silo_element.h"
#include "silo_util.h"

#define LOG_BUFFER_SIZE (BUFFER_SIZE*1024/sizeof(LogRecord))
#define LOG_ALLOC_SIZE (LOG_BUFFER_SIZE+512/sizeof(LogRecord)+1)
#define NID_BUFFER_SIZE (LOG_BUFFER_SIZE/4)     // maybe unused

class LogQueue;
class NotificationId;
class Notifier;
class NidStats;
class NidBuffer;
class LogBufferPool;

class LogBuffer {
    private:
        LogRecord *log_set_;
        LogRecord *log_set_ptr_;
        std::vector<NotificationId> nid_set_;    // nidは実装予定なしだったけどめんどいのでやる
        LogBufferPool &pool_;
    
    public:
        size_t log_set_size_ = 0;
        uint64_t min_epoch_ = ~(uint64_t)0;
        uint64_t max_epoch_ = 0;

        void push(std::uint64_t tid, NotificationId &nid, std::vector<WriteElement> &write_set);
        void pass_nid(NidBuffer &nid_buffer, NidStats &nid_stats, std::uint64_t deq_time);
        void return_buffer();
        bool empty();

        LogBuffer(LogBufferPool &pool) : pool_(pool) {
            nid_set_.reserve(NID_BUFFER_SIZE);
            void *ptr = log_set_ptr_ = new LogRecord[LOG_ALLOC_SIZE];
            std::size_t space = LOG_ALLOC_SIZE * sizeof(LogRecord);
            std::align(512, LOG_BUFFER_SIZE * sizeof(LogRecord), ptr, space);
            if (space < LOG_BUFFER_SIZE * sizeof(LogRecord)) printf("ERR!");    // TODO: ERRをdebugで定義してocall変換するかどうか
            log_set_ = (LogRecord*)ptr;
        }

        ~LogBuffer() {
            delete [] log_set_ptr_;
        }

        inline size_t byte_size() {
            return sizeof(LogHeader) + sizeof(LogRecord) * log_set_size_;
        }

        template<class T>
        void write(T &logfile, size_t &byte_count) {
            if (log_set_size_ == 0) return;
            // prepare header
            alignas(512) LogHeader log_header;
            for (size_t i = 0; i < log_set_size_; i++) {
                log_header.chkSum_ += log_set_[i].computeChkSum();
            }
            log_header.logRecordNum_ = log_set_size_;
            // log_header.convertChkSumIntoComplementOnTwo();
            // write to file
            size_t header_size = sizeof(LogHeader);
            size_t record_size = sizeof(LogRecord) * log_header.logRecordNum_;
            byte_count += header_size + record_size;
            logfile.write((void*)&log_header, header_size);
            logfile.write((void*)log_set_, record_size);
            // ocall_count+2 (because write function called twice)
            // TODO: ocall_countを計測する処理を残すかどうか
            // int expected = ocall_count.load();
            // while (!ocall_count.compare_exchange_weak(expected, expected + 2));
            // clear for next transactions
            log_set_size_ = 0;
        }
};


class LogBufferPool {
    private:
        std::atomic<unsigned int> my_mutex_;

        void my_lock() {
            for (;;) {
                unsigned int lock = 0;
                if (my_mutex_.compare_exchange_weak(lock, 1)) return;
                waitTime_ns(30);
            }
        }

        void my_unlock() {
            my_mutex_.store(0);
        }
    
    public:
        LogQueue *queue_;
        std::mutex mutex_;
        std::condition_variable cv_deq_;
        std::vector<LogBuffer> buffer_;
        std::vector<LogBuffer*> pool_;
        LogBuffer *current_buffer_;
        bool quit_ = false;
        std::uint64_t tx_latency_ = 0;
        std::uint64_t bkpr_latency_ = 0;
        std::uint64_t publish_latency_ = 0;
        std::uint64_t publish_counts_ = 0;

        LogBufferPool() {   // numaなしで実装しているから事故るならここかも
            buffer_.reserve(BUFFER_NUM);
            for (int i = 0; i < BUFFER_NUM; i++) {
                buffer_.emplace_back(*this);
            }
            pool_.reserve(BUFFER_NUM);
            current_buffer_ = &buffer_[0];
            for (int i = 1; i < BUFFER_NUM; i++) {
                pool_.push_back(&buffer_[i]);
            }
            my_mutex_.store(0);
        }
        
        bool is_ready();
        void push(std::uint64_t tid, NotificationId &nid, std::vector<WriteElement> &write_set, bool new_epoch_begins);
        void publish();
        void return_buffer(LogBuffer *lb);
        void terminate();
};

