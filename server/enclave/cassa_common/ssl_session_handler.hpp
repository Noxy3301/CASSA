#pragma once

#include <openssl/ssl.h>
#include <map>
#include <vector>
#include <string>
#include <cassert>

#include "random.h"

// for t_print()
#include "../../../common/common.h"

struct SSLSession {
    SSL* ssl_session;  // SSL session
    long latest_timestamp_sec;  // latest timestamp (second)
    long latest_timestamp_nsec; // latest timestamp (nanosecond)
};

class SSLSessionHandler {
public:
    std::map<std::string, SSLSession> ssl_sessions_; // session ID -> SSL session
    Xoroshiro128Plus rnd_;  // random number generator

    SSLSessionHandler() {}

    /**
     * @brief Convert a number to a character
     * @param[in] num number to convert
     * @return char converted character
    */
    char number_to_char(uint64_t num) {
        if (num < 26) {
            return 'A' + num;
        } else {
            return '0' + (num - 26);
        }
    }

    /**
     * @brief Generate a session ID
     * @return Session ID
     * @note Session ID is a 6-character string (hard-coded)
    */
    std::string generateSessionID() {
        std::string id;
        for (int i = 0; i < 6; i++) {
            id += number_to_char(rnd_.next() % 36);
        }
        return id;
    }

    /**
     * @brief Add a new SSL session to the map
     * @param ssl(SSL*) SSL session
     * @return Session ID
    */
    std::string addSession(SSL *ssl) {
        std::string session_id;
        // repeat until a unique session ID is generated
        while (true) {
            // generate session ID
            session_id = generateSessionID();
            if (ssl_sessions_.count(session_id) == 0) break; // check duplication
        }
        // add session to the map
        SSLSession new_session = {ssl, 0, 0};  // initialize timestamp
        ssl_sessions_.insert(std::make_pair(session_id, new_session));
        return session_id;
    }

    /**
     * @brief Get SSL session from the map
     * @param session_id(std::string) Session ID
     * @return SSL session (nullptr if not found)
    */
    SSL *getSession(std::string session_id) {
        if (ssl_sessions_.count(session_id) == 0) return nullptr;
        return ssl_sessions_[session_id].ssl_session;
    }

    void setTimestamp(std::string session_id, long timestamp_sec, long timestamp_nsec) {
        // get SSL session corresponding to the session ID
        auto it = ssl_sessions_.find(session_id);
        if (it == ssl_sessions_.end()) return;  // if not found, do nothing
    
        // set timestamp
        it->second.latest_timestamp_sec = timestamp_sec;
        it->second.latest_timestamp_nsec = timestamp_nsec;
    }

    long int getTimestampSec(std::string session_id) {
        // get SSL session corresponding to the session ID
        auto it = ssl_sessions_.find(session_id);
        if (it == ssl_sessions_.end()) return 0;  // if not found, do nothing

        return it->second.latest_timestamp_sec;
    }

    long int getTimestampNsec(std::string session_id) {
        // get SSL session corresponding to the session ID
        auto it = ssl_sessions_.find(session_id);
        if (it == ssl_sessions_.end()) return 0;  // if not found, do nothing

        return it->second.latest_timestamp_nsec;
    }

    /**
     * @brief Remove SSL session from the map
     * @param session_id(std::string) Session ID
     * @param ssl_error_code(int) SSL error code
    */
    void removeSession(std::string session_id, int ssl_error_code) {
        // get SSL session corresponding to the session ID
        auto it = ssl_sessions_.find(session_id);
        if (it == ssl_sessions_.end()) return;  // if not found, do nothing

        // error handling
        switch (ssl_error_code) {
            case SSL_ERROR_NONE:
                // this error code is returned when SSL_read/SSL_write succeeds
                t_print(TLS_SERVER "SSL_ERROR_NONE\n");
                break;
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
                t_print(TLS_SERVER "Session ID: %s may have closed unexpectedly (SSL_ERROR_SYSCALL).\n", session_id.c_str());
                break;
            case SSL_ERROR_ZERO_RETURN:
                // this error code is returned when the client closes 
                // the connection with sending a close_notify alert
                t_print(TLS_SERVER "Session ID: %s has closed (SSL_ERROR_ZERO_RETURN).\n", session_id.c_str());
                break;
            default:
                t_print(TLS_SERVER "Unknown error code: %d\n", ssl_error_code);
                break;
        }

        // clean up SSL session
        SSL *ssl_session = it->second.ssl_session;
        if (ssl_session) {
            SSL_free(ssl_session);
        }

        // remove session from the map
        ssl_sessions_.erase(it);
    }
};