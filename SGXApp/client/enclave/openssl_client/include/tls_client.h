#pragma once

#include <openssl/ssl.h>
#include <string>

// int launch_tls_client(char *server_name, char *server_port);
// void ecall_send_data(const char *data, size_t data_size);
// void terminate_ssl_session();

extern SSL *ssl_session;
extern int client_socket;