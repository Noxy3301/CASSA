#pragma once

#include "ansi_color_code.h"

#define LOG_INFO  GRN "[ INFO     ] " CRESET
#define LOG_WARN  YEL "[ WARNING  ] " CRESET
#define LOG_ERROR RED "[ ERROR    ] " CRESET
#define LOG_DEBUG CYN "[ DEBUG    ] " CRESET

#define LOG_SESSION_START_BMAG BMAG "[   "
#define LOG_SESSION_START_RED RED   "[   "
#define LOG_SESSION_END " ] " CRESET

#define LOG_SPACE     "             "