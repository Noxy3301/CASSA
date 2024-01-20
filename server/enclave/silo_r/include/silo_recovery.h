#pragma once

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <string>
#include <vector>
#include <openssl/evp.h>   // For OpenSSL 3.0 compatible cryptographic interfaces
#include <openssl/sha.h>   // For SHA-256 specific constants (SHA256_DIGEST_LENGTH)

#include "../../cassa_common/db_tid.h"
#include "../../../../common/ansi_color_code.h"

#define SHA256_HEXSTR_LEN (SHA256_DIGEST_LENGTH * 2)

class LogRecordWithHash {
public:
    uint64_t tid_;
    std::string op_type_;
    std::string key_;
    std::string value_;
    std::string prev_hash_;

    LogRecordWithHash(uint64_t tid, std::string op_type,
                      std::string key, std::string value,
                      std::string prev_hash)
        : tid_(tid), op_type_(op_type),
          key_(key), value_(value),
          prev_hash_(prev_hash) {}
    
    uint32_t get_epoch_from_tid() { return static_cast<uint32_t>(tid_ >> 32); }
};

// JSONをパースした結果を格納するクラス
class LogBufferWithHash {
public:
    uint32_t epoch_;
    size_t log_record_num_;
    std::string prev_epoch_hash_ = "";
    std::vector<LogRecordWithHash> log_records_;
};

class LogData {
public:
    // log.seal info
    std::string log_file_name_;
    size_t log_file_size_;
    size_t log_file_offset_ = 0;
    bool is_finished_ = false;

    // 
    std::string prev_epoch_hash_ = "";
    std::vector<LogBufferWithHash> log_buffer_with_hashes_;

    std::string tail_log_hash_;

    void remove_log_buffers_of_epoch(uint32_t epoch);

    int validate_log_buffers(std::vector<LogRecordWithHash> &combined_log_sets, uint32_t current_epoch);
    bool validate_log_buffer(const LogBufferWithHash& buffer);

    std::string read_next_log_record();
    LogBufferWithHash parse_log_set(const std::string& json_string);
};

class RecoveryExecutor {
public:
    uint64_t current_epoch_ = 0;    // TODO: 本当はuint32_t, っていうかこれいらない？
    uint64_t durable_epoch_;

    std::vector<LogData> log_data_;

    std::vector<LogRecordWithHash> combined_log_sets;

    RecoveryExecutor() {}

    // recovery
    void perform_recovery();

    // utility
    size_t get_file_size(std::string file_name);
    static std::string read_file(std::string file_name, size_t offset, size_t size);
    static std::string calculate_log_hash(const LogRecordWithHash &log);
    static std::string calculate_log_hash(const std::string &data);

    // pepoch.seal
    uint64_t read_durable_epoch(const std::string file_name);
    std::string read_tail_log_hash(std::string file_name, size_t thid);

    // log.seal
    std::string read_log_file(std::string file_name, size_t offset, size_t size);

    // validation
    void validate_log_data(std::string &logfile, uint64_t &pepoch, uint64_t &tail_log_hash);
};