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

// #include "global_variables.h"

// #include "utils/atomic_wrapper.h"

// #include "silo_cc/include/silo_logger.h"
// #include "silo_cc/include/silo_notifier.h"
// #include "silo_cc/include/silo_util.h"

// #include "../Include/structures.h"
// #include "../../common/cassa/structures.h"

// #include "../../common/cassa/third_party/json.hpp"

// #include "utils/transaction_manager.h"

// #include "../../common/third_party/json.hpp"

#include <string>
#include <vector>

#include "silo_cc/include/silo_transaction.h"

#include "../../common/openssl_utility.h"
#include "../../common/third_party/json.hpp"

#include "cassa_server.h"
#include "global_variables.h"
#include "cassa_common/structures.h"
#include "masstree/include/masstree.h"

#include "silo_cc/include/silo_logger.h"
#include "silo_cc/include/silo_notifier.h"
#include "silo_cc/include/silo_util.h"

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

// // for debug
size_t num_worker_threads;
size_t num_logger_threads;

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
}

void ecall_logger_thread_work(size_t logger_thid) {
    Logger logger(logger_thid, std::ref(notifier), std::ref(loggerResults[logger_thid]));
    notifier.add_logger(&logger);
    std::atomic<Logger*> *logp = &(logs[logger_thid]);
    logp->store(&logger);
    logger.worker();
    return;
}

int jsonToProcedures(std::vector<Procedure> &pro, const nlohmann::json &transactions_json) {
    pro.clear();
    for (const auto &transaction : transactions_json) {
        for (const auto &operation : transaction["transactions"]) {
            std::string operation_str = operation["operation"];
            OpType ope;

            if (operation_str == "INSERT") {
                ope = OpType::INSERT;
            } else if (operation_str == "READ") {
                ope = OpType::READ;
            } else if (operation_str == "WRITE") {
                ope = OpType::WRITE;
            } else {
                t_print(TLS_SERVER "Unknown operation: %s\n", operation_str.c_str());
                return -1;
                // std::cout << "Unknown operation: " << operation_str << std::endl;
                // assert(false);
            }

            std::string key_str = operation["key"];
            std::string value_str = operation.value("value", ""); // If value does not exist (e.g., READ), set empty string

            pro.emplace_back(ope, key_str, value_str); // TODO: txIDcounter
        }
    }

    return 0;
}

std::string execute_transaction(TxExecutor &trans, const std::string &json_str) {
    // convert std::string to nlohmann::json
    nlohmann::json json = nlohmann::json::parse(json_str);
    if (jsonToProcedures(trans.pro_set_, json) != 0) {
        // create error message (std::string)
        std::string return_message = "FIX ME";
        return return_message;
    }
RETRY:
    trans.durableEpochWork(trans.epoch_timer_start, trans.epoch_timer_stop, false); // TODO: falseをどうするか考える
    
    trans.begin();
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

void process_cassa(SSL* ssl_session) {
    // setup worker thread
    int worker_thid = 0;   // TODO: ここはスレッドIDを渡すようにする
    TxExecutor trans(worker_thid, ssl_session);
    WorkerResult &myres = std::ref(workerResults[worker_thid]);
    Logger *logger = nullptr;   // Log bufferを渡すLogger threadを紐づける
    std::atomic<Logger*> *logp = &(logs[worker_thid]);  // loggerのthreadIDを指定したいからgidを使う
    while ((logger = logp->load()) == nullptr) {
        waitTime_ns(100);
    }
    logger->add_tx_executor(trans);

    // クライアントからのデータ受信と処理
    std::string json_str;
    while (true) {
        json_str.clear();
        tls_read_from_session_peer(ssl_session, json_str);
        if (json_str == "/exit") {
            t_print(TLS_SERVER "Received exit command from client\n");
            break;
        } else {
            // 受け取ったデータを処理
            t_print(TLS_SERVER "Received data from client: %s\n", json_str.c_str());
            std::string processed_data = execute_transaction(trans, json_str);
            // 処理結果をクライアントに送信
            if (!processed_data.empty()) {
                tls_write_to_session_peer(ssl_session, processed_data);
            }
        }
    }

    // ワーカースレッドの終了処理
    trans.log_buffer_pool_.terminate();
    logger->worker_end(worker_thid);
}