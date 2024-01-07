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
        // combine all fields into a single string
        std::string data = std::to_string(log.tid_) + log.op_type_ + log.key_ + log.value_ + log.prev_hash_;

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
        
        // if log record exists in buffer, set the hash value of the previous record
        if (!log_set_.empty()) {
            log_record.prev_hash_ = LogRecord_wHash::calculate_log_hash(log_set_.back());
        }
        log_set_.emplace_back(log_record);
    }

    // bool validate() const {
    //     if (log_set_.empty()) {
    //         return true;
    //     }

    //     std::string prev_hash;
    //     for (const auto& log_record : log_set_) {
    //         if (log_record.prev_hash_ != prev_hash) {
    //             return false; // ハッシュの不一致が見つかった
    //         }
    //         prev_hash = LogRecord_wHash::calculate_log_hash(log_record);
    //     }

    //     return true; // すべてのハッシュが一致
    // }

    bool validate() const {
        if (log_set_.empty()) {
            std::cout << "Log set is empty. No validation needed." << std::endl;
            return true; // ログセットが空ならば、検証する必要はない
        }

        std::string prev_hash; // 直前のレコードのハッシュ値を保持する変数
        for (size_t i = 0; i < log_set_.size(); ++i) {
            const auto& log_record = log_set_[i];
            std::cout << "Validating log record " << i << ": ";
            std::cout << "TID=" << log_record.tid_ << ", Prev Hash=" << log_record.prev_hash_ << std::endl;

            // 現在のログレコードの prev_hash が直前のログレコードのハッシュ値と一致するかチェック
            if (log_record.prev_hash_ != prev_hash) {
                std::cout << "Hash mismatch detected at log record " << i << std::endl;
                return false; // ハッシュ値が一致しない場合は、ログが改ざんされた可能性がある
            }
            // SHA-256で現在のレコードのハッシュ値を計算し、次のループでの比較のために保存
            prev_hash = LogRecord_wHash::calculate_log_hash(log_record);
            std::cout << "Calculated hash for log record " << i << ": " << prev_hash << std::endl;
        }

        std::cout << "All log records validated successfully." << std::endl;
        return true; // 全てのハッシュが一致した場合は、ログは正常である
    }

private:
    std::vector<LogRecord_wHash> log_set_;
};

std::string calculate_log_hash(const LogRecord_wHash &log) {
    // combine all fields into a single string
    std::string data = std::to_string(log.tid_) + log.op_type_ + log.key_ + log.value_ + log.prev_hash_;

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

int main() {
    LogBuffer_wHash log_buffer;

    log_buffer.push(1, "INSERT", "key1", "value1");
    log_buffer.push(2, "UPDATE", "key2", "value2");

    // ログバッファの整合性を検証
    if (log_buffer.validate()) {
        std::cout << "Log buffer is valid." << std::endl;
    } else {
        std::cout << "Log buffer has been tampered with." << std::endl;
    }

    return 0;
}