#pragma once

#include <cstdint>
#include <string>

/**
 * @brief Represents the structure of an individual log record.
 */
class RecoveryLogRecord {
public:
    uint64_t tid_;
    std::string operation_type_;
    std::string key_;
    std::string value_;
    std::string prev_hash_;

    RecoveryLogRecord(uint64_t tid, std::string operation_type,
                      std::string key, std::string value,
                      std::string prev_hash)
        : tid_(tid), operation_type_(operation_type),
          key_(key), value_(value),
          prev_hash_(prev_hash) {}

    uint32_t get_epoch_from_tid() { return static_cast<uint32_t>(tid_ >> 32); }
};