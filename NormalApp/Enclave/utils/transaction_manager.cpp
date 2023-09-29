#include "transaction_manager.h"

void TransactionManager::pushTransaction(size_t worker_thid, size_t txIDcounter, const uint8_t* tx_data, size_t tx_size) {
    // Deserialize tx_data to Procedure object
    // For simplicity, let's assume tx_data contains serialized OpType, Key, and Value.
    // The actual implementation may vary based on how tx_data is structured.
    
    OpType opType = deserializeOpType(tx_data);
    std::string key = deserializeKey(tx_data);
    std::string value = deserializeValue(tx_data, opType);
    
    Procedure procedure(txIDcounter, opType, key, value);
    
    // Push the deserialized Procedure object to the corresponding worker's queue
    std::unique_lock<std::mutex> lock(queues_[worker_thid].mutex);
    queues_[worker_thid].queue.push(procedure);
}

std::optional<Procedure> TransactionManager::popTransaction(size_t worker_thid) {
    std::unique_lock<std::mutex> lock(queues_[worker_thid].mutex);
    if (queues_[worker_thid].queue.empty()) return std::nullopt;
    
    Procedure proc = queues_[worker_thid].queue.front();
    queues_[worker_thid].queue.pop();
    return proc;
}

OpType TransactionManager::deserializeOpType(const uint8_t*& tx_data) {
    OpType opType = static_cast<OpType>(*tx_data);
    tx_data += sizeof(uint8_t); // Move pointer to the next data
    return opType;
}

std::string TransactionManager::deserializeKey(const uint8_t*& tx_data) {
    uint32_t key_size;
    std::memcpy(&key_size, tx_data, sizeof(key_size));
    tx_data += sizeof(key_size); // Move pointer to the next data
    std::string key(reinterpret_cast<const char*>(tx_data), key_size);
    tx_data += key_size; // Move pointer to the next data
    return key;
}

std::string TransactionManager::deserializeValue(const uint8_t*& tx_data, OpType opType) {
    if (opType != OpType::WRITE && opType != OpType::INSERT && opType != OpType::RMW) {
        return "";
    }
    uint32_t value_size;
    std::memcpy(&value_size, tx_data, sizeof(value_size));
    tx_data += sizeof(value_size); // Move pointer to the next data
    std::string value(reinterpret_cast<const char*>(tx_data), value_size);
    tx_data += value_size; // Move pointer to the next data
    return value;
}