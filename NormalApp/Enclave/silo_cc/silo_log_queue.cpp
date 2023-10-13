#include "include/silo_log_queue.h"

LogQueue::LogQueue() {
    quit_.store(false);
    data_added_.store(false);
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
        // std::cout << "enq try to get lock" << std::endl;
        std::lock_guard<std::mutex> lock(mutex_);
        // std::cout << "enq start" << std::endl;
        auto &v = queue_[x->min_epoch_];
        v.emplace_back(x);
        data_added_.store(true);
        // std::cout << "data_added_.store(true)" << std::endl;
        // std::cout << "enq end" << std::endl;
    }
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
    uint64_t start_time = rdtscp();
    uint64_t elapsed_time = 0;
    uint64_t timeout_cycles = timeout_us_ * CLOCKS_PER_US;

    // cv_deq_.wait_for(lock, timeout_us_, [this]{return quit_.load() || !queue_.empty();}); を自前で実装
    while (true) {
        // 終了条件
        if (quit_.load() || !queue_.empty()) return true;

        // タイムアウト
        elapsed_time = rdtscp() - start_time;
        if (elapsed_time > timeout_cycles) return false;
        // LogQueue::enq()によってデータが追加されたかどうか(notify_one()の代わり)
        if (data_added_.load()) {
            if (quit_.load() || !queue_.empty()) {
                data_added_.store(false);
                return true;
            }
        }
    }
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
    data_added_.store(true);    // TODO: 元々notify_all()があった場所なんだけど、これで十分か？
}