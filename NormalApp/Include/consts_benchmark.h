#pragma once

// -------------------
// Thread configurations
// -------------------
// The number of worker threads.
#define WORKER_NUM 9
// The number of logger threads.
#define LOGGER_NUM 3

// -------------------
// Data configurations
// -------------------
// The total number of tuples.
#define TUPLE_NUM 1000000

// -------------------
// Execution configurations
// -------------------
// The execution time in seconds.
#define EXTIME 3
// The size of a cache line in bytes.
#define CACHE_LINE_SIZE 64

// -------------------
// Operation configurations
// -------------------
// The maximum number of operations.
#define MAX_OPE 10
// The ratio (percentage) of read operations.
#define RRAITO 50
// Flag to determine if using YCSB (Yahoo! Cloud Serving Benchmark).
#define YCSB false
// The skewness for Zipf distribution. 0 indicates uniform distribution.
#define ZIPF_SKEW 0

// -------------------
// Data size configurations
// -------------------
// The size of the value in bytes.
#define VAL_SIZE 4
// The size of a page in bytes.
#define PAGE_SIZE 4096
// The size of a log set.
#define LOGSET_SIZE 1000

// -------------------
// Display configurations
// -------------------
// Flag to determine if detailed results should be shown. 0 for no, 1 for yes.
#define SHOW_DETAILS 1

// -------------------
// Optimization configurations
// -------------------
// Flag for "no-wait" optimization during validation. 1 to enable, 0 to disable.
#define NO_WAIT_LOCKING_IN_VALIDATION 1