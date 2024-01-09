#pragma once

#include <queue>
#include <mutex>
#include <vector>
#include <string>
#include <cassert>

#include "random.h"
#include "../../../common/common.h"

/**
 * @class TransactionQueue
 * @brief Thread-safe transaction queue for each worker
 * 
 * @note  This class offers functionalities to add and retrieve 
 *        transactions (as JSON strings) to/from a queue. 
 *        If the queue is empty, getTransaction returns an empty string.
*/
class TransactionQueue {
public:
    TransactionQueue() = default;

    /**
     * @brief Enqueue transaction to the queue
     * @param json_transaction(std::string) Transaction in JSON format
    */
    void putTransaction(const std::string &json_transaction) {
        transaction_queue_.push(json_transaction);
    }

    /**
     * @brief Dequeue transaction from the queue if exists
     * @return json_transaction(std::string) Transaction in JSON format
    */
    std::string getTransaction() {
        if (transaction_queue_.empty()) {
            return "";  // if queue is empty, return empty string
        }
        std::string json_transaction = transaction_queue_.front();
        transaction_queue_.pop();
        return json_transaction;
    }

private:
    std::queue<std::string> transaction_queue_; // transaction queue
};

/**
 * @class TransactionBalancer
 * @brief Balancer for transactions
 * 
 * @note  This class utilizes the Xoroshiro128Plus random number
 *        generator to randomly select a queue for adding transactions. 
 *        Each worker thread can retrieve a transaction from a specific queue 
 *        based on its ID.
*/
class TransactionBalancer {
public:
    /**
     * @brief Constructor for TransactionBalancer.
     * @param num_workers(size_t) Number of worker queues to manage.
     */
    TransactionBalancer() = default;

    void init(size_t num_workers) {
        transaction_queues_.resize(num_workers);
        queue_mutexes_.clear();
        for (size_t i = 0; i < num_workers; i++) {
            queue_mutexes_.emplace_back(new std::mutex);
        }
    }

    /**
     * @brief Enqueue a transaction to a randomly selected queue.
     * @param json_transaction(std::string) Transaction in JSON format
     */
    void putTransaction(const std::string &json_transaction) {
        // select worker queue randomly
        assert(transaction_queues_.size() != 0);
        uint64_t worker_id = rnd_.next() % transaction_queues_.size();
        std::lock_guard<std::mutex> lock(*queue_mutexes_[worker_id].get());
        transaction_queues_[worker_id].putTransaction(json_transaction);
    }

    /**
     * @brief Dequeue a transaction from the queue of a specific worker.
     * @param worker_id(size_t) The ID of the worker whose queue will be accessed.
     * @return A transaction if available, or an empty string if the queue is empty.
     */
    std::string getTransaction(size_t worker_id) {
        assert(worker_id < transaction_queues_.size()); // validate worker_id
        std::lock_guard<std::mutex> lock(*queue_mutexes_[worker_id].get());
        return transaction_queues_[worker_id].getTransaction();
    }

private:
    std::vector<TransactionQueue> transaction_queues_;  // transaction queues for workers
    std::vector<std::unique_ptr<std::mutex>> queue_mutexes_;  // mutexes for transaction queues
    Xoroshiro128Plus rnd_;  // random number generator
};