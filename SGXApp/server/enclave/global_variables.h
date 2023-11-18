#pragma once

#include <cstdint>
#include <vector>

#include "masstree/include/masstree.h"

extern Masstree masstree;

extern std::vector<uint64_t> ThLocalEpoch;
extern std::vector<uint64_t> CTIDW;
extern std::vector<uint64_t> ThLocalDurableEpoch;
extern uint64_t DurableEpoch;
extern uint64_t GlobalEpoch;

extern bool db_start;
extern bool db_quit;

extern size_t num_worker_threads;
extern size_t num_logger_threads;