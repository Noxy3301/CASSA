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
// Buffer configurations
// -------------------
// The number of buffers.
#define BUFFER_NUM 2
// The size of each buffer in bytes.
#define BUFFER_SIZE 512