#pragma once

// AppとEnclaveでのEcall/Ocallを疑似的に再現するために相互インクルードする
#include "../App/App.h"

#include <cstddef>

#include "../Include/consts.h"

extern void ecall_push_tx(size_t worker_thid, size_t txIDcounter, const uint8_t* tx_data, size_t tx_size);
extern void ecall_initializeGlobalVariables(size_t worker_num, size_t logger_num);
extern void ecall_waitForReady();
extern void ecall_terminateThreads();
extern void ecall_worker_thread_work(size_t worker_thid, size_t logger_thid);
extern void ecall_logger_thread_work(size_t logger_thid);