#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <mutex>
#include <unordered_set>
#include <condition_variable>

#include "silo_tsc.h"
#include "../../cassa_common/db_tid.h"

#include <openssl/ssl.h>

class PepochFile {  // A:siloRでpepochを書き出しているのでそれに則っているらしい
public:
    void write_epoch(uint64_t epoch);

private:
    std::string file_name_ = "logs/pepoch";
    uint64_t *addr_;
    int fd_ = -1;
};

class NotificationId {
public:
    // session info
    std::string session_id_;
    uint64_t session_tx_id_;    // in-session transaction ID

    // TID used for comparing with DurableEpoch
    uint64_t tid_;

    // transaction info (for read items)
    std::vector<std::pair<std::string, std::string>> read_key_value_pairs; // key, value

    // analysis info
    uint64_t tx_start_time_;        // transaction start time
    uint64_t tx_logging_time_ = 0;  // transaction logging time
    uint64_t tx_commit_time_ = 0;   // transaction commit time

    NotificationId(std::string session_id, uint64_t session_tx_id, uint64_t tx_start_time)
        : session_id_(session_id), session_tx_id_(session_tx_id), tx_start_time_(tx_start_time) {}
    NotificationId() { NotificationId("", 0, 0); }

    // NOTE: NotificationIdのtidはLogBufferPool::push()のタイミングで書き込まれる
    uint64_t epoch() {
        TIDword tid;
        tid.obj_ = tid_;
        return tid.epoch;
    }
};

class Logger;

// TODO:コピペだから理解する、というか要らないかも
class NidBufferItem {
public:
    uint64_t epoch_;
    std::vector<NotificationId> buffer_;
    NidBufferItem *next_ = NULL;
    NidBufferItem(uint64_t epoch) : epoch_(epoch) { buffer_.reserve(512); } // TODO: ここメモリ領域確保しすぎて爆発していたけど、なんでreserveしているんだっけ？
};

// TODO:コピペだから理解する、というか要らないかも
class NidBuffer {
public:
    NidBufferItem *front_;
    NidBufferItem *end_;

    NidBuffer() {
        front_ = end_ = new NidBufferItem(0);
    }
    
    ~NidBuffer() {
        auto itr = front_;
        while (itr != nullptr) {
            auto next = itr->next_;
            delete itr;
            itr = next;
        }
    }

    void store(std::vector<NotificationId> &nid_buffer, uint64_t epoch);
    void notify(uint64_t min_dl);
    size_t size() { return size_; }
    bool empty() { return size_ == 0; }
    uint64_t min_epoch() { return front_->epoch_; }
    
private:
    size_t size_ = 0;
    uint64_t max_epoch_ = 0;
    std::mutex mutex_;
};

class Notifier {
public:
    PepochFile pepoch_file_;
    NidBuffer buffer_;

    Notifier();

    void add_logger(Logger *logger);
    uint64_t check_durable();
    void make_durable(NidBuffer &buffer, bool quit);
    void logger_end(Logger *logger);

private:
    std::mutex mutex_;
    uint64_t start_clock_;
    std::vector<std::vector<uint64_t>> epoch_log_;
    std::unordered_set<Logger*> logger_set_;
};