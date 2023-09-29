#pragma once

#include <mutex>
#include <vector>
#include <queue>
#include <optional>
#include <cstring>

#include "../silo_cc/include/silo_procedure.h"

class TransactionManager {
public:
    TransactionManager(size_t worker_num) : queues_(worker_num) {}

    void pushTransaction(size_t worker_thid, size_t txIDcounter, const uint8_t* tx_data, size_t tx_size);
    std::optional<Procedure> popTransaction(size_t worker_thid);

private:
    struct ThreadQueue {
        std::queue<Procedure> queue;
        std::mutex mutex;
    };

    std::vector<ThreadQueue> queues_;
    
    OpType deserializeOpType(const uint8_t*& tx_data);
    std::string deserializeKey(const uint8_t*& tx_data);
    std::string deserializeValue(const uint8_t*& tx_data, OpType opType);
};