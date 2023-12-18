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

#include <openssl/evp.h>
#include <openssl/ssl.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <vector>
#include <string>

#include "include/tls_server.h"
#include "../../../common/openssl_utility.h"

#include "../cassa_server.h"

int verify_callback(int preverify_ok, X509_STORE_CTX* ctx);

extern "C" {
    sgx_status_t ocall_close(int *ret, int fd);
};

int create_listener_socket(int port, int& server_socket) {
    int ret = -1;
    const int reuse = 1;
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        t_print(TLS_SERVER "socket creation failed\n");
        goto exit;
    }

    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const void*)&reuse, sizeof(reuse)) < 0) {
        t_print(TLS_SERVER "setsocket failed \n");
        goto exit;
    }

    if (fcntl_set_nonblocking(server_socket) < 0) {
        t_print(TLS_SERVER "set_non_blocking failed \n");
        goto exit;
    }

    if (bind(server_socket, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        t_print(TLS_SERVER "Unable to bind socket to the port\n");
        goto exit;
    }

    if (listen(server_socket, 20) < 0) {
        t_print(TLS_SERVER "Unable to open socket for listening\n");
        goto exit;
    }
    ret = 0;
exit:
    return ret;
}

SSL *accept_client_connection(int server_socket_fd, SSL_CTX* ssl_server_ctx) {
    int ret = -1; // dummy variable for ocall_close()
    struct sockaddr_in addr;
    uint len = sizeof(addr);
    int client_socket_fd;

    t_print(TLS_SERVER "waiting for client connection ...\n");
    while (true) {
        client_socket_fd = accept(server_socket_fd, (struct sockaddr*)&addr, &len);
        if (client_socket_fd >= 0) break;
    }

    // create a new SSL structure for a connection
    SSL* ssl_session = SSL_new(ssl_server_ctx);
    if (ssl_session == nullptr) {
        t_print(TLS_SERVER "Unable to create a new SSL connection state object\n");
        ocall_close(&ret, client_socket_fd);
        return nullptr;
    }

    SSL_set_fd(ssl_session, client_socket_fd);

    // wait for a TLS/SSL client to initiate a TLS/SSL handshake
    int test_error = SSL_accept(ssl_session);
    if (test_error <= 0) {
        t_print(TLS_SERVER "SSL handshake failed, error(%d)(%d)\n",
                test_error, SSL_get_error(ssl_session, test_error));
        SSL_free(ssl_session);
        ocall_close(&ret, client_socket_fd);
        return nullptr;
    }

    return ssl_session;
}

int set_up_ssl_session(char* server_port, SSL_CTX** out_ssl_server_ctx, int* out_server_socket_fd) {
    unsigned int server_port_number = (unsigned int)atoi(server_port);
    X509* certificate = nullptr;
    EVP_PKEY* pkey = nullptr;
    SSL_CONF_CTX* ssl_confctx = SSL_CONF_CTX_new();
    SSL_CTX* ssl_server_ctx = nullptr;

    if ((ssl_server_ctx = SSL_CTX_new(TLS_server_method())) == nullptr) {
        t_print(TLS_SERVER "unable to create a new SSL context\n");
        goto exit;
    }

    if (initalize_ssl_context(ssl_confctx, ssl_server_ctx) != SGX_SUCCESS) {
        t_print(TLS_SERVER "unable to create a initialize SSL context\n ");
        goto exit;
    }

    SSL_CTX_set_verify(ssl_server_ctx, SSL_VERIFY_PEER, &verify_callback);
    t_print(TLS_SERVER "Load TLS certificate and key\n");
    if (load_tls_certificates_and_keys(ssl_server_ctx, certificate, pkey) != 0) {
        t_print(TLS_SERVER " unable to load certificate and private key on the server\n ");
        goto exit;
    }
    
    if (create_listener_socket(server_port_number, *out_server_socket_fd) != 0) {
        t_print(TLS_SERVER " unable to create listener socket on the server\n ");
        goto exit;
    }

    *out_ssl_server_ctx = ssl_server_ctx;
    return 0;

exit:
    // リソースの解放
    if (ssl_server_ctx) SSL_CTX_free(ssl_server_ctx);
    if (ssl_confctx) SSL_CONF_CTX_free(ssl_confctx);
    if (certificate) X509_free(certificate);
    if (pkey) EVP_PKEY_free(pkey);
    return -1;
}