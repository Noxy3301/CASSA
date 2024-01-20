#include "include/silo_recovery.h"

#include "../cassa_server_t.h" // for ocall
#include "../../../common/common.h" // for t_print()
#include "../../../common/third_party/json.hpp" // for nlohmann::json

void RecoveryExecutor::perform_recovery() {
    /**
     * OVERVIEW
     * 
     * 1. Read size of pepoch.seal
     * 2. Read durable epoch from pepoch.seal (first 4 bytes)
     * 3. Read each tail log hash from pepoch.seal ((sizeof(pepoch.seal) - sizeof(uint64_t)) / SHA256_HEXSTR_LEN)
     * 4. For each log.seal file, read the size of log.seal
     * 5. Read the corresponding log data until the end of the file (based on the size read in step 4)
     * 6. Check tid.epoch of log data
     *     a. If `tid.epoch` == `current_epoch`, deserialize the data from JSON format and read the next log data
     *     b. If `tid.epoch` > `current_epoch`, stop reading log data and perform validation
     * 7. Validate epoch level hash
     *     a. If it's the first log data, verify that `prev_epoch` matches the hash of the `credential_data`
     *     b. if it's the last log data, Verify that the log data's hash matches the `tail_log_hash`
     *     c. For intermediate log data, ensure `prev_epoch` matches the hash of the immediately preceding log entry
     * 8. Validate log level hash
     *     a. If it's the first log data, verify that `prev_hash` matches the hash of the last log data
     *     b. If it's not the first log data, verify that `prev_hash` matches the hash of the previous log data
     * 9. Replay log data
     * 10. If `current_epoch` < `durable_epoch`, increment current_epoch and repeat from step 5
     * 
     * NOTE: SHA256 hash values are stored as Hex Strings, requiring SHA256_HEXSTR_LEN bytes to be read from files.
     *       This is defined as SHA256_DIGEST_LENGTH * 2 for initial simplicity in implementation.
     */

    // 1. Read size of pepoch.seal
    size_t epoch_file_size = get_file_size("log/pepoch.seal");
    t_print(DEBUG "log/pepoch.seal: size: %lu\n", epoch_file_size);

    // 2. Read durable epoch from pepoch.seal (first 8 bytes)
    this->durable_epoch_ = read_durable_epoch("log/pepoch.seal");
    t_print(DEBUG "durable_epoch: %lu\n", this->durable_epoch_);

    // 3. Read each tail log hash from pepoch.seal ((sizeof(pepoch.seal) - sizeof(uint64_t)) / SHA256_HEXSTR_LEN)
    for (size_t i = 0; i < (epoch_file_size - sizeof(uint64_t)) / SHA256_HEXSTR_LEN; i++) {
        std::string tail_log_hash = read_file("log/pepoch.seal",
                                              sizeof(uint64_t) + i * SHA256_HEXSTR_LEN,
                                              SHA256_HEXSTR_LEN);
        t_print(DEBUG "tail_log_hash: %s\n", tail_log_hash.c_str());

        // Create a new LogData instance
        LogData log_data;
        log_data.log_file_name_ = "log/log" + std::to_string(i) + ".seal";
        log_data.tail_log_hash_ = tail_log_hash;
        this->log_data_.push_back(log_data);
    }

    t_print(DEBUG "log_data_.size(): %lu\n", this->log_data_.size());

    // 4. For each log.seal file, read the size of log.seal
    for (auto &log_data : this->log_data_) {
        size_t log_file_size = get_file_size(log_data.log_file_name_);
        t_print(DEBUG "%s: size: %lu\n", log_data.log_file_name_.c_str(), log_file_size);

        log_data.log_file_size_ = log_file_size;
    }

    // Iterate over each epoch until the current epoch is less than or equal to the durable epoch
    while (this->current_epoch_ <= this->durable_epoch_) {
        t_print("current_epoch_: %lu, durable_epoch_: %lu\n", this->current_epoch_, this->durable_epoch_);
        for (auto &log_data : this->log_data_) {
            while (!log_data.is_finished_) {
                // Check if the first log record's epoch is greater than the current epoch
                // t_print(DEBUG "log_data.log_buffers_.size(): %lu\n", log_data.log_buffers_.size());
                if (log_data.log_buffer_with_hashes_.size() > 0) {
                    t_print(DEBUG "log_data.log_buffers_.front().log_records_.front().get_epoch_from_tid(): %u\n", log_data.log_buffer_with_hashes_.front().log_records_.front().get_epoch_from_tid());
                    if (log_data.log_buffer_with_hashes_.front().log_records_.front().get_epoch_from_tid() > this->current_epoch_) {
                        t_print(DEBUG "log_data.log_buffers_.front().log_records_.front().get_epoch_from_tid(): %u, skipped\n", log_data.log_buffer_with_hashes_.front().log_records_.front().get_epoch_from_tid());
                        break;
                    }
                }

                // Read next log record from file
                std::string log_data_str = log_data.read_next_log_record();
                
                // If log_data_str is empty, end of log file is reached
                if (log_data_str.empty()) {
                    log_data.is_finished_ = true;
                    break;
                }

                // Parse log data to create LogBufferWithHash
                LogBufferWithHash log_set = log_data.parse_log_set(log_data_str);
                for (auto &log_record : log_set.log_records_) {
                    t_print(DEBUG "log_record.tid_: %lu, epoch: %u\n", log_record.tid_, log_record.get_epoch_from_tid());
                    t_print(DEBUG "log_record.op_type_: %s\n", log_record.op_type_.c_str());
                    t_print(DEBUG "log_record.key_: %s\n", log_record.key_.c_str());
                    t_print(DEBUG "log_record.value_: %s\n", log_record.value_.c_str());
                    t_print(DEBUG "log_record.prev_hash_: %s\n", log_record.prev_hash_.c_str());
                }

                // Add log set to log_buffers_ for processing
                log_data.log_buffer_with_hashes_.push_back(log_set);
            }
        }

        // ここまでくるとcurrent_epochより前のlog_setはすでにlog_data_.log_buffers_に格納されている
        // または、current_epochより大きいlog_setがlog_data_.log_buffers_に格納されている

        // validation

        for (auto &log_data : this->log_data_) {    // 各log.sealに対して
            for (auto &log_buffer : log_data.log_buffer_with_hashes_) {   // 各log_set(JSONだったやつ)に対して
                if (log_buffer.epoch_ == this->current_epoch_) {
                    // 7. Validate epoch level hash
                    //     a. If it's the first log data, verify that `prev_epoch` matches the hash of the `credential_data`
                    //     b. if it's the last log data, Verify that the log data's hash matches the `tail_log_hash`
                    //     c. For intermediate log data, ensure `prev_epoch` matches the hash of the immediately preceding log entry


                    // delete log_buffer from log_data_.log_buffers_
                } else {
                    break;
                }
            }
        }

        // combine log_set

        // sort as tid

        // replay log

        // Increment current_epoch for the next iteration
        for (auto &log_data : this->log_data_) {
            log_data.remove_log_buffers_of_epoch(current_epoch_);
        }
        this->current_epoch_++;
    }
    //     // 7. Validate epoch level hash
    //     for (auto &log_data : this->log_data_) {
    //         for (auto &log_set : log_data.log_buffers_) {
    //             t_print(DEBUG "log_set.log_record_num_: %lu\n", log_set.log_record_num_);
    //             t_print(DEBUG "log_set.prev_epoch_hash_: %s\n", log_set.prev_epoch_hash_.c_str());
    //             for (auto &log_record : log_set.log_records_) {
    //                 t_print(DEBUG "log_record.tid_: %lu\n", log_record.tid_);
    //                 t_print(DEBUG "log_record.op_type_: %s\n", log_record.op_type_.c_str());
    //                 t_print(DEBUG "log_record.key_: %s\n", log_record.key_.c_str());
    //                 t_print(DEBUG "log_record.value_: %s\n", log_record.value_.c_str());
    //                 t_print(DEBUG "log_record.prev_hash_: %s\n", log_record.prev_hash_.c_str());
    //             }
    //         }
    //     }
    // }
}

