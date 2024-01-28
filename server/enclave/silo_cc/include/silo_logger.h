#pragma once

#include <mutex>
#include <condition_variable>
#include <vector>
#include <unordered_set>
#include <cstdint>

#include "silo_log_queue.h"
#include "silo_log_writer.h"
#include "silo_notifier.h"
#include "silo_log_writer.h"
#include "silo_transaction.h"

#include "../../silo_r/include/silor_util.h"
#include "../../cassa_common/pass_phrase.h"

class Logger {
public:
    size_t thid_;
    std::vector<int> thid_vec_;
    std::unordered_set<int> thid_set_;
    LogQueue queue_;
    PosixWriter logfile_;
    std::string logdir_;
    std::string logpath_;
    std::uint64_t rotate_epoch_ = 100;
    Notifier notifier_;
    Notifier &notifier_stats_;
    // NidStats nid_stats_;
    NidBuffer nid_buffer_;
    std::size_t byte_count_ = 0;
    std::size_t write_count_ = 0;
    std::size_t buffer_count_ = 0;
    std::uint64_t wait_latency_ = 0;
    std::uint64_t write_latency_ = 0;
    std::uint64_t write_start_ = 0;
    std::uint64_t write_end_ = 0;
    // Stats depoch_diff_;
    LoggerResult &logger_result_;

    // for log chain
    std::string prev_epoch_hash_ = compute_hash_from_string(PASSPHRASE);

    Logger(size_t i, Notifier &n, LoggerResult &myres)
        : thid_(i), notifier_stats_(n), logger_result_(myres) {}

    void add_tx_executor(TxExecutor &trans);
    void worker();
    void send_nid_to_notifier(std::uint64_t min_epoch, bool quit);
    std::uint64_t find_min_epoch();
    void wait_deq();
    void worker_end(int thid);
    void logger_end();
    void store_result();

private:
    std::mutex mutex_;
    std::condition_variable cv_finish_;
    bool joined_ = false;
    std::size_t capacity_ = 1000;
    unsigned int counter_ = 0;

    void logging(bool quit);
};