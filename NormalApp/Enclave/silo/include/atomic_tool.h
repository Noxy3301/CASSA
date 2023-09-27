#pragma once

#include <cstdint>

#include "common.h"

#define INLINE __attribute__((always_inline)) inline

INLINE uint64_t atomicLoadGE() {
    uint64_t result = __atomic_load_n(&(GlobalEpoch), __ATOMIC_ACQUIRE);
    return result;
}

INLINE void atomicStoreThLocalEpoch(unsigned int thid, uint64_t newval) {
    __atomic_store_n(&(ThLocalEpoch[thid]), newval, __ATOMIC_RELEASE);
}

INLINE void atomicAddGE() {
    uint64_t expected, desired;
    expected = atomicLoadGE();
    for (;;) {
        desired = expected + 1;
        if (__atomic_compare_exchange_n(&(GlobalEpoch), &expected, desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQ_REL)) break;
    }
}