#pragma once

#include <cstdint>

// #include "atomic_tool.h"
// #include "procedure.h"
// #include "random.h"

#include "silo_tsc.h"
#include "../../../Include/consts.h"
#include "../../global_variables.h"

inline uint64_t atomicLoadGE() {
    uint64_t result = __atomic_load_n(&(GlobalEpoch), __ATOMIC_ACQUIRE);
    return result;
}

inline void atomicStoreThLocalEpoch(unsigned int thid, uint64_t newval) {
    __atomic_store_n(&(ThLocalEpoch[thid]), newval, __ATOMIC_RELEASE);
}

inline void atomicAddGE() {
    uint64_t expected, desired;
    expected = atomicLoadGE();
    for (;;) {
        desired = expected + 1;
        if (__atomic_compare_exchange_n(&(GlobalEpoch), &expected, desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) break;
    }
}

/**
 * @brief 2つのタイムスタンプの差が指定された閾値より大きいかどうかを確認する
 * @param start 開始タイムスタンプ
 * @param stop 終了タイムスタンプ
 * @param threshold 差を比較する閾値
 * @return startとstopの差がthresholdより大きい場合はtrue、それ以外はfalse
 */
inline static bool chkClkSpan(const uint64_t start, const uint64_t stop, const uint64_t threshold) {
    uint64_t diff = 0;
    diff = stop - start;
    if (diff > threshold) {
        return true;
    } else {
        return false;
    }
}

/**
 * @brief 指定された時間（ナノ秒）だけ処理を停止する
 * @param time 停止する時間（ナノ秒）
 * @note この関数は、処理の停止にrdtscpを用いており、指定された時間だけ処理をブロックする
 */
inline static void waitTime_ns(const uint64_t time) {
    uint64_t start = rdtscp();
    uint64_t end = 0;
    for (;;) {
        end = rdtscp();
        if (end - start > time * CLOCKS_PER_US) break;   // ns換算にしたいけど除算はコストが高そうなので1000倍して調整
    }
}

extern bool chkEpochLoaded();
extern void siloLeaderWork(uint64_t &epoch_timer_start, uint64_t &epoch_timer_stop);