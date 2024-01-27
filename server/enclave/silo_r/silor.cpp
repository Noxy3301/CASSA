#include "include/silor.h"

int RecoveryManager::execute_recovery() {
    /**
     * Executes the recovery process by performing the following steps:
     * 1. Reads the durable epoch from EPOCH_FILE_PATH (pepoch.seal) which indicates the last consistent state of the database.
     * 2. Reads the hashes of the last log records from the durable epoch file. These hashes are stored as 64-byte hexadecimal strings.
     * 3. Determines the size of each log file and reads log records for the current epoch. Log records are decrypted and deserialized into RecoveryLogSet structures.
     * 4. Verifies log-level integrity by checking if the prev_hash in each log record correctly points to the previous log record's hash.
     * 5. Performs epoch-level integrity verification to ensure all log records form a unidirectional hash chain, cyclically linked to credential data and the last hash value in pepoch.seal.
     * 6. Sorts logs by their transaction ID (tid) and replays them to reconstruct the database state.
     * 7. Repeats the process for each epoch until the durable epoch is reached.
     *
     * On successful completion, the global epoch is set to the durable epoch, signifying the end of recovery.
     */

    // Read the size of the durable epoch file
    size_t epoch_file_size = get_file_size(EPOCH_FILE_PATH);

    // Read the durable epoch which indicates the last consistent state of the database
    this->durable_epoch_ = read_durable_epoch(EPOCH_FILE_PATH);

    // Read the hashes of the last log records from the durable epoch file
    for (size_t i = 0; i < (epoch_file_size - sizeof(uint64_t)) / SHA256_HEXSTR_LEN; i++) {
        std::string last_log_hash = read_file(EPOCH_FILE_PATH, sizeof(uint64_t) + i * SHA256_HEXSTR_LEN, SHA256_HEXSTR_LEN);

        // Creating a new log archive for each log file
        RecoveryLogArchive log_archive;
        log_archive.log_file_name_ = "log/log" + std::to_string(i) + ".seal";
        log_archive.last_log_hash_ = last_log_hash;
        this->log_archives_.push_back(log_archive);
    }

    // Determine the size of each log file and read log records for the current epoch
    for (auto &log_archive : this->log_archives_) {
        size_t log_file_size = get_file_size(log_archive.log_file_name_);
        log_archive.log_file_size_ = log_file_size;
    }

    // Iterate through each epoch and process the corresponding log records
    while (this->current_epoch_ <= this->durable_epoch_) {
        this->current_epoch_log_records_.clear();

        // Print recovery progress percentage
        double progress = (this->current_epoch_ == 0) ? 0.0 : static_cast<double>(this->current_epoch_) / this->durable_epoch_ * 100.0;
        if (progress != this->recovery_progress_) {
            this->recovery_progress_ = progress;
            t_print("Recovery progress: %.2f%%\r", this->recovery_progress_);
        }

        // Read log records from each log archive and deserialize them
        for (auto &log_archive : this->log_archives_) {
            
            while (true) {
                // Skip if current epoch exceeded or file fully read
                if (log_archive.is_all_data_read_ || log_archive.log_file_size_ <= log_archive.current_read_offset_) break;

                // Skip if the front of buffered_log_records_ higher than current epoch
                if (!log_archive.buffered_log_records_.empty()) {
                    if (log_archive.buffered_log_records_.front().log_sets_.front().get_epoch_from_tid() > this->current_epoch_) {
                        break;
                    }
                }

                // Read next log record from file
                std::string log_record_string = log_archive.fetch_next_log_record();
                if (log_record_string.empty()) {
                    log_archive.is_all_data_read_ = true;
                    break;
                }

                // If last log hash is matched but log data still exists, it's an inconsistency
                if (log_archive.is_last_log_hash_matched && !log_record_string.empty()) {
                    t_print(BRED "Inconsistency detected: Log data exists even though last log hash matched.\n" CRESET);
                    return -1;
                }

                // Deserialize log_set and add to buffer
                RecoveryLogSet log_set = log_archive.deserialize_log_set(log_record_string);
                log_archive.buffered_log_records_.push_back(log_set);
            }
        }

        // Validate log records for the current epoch and sort by tid
        for (auto &log_archive : this->log_archives_) {
            if (log_archive.verify_epoch_level_integrity(this->current_epoch_log_records_, this->current_epoch_) != 0) return -1;
        }

        // Sorting log records by tid
        std::sort(this->current_epoch_log_records_.begin(), this->current_epoch_log_records_.end(), [](const RecoveryLogRecord &a, const RecoveryLogRecord &b) {
            return a.tid_ < b.tid_;
        });

        // Replay log records for the current epoch to reconstruct the database state
        GarbageCollector gc;    // Placeholder for garbage collection
        for (auto &log_record : this->current_epoch_log_records_) {
            this->processed_operation_num_++;
            if (log_record.operation_type_ == "INSERT") {
                Key key(log_record.key_);
                Value *value = new Value(log_record.value_);
                masstree.insert_value(key, value, gc);
            } else if (log_record.operation_type_ == "WRITE") {
                Key key(log_record.key_);
                Value *found_value = masstree.get_value(key);
                found_value->body_ = log_record.value_;
            } else {
                t_print(BRED "Unknown operation_type_: %s\n" CRESET, log_record.operation_type_.c_str());
                return -1;
            }
        }

        // Increment current epoch to continue the recovery process
        this->current_epoch_++;
    }

    // Set the global epoch to the durable epoch after recovery completion
    GlobalEpoch = this->durable_epoch_;
    t_print(BGRN "\nRecovery finished. GlobalEpoch: %lu, %lu operations processed.\n" CRESET, GlobalEpoch, this->processed_operation_num_);

    return 0;
}

/**
 * @brief Read the durable epoch from the specified file. (`pepoch.seal`)
 * 
 * @param file_name The file name to read.
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