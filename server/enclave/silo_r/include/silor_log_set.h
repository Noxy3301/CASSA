#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "silor_log_record.h"

/**
 * @brief Temporarily holds multiple log records for recovery processing.
 */
class RecoveryLogSet {
public:
    uint32_t epoch_;
    size_t log_record_num_;
    std::string prev_epoch_hash_ = "";
    std::vector<RecoveryLogRecord> log_records_;
};