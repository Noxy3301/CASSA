#include "include/silor.h"

void RecoveryManager::execute_recovery() {
    // 1. Read the size of `EPOCH_FILE_PATH`
    size_t epoch_file_size = get_file_size(EPOCH_FILE_PATH);
    t_print(DEBUG EPOCH_FILE_PATH ": size: %lu\n", epoch_file_size);

    // 2. Read the durable epoch from `EPOCH_FILE_PATH`
    this->durable_epoch_ = read_durable_epoch(EPOCH_FILE_PATH);
    t_print(DEBUG EPOCH_FILE_PATH ": durable epoch: %lu\n", this->durable_epoch_);

    // 3. Read each last log hash from pepoch.seal 
    //    (sizeof(pepoch.seal) - sizeof(uint64_t)) / SHA256_HEXSTR_LEN
    for (size_t i = 0; i < (epoch_file_size - sizeof(uint64_t)) / SHA256_HEXSTR_LEN; i++) {
        std::string last_log_hash = read_file(EPOCH_FILE_PATH, sizeof(uint64_t) + i * SHA256_HEXSTR_LEN, SHA256_HEXSTR_LEN);
        t_print(DEBUG "last_log_hash[%lu]: %s\n", i, last_log_hash.c_str());

        // Create a new log archive
        RecoveryLogArchive log_archive;
        log_archive.log_file_name_ = "log/log" + std::to_string(i) + ".seal";
        log_archive.last_log_hash_ = last_log_hash;
        this->log_archives_.push_back(log_archive);
    }

    // 4. Read the log records of the current epoch from the log archive(s)
    for (auto &log_archive : this->log_archives_) {
        size_t log_file_size = get_file_size(log_archive.log_file_name_);
        log_archive.log_file_size_ = log_file_size;
    }

    while (this->current_epoch_ <= this->durable_epoch_) {
        this->current_epoch_log_records_.clear();

        // Print recovery progress
        double progress = (this->current_epoch_ == 0) ? 0.0 : static_cast<double>(this->current_epoch_) / this->durable_epoch_ * 100.0;
        if (progress != this->recovery_progress_) {
            this->recovery_progress_ = progress;
            t_print("Recovery progress: %.2f%%\r", this->recovery_progress_);
        }

        // 4.1. Read the log records of the current epoch from the log archive(s)
        for (auto &log_archive : this->log_archives_) {
            
            while (true) {
                // Skip if current epoch exceeded or file fully read
                if (log_archive.is_all_data_read_ || log_archive.log_file_size_ <= log_archive.current_read_offset_) break;

                // Skip if the front of buffered_log_records_ higher than current epoch
                if (!log_archive.buffered_log_records_.empty()) {
                    if (log_archive.buffered_log_records_.front().log_records_.front().get_epoch_from_tid() > this->current_epoch_) {
                        break;
                    }
                }

                // Read next log record from file
                std::string log_record_string = log_archive.fetch_next_log_record();

                // If log_record_string is empty, the file is fully read, skip
                if (log_record_string.empty()) {
                    log_archive.is_all_data_read_ = true;
                    break;
                }

                // Deserialize log_set
                RecoveryLogSet log_set = log_archive.deserialize_log_set(log_record_string);

                // Add log record to buffer
                log_archive.buffered_log_records_.push_back(log_set);
            }
        }

        // 4.2. Validate the log records of the current epoch
        for (auto &log_archive : this->log_archives_) {
            if (log_archive.verify_epoch_level_integrity(this->current_epoch_log_records_, this->current_epoch_) != 0) return;
        }

        // Sort log records by tid
        std::sort(this->current_epoch_log_records_.begin(), this->current_epoch_log_records_.end(), [](const RecoveryLogRecord &a, const RecoveryLogRecord &b) {
            return a.tid_ < b.tid_;
        });

        // Replay the log records of the current epoch
        GarbageCollector gc;    // TODO: これどうしよう
        for (auto &log_record : this->current_epoch_log_records_) {
            this->processed_operation_num_++;
            if (log_record.operation_type_ == "INSERT") {
                Key key(log_record.key_);
                Value *value = new Value(log_record.value_);
                masstree.insert_value(key, value, gc);
                // t_print("INSERT: %s, %s\n", log_record.key_.c_str(), log_record.value_.c_str());
            } else if (log_record.operation_type_ == "WRITE") {
                Key key(log_record.key_);
                Value *found_value = masstree.get_value(key);
                found_value->body_ = log_record.value_;
                // t_print("WRITE: %s, %s\n", log_record.key_.c_str(), log_record.value_.c_str());
            } else {
                t_print(BRED "Unknown operation_type_: %s\n" CRESET, log_record.operation_type_.c_str());
                return;
            }
        }

        // Increment current epoch
        this->current_epoch_++;
    }

    // Set the global epoch to the durable epoch
    GlobalEpoch = this->durable_epoch_;
    t_print(BGRN "\nRecovery finished. GlobalEpoch: %lu, %lu operations processed.\n" CRESET, GlobalEpoch, this->processed_operation_num_);
}

/**
 * @brief Read the durable epoch from the specified file. (`pepoch.seal`)
 * 
 * @param file_name The file name to read.
 * 
 * @return The durable epoch.
 * 
 * @note The durable epoch is stored in the first 8 bytes of the file.
*/
uint64_t RecoveryManager::read_durable_epoch(std::string file_name) {
    const size_t size = sizeof(uint64_t);
    std::string file_data = read_file(file_name, 0, size);

    uint64_t durable_epoch = 0;
    if (file_data.size() >= size) {
        // Convert the first 8 bytes of the file data to uint64_t
        durable_epoch = *reinterpret_cast<const uint64_t*>(file_data.data());
    }

    return durable_epoch;
}