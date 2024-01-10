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

// CASSA/Masstree
#include "masstree/include/masstree.h"

// CASSA/Utilities 
#include "cassa_common/transaction_balancer.hpp"

// OpenSSL Utilities
#include "../../common/openssl_utility.h"
#include "openssl_server/include/tls_server.h"

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
            SSL* ssl_session = it->second;

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
                if (received_data == "/get_session_id") {
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
 * @brief Converts a JSON string to a vector of `Procedure` objects.
 * 
 * @param procedures A vector of `Procedure` objects to store the result.
 * @param json_str A JSON string to be converted.
*/
int json_to_procedures(std::vector<Procedure> &procedures, const std::string &json_str) {
    auto json = nlohmann::json::parse(json_str);

    // parse transaction
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
            return -1;
        }

        std::string key_str = operation["key"];
        std::string value_str = operation.value("value", "");   // If value does not exist (e.g., READ), set empty string
        procedures.emplace_back(op_type, key_str, value_str);
    }

    return 0;
}

std::string execute_transaction(TxExecutor &trans, const std::string &json_str) {
    // convert json(string) to procedures
    if (json_to_procedures(trans.pro_set_, json_str) != 0) {
        // create error message (std::string)
        std::string return_message = "FIX ME";
        return return_message;
    }
RETRY:
    trans.durableEpochWork(trans.epoch_timer_start, trans.epoch_timer_stop, false); // TODO: falseをどうするか考える
    
    trans.begin(""); // TODO: session_idを渡すようにする
    Status status = Status::OK;
    std::vector<std::string> values;
    std::string return_value;  // 返される値を格納するための string オブジェクトを作成
    std::string return_message;
    for (auto itr = trans.pro_set_.begin(); itr != trans.pro_set_.end(); itr++) {
        switch ((*itr).ope_) {
            case OpType::INSERT:
                status = trans.insert((*itr).key_, (*itr).value_);
                if (status == Status::WARN_ALREADY_EXISTS) {
                    t_print(TLS_SERVER "key: %s is already exists\n", (*itr).key_.c_str());
                    return_message += "key: " + (*itr).key_ + " is already exists\n";
                }
                break;
            case OpType::READ:
                // TODO: 恐らくrmvでreadで取得したValueを使うはずだから渡せるように用意する
                status = trans.read((*itr).key_, return_value);
                if (status == Status::WARN_NOT_FOUND) {
                    t_print(TLS_SERVER "key: %s is not found\n", (*itr).key_.c_str());
                    return_message += "key: " + (*itr).key_ + " is not found\n";
                } else if (status == Status::OK) {
                    values.push_back(return_value);
                }
                break;
            case OpType::WRITE:
                status = trans.write((*itr).key_, (*itr).value_);
                if (status == Status::WARN_NOT_FOUND) {
                    t_print(TLS_SERVER "key: %s is not found\n", (*itr).key_.c_str());
                    return_message += "key: " + (*itr).key_ + " is not found\n";
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
            // std::cout << "transaction has been aborted." << std::endl;
            // t_print(TLS_SERVER 
            trans.abort();
            return_message += "transaction has been aborted.\n";
            return return_message;
        }
    }

    if (trans.validationPhase()) {
        trans.writePhase();
        t_print(DEBUG TLS_SERVER "transaction has been committed\n");
        // storeRelease(myres.local_commit_count_, loadAcquire(myres.local_commit_count_) + 1);
        // std::cout << "this transaction has been committed" << std::endl;
        // cv.notify_one();
        // TODO: この処理をnotifier側でやる
        // ocall_print_commit_message(txID, worker_thid);
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

    // for debug
    uint64_t prev_global_epoch = 0;

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
        execute_transaction(trans, json_str);
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