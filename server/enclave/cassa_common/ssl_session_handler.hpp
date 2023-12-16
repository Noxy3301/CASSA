#pragma once

#include <openssl/ssl.h>
#include <map>
#include <vector>

#include "random.h"

class SSLSessionHandler {
public:
    std::map<uint64_t, SSL*> ssl_sessions_; // session ID -> SSL session
    Xoroshiro128Plus rnd_;  // random number generator

    SSLSessionHandler() {}

    /**
     * @brief Add a new SSL session to the map
     * @param ssl(SSL*) SSL session
     * @return Session ID
    */
    uint64_t addSession(SSL *ssl) {
        uint64_t session_id;
        while (true) {
            // generate session ID
            session_id = rnd_.next();
            if (ssl_sessions_.count(session_id) == 0) break; // check duplication
        }
        // add session to the map
        ssl_sessions_.insert(std::make_pair(session_id, ssl));
        return session_id;
    }

    /**
     * @brief Get SSL session from the map
     * @param session_id(uint64_t) Session ID
     * @return SSL session (nullptr if not found)
    */
    SSL *getSession(uint64_t session_id) {
        if (ssl_sessions_.count(session_id) == 0) return nullptr;
        return ssl_sessions_[session_id];
    }

    /**
     * @brief Remove SSL session from the map
     * @param session_id(uint64_t) Session ID
    */
    void removeSession(uint64_t session_id) {
        if (ssl_sessions_.count(session_id) == 0) return;
        ssl_sessions_.erase(session_id);
    }
};