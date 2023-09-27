#pragma once

#include <cstdint>

class Result {
    public:
        uint64_t local_abort_counts_ = 0;
        uint64_t local_commit_counts_ = 0;
        uint64_t total_abort_counts_ = 0;
        uint64_t total_commit_counts_ = 0;

        uint64_t local_abort_res_counts_[4] = {0};
};

class ResultLog {
    public:
        Result result_;
        uint64_t local_bkpr_latency_ = 0;
        uint64_t local_txn_latency_ = 0;
        uint64_t local_wait_depoch_latency_ = 0;
        uint64_t local_publish_latency_ = 0;
        uint64_t local_publish_counts_ = 0;

        uint64_t total_bkpr_latency_ = 0;
        uint64_t total_txn_latency_ = 0;
        uint64_t total_wait_depoch_latency_ = 0;
        uint64_t total_publish_latency_ = 0;
        uint64_t total_publish_counts_ = 0;
};