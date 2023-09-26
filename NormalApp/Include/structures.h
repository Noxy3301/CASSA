#pragma once

#include <stdint.h>

class WorkerResult {
public:
    uint64_t local_commit_count_ = 0;
    uint64_t local_abort_count_ = 0;
    uint64_t local_abort_vp1_count_ = 0;
    uint64_t local_abort_vp2_count_ = 0;
    uint64_t local_abort_vp3_count_ = 0;
    uint64_t local_abort_nullBuffer_count_ = 0;
};

class LoggerResult {
public:
    uint64_t byte_count_ = 0;
    uint64_t write_latency_ = 0;
    uint64_t wait_latency_ = 0;
};

enum AbortReason : uint8_t {
    ValidationPhase1,
    ValidationPhase2,
    ValidationPhase3,
    NullCurrentBuffer,
};

enum LoggerResultType : uint8_t {
    ByteCount,
    WriteLatency,
    WaitLatency,
};