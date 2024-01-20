/**
*
* MIT License
*
* Copyright (c) Open Enclave SDK contributors.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE
*
*/
#include <string>
#include <vector>
#include <sstream>
#include <mutex>

// SGX Libraries for sgx_rand_read()
#include "sgx_trts.h"

// CASSA Server Core Features
#include "cassa_server.h"
#include "global_variables.h"
#include "cassa_common/structures.h"

// CASSA/Silo_CC
#include "silo_cc/include/silo_transaction.h"
#include "silo_cc/include/silo_logger.h"
#include "silo_cc/include/silo_notifier.h"
#include "silo_cc/include/silo_util.h"

// CASSA/Silo_Recovery
#include "silo_r/include/silo_recovery.h"

// CASSA/Masstree
#include "masstree/include/masstree.h"

// CASSA/Utilities 
#include "cassa_common/transaction_balancer.hpp"

// OpenSSL Utilities
#include "../../common/openssl_utility.h"
#include "openssl_server/include/tls_server.h"
#include "cassa_common/json_message_formats.hpp"

// Third Party Libraries
#include "../../common/third_party/json.hpp"

uint64_t GlobalEpoch = 1;                  // Global Epoch
std::vector<uint64_t> ThLocalEpoch;        // Each worker thread processes transaction using its local epoch, updated during validationPhase or epochWork.
std::vector<uint64_t> CTIDW;               // The last committed TID, updated during the publishing of the current buffer phase.

uint64_t DurableEpoch;                     // Durable Epoch, 永続化された全てのデータのエポックの最大値を表す(epoch <= DのtxはCommit通知ができる)
std::vector<uint64_t> ThLocalDurableEpoch; // 各ロガースレッドのLocal durable epoch, Global durable epcohの算出に使う

std::vector<WorkerResult> workerResults;   // ワーカースレッドの実行結果を格納するベクター
std::vector<LoggerResult> loggerResults;   // ロガースレッドの実行結果を格納するベクター

std::atomic<Logger *> logs[LOGGER_NUM];    // ロガーオブジェクトのアトミックなポインタを保持する配列, 複数のスレッドからの安全なアクセスが可能
Notifier notifier;                         // 通知オブジェクト, スレッド間でのイベント通知を管理する

Masstree masstree;

// for debug
size_t num_worker_threads;
size_t num_logger_threads;

SSLSessionHandler ssl_session_handler;
TransactionBalancer tx_balancer;

void ecall_perform_recovery() {
    RecoveryExecutor recovery_executor;
    recovery_executor.perform_recovery();
}

void ecall_initialize_global_variables(size_t worker_num, size_t logger_num) {
    // Global epochを初期化する
    // TODO: pepochから読み込むようにする

    ThLocalEpoch.resize(worker_num);
    CTIDW.resize(worker_num);
    ThLocalDurableEpoch.resize(logger_num);
    workerResults.resize(worker_num);
    loggerResults.resize(logger_num);

    num_worker_threads = worker_num;
    num_logger_threads = logger_num;

    tx_balancer.init(worker_num);
}

int fcntl_set_nonblocking(int fd) {
    int retval;
    sgx_status_t status = u_fcntl_set_nonblocking(&retval, fd);
    if (status != SGX_SUCCESS) {
        t_print(TLS_SERVER "SGX error while setting non-blocking: %d\n", status);
        return -1;
    }
    return retval;
}


void ecall_ssl_connection_acceptor(char* server_port, int keep_server_up) {
    SSL_CTX *ssl_server_ctx = nullptr;
    int server_socket_fd = -1;

    // set up SSL session
    if (set_up_ssl_session(server_port, &ssl_server_ctx, &server_socket_fd) != 0) {
        t_print(TLS_SERVER "Failed to set up SSL session\n");
        return;
    }

    // wait for client connection
    while (true) {
        // TODO: 終了条件を設定する？
        SSL *ssl_session = accept_client_connection(server_socket_fd, ssl_server_ctx);
        if (ssl_session == nullptr) {
            t_print(TLS_SERVER "Error: accept_client_connection() failed\n");
            break;
        } else {
            std::string session_id = ssl_session_handler.addSession(ssl_session);
            t_print(TLS_SERVER "Accepted client connection (session_id: %s)\n", session_id.c_str());

            // print active session
            t_print(TLS_SERVER "Active session: %lu\n", ssl_session_handler.ssl_sessions_.size());
        }
    }

    // clean up
    if (ssl_server_ctx) SSL_CTX_free(ssl_server_ctx);
    ocall_close(nullptr, server_socket_fd);
}

