#include "include/silor_log_archive.h"

/**
 * @brief Verify the integrity of the specified epoch log sets and insert them into the combined log sets.
*/
int RecoveryLogArchive::verify_epoch_level_integrity(std::vector<RecoveryLogRecord> &combined_log_sets, uint32_t current_epoch) {
    for (auto it = this->buffered_log_records_.begin(); it != this->buffered_log_records_.end();) {

        // Epoch-level integrity check
        if (it->epoch_ == current_epoch) {
            if (it->prev_epoch_hash_ == "") {
                /**
                 * 1つ目のlog_bufferの場合、this->prev_epoch_hash_はcredential_dataのhashと一致するはず
                 * TODO: credential_dataのhashを取得する(あとで)
                */
                // t_print(DEBUG "it->prev_epoch_hash_ is empty.\n");
            } else if (it->prev_epoch_hash_ != this->previous_epoch_hash_) {
                t_print(DEBUG BRED"Validation failed for epoch %u\n" CRESET, current_epoch);
                return -1;
            }

            // Compute and concatenate hashes of log records for the current epoch to create an epoch-level hash chain
            std::string accmulated_hash = "";
            for (const auto &log_record : it->log_records_) {
                accmulated_hash += compute_hash_from_log_record(log_record);
            }
            // Update RecoveryLogArchive.previous_epoch_hash_
            this->previous_epoch_hash_ = compute_hash_from_string(accmulated_hash);

            // Log-level integrity check
            if (!verify_log_level_integrity(*it)) {
                t_print(DEBUG BRED"Validation failed for epoch %u\n" CRESET, current_epoch);
                return -2;
            }

            // Insert it into the combined log sets and remove it from the buffer
            for (auto &log_record : this->buffered_log_records_) {
                combined_log_sets.insert(combined_log_sets.end(), log_record.log_records_.begin(), log_record.log_records_.end());
            }
            it = this->buffered_log_records_.erase(it); // erase() returns iterator to the next element

        } else {
            // Increment iterator only if not erased to avoid skipping next element after erase
            it++;
        }
    }

    return 0;
}

bool RecoveryLogArchive::verify_log_level_integrity(const RecoveryLogSet &buffer) {
    // If there is only one log in the set, prev_hash will be its own hash
    if (buffer.log_records_.size() == 1) {
        return buffer.log_records_[0].prev_hash_ == compute_hash_from_log_record(buffer.log_records_[0]);
    }

    // Variable to hold the hash of the previous record (for the first log, it's the hash of the last log)
    std::string prev_hash = compute_hash_from_log_record(buffer.log_records_.back());

    for (size_t i = 0; i < buffer.log_records_.size(); i++) {
        const auto &log_record = buffer.log_records_[i];

        // Check if the current log record's prev_hash matches the hash of the previous log record
        // If hashes do not match, the log might have been tampered with
        if (log_record.prev_hash_ != prev_hash) {
            t_print(DEBUG BRED "Hash mismatch detected at log record %lu\n" CRESET, i);
            return false;
        }

        // Calculate the hash of the current record with SHA-256 and save for the next loop's comparison
        prev_hash = compute_hash_from_log_record(log_record);
    }

    return true;
}

std::string RecoveryLogArchive::fetch_next_log_record() {
    // Read size of log record (first 8 bytes, sizeof(size_t))
    std::string size_string = read_file(this->log_file_name_, this->current_read_offset_, sizeof(size_t));

    // Convert size_string to size_t
    size_t log_length;
    std::memcpy(&log_length, size_string.data(), sizeof(size_t));
    this->current_read_offset_ += sizeof(size_t);

    // Read log record (log_length bytes)
    std::string log_record_string = read_file(this->log_file_name_, this->current_read_offset_, log_length);
    this->current_read_offset_ += log_length;

    return log_record_string;
}

RecoveryLogSet RecoveryLogArchive::deserialize_log_set(const std::string &json_string) {
    // Parse JSON string
    nlohmann::json json_data = nlohmann::json::parse(json_string);

    // Create a new log buffer
    RecoveryLogSet buffer;

    // Set log_header
    if (json_data.contains("log_header") && json_data["log_header"].is_object()) {
        buffer.log_record_num_ = json_data["log_header"]["log_record_num"].get<size_t>();
        buffer.prev_epoch_hash_ = json_data["log_header"]["prev_epoch_hash"].get<std::string>();
    }

    // Set log_set
    if (json_data.contains("log_set") && json_data["log_set"].is_array()) {
        for (const auto& item : json_data["log_set"]) {
            uint64_t tid = item["tid"].get<uint64_t>();  // Convert uint64_t to TIDWord
            std::string op_type = item["op_type"].get<std::string>();
            std::string key = item["key"].get<std::string>();
            std::string value = item["val"].get<std::string>();
            std::string prev_hash = item.contains("prev_hash") ? item["prev_hash"].get<std::string>() : "";

            buffer.log_records_.emplace_back(tid, op_type, key, value, prev_hash);
        }
    }

    // Set epoch
    buffer.epoch_ = buffer.log_records_.back().get_epoch_from_tid();

    return buffer;
}