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
    int set_up_tls_server(char* server_port, bool keep_server_up);
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

void process_ssl_session(SSL *ssl_session) {
    t_print("\n[Processing TLS Session]\n");

    // you can execute some function here
    process_cassa(ssl_session);

    // セッションの終了処理
    SSL_shutdown(ssl_session);
    SSL_free(ssl_session);
}

SSL *accept_client_connection(int server_socket_fd, SSL_CTX* ssl_server_ctx) {
    int ret = -1; // dummy variable for ocall_close()
    struct sockaddr_in addr;
    uint len = sizeof(addr);

    t_print(TLS_SERVER "waiting for client connection ...\n");
    int client_socket_fd = accept(server_socket_fd, (struct sockaddr*)&addr, &len);
    if (client_socket_fd < 0) {
        t_print(TLS_SERVER "Unable to accept the client request\n");
        return nullptr;
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

int handle_communication_until_done(int &server_socket_fd, SSL_CTX *&ssl_server_ctx, bool keep_server_up) {
    while (keep_server_up) {
        SSL *ssl_session = accept_client_connection(server_socket_fd, ssl_server_ctx);
        if (ssl_session == nullptr) {
            t_print(TLS_SERVER "Error: accept_client_connection() failed\n");
            return -1;
        } else {
            process_ssl_session(ssl_session);
            t_print(TLS_SERVER "TLS session closed\n");
        }
    }
    return 0;
}

int set_up_tls_server(char* server_port, bool keep_server_up) {
    // SSLコンテキストの設定と、リスナーソケットの作成を行う
    int ret = 0;
    int server_socket_fd;
    int client_socket_fd = -1;
    unsigned int server_port_number;

    t_print("here?1\n");

    X509* certificate = nullptr;
    EVP_PKEY* pkey = nullptr;
    SSL_CONF_CTX* ssl_confctx = SSL_CONF_CTX_new();

    SSL_CTX* ssl_server_ctx = nullptr;
    SSL* ssl_session = nullptr;

    if ((ssl_server_ctx = SSL_CTX_new(TLS_server_method())) == nullptr) {
        t_print(TLS_SERVER "unable to create a new SSL context\n");
        goto exit;
    }

    t_print("here?2\n");

    if (initalize_ssl_context(ssl_confctx, ssl_server_ctx) != SGX_SUCCESS) {
        t_print(TLS_SERVER "unable to create a initialize SSL context\n ");
        goto exit;
    }

    t_print("here?3\n");

    SSL_CTX_set_verify(ssl_server_ctx, SSL_VERIFY_PEER, &verify_callback);
    t_print(TLS_SERVER "Load TLS certificate and key\n");
    if (load_tls_certificates_and_keys(ssl_server_ctx, certificate, pkey) != 0) {
        t_print(TLS_SERVER " unable to load certificate and private key on the server\n ");
        goto exit;
    }

    t_print("here?4\n");
    
    server_port_number = (unsigned int)atoi(server_port); // convert to char* to int
    if (create_listener_socket(server_port_number, server_socket_fd) != 0) {
        t_print(TLS_SERVER " unable to create listener socket on the server\n ");
        goto exit;
    }

    t_print("here?5\n");

    t_print("\n[Establishing TLS Connection]\n");
    ret = handle_communication_until_done(server_socket_fd, ssl_server_ctx, keep_server_up);
    if (ret != 0) {
        t_print(TLS_SERVER "server communication error %d\n", ret);
        goto exit;
    }

exit:
    ocall_close(&ret, server_socket_fd); // close the server socket connections
    if (ret != 0) t_print(TLS_SERVER "OCALL: error closing server socket\n");

    if (ssl_server_ctx)
        SSL_CTX_free(ssl_server_ctx);
    if (ssl_confctx)
        SSL_CONF_CTX_free(ssl_confctx);
    if (certificate)
        X509_free(certificate);
    if (pkey)
        EVP_PKEY_free(pkey);

    return (ret);
}