void ecall_ssl_session_monitor() {
    while (true) {
        // TODO: 終了条件を設定する？

        auto it = ssl_session_handler.ssl_sessions_.begin();
        while (it != ssl_session_handler.ssl_sessions_.end()) {
            std::string session_id = it->first;
            SSL* ssl_session = it->second.ssl_session;

            // lock mutex
            std::lock_guard<std::mutex> lock(*it->second.ssl_session_mutex);

            // check if the session is alive
            if (!ssl_session || SSL_get_shutdown(ssl_session)) {
                t_print(TLS_SERVER "Session ID: %s has closed\n", session_id.c_str());
                // remove the session from the map and reset iterator
                // NOTE: SSL_ERROR_NONE is normal termination
                ssl_session_handler.removeSession(session_id, SSL_ERROR_NONE);
                it = ssl_session_handler.ssl_sessions_.begin();
                
                // print active session
                t_print(TLS_SERVER "Active session: %lu\n", ssl_session_handler.ssl_sessions_.size());
                continue;
            }

            // check if the session has received data
            char buffer[1];
            int result = SSL_peek(ssl_session, buffer, sizeof(buffer));
            if (result > 0) {
                // receive data from the session
                std::string received_data;
                tls_read_from_session_peer(ssl_session, received_data);
                t_print(TLS_SERVER "Received data from client: %s\n", received_data.c_str());
                
                // handle if reveiced data is command
                std::string command, token_sec, token_nsec;
                std::istringstream iss(received_data);
                std::getline(iss, command, ' ');    // get first token
                if (command == "/get_session_id") {
                    // get timestamp from the session
                    if (!std::getline(iss, token_sec, ' ') || !std::getline(iss, token_nsec)) {
                        t_print(TLS_SERVER "Invalid command format\n");
                        return;
                    }

                    // convert string to long int
                    long int timestamp_sec = std::stol(token_sec);
                    long int timestamp_nsec = std::stol(token_nsec);

                    // set timestamp to SSLSession object
                    ssl_session_handler.setTimestamp(session_id, timestamp_sec, timestamp_nsec);

                    // notify session ID to client
                    t_print(TLS_SERVER "\033[32m" "Session ID: " "\033[0m" "%s\n", session_id.c_str());
                    tls_write_to_session_peer(ssl_session, session_id);
                    it++;
                    continue;
                }
                
                // TODO: execute transaction
                tx_balancer.putTransaction(received_data);

                // // debug
                // tls_write_to_session_peer(ssl_session, received_data);
            } else if (result == 0) {
                // remove the session from the map and reset iterator
                // NOTE: SSL_ERROR_NONE is normal termination
                ssl_session_handler.removeSession(session_id, SSL_ERROR_NONE);
                it = ssl_session_handler.ssl_sessions_.begin();

                // print active session
                t_print(TLS_SERVER "Active session: %lu\n", ssl_session_handler.ssl_sessions_.size());
                continue;
            } else { // result <= 0
                // remove the session from the map and reset iterator
                int ssl_error_code = SSL_get_error(ssl_session, result);
                ssl_session_handler.removeSession(session_id, ssl_error_code);

                // print active session (except for SSL_ERROR_WANT_READ)
                if (ssl_error_code != SSL_ERROR_WANT_READ) {
                    it = ssl_session_handler.ssl_sessions_.begin();
                    t_print(TLS_SERVER "Active session: %lu\n", ssl_session_handler.ssl_sessions_.size());
                    continue;
                }
            }
            it++;
        }
    }
}

/**
 * @brief Compares two timestamps.
 *
 * This function compares two timestamps (seconds and nanoseconds) 
 * to determine which one is more recent.
 *
 * @param timestamp_sec The seconds part of the new timestamp to compare.
 * @param timestamp_nsec The nanoseconds part of the new timestamp to compare.
 * @param latest_timestamp_sec The seconds part of the latest known timestamp.
 * @param latest_timestamp_nsec The nanoseconds part of the latest known timestamp.
 *
 * @return Returns 1 if the new timestamp is more recent than the latest timestamp.
 *         Returns -1 if the new timestamp is older than the latest timestamp.
 *         Returns 0 if both timestamps are equal.
 */
int compare_timestamps(long int timestamp_sec,
                       long int timestamp_nsec,
                       long int latest_timestamp_sec,
                       long int latest_timestamp_nsec) {
    if (timestamp_sec > latest_timestamp_sec) {
        return 1;
    } else if (timestamp_sec < latest_timestamp_sec) {
        return -1;
    } else {
        if (timestamp_nsec > latest_timestamp_nsec) {
            return 1;
        } else if (timestamp_nsec < latest_timestamp_nsec) {
            return -1;
        } else {
            return 0;
        }
    }
}

