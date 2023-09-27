#include <algorithm> // for sort function

#include "../../Include/consts.h"

#include "include/atomic_tool.h"
#include "include/atomic_wrapper.h"
#include "include/transaction.h"
#include "include/log.h"
#include "include/util.h"

TxExecutor::TxExecutor(int thid) : thid_(thid) {
    read_set_.reserve(MAX_OPE);
    write_set_.reserve(MAX_OPE);
    pro_set_.reserve(MAX_OPE);

    max_rset_.obj_ = 0;
    max_wset_.obj_ = 0;
}

void TxExecutor::begin() {
    status_ = TransactionStatus::InFlight;
    max_wset_.obj_ = 0;
    max_rset_.obj_ = 0;
    abort_res_ = 0;
    nid_ = NotificationId(nid_counter_++, thid_, rdtscp());
}

void TxExecutor::read(uint64_t key) {
    TIDword expected, check;
    if (searchReadSet(key) || searchWriteSet(key)) goto FINISH_READ;
    Tuple *tuple;
    
    // TODO: [masstree] masstree_getに変更
    tuple = Table.get(key);
    
    expected.obj_ = loadAcquire(tuple->tidword_.obj_);
    
    for (;;) {
        // (a) reads the TID word, spinning until the lock is clear
        while (expected.lock) {
            expected.obj_ = loadAcquire(tuple->tidword_.obj_);
        }
        // (b) checks whether the record is the latest version
        // omit. because this is implemented by single version

        // (c) reads the data
        // memcpy(return_val_, tuple->value_, sizeof(uint64_t));
        return_val_ = tuple->val_;    // 値渡し(memcpyと同じ挙動)になっているはず

        // (d) performs a memory fence
        // don't need.
        // order of load don't exchange.    // Q:解説募
        check.obj_ = loadAcquire(tuple->tidword_.obj_);
        if (expected == check) break;
        expected = check;
    }
    read_set_.emplace_back(key, tuple, return_val_, expected);

    FINISH_READ:
    return;
}

void TxExecutor::write(uint64_t key, uint64_t val) {
    if (searchWriteSet(key)) goto FINISH_WRITE;
    Tuple *tuple;
    ReadElement *re;
    re = searchReadSet(key);
    if (re) {   //HACK: 仕様がわかってないよ(田中先生も)
        tuple = re->rcdptr_;
    } else {
        // TODO: [masstree] masstree_getに変更
        // TODO: getとかに一元化したほうが良いかも
        tuple = Table.get(key);
    }
    write_set_.emplace_back(key, tuple, val);

    FINISH_WRITE:
    return;
}

ReadElement *TxExecutor::searchReadSet(uint64_t key) {
    for (auto itr = read_set_.begin(); itr != read_set_.end(); itr++) {
        if ((*itr).key_ == key) return &(*itr);
    }
    return nullptr;
}

WriteElement *TxExecutor::searchWriteSet(std::uint64_t key) {
    for (auto itr = write_set_.begin(); itr != write_set_.end(); itr++) {
        if ((*itr).key_ == key) return &(*itr);
    }
    return nullptr;
}

void TxExecutor::unlockWriteSet() {
    TIDword expected, desired;
    for (auto itr = write_set_.begin(); itr != write_set_.end(); itr++) {
        expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
        desired = expected;
        desired.lock = 0;
        storeRelease((*itr).rcdptr_->tidword_.obj_, desired.obj_);
    }
}

void TxExecutor::unlockWriteSet(std::vector<WriteElement>::iterator end) {
    TIDword expected, desired;
    for (auto itr = write_set_.begin(); itr != end; itr++) {
        expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
        desired = expected;
        desired.lock = 0;
        storeRelease((*itr).rcdptr_->tidword_.obj_, desired.obj_);
    }
}

void TxExecutor::lockWriteSet() {
    TIDword expected, desired;
    for (auto itr = write_set_.begin(); itr != write_set_.end(); itr++) {
        for (;;) {
            expected.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
            if (expected.lock) {
#if NO_WAIT_LOCKING_IN_VALIDATION
                this->status_ = TransactionStatus::Aborted; // w-w conflictは即abort
                if (itr != write_set_.begin()) unlockWriteSet(itr);
                return;
#endif
            } else {
                desired = expected;
                desired.lock = 1;
                if (compareExchange((*itr).rcdptr_->tidword_.obj_, expected.obj_, desired.obj_))
                break;
            }
        }
        max_wset_ = std::max(max_wset_, expected);
    }
}

