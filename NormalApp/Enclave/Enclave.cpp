#include <vector>

#include "global_variables.h"

#include "Enclave.h"
#include "utils/atomic_wrapper.h"

#include "silo_cc/include/silo_logger.h"
#include "silo_cc/include/silo_notifier.h"
#include "silo_cc/include/silo_util.h"

#include "../Include/structures.h"

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

Masstree Table;

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