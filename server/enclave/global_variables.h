#pragma once

#include <cstdint>
#include <vector>
#include <atomic>

#include "masstree/include/masstree.h"

extern Masstree masstree;

extern std::vector<uint64_t> ThLocalEpoch;
extern std::vector<uint64_t> CTIDW;
extern std::vector<uint64_t> ThLocalDurableEpoch;
extern uint64_t DurableEpoch;
extern uint64_t GlobalEpoch;

extern size_t num_worker_threads;
extern size_t num_logger_threads;