#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <cstring>

class LogHeader {
public:
    uint64_t chkSum_ = 0;
    size_t logRecordNum_ = 0;       // Number of log records in the log package
    std::vector<size_t> offsets_;   // Offsets of each LogRecord in the log package data
    std::vector<size_t> lengths_;   // Lengths of each LogRecord in the log package data

    void addRecord(size_t offset, size_t length) {
        offsets_.push_back(offset);
        lengths_.push_back(length);
        logRecordNum_++;
    }

    void init() {
        logRecordNum_ = 0;
        offsets_.clear();
        lengths_.clear();
    }
};

class LogRecord {
public:
    uint64_t tid_;
    std::string key_;
    std::string val_;

    LogRecord() : tid_(0), key_(""), val_("") {}
    LogRecord(uint64_t tid, std::string key, std::string val) 
        : tid_(tid), key_(key), val_(val) {}


    std::vector<char> serialize() const {
        // Serialize tid_, key_, and val_ into a byte vector
        std::vector<char> data;
        // Serialize tid_
        data.resize(sizeof(tid_));
        std::memcpy(data.data(), &tid_, sizeof(tid_));
        
        // Serialize key_
        auto key_size = static_cast<uint32_t>(key_.size());
        data.resize(data.size() + sizeof(key_size) + key_size);
        std::memcpy(data.data() + sizeof(tid_), &key_size, sizeof(key_size));
        std::memcpy(data.data() + sizeof(tid_) + sizeof(key_size), key_.data(), key_size);

        // Serialize val_
        auto val_size = static_cast<uint32_t>(val_.size());
        data.resize(data.size() + sizeof(val_size) + val_size);
        std::memcpy(data.data() + sizeof(tid_) + sizeof(key_size) + key_size, &val_size, sizeof(val_size));
        std::memcpy(data.data() + sizeof(tid_) + sizeof(key_size) + key_size + sizeof(val_size), val_.data(), val_size);

        return data;
    }

    uint64_t computeChkSum() {
        uint64_t chkSum = 0;
        chkSum += tid_; // Compute chkSum for tid_
        for (auto c : key_) chkSum += c;    // Compute chkSum for key_
        for (auto c : val_) chkSum += c;    // Compute chkSum for val_
        return chkSum;
    }
};

class LogPackage {
public:
    LogHeader header_;
    std::vector<char> data_; // Serialized LogRecords

    void addRecord(const LogRecord &record) {
        auto serialized_record = record.serialize();
        
        // Update the header with the offset and length of the new record
        header_.addRecord(data_.size(), serialized_record.size());
        
        // Append the serialized record to the data
        data_.insert(data_.end(), serialized_record.begin(), serialized_record.end());
    }
};