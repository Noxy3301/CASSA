#include <openssl/sha.h>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <vector>

// dummy log record
class LogRecord_wHash {
public:
    uint64_t tid_;
    std::string op_type_;
    std::string key_;
    std::string value_;
    std::string prev_hash_;

    LogRecord_wHash(uint64_t tid, const std::string &op_type, 
              const std::string &key, const std::string &value)
        : tid_(tid), op_type_(op_type), key_(key), value_(value) {}

    static std::string calculate_log_hash(const LogRecord_wHash &log) {
        // combine all fields into a single string (except prev_hash)
        std::string data = std::to_string(log.tid_) + log.op_type_ + log.key_ + log.value_;

        // calculate SHA-256 hash
        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_CTX sha256;
        SHA256_Init(&sha256);
        SHA256_Update(&sha256, data.c_str(), data.size());
        SHA256_Final(hash, &sha256);

        // convert hash to hex string
        std::stringstream ss;
        for (unsigned char i : hash) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)i;
        }
        return ss.str();
    }
};

class LogBuffer_wHash {
public:
    size_t log_set_size_ = 0;
    uint64_t min_epoch_ = ~(uint64_t)0;
    uint64_t max_epoch_ = 0;

    LogBuffer_wHash() {}

    void push(uint64_t tid, const std::string &op_type, const std::string &key, const std::string &value) {
        LogRecord_wHash log_record(tid, op_type, key, value);
        
        if (!log_set_.empty()) {
            // if log record exists in buffer, set the hash value of the previous record
            log_record.prev_hash_ = LogRecord_wHash::calculate_log_hash(log_set_.back());
        }

        // push log to log_buffer and update 
        log_set_.emplace_back(log_record);
        log_set_.front().prev_hash_ = LogRecord_wHash::calculate_log_hash(log_record);
    }

    bool validate() const {
        // No need to validate if the log set is empty
        if (log_set_.empty()) {
            std::cout << "Log set is empty. No validation needed." << std::endl;
            return true;
        }
        
        // If there is only one log in the set, prev_hash will be its own hash
        if (log_set_.size() == 1) {
            return log_set_.front().prev_hash_ == LogRecord_wHash::calculate_log_hash(log_set_.front());
        }

        // Variable to hold the hash of the previous record (for the first log, it's the hash of the last log)
        std::string prev_hash = LogRecord_wHash::calculate_log_hash(log_set_.back());

        for (size_t i = 0; i < log_set_.size(); i++) {
            const auto &log_record = log_set_[i];
            std::cout << "Validating log record " << i << ": ";
            std::cout << "TID = " << log_record.tid_ << ", Prev Hash = " << log_record.prev_hash_ << std::endl;

            // Check if the current log record's prev_hash matches the hash of the previous log record
            // If hashes do not match, the log might have been tampered with
            if (log_record.prev_hash_ != prev_hash) {
                std::cout << "Hash mismatch detected at log record " << i << std::endl;
                return false;
            }

            // Calculate the hash of the current record with SHA-256 and save for the next loop's comparison
            prev_hash = LogRecord_wHash::calculate_log_hash(log_record);
            std::cout << "Calculated hash for log record " << i << ": " << prev_hash << std::endl;
        }

        // If all hashes match, the log is considered valid
        std::cout << "All log records validated successfully." << std::endl;
        return true;
    }

    std::vector<LogRecord_wHash> get_log_set() {
        return log_set_;
    }

private:
    std::vector<LogRecord_wHash> log_set_;
};

void print_log_chain(const std::vector<LogRecord_wHash>& log_set) {
    for (size_t i = 0; i < log_set.size(); ++i) {
        const auto& record = log_set[i];
        std::string current_hash = LogRecord_wHash::calculate_log_hash(record);

        std::cout << "Record " << i << ":" << std::endl;
        std::cout << "    TID: "           << record.tid_ << std::endl;
        std::cout << "    Operation: "     << record.op_type_ << std::endl;
        std::cout << "    Key: "           << record.key_ << std::endl;
        std::cout << "    Value: "         << record.value_ << std::endl;
        std::cout << "    Current Hash : " << current_hash << std::endl;
        std::cout << "    Previous Hash: " << record.prev_hash_ << std::endl << std::endl;
    }
}

int main() {
    LogBuffer_wHash log_buffer_none;
    LogBuffer_wHash log_buffer_single;
    LogBuffer_wHash log_buffer_multiple;

    log_buffer_single.push(1, "INSERT", "key1", "value1");

    log_buffer_multiple.push(1, "INSERT", "key1", "value1");
    log_buffer_multiple.push(2, "UPDATE", "key2", "value2");
    log_buffer_multiple.push(3, "WRITE", "key3", "value3");
    log_buffer_multiple.push(4, "READ", "key4", "");

    // ログバッファの整合性を検証
    std::cout << "\n=== Validate log_buffer with no log ===" << std::endl;
    if (log_buffer_none.validate()) {
        std::cout << "Log buffer is valid." << std::endl;
    } else {
        std::cout << "Log buffer has been tampered with." << std::endl;
    }
    print_log_chain(log_buffer_none.get_log_set());

    std::cout << "\n=== Validate log_buffer with one log ===" << std::endl;
    if (log_buffer_single.validate()) {
        std::cout << "Log buffer is valid." << std::endl;
    } else {
        std::cout << "Log buffer has been tampered with." << std::endl;
    }
    print_log_chain(log_buffer_single.get_log_set());

    std::cout << "\n=== Validate log_buffer with two or more logs ===" << std::endl;
    if (log_buffer_multiple.validate()) {
        std::cout << "Log buffer is valid." << std::endl;
    } else {
        std::cout << "Log buffer has been tampered with." << std::endl;
    }
    print_log_chain(log_buffer_multiple.get_log_set());

    return 0;
}