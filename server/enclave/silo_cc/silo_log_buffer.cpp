#include "include/silo_log_buffer.h"
#include "../../../common/third_party/json.hpp"

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
        log_set_.emplace_back(tid, itr.op_, key, val);
        log_set_size_++;
    }

    // read only transactionの場合、LogBufferのCurrent TIDを更新する必要はない
    if (write_set.size() == 0) return;
    nid_set_.emplace_back(nid);

    // epoch tracking(1つのLogbufferに違うEpochのログが入らないかチェックしているらしい)
    TIDword tidw;
    tidw.obj_ = tid;
    std::uint64_t epoch = tidw.epoch;
    if (epoch < min_epoch_) min_epoch_ = epoch;
    if (epoch > max_epoch_) max_epoch_ = epoch;
    assert(min_epoch_ == max_epoch_);
};

void LogBuffer::pass_nid(NidBuffer &nid_buffer) {
    // if nid_set_ is empty, return
    size_t n = nid_set_.size();
    if (n == 0) return;

    // update each nid's tx_logging_time_
    uint64_t t = rdtscp();
    for (auto &nid : nid_set_) {
        nid.tx_logging_time_ = t;
    }

    // copy Notification ID
    nid_buffer.store(nid_set_, min_epoch_);

    // clear nid_set_
    nid_set_.clear();

    // init epoch
    min_epoch_ = ~(uint64_t)0;
    max_epoch_ = 0;
}

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

std::string LogBuffer::calculate_hash(const uint64_t tid,
                                      const std::string &op_type,
                                      const std::string &key,
                                      const std::string &value) {
    // combine all fields into a single string (except prev_hash)
    std::string data = std::to_string(tid) + op_type + key + value;

    // calculate SHA-256 hash
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int lengthOfHash = 0;

    EVP_MD_CTX* sha256 = EVP_MD_CTX_new();
    EVP_DigestInit_ex(sha256, EVP_sha256(), NULL);
    EVP_DigestUpdate(sha256, data.c_str(), data.size());
    EVP_DigestFinal_ex(sha256, hash, &lengthOfHash);
    EVP_MD_CTX_free(sha256);

    // convert hash to hex string without using stringstream
    char buffer[3]; // 2 characters + null terminator for each byte
    std::string hex_str;
    for (unsigned char i : hash) {
        snprintf(buffer, sizeof(buffer), "%02x", i);
        hex_str += buffer;
    }
    return hex_str;
}

std::string LogBuffer::create_json_log() {
    assert(log_set_size_ > 0);

    // Create log_header
    nlohmann::json json_log = nlohmann::json::object();
    json_log["log_header"] = {
        {"prev_epoch_hash", 0}, // TODO: prev_log_hashを引数で受け入れる
        {"log_record_num", log_set_.size()}
    };

    // Create log_set
    nlohmann::json json_log_set = nlohmann::json::array();
    for (const auto& record : log_set_) {
        nlohmann::json json_record = nlohmann::json::object();
        json_record["tid"] = record.tid_;
        json_record["op_type"] = OpType_to_string(record.op_type_);
        json_record["key"] = record.key_;
        json_record["val"] = record.value_;
        // json_record["debug_current_hash"] = LogBuffer::calculate_hash(record.tid_, OpType_to_string(record.op_type_), record.key_, record.value_);

        if (!json_log_set.empty()) {
            // If log record exists in buffer, set the hash value of the previous record
            std::string prev_hash = LogBuffer::calculate_hash(json_log_set.back()["tid"],
                                                              json_log_set.back()["op_type"],
                                                              json_log_set.back()["key"],
                                                              json_log_set.back()["val"]);
            json_record["prev_hash"] = prev_hash;
        }

        // Add and update the hash value of the first log record
        json_log_set.push_back(json_record);
        json_log_set.front()["prev_hash"] = LogBuffer::calculate_hash(json_log_set.back()["tid"],
                                                                      json_log_set.back()["op_type"],
                                                                      json_log_set.back()["key"],
                                                                      json_log_set.back()["val"]);
    }

    // Add log_set to log
    json_log["log_set"] = json_log_set;

    return json_log.dump();
}

std::string LogBuffer::OpType_to_string(OpType op_type) {
    switch (op_type) {
        case OpType::NONE:   return "NONE";
        case OpType::READ:   return "READ";
        case OpType::WRITE:  return "WRITE";
        case OpType::INSERT: return "INSERT";
        case OpType::DELETE: return "DELETE";
        case OpType::SCAN:   return "SCAN";
        case OpType::RMW:    return "RMW";
        default:
            assert(false);
            return "";
    }
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
    
    // Prepare the buffer to include the size of the data for recovery
    uint32_t log_size = static_cast<uint32_t>(json_log.size());
    std::string size_str(reinterpret_cast<char*>(&log_size), sizeof(uint32_t));

    // Concatenate the size string with the log data
    std::string buffer = size_str + json_log;

    // Write the buffer to the log file
    logfile.write((void*)buffer.data(), buffer.size());

    // clear for next transactions
    log_set_size_ = 0;
    log_set_.clear();
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
    // update nid
    nid.tid_ = tid;
    nid.tx_logging_time_ = rdtscp();

    // check buffer capa
    assert(current_buffer_ != NULL);
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