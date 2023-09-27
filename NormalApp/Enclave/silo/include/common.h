#pragma once

#include <cstdint>
#include <vector>

#include "tuple.h"

#include "../../Include/consts.h"
#include "../OCH.cpp"

#if INDEX_PATTERN == INDEX_USE_MASSTREE
// do declare masstree table
#elif INDEX_PATTERN == INDEX_USE_OCH
extern OptCuckoo<Tuple*> Table;
#endif

extern std::vector<uint64_t> ThLocalEpoch;
extern std::vector<uint64_t> CTIDW;
extern std::vector<uint64_t> ThLocalDurableEpoch;
extern uint64_t DurableEpoch;
extern uint64_t GlobalEpoch;

// std::vector<Result> results(THREAD_NUM); //TODO: mainの方でもsiloresultを定義しているので重複回避目的で名前変更しているけどここら辺どうやって扱うか考えておく

extern bool start;
extern bool quit;