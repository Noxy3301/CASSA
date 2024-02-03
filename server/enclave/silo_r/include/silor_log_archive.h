#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../../../../common/common.h" // for t_print()
#include "../../../../common/ansi_color_code.h"
#include "../../../../common/log_macros.h"
#include "../../../../common/third_party/json.hpp" // for nlohmann::json

#include "silor_log_record.h"
#include "silor_util.h"
#include "silor_log_set.h"

#include "../../cassa_common/pass_phrase.h"

/**
 * @brief Stores and archives log data for recovery.
 */
class RecoveryLogArchive {
public:
    // log.seal
    std::string log_file_name_;
    uint64_t log_file_size_;

    uint64_t current_read_offset_ = 0;
    bool is_all_data_read_ = false;
    bool is_last_log_hash_matched = false;

    std::string previous_epoch_hash_ = compute_hash_from_string(PASSPHRASE);
    std::vector<RecoveryLogSet> buffered_log_records_;
    std::string last_log_hash_ = "";

    bool verify_log_level_integrity(const RecoveryLogSet &buffer);
    int verify_epoch_level_integrity(std::vector<RecoveryLogRecord> &combined_log_sets, uint32_t current_epoch);

    std::string fetch_next_log_record();
    RecoveryLogSet deserialize_log_set(const std::string &json_string);
};