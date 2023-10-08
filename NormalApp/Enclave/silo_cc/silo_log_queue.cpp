#include "include/silo_log_queue.h"

LogQueue::LogQueue() {
    my_mutex_.store(0);
    quit_.store(false);
    // timeout_ = std::chrono::microseconds((int)(EPOCH_TIME*1000));
    timeout_us_ = (int)EPOCH_TIME*1000;
}

/**
 * @brief Enqueues a `LogBuffer` pointer into the log queue, organizing it by its `min_epoch_`.
 * 
 * @details This method adds a `LogBuffer` pointer `x` into the `queue_` map, 
 * using `min_epoch_` as its key. The `queue_` is a data structure defined 
 * as `std::map<uint64_t, std::vector<LogBuffer*>>`, where:
 * - `uint64_t` is an unsigned 64-bit integer, used as the key 
 *   (in this context, it is the `min_epoch_` of a `LogBuffer`).
 * - `std::vector<LogBuffer*>` is a vector that stores pointers to `LogBuffer` objects.
 *
 * The method essentially performs the following steps:
 * 1. Acquires a lock to ensure safe access from multiple threads.
 * 2. Enqueues the `LogBuffer` pointer `x` to the vector associated with 
 *    its `min_epoch_` key in the `queue_` map. If the key does not exist, 
 *    a new vector is created and associated with that key.
 * 3. Releases the lock.
 *
 * Therefore, all `LogBuffer` pointers with the same `min_epoch_` are stored
 * in the same vector, and can be quickly accessed by using the epoch as a key.
 * 
 * @param x Pointer to the `LogBuffer` to be enqueued.
 */
void LogQueue::enq(LogBuffer* x) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto &v = queue_[x->min_epoch_];
        v.emplace_back(x);
    }
    logging_unlock();   // TODO: remove this line and uncomment the following line
    // cv_deq_.notify_one();
}

/**
 * @brief Waits until a `LogBuffer` is ready to dequeue or a timeout occurs.
 * 
 * @details Checks for available `LogBuffer` in the `queue_` or whether `quit_` 
 * is set, returning `true` if either condition is met. If neither condition 
 * is met within a specified timeout duration, it returns `false`.
 * 
 * @return True if a `LogBuffer` is ready to be dequeued or `quit_` is set; 
 * false if the operation times out.
 */
bool LogQueue::wait_deq() {
    if (quit_.load() || !queue_.empty()) return true;

    std::unique_lock<std::mutex> lock(mutex_);  // CHECK: これcv_deqで使ってるやつだっけ？
    unsigned int expected = 0;
    uint64_t wait_start = rdtscp();
    uint64_t wait_current = rdtscp();

    while (wait_current - wait_start < timeout_us_ * CLOCKS_PER_US) {   // timeoutしていないなら
        if (quit_.load() || !queue_.empty()) return true;
        if (logging_status_mutex_.compare_exchange_strong(expected, 1)) return true;    // loggerがloggingできる状態であるかをCASでlockを試みつつ確認
        wait_current = rdtscp();
    }

    return false;
    // return cv_deq_.wait_for(lock, timeout_us_, [this]{return quit_.load() || !queue_.empty();});
}

/**
 * @brief Checks whether the `LogQueue` should quit.
 * 
 * @return True if `quit_` is set and `queue_` is empty; otherwise, false.
 */
bool LogQueue::quit() {
    return quit_.load() && queue_.empty();
}

/**
 * @brief Dequeues and returns all `LogBuffer` pointers from the log queue.
 * 
 * @details This method retrieves and removes all `LogBuffer` pointers from the 
 * `queue_`, which is a `std::map` where each key-value pair represents an epoch
 *  and its associated `LogBuffer` pointers. Specifically, the method does the following:
 * 
 * 1. Counts the total number of `LogBuffer` pointers in `queue_` to be able 
 *    to reserve sufficient space in the resulting vector.
 * 2. Creates a `std::vector<LogBuffer*>` named `ret` and reserves space for 
 *    the total number of `LogBuffer` pointers counted in step 1.
 * 3. Iterates through `queue_`, appending all `LogBuffer` pointers to `ret` 
 *    and erasing the entries from `queue_`.
 * 
 * For example, if `queue_` is as follows:
 * ```
 * queue_ = {
 *     1 : [A, B],        // LogBuffers for epoch 1
 *     2 : [C, D, E, F]   // LogBuffers for epoch 2
 * }
 * ```
 * After the method execution, `queue_` will be empty and `ret` will contain 
 * `[A, B, C, D, E, F]`, holding all `LogBuffer` pointers previously in `queue_`.
 * 
 * @return A `std::vector` containing all `LogBuffer` pointers previously stored in `queue_`.
 */
std::vector<LogBuffer*> LogQueue::deq() {
    std::lock_guard<std::mutex> lock(mutex_);
    size_t number_of_log_buffer = 0;

    // queue_にあるLogBufferのポインタの数を数える
    for (auto itr = queue_.begin(); itr != queue_.end(); itr++) {
        auto q = itr->second;
        number_of_log_buffer += q.size();
    }

    std::vector<LogBuffer*> ret;
    ret.reserve(number_of_log_buffer);

    // queue_からLogBufferのポインタを取り出す
    while (!queue_.empty()) {
        auto itr = queue_.cbegin();
        auto &v = itr->second;
        std::copy(v.begin(), v.end(), std::back_inserter(ret));
        queue_.erase(itr);
    }

    return ret;
}

/**
 * @brief Checks if the log queue is empty.
 * 
 * @return `true` if `queue_` is empty, otherwise `false`.
 */
bool LogQueue::empty() {
    return queue_.empty();
}

/**
 * @brief Retrieves the smallest epoch present in the log queue.
 * 
 * @return The smallest epoch key in `queue_`, or the maximum possible 
 *         `uint64_t` value if `queue_` is empty.
 */
uint64_t LogQueue::min_epoch() {
    if (empty()) {
        return ~(uint64_t)0;
    } else {
        return queue_.cbegin()->first;
    }
}

/**
 * @brief Initiates the termination of the log queue's operation.
 * 
 * @details Sets the `quit_` flag to `true` and unlocks the logging mutex. 
 *          Note that notification to any waiting threads is commented out.
 */
void LogQueue::terminate() {
    quit_.store(true);
    logging_unlock();
    // cv_deq_.notify_all();
}

/**
 * @brief Acquires a lock on `my_mutex_`.
 */
void LogQueue::my_lock() {
    for (;;) {
        unsigned int lock = 0;  // expectedとして使う
        if (my_mutex_.compare_exchange_strong(lock, 1)) return;
        waitTime_ns(30);
    }
}

/**
 * @brief Releases the lock on `my_mutex_`.
 */
void LogQueue::my_unlock() {
    my_mutex_.store(0);
}

/**
 * @brief Acquires a lock on `logging_status_mutex_`.
 * 
 * @note This function is slated for removal in future releases due to the 
 * availability of `condition_variable`'s `notify_one()` in SGX SDK v2.17 and onwards.
 */
void LogQueue::logging_lock() {
    for (;;) {
        unsigned int lock = 0;
        if (logging_status_mutex_.compare_exchange_strong(lock, 1)) return;
        waitTime_ns(30);
    }
}

/**
 * @brief Releases the lock on `logging_status_mutex_`.
 * 
 * @note This function is slated for removal in future releases due to the 
 * availability of `condition_variable`'s `notify_one()` in SGX SDK v2.17 and onwards.
 */
void LogQueue::logging_unlock() {
    logging_status_mutex_.store(0);
}