#pragma once

#include <openssl/ssl.h>

// OpenSSL Session Handler
#include "cassa_common/ssl_session_handler.hpp"

extern SSLSessionHandler ssl_session_handler;

int fcntl_set_nonblocking(int fd);