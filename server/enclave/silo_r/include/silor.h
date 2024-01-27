#pragma once

#include <cstdint>
#include <vector>

#include "../../../../common/common.h" // for t_print()
#include "../../cassa_server.h"

#include "silor_log_archive.h"
#include "silor_log_record.h"

#define EPOCH_FILE_PATH "log/pepoch.seal"
#define SHA256_HEXSTR_LEN (SHA256_DIGEST_LENGTH * 2)

class RecoveryManager {
public:
    uint64_t current_epoch_ = 0;
    uint64_t durable_epoch_;

    double recovery_progress_ = -1.0;
    uint64_t processed_operation_num_ = 0;

    std::vector<RecoveryLogArchive> log_archives_;
    std::vector<RecoveryLogRecord> current_epoch_log_records_;

    // recovery function
    int execute_recovery();

    // pepoch.seal
    uint64_t read_durable_epoch(std::string file_name);
};