/**
 * @brief Converts a JSON string to a vector of `Procedure` objects.
 * 
 * @param session_id A string to store the client session ID.
 * @param procedures A vector of `Procedure` objects to store the result.
 * @param json_str A JSON string to be converted.
 * 
 * @return Returns 0 if the conversion is successful.
 *         Returns -1 if the timestamp is older than the latest timestamp.
 *         Returns -2 if the operation is unknown.
*/
int json_to_procedures(std::string &session_id, std::vector<Procedure> &procedures, const std::string &json_str) {
    // parse json
    auto json = nlohmann::json::parse(json_str);
    std::string client_session_id = json["client_sessionID"];
    long int timestamp_sec = json["timestamp_sec"].get<long int>();
    long int timestamp_nsec = json["timestamp_nsec"].get<long int>();
    session_id = client_session_id;

    // Check timestamp to prevent replay attack
    if (compare_timestamps(timestamp_sec, timestamp_nsec,
                           ssl_session_handler.getTimestampSec(client_session_id),
                           ssl_session_handler.getTimestampNsec(client_session_id)) <= 0) {
        t_print(TLS_SERVER "Replay attack detected or old timestamp received.\n");
        return -1;
    }

    // Update timestamp in SSL session
    ssl_session_handler.setTimestamp(client_session_id, timestamp_sec, timestamp_nsec);
    t_print(DEBUG TLS_SERVER "client_session_id: %s, timestamp_sec: %ld, timestamp_nsec: %ld\n", client_session_id.c_str(), timestamp_sec, timestamp_nsec); // for debug

    // retrieve operations
    const auto &transactions_json = json["transaction"];
    procedures.clear();

    for (const auto &operation : transactions_json) {
        std::string operation_str = operation["operation"];
        OpType op_type;

        if (operation_str == "INSERT") {
            op_type = OpType::INSERT;
        } else if (operation_str == "READ") {
            op_type = OpType::READ;
        } else if (operation_str == "WRITE") {
            op_type = OpType::WRITE;
        } else {
            t_print(TLS_SERVER "Unknown operation: %s\n", operation_str.c_str());
            return -2;
        }

        std::string key_str = operation["key"];
        std::string value_str = operation.value("value", "");   // If value does not exist (e.g., READ), set empty string
        procedures.emplace_back(op_type, key_str, value_str);
    }

    return 0;
}

/**
 * @brief Executes a transaction based on the given JSON string.
 * 
 * @param trans A reference to the TxExecutor object
 * @param json_str A JSON formatted string representing the transaction operations.
 * @param error_message_content A reference to a string where error messages, if any,
 *                              will be stored.
 * 
 * @return Returns 0 if the transaction is successfully committed.
 *         Returns -1 if there is a problem with the JSON conversion.
 *           (e.g., replay attack detection or unknown operation.)
 *         Returns -2 if the transaction execution fails and is aborted.
 */
