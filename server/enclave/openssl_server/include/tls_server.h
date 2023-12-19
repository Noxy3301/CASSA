#pragma once

SSL *accept_client_connection(int server_socket_fd, SSL_CTX* ssl_server_ctx);
int set_up_ssl_session(char* server_port, SSL_CTX** out_ssl_server_ctx, int* out_server_socket_fd);