#pragma once

#include <vector>
#include <cstdint>
#include <algorithm>

#include "../../utils/status.h"
#include "../../utils/db_key.h"
#include "../../utils/db_value.h"
#include "../../utils/db_tid.h"
#include "../../utils/atomic_wrapper.h"


#include "silo_element.h"
#include "silo_procedure.h"
#include "silo_log.h"
#include "silo_notifier.h"
#include "silo_log_buffer.h"

enum class TransactionStatus : uint8_t {
    Invalid,
    InFlight,
    Committed,
    Aborted,
};

class TxExecutor {
public:
    // operation sets
    std::vector<ReadElement> read_set_;
    std::vector<WriteElement> write_set_;
    std::vector<Procedure> pro_set_;

    // transaction status
    TransactionStatus status_;
    size_t worker_thid_;
    size_t logger_thid_;
    // should I implement result object?

    uint64_t epoch_timer_start;
    uint64_t epoch_timer_stop;

    // for calcurate TID
    TIDword mrctid_;
    TIDword max_rset_, max_wset_;

    // for logging
    LogBufferPool log_buffer_pool_;
    NotificationId nid_;
    uint32_t nid_counter_ = 0;

    // for garbage collection
    GarbageCollector gc_;

    TxExecutor(size_t worker_thid) : worker_thid_(worker_thid) {
        read_set_.clear();
        write_set_.clear();
        pro_set_.clear();

        max_rset_.obj_ = 0;
        max_wset_.obj_ = 0;
    }

    // トランザクションのライフサイクル管理
    void begin(); // トランザクションの開始
    void abort(); // トランザクションの中止
    bool commit(); // トランザクションのコミット
    
    // トランザクションの操作
    Status insert(std::string &str_key, std::string &str_value);
    // void tx_delete(Key &key); // キーの削除
    Status read(std::string &str_key);
    Status read_internal(Key &key, Value *value);
    Status write(std::string &str_key, std::string &str_value);
    // Status scan(Key &left_key, bool l_exclusive, Key &right_key, bool r_exclusive, std::vector<Value *> &result); // キーの範囲スキャン
    
    // 並行制御とロック管理
    void lockWriteSet(); // 書き込みセットのロック
    void unlockWriteSet(); // 書き込みセットのアンロック
    void unlockWriteSet(std::vector<WriteElement>::iterator end); // 指定位置までの書き込みセットのアンロック
    bool validationPhase(); // 検証フェーズの実行
    void writePhase(); // 書き込みフェーズの実行
    
    // Write-Ahead Logging
    void wal(std::uint64_t ctid); // Write-Ahead Loggingの実行
    
    // エポック管理
    bool isLeader(); // リーダーかどうかの確認
    void leaderWork(); // リーダーの作業
    void epochWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop); // エポックの作業
    void durableEpochWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop, const bool &quit); // 永続的なエポックの作業
    
    // 内部処理とヘルパーメソッド
    ReadElement *searchReadSet(Key &key); // 読み取りセットの検索
    WriteElement *searchWriteSet(Key &key); // 書き込みセットの検索
};