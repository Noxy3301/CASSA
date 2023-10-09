#pragma once

#include <vector>
#include <cstdint>
#include <string>

class LogHeader {
public:
    uint64_t check_sum_;
    uint64_t log_record_num_;
    uint64_t local_durable_epoch_;
    uint64_t epoch_;

    // コンストラクタ
    LogHeader(uint64_t check_sum, uint64_t log_record_num, 
              uint64_t local_durable_epoch, uint64_t epoch)
        : check_sum_(check_sum), log_record_num_(log_record_num), 
          local_durable_epoch_(local_durable_epoch), epoch_(epoch) {}
};

class LogRecord {
public:
    uint64_t tid_;
    OpType op_type_;
    std::string key_;
    std::string value_;

    // コンストラクタ
    LogRecord(uint64_t tid, OpType op_type, 
              const std::string &key, const std::string &value)
        : tid_(tid), op_type_(op_type), 
          key_(key), value_(value) {}
};

class LogPackage {
public:
    LogHeader log_header_;
    std::vector<LogRecord> log_records_;

    // コンストラクタ
    LogPackage(const LogHeader &log_header, const std::vector<LogRecord> &log_records)
        : log_header_(log_header), log_records_(log_records) {}
};