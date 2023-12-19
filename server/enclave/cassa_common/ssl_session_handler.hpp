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
     * @param ssl_error_code(int) SSL error code
    */
    void removeSession(uint64_t session_id, int ssl_error_code) {
        // get SSL session corresponding to the session ID
        auto it = ssl_sessions_.find(session_id);
        if (it == ssl_sessions_.end()) return;  // if not found, do nothing

        // error handling
        switch (ssl_error_code) {
            case SSL_ERROR_SSL:
                // this error code is returned when an error occurred 
                // (e.g, protocol error, handshake failure)
                t_print(TLS_SERVER "SSL_ERROR_SSL\n");
                break;
            case SSL_ERROR_WANT_READ:
                // this error code is returned when SSL_read finds no data
                // in non-blocking mode, do nothing as it's normal behavior.
                return;
            case SSL_ERROR_SYSCALL:
                // this error code is returned when the client closes 
                // the connection without sending a close_notify alert
                t_print(TLS_SERVER "Session ID: %lu may have closed unexpectedly (SSL_ERROR_SYSCALL).\n", session_id);
                break;
            case SSL_ERROR_ZERO_RETURN:
                // this error code is returned when the client closes 
                // the connection with sending a close_notify alert
                t_print(TLS_SERVER "Session ID: %lu has closed (SSL_ERROR_ZERO_RETURN).\n", session_id);
                break;
            default:
                t_print(TLS_SERVER "Unknown error code: %d\n", ssl_error_code);
                break;
        }

        // clean up SSL session
        SSL *ssl_session = it->second;
        if (ssl_session) {
            SSL_free(ssl_session);
        }

        // remove session from the map
        ssl_sessions_.erase(it);
    }
};