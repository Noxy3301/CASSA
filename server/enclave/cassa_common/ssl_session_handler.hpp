#pragma once

#include <openssl/ssl.h>
#include <map>
#include <vector>
#include <string>
#include <cassert>
#include <mutex>

#include "random.h"

// for t_print()
#include "../../../common/common.h"
#include "../../../common/log_macros.h"

struct SSLSession {
    SSL* ssl_session;  // SSL session
    long latest_timestamp_sec;  // latest timestamp (second)
    long latest_timestamp_nsec; // latest timestamp (nanosecond)
    std::unique_ptr<std::mutex> ssl_session_mutex;  // mutex for SSL session

    SSLSession(SSL* ssl) 
        : ssl_session(ssl), latest_timestamp_sec(0), 
          latest_timestamp_nsec(0), ssl_session_mutex(std::make_unique<std::mutex>()) {}
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
        SSLSession new_session(ssl);
        ssl_sessions_.emplace(std::make_pair(session_id, std::move(new_session)));
        return session_id;
    }

    /**
     * @brief Retrieves the SSLSession structure associated with a given session ID.
     * @param session_id Session ID
     * @return Pointer to the SSLSession structure, or nullptr if not found.
    */
    SSLSession* getSession(std::string session_id) {
        auto it = ssl_sessions_.find(session_id);
        if (it == ssl_sessions_.end()) return nullptr;  // if not found, return nullptr

        return &(it->second);  // return SSLSession pointer
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
                // This error code is returned when SSL_read/SSL_write succeeds
                // t_print(LOG_SESSION_START_BMAG "%s" LOG_SESSION_END BGRN "SSL operation completed successfully.\n" CRESET, session_id.c_str());  // remove SessionでのSSL_ERROR_NONEは正常にセッションが閉じられたことを示すはず
                t_print(LOG_SESSION_START_BMAG "%s" LOG_SESSION_END BGRN "Session closed successfully.\n" CRESET, session_id.c_str());
                break;
            case SSL_ERROR_SSL:
                // This error code is returned when an error occurred (e.g, protocol error, handshake failure)
                t_print(LOG_SESSION_START_BMAG "%s" LOG_SESSION_END BRED "A protocol error or handshake failure occurred.\n" CRESET, session_id.c_str());
                break;
            case SSL_ERROR_WANT_READ:
                // This error code is returned when SSL_read finds no data in non-blocking mode, do nothing as it's normal behavior.
                return;
            case SSL_ERROR_SYSCALL:
                // This error code is returned when the client closes the connection without sending a close_notify alert
                t_print(LOG_SESSION_START_BMAG "%s" LOG_SESSION_END BRED "Session may have closed unexpectedly.\n" CRESET, session_id.c_str());
                break;
            case SSL_ERROR_ZERO_RETURN:
                // This error code is returned when the client closes the connection with sending a close_notify alert
                t_print(LOG_SESSION_START_BMAG "%s" LOG_SESSION_END BGRN "Session closed successfully.\n" CRESET, session_id.c_str());
                break;
            default:
                t_print(LOG_SESSION_START_BMAG "%s" LOG_SESSION_END BRED "Unknown error code: %d\n" CRESET, session_id.c_str(), ssl_error_code);
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