bool TxExecutor::validationPhase() {
    // Phase1, sorting write_set_
    sort(write_set_.begin(), write_set_.end());
    lockWriteSet();
#if NO_WAIT_LOCKING_IN_VALIDATION
    if (this->status_ == TransactionStatus::Aborted) {
        abort_res_ = 1;
        return false;  // w-w conflict検知時に弾く用
    }
#endif
    asm volatile("":: : "memory");
    atomicStoreThLocalEpoch(thid_, atomicLoadGE());
    asm volatile("":: : "memory");
    // Phase2, Read validation
    TIDword check;
    for (auto itr = read_set_.begin(); itr != read_set_.end(); itr++) {
        // 1. tid of read_set_ changed from it that was got in Read Phase.
        check.obj_ = loadAcquire((*itr).rcdptr_->tidword_.obj_);
        if ((*itr).get_tidword().epoch != check.epoch || (*itr).get_tidword().TID != check.TID) {
            this->status_ = TransactionStatus::Aborted;
            abort_res_ = 2;
            unlockWriteSet();
            return false;
        }
        // 2. "omit" if (!check.latest) return false;
        
        // 3. the tuple is locked and it isn't included by its write set.
        if (check.lock && !searchWriteSet((*itr).key_)) {
            this->status_ = TransactionStatus::Aborted;
            abort_res_ = 3;
            unlockWriteSet();
            return false;
        }
        max_rset_ = std::max(max_rset_, check); // TID算出用
    }

    // goto Phase 3
    this->status_ = TransactionStatus::Committed;
    return true;
}

void TxExecutor::wal(std::uint64_t ctid) {
    TIDword old_tid;
    TIDword new_tid;
    old_tid.obj_ = loadAcquire(CTIDW[thid_]);
    new_tid.obj_ = ctid;
    bool new_epoch_begins = (old_tid.epoch != new_tid.epoch);
    log_buffer_pool_.push(ctid, nid_, write_set_, write_val_, new_epoch_begins);
    if (new_epoch_begins) {
      // store CTIDW
      __atomic_store_n(&(CTIDW[thid_]), ctid, __ATOMIC_RELEASE);
    }
}

void TxExecutor::writePhase() {
    TIDword tid_a, tid_b, tid_c;    // TIDの算出
    // (a) transaction中でRead/Writeの最大TIDより1大きい
    tid_a = std::max(max_wset_, max_rset_);
    tid_a.TID++;
    // (b) workerが発行したTIDより1大きい
    tid_b = mrctid_;
    tid_b.TID++;
    // (c) 上位32bitがcurrent epoch
    tid_c.epoch = ThLocalEpoch[thid_];

    TIDword maxtid = std::max({tid_a, tid_b, tid_c});
    maxtid.lock = 0;
    maxtid.latest = 1;
    mrctid_ = maxtid;

    // TODO: wal
    wal(maxtid.obj_);

    // write
    for (auto itr = write_set_.begin(); itr != write_set_.end(); itr++) {
        (*itr).rcdptr_->val_ = (*itr).get_val();    // write_set_に登録したvalをTable(rcdptr_)のvalに移す
        storeRelease((*itr).rcdptr_->tidword_.obj_, maxtid.obj_);
    }        
    read_set_.clear();
    write_set_.clear();
}

void TxExecutor::abort() {
    read_set_.clear();
    write_set_.clear();
}



bool TxExecutor::pauseCondition() {
    auto dlepoch = loadAcquire(ThLocalDurableEpoch[logger_thid_]);
    return loadAcquire(ThLocalEpoch[thid_]) > dlepoch + EPOCH_DIFF;
}

void TxExecutor::epochWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop) {
    waitTime_ns(1*1000);
    if (thid_ == 0) leaderWork(epoch_timer_start, epoch_timer_stop);
    TIDword old_tid;
    old_tid.obj_ = loadAcquire(CTIDW[thid_]);
    // load GE
    atomicStoreThLocalEpoch(thid_, atomicLoadGE());
    uint64_t new_epoch = loadAcquire(ThLocalEpoch[thid_]);
    if (old_tid.epoch != new_epoch) {
        TIDword tid;
        tid.epoch = new_epoch;
        tid.lock = 0;
        tid.latest = 1;
        // store CTIDW
        __atomic_store_n(&(CTIDW[thid_]), tid.obj_, __ATOMIC_RELEASE);
    }
}

// TODO:コピペなので要確認
void TxExecutor::durableEpochWork(uint64_t &epoch_timer_start,
                                   uint64_t &epoch_timer_stop, const bool &quit) {
  std::uint64_t t = rdtscp();
  // pause this worker until Durable epoch catches up
  if (EPOCH_DIFF > 0) {
    if (pauseCondition()) {
      log_buffer_pool_.publish();
      do {
        epochWork(epoch_timer_start, epoch_timer_stop);
        if (loadAcquire(quit)) return;
      } while (pauseCondition());
    }
  }
  while (!log_buffer_pool_.is_ready()) {
    epochWork(epoch_timer_start, epoch_timer_stop);
    if (loadAcquire(quit)) return;
  }
  if (log_buffer_pool_.current_buffer_==NULL) std::abort();
  abort_res_ = 4;
  // sres_lg_->local_wait_depoch_latency_ += rdtscp() - t;
  // NOTE: sres_lg_使ってないので
}
