#include "include/silo_log_buffer.h"

/**
 * @brief Adds a set of write records to the log buffer and updates epoch tracking.
 * 
 * @param tid Thread ID associated with the write set.
 * @param nid Notification ID associated with the write set.    // CHECK: notification ID?
 * @param write_set A vector of WriteElement objects, each representing a write/insert operation.
 */
void LogBuffer::push(std::uint64_t tid, NotificationId &nid, std::vector<WriteElement> &write_set) {
    // create log records
    for (auto &itr : write_set) {
        std::string key = itr.key_.uint64t_to_string(itr.key_.slices, itr.key_.lastSliceSize);
        std::string val = itr.get_new_value_body();
        log_set_.emplace_back(tid, static_cast<uint8_t>(itr.op_), key, val);
        log_set_size_++;
    }
    nid_set_.emplace_back(nid);

    // epoch tracking(1つのLogbufferに違うEpochのログが入らないかチェックしているらしい)
    TIDword tidw;
    tidw.obj_ = tid;
    std::uint64_t epoch = tidw.epoch;
    if (epoch < min_epoch_) min_epoch_ = epoch;
    if (epoch > max_epoch_) max_epoch_ = epoch;
    assert(min_epoch_ == max_epoch_);
};

/**
 * @brief Returns the buffer to the pool for recycling.
 * 
 * @details This method returns the current `LogBuffer` instance back to the `LogBufferPool`
 * (`pool_`) to which it belongs, allowing it to be reused in future logging operations.
 */
void LogBuffer::return_buffer() {
    pool_.return_buffer(this);
}

/**
 * @brief Checks if the buffer is empty.
 * 
 * @return true if `nid_set_` is empty, false otherwise.
 */
bool LogBuffer::empty() {
    return nid_set_.empty();
}

std::string LogBuffer::create_json_log() {
    assert(log_set_size_ > 0);
    std::string json = "{";

    // compute check_sum
    uint64_t check_sum = 0;
    for (size_t i = 0; i < log_set_.size(); i++) {
        check_sum += log_set_[i].tid_;
    }

    // create log_header
    // NOTE: 本来はlocal_durable_epochをログと一緒に書き出す必要があるけど、pepochで別に書き出しているので省略
    json += "\"log_header\": {";
    json += "\"check_sum\": " + std::to_string(check_sum) + ",";
    json += "\"log_record_num\": " + std::to_string(log_set_.size()) + ",";
    json += "},";

    // create log_set
    json += "\"log_set\": [";
    for (size_t i = 0; i < log_set_.size(); i++) {
        json += "{";
        json += "\"tid\": " + std::to_string(log_set_[i].tid_) + ",";
        json += "\"op_type\": " + std::to_string(log_set_[i].op_type_) + ",";
        json += "\"key\": \"" + log_set_[i].key_ + "\",";
        json += "\"val\": \"" + log_set_[i].value_ + "\"";
        json += "}";
        if (i < log_set_.size() - 1) {
            json += ",";
        }
    }
    json += "]}";
    // CHECK: 最後に,を入れておいた方が良いのでは？

    return json;
}

/**
 * @brief Writes the log records in the buffer to the log file.
 * 
 * @param logfile A reference to the file object where the log records will be written(e.g., PosixWriter).
 * @param byte_count A reference to a size_t variable where the total byte count is accumulated.
*/
void LogBuffer::write(PosixWriter &logfile, size_t &byte_count) {
    if (log_set_size_ == 0) return;

    // create json format of logs
    std::string json_log = create_json_log();
    logfile.write((void*)json_log.data(), json_log.size());

    // clear for next transactions
    // CHECK: これあってる？
    log_set_size_ = 0;
    log_set_.clear();
    nid_set_.clear();
}

/**
 * @brief Constructs a new LogBufferPool, initializing its buffer and pool.
 * 
 * @details The `LogBufferPool` constructor reserves space for a predefined number of `LogBuffer` 
 * objects in its `buffer_` vector, and initializes them by passing its own reference to them. 
 * It sets the first `LogBuffer` object as the `current_buffer_` and the rest are stored 
 * in the `pool_` for future use.
 * 
 * @note The predefined number of `LogBuffer` objects is defined by `BUFFER_NUM`.
 */
LogBufferPool::LogBufferPool() {
    buffer_.reserve(BUFFER_NUM);
    for (int i = 0; i < BUFFER_NUM; i++) {
        // LogBufferのコンストラクタにLogBufferPoolの参照を渡している
        buffer_.emplace_back(*this);
    }
    
    // 1つだけcurrent_buffer_に入れておく
    pool_.reserve(BUFFER_NUM);
    current_buffer_ = &buffer_[0];

    // 2つ目以降はpool_に入れておく
    for (int i = 1; i < BUFFER_NUM; i++) {
        pool_.push_back(&buffer_[i]);
    }
    my_mutex_.store(0);
}

