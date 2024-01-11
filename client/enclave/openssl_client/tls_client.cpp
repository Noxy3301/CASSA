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

#include <errno.h>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>

#include <string>
#include <vector>
#include <sstream>

#include "include/tls_client.h"
#include "../../../common/openssl_utility.h"
#include "../cassa_client_t.h"

int verify_callback(int preverify_ok, X509_STORE_CTX* ctx);

// extern "C" {
//     int launch_tls_client(char *server_name, char *server_port);
// };


unsigned long inet_addr2(const char *str) {
    unsigned long lHost = 0;
    char *pLong = (char *)&lHost;
    char *p = (char *)str;
    while (p) {
        *pLong++ = atoi(p);
        p = strchr(p, '.');
        if (p) ++p;
    }
    return lHost;
}

// This routine conducts a simple HTTP request/response communication with server
int communicate_with_server(SSL* ssl) {
    int ret = 1;

    t_print("\n\n=====\n\n");


    // JSONデータをstd::stringに格納
    std::string json_data = R"({"key": "value"})";

    // サーバーにJSONデータを送信
    tls_write_to_session_peer(ssl, json_data);

    // サーバーからの応答を受け取る
    std::string response;
    tls_read_from_session_peer(ssl, response);

    // 応答を処理（ダミー）
    t_print(TLS_CLIENT "Response from server: %s\n", response.c_str());

    t_print("\n\n=====\n\n");
    ret = 0;

done:
    return ret;
}

// create a socket and connect to the server_name:server_port
int create_socket(char* server_name, char* server_port) {
    int sockfd = -1;
    struct sockaddr_in dest_sock;
    int res = -1;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        t_print(TLS_CLIENT "Error: Cannot create socket %d.\n", errno);
        goto done;
    }

    dest_sock.sin_family = AF_INET;
    dest_sock.sin_port = htons(atoi(server_port));
    dest_sock.sin_addr.s_addr = inet_addr2(server_name);
    bzero(&(dest_sock.sin_zero), sizeof(dest_sock.sin_zero));
    
    if (connect(sockfd, (sockaddr*) &dest_sock, sizeof(struct sockaddr)) == -1) {
        t_print(TLS_CLIENT "failed to connect to %s:%s (errno=%d)\n",
                server_port, server_port, errno);
        ocall_close(&res, sockfd);
        if (res != 0) t_print(TLS_CLIENT "OCALL: error closing socket\n");
        sockfd = -1;
        goto done;
    } else {
        t_print(TLS_CLIENT "Connected to %s:%s\n", server_name, server_port);
    }

done:
    return sockfd;
}


SSL* ssl_session = nullptr;
int client_socket = -1;

int launch_tls_client(char* server_name, char* server_port) {
    int ret = -1;
    int error = 0;
    SSL_CTX* ssl_client_ctx = nullptr;
    
    X509* cert = nullptr;
    EVP_PKEY* pkey = nullptr;
    SSL_CONF_CTX* ssl_confctx = SSL_CONF_CTX_new();

    ssl_session = nullptr;
    client_socket = -1;

    // create and initialize the SSL_CTX structure
    if ((ssl_client_ctx = SSL_CTX_new(TLS_client_method())) == nullptr) {
        t_print(TLS_CLIENT "unable to create a new SSL context\n");
        goto done;
    }

    if (initalize_ssl_context(ssl_confctx, ssl_client_ctx) != SGX_SUCCESS) {
        t_print(TLS_CLIENT "unable to create a initialize SSL context\n ");
        goto done;
    }

    // specify the verify_callback for custom verification
    SSL_CTX_set_verify(ssl_client_ctx, SSL_VERIFY_PEER, &verify_callback);
    t_print(TLS_CLIENT "Load TLS certificate and key\n");
    if (load_tls_certificates_and_keys(ssl_client_ctx, cert, pkey) != 0) {
        t_print(TLS_CLIENT " unable to load certificate and private key on the client\n");
        goto done;
    }

    if ((ssl_session = SSL_new(ssl_client_ctx)) == nullptr) {
        t_print(TLS_CLIENT "Unable to create a new SSL connection state object\n");
        goto done;
    }

    // create a socket and initiate a TCP connect to server
    t_print("\n[Establishing TLS Connection]\n");
    client_socket = create_socket(server_name, server_port);
    if (client_socket == -1) {
        t_print(TLS_CLIENT "create a socket and initiate a TCP connect to server: %s:%s "
                "(errno=%d)\n", server_name, server_port, errno);
        goto done;
    }

    // set up ssl socket and initiate TLS connection with TLS server
    SSL_set_fd(ssl_session, client_socket);

    if ((error = SSL_connect(ssl_session)) != 1) {
        t_print(TLS_CLIENT "Error: Could not establish a TLS session ret2=%d "
                "SSL_get_error()=%d\n", error, SSL_get_error(ssl_session, error));
        goto done;
    } else {
        t_print(TLS_CLIENT "TLS Version: %s\n", SSL_get_version(ssl_session));
    }
    ret = 0;    // success

done:
    // Free the structures we don't need anymore
    if (cert)
        X509_free(cert);
    if (pkey)
        EVP_PKEY_free(pkey);
    if (ssl_client_ctx)
        SSL_CTX_free(ssl_client_ctx);
    if (ssl_confctx)
        SSL_CONF_CTX_free(ssl_confctx);
    return ret;
}

/**
 * @brief Send data to the server.
 * @param data Data to send.
 * @param data_size Size of data.
*/
void ecall_send_data(const char *data, size_t data_size) {
    std::string data_str(reinterpret_cast<const char*>(data), data_size);
    t_print(TLS_CLIENT "send data to server: %s\n", data_str.c_str());
    tls_write_to_session_peer(ssl_session, data_str);

    // ここで受け取るのは適切じゃないけどテストということで
    std::string response;
    tls_read_from_session_peer(ssl_session, response);

    // handle if reveiced data is command
    std::string command;
    std::istringstream iss(data_str);
    std::getline(iss, command, ' ');    // get first token
    if (command == "/get_session_id") {
        int ocall_ret;
        sgx_status_t ocall_status;
        t_print(TLS_CLIENT "Session ID: %s\n", response.c_str());

        // convert std::string to const uint8_t*
        const uint8_t* session_id_data = reinterpret_cast<const uint8_t*>(response.c_str());
        size_t session_id_size = response.size();
        ocall_status = ocall_set_client_session_id(&ocall_ret, session_id_data, session_id_size);
        if (ocall_status  != SGX_SUCCESS) {
            t_print(TLS_CLIENT "OCALL: error set client session id\n");
        }
    } else {
        t_print(TLS_CLIENT "Response from server: %s\n", response.c_str());
    }
}

void terminate_ssl_session() {
    t_print(TLS_CLIENT "called terminate_ssl_session\n");
    // cleanup global variables
    int ret = 0;
    if (client_socket != -1) {
        ocall_close(&ret, client_socket);
        if (ret != 0) t_print(TLS_CLIENT "OCALL: error close socket\n");
    }

    if (ssl_session) {
        SSL_shutdown(ssl_session);
        SSL_free(ssl_session);
    }
}