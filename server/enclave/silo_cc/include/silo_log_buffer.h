#pragma once

#include <vector>
#include <condition_variable>
#include <cstdint>
#include <atomic>
#include <memory>   // for std::align
#include <openssl/evp.h>    // For OpenSSL 3.0 compatible cryptographic interfaces
#include <openssl/sha.h>    // For SHA-256 specific constants like SHA256_DIGEST_LENGTH

#include "../../cassa_common/consts.h"

#include "silo_element.h"
#include "silo_log_queue.h"
#include "silo_log_writer.h"
#include "silo_log.h"
#include "silo_notifier.h"
#include "silo_util.h"

class LogQueue;
class NotificationId;
class Notifier;
class NidStats;
class NidBuffer;
class LogBufferPool;

/**
 * @brief A buffer for storing log records and associated notification IDs.
 */
class LogBuffer {
public:
    size_t log_set_size_ = 0;
    uint64_t min_epoch_ = ~(uint64_t)0;
    uint64_t max_epoch_ = 0;

    LogBuffer(LogBufferPool &pool) : pool_(pool) {}

    void push(std::uint64_t tid, NotificationId &nid, std::vector<WriteElement> &write_set);
    void pass_nid(NidBuffer &nid_buffer);
    void return_buffer();
    bool empty();
    std::string calculate_hash(const uint64_t tid, const std::string &op_type, const std::string &key, const std::string &value);
    std::string calculate_hash(const std::string &data);
    std::string create_json_log(std::string &prev_epoch_hash, std::string &current_epoch_hash);
    std::string OpType_to_string(OpType op_type);

    std::string write(size_t thid, PosixWriter &logfile, std::string &prev_epoch_hash);

private:
    std::vector<LogRecord> log_set_;
    std::vector<NotificationId> nid_set_;    // nidは実装予定なしだったけどめんどいのでやる
    LogBufferPool &pool_;
};

/**
 * @brief Manages a pool of LogBuffer instances and handles concurrent 
 *        access and lifecycle management of log buffers.
*/
class LogBufferPool {
public:
    LogQueue *queue_;
    std::mutex mutex_;
    std::vector<LogBuffer> buffer_;
    std::vector<LogBuffer*> pool_;
    LogBuffer *current_buffer_;
    bool quit_ = false;
    std::uint64_t tx_latency_ = 0;
    std::uint64_t bkpr_latency_ = 0;
    std::uint64_t publish_latency_ = 0;
    std::uint64_t publish_counts_ = 0;

    LogBufferPool();
    
    bool is_ready();
    void push(std::uint64_t tid, NotificationId &nid, std::vector<WriteElement> &write_set, bool new_epoch_begins);
    void publish();
    void return_buffer(LogBuffer *lb);
    void terminate();

private:
    std::atomic<unsigned int> my_mutex_;

    void my_lock();
    void my_unlock();
};