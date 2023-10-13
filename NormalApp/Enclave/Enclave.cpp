#include <vector>

#include <iostream> // for debug

#include "global_variables.h"

#include "Enclave.h"
#include "utils/atomic_wrapper.h"

#include "silo_cc/include/silo_logger.h"
#include "silo_cc/include/silo_notifier.h"
#include "silo_cc/include/silo_util.h"

#include "../Include/structures.h"

#include "utils/transaction_manager.h"

uint64_t GlobalEpoch = 1;                  // Global Epoch
std::vector<uint64_t> ThLocalEpoch;        // 各ワーカースレッドのLocal epoch, Global epochを参照せず、epoch更新時に更新されるLocal epochを参照してtxを処理する
std::vector<uint64_t> CTIDW;               // 各ワーカースレッドのCommit Timestamp ID, TID算出時にWorkerが発行したTIDとして用いられたものが保存される(TxExecutorのmrctid_と同じ値を持っているはず...)

uint64_t DurableEpoch;                     // Durable Epoch, 永続化された全てのデータのエポックの最大値を表す(epoch <= DのtxはCommit通知ができる)
std::vector<uint64_t> ThLocalDurableEpoch; // 各ロガースレッドのLocal durable epoch, Global durable epcohの算出に使う

std::vector<WorkerResult> workerResults;   // ワーカースレッドの実行結果を格納するベクター
std::vector<LoggerResult> loggerResults;   // ロガースレッドの実行結果を格納するベクター

std::atomic<Logger *> logs[LOGGER_NUM];    // ロガーオブジェクトのアトミックなポインタを保持する配列, 複数のスレッドからの安全なアクセスが可能
Notifier notifier;                         // 通知オブジェクト, スレッド間でのイベント通知を管理する

std::vector<bool> readys;                  // 各ワーカースレッドの準備状態を表すベクター, 全てのスレッドが準備完了するのを待つ

bool db_start = false;
bool db_quit = false;

size_t num_worker_threads;
size_t num_logger_threads;

Masstree masstree;
TransactionManager TxManager(WORKER_NUM);

// TODO: この実装は絶対に良くないので変える、一旦デバッグ用ということで
// std::condition_variable cv;
// std::mutex mtx;

void ecall_push_tx(size_t worker_thid, size_t txIDcounter, const uint8_t* tx_data, size_t tx_size) {
    // Push the transaction to the appropriate queue in TransactionManager
    // std::unique_lock<std::mutex> lock(mtx);
    TxManager.pushTransaction(worker_thid, txIDcounter, tx_data, tx_size);
    // cv.wait(lock); // コミットが完了するまで待つ
}

void ecall_initializeGlobalVariables(size_t worker_num, size_t logger_num) {
    // Global epochを初期化する
    // TODO: pepochから読み込むようにする

    ThLocalEpoch.resize(worker_num);
    CTIDW.resize(worker_num);
    ThLocalDurableEpoch.resize(logger_num);
    workerResults.resize(worker_num);
    loggerResults.resize(logger_num);
    readys.resize(worker_num);

    num_worker_threads = worker_num;
    num_logger_threads = logger_num;
}

void ecall_waitForReady() {
    // 全てのスレッドが準備完了するのを待つ
    for (size_t i = 0; i < readys.size(); i++) {
        while (!readys[i]) {
            waitTime_ns(100);
        }
    }
    db_start = true;
}

void ecall_terminateThreads() {
    db_quit = true;
}

void ecall_worker_thread_work(size_t worker_thid, size_t logger_thid) {
    // ワーカースレッドの準備
    TxExecutor trans(worker_thid);
    WorkerResult &myres = std::ref(workerResults[worker_thid]);

    // Log bufferを渡すLogger threadを紐づける
    Logger *logger = nullptr;
    std::atomic<Logger*> *logp = &(logs[logger_thid]);  // loggerのthreadIDを指定したいからgidを使う
    while ((logger = logp->load()) == nullptr) {
        waitTime_ns(100);
    }
    logger->add_tx_executor(trans);

    // worker_threadの準備完了フラグをセットして他のスレッドの準備完了を待機する
    readys[worker_thid] = true;
    while (!db_start) {
        waitTime_ns(100);
    }

    // ワーカースレッドのメインループ
    while (!db_quit) {
        // ワーカースレッドのメイン処理
        waitTime_ns(100);

    RETRY:
        // if (worker_thid == 0) trans.leaderWork();
        trans.durableEpochWork(trans.epoch_timer_start, trans.epoch_timer_stop, db_quit);

        if (__atomic_load_n(&db_quit, __ATOMIC_ACQUIRE)) break;

        std::optional<Procedure> proc = TxManager.popTransaction(worker_thid);
        if (proc.has_value()) {
            // procedureをKey/Valueに展開する
            size_t txID = proc.value().txIDcounter_;
            // OpType opType = proc.value().ope_;
            // Key key(proc.value().key_);
            // Value *value = new Value(proc.value().value_);  // TODO: heapに確保しているから適切なタイミングでdeleteする必要がある

            // procedureを実行する
            trans.begin();
            // TODO: 複数のprocedureをまとめて実行するようにする、すなわちpro_set_に登録して、一括で実行する

            Status status = Status::OK;
            OpType opType = proc.value().ope_;
            std::string key = proc.value().key_;
            std::string value = proc.value().value_;

            switch (opType) {
                case OpType::INSERT:
                    status = trans.insert(key, value);
                    if (status == Status::WARN_ALREADY_EXISTS) {
                        std::cout << "[ WARN     ] " << "key: " << key << " is already exists" << std::endl;
                        // TODO: transaction abort
                    }
                    break;
                case OpType::READ:
                    // TODO: 恐らくrmvでreadで取得したValueを使うはずだから渡せるように用意する
                    trans.read(key);
                    if (status == Status::WARN_NOT_FOUND) {
                        std::cout << "[ WARN     ] " << "key: " << key << " is not found" << std::endl;
                        // TODO: transaction abort
                    }
                    break;
                case OpType::WRITE:
                    trans.write(key, value);
                    if (status == Status::WARN_NOT_FOUND) {
                        std::cout << "[ WARN     ] " << "key: " << key << " is not found" << std::endl;
                        // TODO: transaction abort
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

            // std::cout << trans.read_set_.size() << std::endl;
            // std::cout << trans.write_set_.size() << std::endl;

            if (trans.validationPhase()) {
                trans.writePhase();
                storeRelease(myres.local_commit_count_, loadAcquire(myres.local_commit_count_) + 1);
                // std::cout << "this transaction has been committed" << std::endl;
                // cv.notify_one();
                ocall_print_commit_message(txID, worker_thid);
            } else {
                trans.abort();
                goto RETRY;
            }
        }
    }


    // ワーカースレッドの終了処理
    trans.log_buffer_pool_.terminate();
    logger->worker_end(worker_thid);

}

void ecall_logger_thread_work(size_t logger_thid) {
    Logger logger(logger_thid, std::ref(notifier), std::ref(loggerResults[logger_thid]));
    notifier.add_logger(&logger);
    std::atomic<Logger*> *logp = &(logs[logger_thid]);
    logp->store(&logger);
    logger.worker();
    return;
}