size_t RecoveryExecutor::get_file_size(std::string file_name) {
    size_t file_size = 0;
    sgx_status_t ocall_status = ocall_get_file_size(&file_size, reinterpret_cast<const uint8_t*>(file_name.c_str()), file_name.size());
    assert(ocall_status == SGX_SUCCESS);
    return file_size;
}

std::string RecoveryExecutor::read_file(std::string file_name, size_t offset, size_t size) {
    std::string file_data;
    file_data.resize(size);
    
    // OCALL関数の戻り値を格納する変数
    int ocall_retval;

    // OCALLを実行してファイルの内容を読み込む
    sgx_status_t ocall_status = ocall_read_file(&ocall_retval, 
                                                reinterpret_cast<const uint8_t*>(file_name.c_str()), 
                                                file_name.size(), 
                                                reinterpret_cast<uint8_t*>(&file_data[0]), 
                                                offset, 
                                                size);
    assert(ocall_status == SGX_SUCCESS);
    // assert(ocall_retval == 0);

    return file_data;
}

uint64_t RecoveryExecutor::read_durable_epoch(const std::string file_name) {
    const size_t size = sizeof(uint64_t);
    std::string file_data = read_file(file_name, 0, size);

    uint64_t durable_epoch = 0;
    if (file_data.size() >= size) {
        // Convert the first 8 bytes of the file data to uint64_t
        durable_epoch = *reinterpret_cast<const uint64_t*>(file_data.data());
    }

    return durable_epoch;
}

