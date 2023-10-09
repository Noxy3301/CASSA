/**
 * This file contains macro definitions configuring various aspects of 
 * the database system program, including thread, data, execution, 
 * time, buffer, data size, display, optimization, and more.
 *
 * These macro definitions and data structures are shared between
 * App and Enclave, ensuring consistent data and mutual understanding,
 * thereby improving the readability and simplification of the 
 * overall system.
 */

#pragma once

#include "consts_benchmark.h"
#include "structures.h"

// -------------------
// Time configurations
// -------------------
// The epoch duration in milliseconds.
#define EPOCH_TIME 40
// Clocks per microsecond for the target hardware.
#define CLOCKS_PER_US 2900

// -------------------
// Thread configurations
// -------------------
// The number of worker threads.
#define WORKER_NUM 2
// The number of logger threads.
// Note: It is recommended to set the number of logger threads (LOGGER_NUM) to be less than the number of worker threads (WORKER_NUM).
//       Ideally, WORKER_NUM should be an integer multiple of LOGGER_NUM.
#define LOGGER_NUM 1

// -------------------
// Buffer configurations
// -------------------
// The number of buffers.
#define BUFFER_NUM 2
// The maximum number of log entries that can be buffered before triggering a publish.
#define MAX_BUFFERED_LOG_ENTRIES 1000
// The epoch difference (used for adjusting epoch-based mechanisms, but not used in this implementation).
#define EPOCH_DIFF 0

// -------------------
// Cache line size configurations
// -------------------
#define CACHE_LINE_SIZE 64