int execute_transaction(TxExecutor &trans, const std::string &json_str, std::string &error_message_content) {
    // convert json(string) to procedures
    int convert_result = json_to_procedures(trans.session_id_, trans.pro_set_, json_str);

    // Check the result of conversion using switch
    switch (convert_result) {
        case 0:
            break;  // Success, do nothing
        case -1:
            error_message_content = "Error: Replay attack detected or old timestamp received.";
            return -1;  // json conversion failed
        case -2:
            error_message_content = "Error: Unknown operation in transaction.";
            return -1;  // json conversion failed
        default:
            error_message_content = "Error: Unexpected error occurred during transaction processing.";
            return -1;  // json conversion failed
    }

    // Proceed with transaction execution if the conversion is successful
RETRY:
    trans.durableEpochWork(trans.epoch_timer_start, trans.epoch_timer_stop, false); // TODO: falseをどうするか考える
    
    trans.begin(trans.session_id_);
    Status status = Status::OK;
    std::string read_value; // Store the value retrieved by the read operation

    for (auto itr = trans.pro_set_.begin(); itr != trans.pro_set_.end(); itr++) {
        switch ((*itr).ope_) {
            case OpType::INSERT:
                status = trans.insert((*itr).key_, (*itr).value_);
                if (status == Status::WARN_ALREADY_EXISTS) {
                    t_print(DEBUG TLS_SERVER "Key: %s is already exists\n", (*itr).key_.c_str());
                    error_message_content += "Key: " + (*itr).key_ + " is already exists\n";
                }
                break;
            case OpType::READ:
                status = trans.read((*itr).key_, read_value);
                if (status == Status::WARN_NOT_FOUND) {
                    t_print(DEBUG TLS_SERVER "Key: %s is not found\n", (*itr).key_.c_str());
                    error_message_content += "Key: " + (*itr).key_ + " is not found\n";
                } else if (status == Status::OK) {
                    trans.nid_.read_key_value_pairs.emplace_back((*itr).key_, read_value);
                }
                break;
            case OpType::WRITE:
                status = trans.write((*itr).key_, (*itr).value_);
                if (status == Status::WARN_NOT_FOUND) {
                    t_print(DEBUG TLS_SERVER "Key: %s is not found\n", (*itr).key_.c_str());
                    error_message_content += "Key: " + (*itr).key_ + " is not found\n";
                }
                break;
            // case OpType::RMW:
            //     trans.rmw(key, value);
            //     break;
            // case OpType::SCAN:
            //     trans.scan(key);
            //     break;
            // case OpType::DELETE:
            //     trans.del(key);
            //     break;
            default:
                assert(false);  // ここには来ないはず
                break;
        }
    
        if (status != Status::OK) {
            trans.abort();
            error_message_content += "Transaction has been aborted.\n";
            return -2; // transaction execution failed
        }
    }

    if (trans.validationPhase()) {
        trans.writePhase();
        t_print(DEBUG TLS_SERVER "transaction has been committed\n");
        return 0;
    } else {
        trans.abort();
        goto RETRY;
    }
}

void ecall_execute_worker_task(size_t worker_thid, size_t logger_thid) {
    TxExecutor trans(worker_thid);
    WorkerResult &myres = std::ref(workerResults[worker_thid]);
    Logger *logger = nullptr;   // Log bufferを渡すLogger threadを紐づける
    std::atomic<Logger*> *logp = &(logs[logger_thid]);  // loggerのthreadIDを指定したいからgidを使う
    while ((logger = logp->load()) == nullptr) {
        waitTime_ns(100);
    }
    logger->add_tx_executor(trans);
    trans.epoch_timer_start = rdtscp();

    t_print(TLS_SERVER "wID: %d | Worker thread has started\n", worker_thid);

    // クライアントからのデータ受信と処理
    std::string json_str;
    while (true) {
        // Advance global epoch and syncronize thread local epoch
        trans.durableEpochWork(trans.epoch_timer_start, trans.epoch_timer_stop, false); // TODO: falseをどうするか考える
        
        // receive data from TransactionBalancer
        json_str = tx_balancer.getTransaction(worker_thid);
        if (json_str.empty()) {
            waitTime_ns(100);
            continue;
        }

        // execute transaction
        std::string error_message_content = "OK";
        int result = execute_transaction(trans, json_str, error_message_content);

        /**
         * If the result is 0 (i.e., success) and the transaction is read-only,
         * send a success message to the client directly from here
        */
        std::string json_message_dump;
        bool send_responce = false;
        if (result == 0 && trans.write_set_.size() == 0) {
            t_print(DEBUG TLS_SERVER "read-only transaction, read items: %d\n", trans.nid_.read_key_value_pairs.size());
            json_message_dump = create_message(result, error_message_content, trans.nid_.read_key_value_pairs).dump();
            send_responce = true;
        } else if (result != 0) {
            json_message_dump = create_message(result, error_message_content).dump();
            send_responce = true;
        }

        // send message to client if read-only transaction or transaction execution failed
        if (send_responce) {
            SSLSession *session = ssl_session_handler.getSession(trans.session_id_);
            if (session != nullptr) {
                SSL *ssl = session->ssl_session;
                std::lock_guard<std::mutex> lock(*session->ssl_session_mutex);
                tls_write_to_session_peer(ssl, json_message_dump);
            } else {
                t_print(DEBUG TLS_SERVER "session == nullptr, skipped\n");
            }
        }
    }

    // ワーカースレッドの終了処理
    trans.log_buffer_pool_.terminate();
    logger->worker_end(worker_thid);
}

void ecall_execute_logger_task(size_t logger_thid) {
    Logger logger(logger_thid, std::ref(notifier), std::ref(loggerResults[logger_thid]));
    notifier.add_logger(&logger);
    std::atomic<Logger*> *logp = &(logs[logger_thid]);
    logp->store(&logger);
    t_print(TLS_SERVER "lID: %d | Logger thread has started\n", logger_thid);
    logger.worker();
    return;
}