/**
 * @brief Checks if the LogBufferPool is ready to accept log records.
 * 
 * @details This method checks whether the `current_buffer_` is not NULL or if the logging system 
 * is in the process of quitting (`quit_` is true). If neither condition is true, it 
 * locks the `LogBufferPool`, checks if there are available `LogBuffer` instances in 
 * the `pool_`, and if so, sets one as the `current_buffer_`.
 * 
 * @return true if `current_buffer_` is not NULL or `quit_` is true, 
 *         false if no `LogBuffer` is set as `current_buffer_` after the check.
 */
bool LogBufferPool::is_ready() {
    if (current_buffer_ != NULL || quit_) return true;
    bool r = false;

    my_lock();
    // pool_から一個取り出してcurrent_buffer_にセットする
    if (!pool_.empty()) {
        current_buffer_ = pool_.back();
        pool_.pop_back();
        r = true;
    }
    my_unlock();

    return r;
}

/**
 * @brief Pushes a set of write operations into the current buffer.
 * 
 * @details The method checks whether the log set size of the `current_buffer_` exceeds a predefined limit 
 * (`MAX_BUFFERED_LOG_ENTRIES`) or if a new epoch is beginning (`new_epoch_begins`). If either condition is 
 * true, the `publish()` method is called.
 * 
 * Subsequently, the method waits until the `current_buffer_` is available and ready for use by repeatedly 
 * checking `is_ready()` and pausing for 10,000 ns (10 ms) if not ready. 
 * 
 * If the `LogBufferPool` is in a quitting state (`quit_` is true), the method returns early. 
 * Otherwise, the set of write operations (`write_set`) is pushed into the `current_buffer_`.
 * 
 * @param tid The transaction ID associated with the set of write operations.
 * @param nid A notification ID object to be associated with the transaction.
 * @param write_set A vector containing the set of write operations to be logged.
 * @param new_epoch_begins A boolean flag indicating whether a new epoch is beginning.
 */
void LogBufferPool::push(std::uint64_t tid, NotificationId &nid, std::vector<WriteElement> &write_set, bool new_epoch_begins) {
    nid.tid_ = tid;
    nid.t_mid_ = rdtscp();
    assert(current_buffer_ != NULL);

    // check buffer capa
    if (current_buffer_->log_set_size_ > MAX_BUFFERED_LOG_ENTRIES || new_epoch_begins) {
        publish();
    }

    // Pause and wait for 10,000 ns (10 ms) until current_buffer_ is available and ready for use
    while (!is_ready()) waitTime_ns(10*1000);

    // If the LogBufferPool is quitting, return
    if (quit_) return;

    current_buffer_->push(tid, nid, write_set);
};

/**
 * @brief Publishes the current buffer to the logging queue.
 * 
 * @details This method ensures that the current buffer is ready for publishing and enqueues it to the log 
 * queue for further processing (such as writing to disk or other I/O operations). 
 * 
 * If the `current_buffer_` is not empty, it is enqueued to the log queue (`queue_`), and 
 * `current_buffer_` is set to NULL, indicating that the buffer has been transferred to the queue. 
 * Any analysis-related operations are omitted in this implementation.
 */
void LogBufferPool::publish() {
    while (!is_ready()) waitTime_ns(5*1000);
    assert(current_buffer_ != NULL);

    // enqueue
    if (!current_buffer_->empty()) {
        LogBuffer *p = current_buffer_;
        current_buffer_ = NULL;
        queue_->enq(p);
    }
}

/**
 * @brief Returns a log buffer back to the pool and notifies one waiting thread.
 * 
 * @param lb Pointer to the `LogBuffer` instance to be returned back to the pool.
 */
void LogBufferPool::return_buffer(LogBuffer *lb) {
    my_lock();
    pool_.emplace_back(lb);
    my_unlock();

    cv_deq_.notify_one();
}

/**
 * @brief Terminates the `LogBufferPool`, ensuring the `current_buffer_` is published if non-empty.
 * 
 * @details Sets the `quit_` flag to `true` and checks if `current_buffer_` is not NULL and not empty. 
 * In case `current_buffer_` is not NULL, indicating there are remaining buffers, it calls 
 * `publish()` to ensure that the contents are published before termination.
 */
void LogBufferPool::terminate() {
    quit_ = true;
    // current_buffer_がNULL->bufferの中身全部吐き出してる
    if (current_buffer_ != NULL) {
        if (!current_buffer_->empty()) {
            publish();    
        }
    }
}

/**
 * @brief Acquires a lock using a spinlock mechanism.
 */
void LogBufferPool::my_lock() {
    for (;;) {
        unsigned int lock = 0;
        if (my_mutex_.compare_exchange_weak(lock, 1)) return;
        waitTime_ns(30);
    }
}

/**
 * @brief Releases the lock acquired by `my_lock()`.
 */
void LogBufferPool::my_unlock() {
    my_mutex_.store(0);
}