/**
*
* MIT License
*
* Copyright (c) Open Enclave SDK contributors.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE
*
*/

#include "openssl_utility.h"

// SSL/TLSでは、1度に読めるデータの最大長は2^14バイト(16KB)となっているらしい
int tls_read_from_session_peer(SSL *&ssl_session, std::string &payload) {
    // データサイズを受信
    size_t data_size = 0;
    int bytes_read = SSL_read(ssl_session, &data_size, sizeof(data_size));
    if (bytes_read <= 0) {
        int error = SSL_get_error(ssl_session, data_size);
#ifdef USE_SGX
        PRINT("Failed to read data size, SSL_read returned error=%d\n", error);
#else
        printf("Failed to read data size, SSL_read returned error=%d\n", error);
#endif
        return bytes_read;
    }

    // データを受信
    payload.clear();
    payload.reserve(data_size);
    std::vector<char> buffer(data_size);

    while (data_size > 0) {
        bytes_read = SSL_read(ssl_session, buffer.data(), data_size);
        if (bytes_read <= 0) {
            int error = SSL_get_error(ssl_session, bytes_read);
            if (error == SSL_ERROR_WANT_READ) {
                continue;
            }
#ifdef USE_SGX
            PRINT("Failed to read payload, SSL_read returned %d\n", error);
#else
            printf("Failed to read payload, SSL_read returned %d\n", error);
#endif
            return bytes_read;
        }
        payload.append(buffer.data(), bytes_read);  // SSL_read()はbufferを上書きするのでbytes_read分だけappendする
        data_size -= bytes_read;
    }

    return 0;
}


int tls_write_to_session_peer(SSL *&ssl_session, const std::string &payload) {
    // データサイズを送信
    size_t data_size = payload.size();
    int bytes_written = SSL_write(ssl_session, &data_size, sizeof(data_size));
    if (bytes_written <= 0) {
        int error = SSL_get_error(ssl_session, data_size);
#ifdef USE_SGX
        PRINT("Failed to write data size, SSL_write returned error=%d\n", error);
#else
        printf("Failed to write data size, SSL_write returned error=%d\n", error);
#endif
        return bytes_written;
    }

    // データを送信
    const char *payload_ptr = payload.data();
    size_t remaining = payload.size();

    while (remaining > 0) {
        bytes_written = SSL_write(ssl_session, payload_ptr, remaining);
        if (bytes_written <= 0) {
            int error = SSL_get_error(ssl_session, bytes_written);
            if (error == SSL_ERROR_WANT_WRITE) {
                continue;
            }
#ifdef USE_SGX
            PRINT("Failed to write payload, SSL_write returned %d\n", error);
#else
            printf("Failed to write payload, SSL_write returned %d\n", error);
#endif
            return bytes_written;
        }
        payload_ptr += bytes_written;
        remaining -= bytes_written;
    }

    return 0;
}