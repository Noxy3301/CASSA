#include "include/silo_notifier.h"

// #include <iostream> // TODO: std::cerr周りで使っているけど将来的には消す
// #include <fcntl.h>      // open用
// #include <unistd.h>     // write用
// #include <sys/mman.h>   // mmap用

#include "../global_variables.h"
#include "include/silo_logger.h"

#include "../cassa_server_t.h"  // for ocall_save_pepochfile
#include "../cassa_server.h"    // for ssl_session_handler
#include "../../../common/openssl_utility_enclave.h" // for tls communication
#include "../../../common/common.h" // for t_print
#include "../cassa_common/ssl_session_handler.hpp" // for SSLSession
#include "../cassa_common/json_message_formats.hpp" // for create_message

/**
 * @brief Writes the (durable) epoch to the file.
 * 
 * @param epoch The epoch to write.
 */
void PepochFile::write_epoch(std::uint64_t epoch) {
    int ocall_ret;
    sgx_status_t ocall_status = ocall_save_pepochfile(&ocall_ret, (uint8_t*)&epoch, sizeof(epoch));
    assert(ocall_ret == 0);
}

void NidBuffer::store(std::vector<NotificationId> &nid_buffer, std::uint64_t epoch) {
  NidBufferItem *itr = front_;

  // search for a suitable buffer that has an equivalent epoch
  while (itr->epoch_ < epoch) {
    if (itr->next_ == NULL) {
      // if no suitable buffer is found, create a new one
      itr->next_ = end_ = new NidBufferItem(epoch);
      if (max_epoch_ < epoch) max_epoch_ = epoch;
      //printf("create end_=%lx end_->next_=%lx end_->epoch_=%lu epoch=%lu max_epoch_=%lu\n",(uint64_t)end_,(uint64_t)end_->next_, end_->epoch_, epoch, max_epoch_);
    }
    itr = itr->next_;
  }
  
  // store notification ids to the buffer
  assert(itr->epoch_ == epoch);
  std::copy(nid_buffer.begin(), nid_buffer.end(), std::back_inserter(itr->buffer_));
  size_ += nid_buffer.size();
}

// NOTE: NotifyStatsはデータ取得用なので削除
void NidBuffer::notify(std::uint64_t min_dl) {
    if (front_ == NULL) return;

    NidBufferItem *orig_front = front_;
    while (front_->epoch_ <= min_dl) {
        for (auto &nid : front_->buffer_) {
            // notify client here
            nid.tx_commit_time_ = rdtscp();
            SSLSession *session = ssl_session_handler.getSession(nid.session_id_);
            if (session == nullptr) {
                // TODO: Notify if the client's session does not exist on the server
                t_print("session == nullptr\n");
                continue;
            } else {
                // create json format of message
                nlohmann::json json_message = create_message(0, "Notifier: OK", nid.read_key_value_pairs);
                std::string json_message_dump = json_message.dump();

                // send message to client
                SSL *ssl = session->ssl_session;
                std::lock_guard<std::mutex> lock(*session->ssl_session_mutex);
                tls_write_to_session_peer(ssl, json_message_dump); 
            }
        }

        // clear buffer
        // NOTE: NidBuffer.size_使ってなさそうなのでこれ使ってる関数と一緒に消すかも
        size_t n = front_->buffer_.size();
        size_ -= n;
        front_->buffer_.clear();
        if (front_->next_ == NULL) break;

        // recycle buffer
        front_->epoch_ = end_->epoch_ + 1;  // front_のepochをend_の次にする
        end_->next_ = front_;               // end_の次をfront_にする
        end_ = front_;                      // end_をfront_にする
        front_ = front_->next_;             // front_を次にする
        end_->next_ = NULL;                 // end_(最後尾の元front_)の次をnullにする
        if (front_ == orig_front) break;
    }
}



Notifier::Notifier() {
    start_clock_ = rdtscp();
}

void Notifier::logger_end(Logger *logger) {
    std::unique_lock<std::mutex> lock(mutex_);
    logger_set_.erase(logger);
}

/**
 * @brief Adds a logger to the logger set.
 */
void Notifier::add_logger(Logger *logger) {
    std::unique_lock<std::mutex> lock(mutex_);
    logger_set_.emplace(logger);
}

/**
 * @brief Checks and updates the durable epoch.
 * 
 * @return The minimum durable epoch.
 * 
 * @details
 * Calculates the minimum durable epoch across all threads and updates the 
 * global durable epoch if necessary. It also writes the updated durable epoch
 * to a file for persistence.
 */
uint64_t Notifier::check_durable() {
    // calculate min(d_l)
    uint64_t min_dl = __atomic_load_n(&(ThLocalDurableEpoch[0]), __ATOMIC_ACQUIRE);
    for (unsigned int i = 1; i < num_logger_threads; i++) {
        uint64_t dl = __atomic_load_n(&(ThLocalDurableEpoch[i]), __ATOMIC_ACQUIRE);
        if (dl < min_dl) {
            min_dl = dl;
        }
    }

    // load and update durable epoch if necessary
    uint64_t dl = __atomic_load_n(&(DurableEpoch), __ATOMIC_ACQUIRE);
    if (dl < min_dl) {
        bool cas_success = __atomic_compare_exchange_n(&(DurableEpoch), &dl, min_dl, false, __ATOMIC_RELEASE, __ATOMIC_ACQUIRE);
        if (cas_success) {
            // store durable epoch
            pepoch_file_.write_epoch(min_dl);
        }
    }

    return min_dl;
}

/**
 * @brief Makes the current notification buffer durable and notifies the client.
 * 
 * @param nid_buffer Notification ID buffer containing epoch information.
 * @param quit Boolean flag to indicate whether to terminate the operation.
 * 
 * @details
 * Checks the durability of epochs and compares the minimum durable epoch with 
 * the minimum epoch in the nid_buffer.
 * If suitable for notification, it notifies the client with either the minimum
 * durable epoch or a termination signal based on the `quit` flag.
 */
void Notifier::make_durable(NidBuffer &nid_buffer, bool quit) {
    auto min_dl = check_durable();
    if (nid_buffer.min_epoch() > min_dl) return;

    // notify client
    uint64_t epoch = (quit) ? (~(uint64_t)0) : min_dl;
    nid_buffer.notify(epoch);
}