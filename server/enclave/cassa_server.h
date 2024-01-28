#pragma once

#include <openssl/ssl.h>

// OpenSSL Session Handler
#include "cassa_common/ssl_session_handler.hpp"

// CASSA/Masstree
#include "masstree/include/masstree.h"

extern SSLSessionHandler ssl_session_handler;
extern Masstree masstree;
extern uint64_t GlobalEpoch;

int fcntl_set_nonblocking(int fd);