// validation後に特定のepochのlog_buffers_要素を削除する
void LogData::remove_log_buffers_of_epoch(uint32_t epoch) {
    auto it = std::remove_if(log_buffer_with_hashes_.begin(), log_buffer_with_hashes_.end(),
                             [epoch](const LogBufferWithHash& buffer) {
                                 return buffer.epoch_ == epoch;
                             });
    log_buffer_with_hashes_.erase(it, log_buffer_with_hashes_.end());
}


std::string LogData::read_next_log_record() {
    // 1. Read size of log record (first 8 bytes)
    std::string log_record_size_str = RecoveryExecutor::read_file(this->log_file_name_,
                                                                  this->log_file_offset_,
                                                                  sizeof(size_t));
    size_t log_length;
    std::memcpy(&log_length, log_record_size_str.data(), sizeof(size_t));
    this->log_file_offset_ += sizeof(size_t);

    // 2. Read log record (Based on the size read in step 1)
    std::string log_record_str = RecoveryExecutor::read_file(this->log_file_name_, this->log_file_offset_, log_length);
    this->log_file_offset_ += log_length;

    return log_record_str;
}

LogBufferWithHash LogData::parse_log_set(const std::string& json_string) {
    nlohmann::json json_data = nlohmann::json::parse(json_string);

    // 新しいLogBufferWithHashオブジェクトを作成
    LogBufferWithHash new_log_buffer;

    // log_headerの処理
    if (json_data.contains("log_header") && json_data["log_header"].is_object()) {
        new_log_buffer.log_record_num_ = json_data["log_header"]["log_record_num"].get<size_t>();
        new_log_buffer.prev_epoch_hash_ = json_data["log_header"]["prev_epoch_hash"].get<std::string>();
    }

    // log_setの処理
    if (json_data.contains("log_set") && json_data["log_set"].is_array()) {
        for (const auto& item : json_data["log_set"]) {
            uint64_t tid = item["tid"].get<uint64_t>();  // Convert uint64_t to TIDWord
            std::string op_type = item["op_type"].get<std::string>();
            std::string key = item["key"].get<std::string>();
            std::string value = item["val"].get<std::string>();
            std::string prev_hash = item.contains("prev_hash") ? item["prev_hash"].get<std::string>() : "";

            new_log_buffer.log_records_.emplace_back(tid, op_type, key, value, prev_hash);
        }
    }

    // epochの処理
    new_log_buffer.epoch_ = new_log_buffer.log_records_.back().get_epoch_from_tid();

    return new_log_buffer;
}