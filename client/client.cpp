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

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>
#include <sys/socket.h>
#include <unistd.h>

#include "sgx_utls.h"
#include <string.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <openssl/x509_vfy.h>

#include "../common/common.h"

// ANSI color codes
#include "../common/ansi_color_code.h"

// Utilities
#include "include/command_handler.hpp"
#include "include/parse_command.hpp"

#include "../common/openssl_utility.h"

int verify_callback(int preverify_ok, X509_STORE_CTX* ctx);
int create_socket(char* server_name, char* server_port);

std::string client_session_id;

/**
 * @brief Send data to the server.
 * @param data Data to send.
 * @param data_size Size of data.
*/
void send_data(SSL *ssl_session, const char *data, size_t data_size) {
    std::string data_str(reinterpret_cast<const char*>(data), data_size);
    printf(TLS_CLIENT "send data to server: %s\n", data_str.c_str());
    tls_write_to_session_peer(ssl_session, data_str);

    // ここで受け取るのは適切じゃないけどテストということで
    std::string response;
    tls_read_from_session_peer(ssl_session, response);

    // handle if reveiced data is command
    std::string command;
    std::istringstream iss(data_str);
    std::getline(iss, command, ' ');    // get first token
    if (command == "/get_session_id") {
        printf(TLS_CLIENT "Session ID: %s\n", response.c_str());
        client_session_id = response;
    } else {
        printf(TLS_CLIENT "Response from server: %s\n", response.c_str());
    }
}

/**
 * @brief request session ID from server
 * @note This function is utilizing send_data() 
 *       which non-blockingly fetch the data to receive 
 *       session ID from the server for first.
*/
void request_session_id(SSL *ssl_session) {
    // get current timestamp
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long int timestamp_sec = ts.tv_sec;
    long int timestamp_nsec = ts.tv_nsec;

    std::string command = std::string("/get_session_id") + " " + std::to_string(timestamp_sec) + " " + std::to_string(timestamp_nsec);

    // send the command to the server
    send_data(ssl_session, command.c_str(), command.length());
}

void handle_command(SSL *ssl_session) {
    std::cout << BGRN << "[ INFO     ] " << CRESET 
              << "Awaiting User Commands... (type '/help' for available commands, '/exit' to quit)" << std::endl;

    CommandHandler command_handler;
    std::vector<std::string> operations;
    bool in_transaction = false;

    // wait until ocall_set_client_session_id() sets the session ID assigned by the server
    while (client_session_id.empty());

    while (true) {
        // if in transaction, print the current operations
        if (in_transaction) {
            for (size_t i = 0; i < operations.size(); i++) {
                std::cout << CYN << std::setw(4) << std::right << i + 1 << " " << CRESET 
                          << operations[i] << std::endl;
            }
        }

        // if in transaction, print the green prompt
        if (in_transaction) {
            std::cout << CYN << "> " << CRESET;
        } else {
            std::cout << "> ";
        }

        // get command from user
        std::string command;
        std::getline(std::cin, command);

        // exit the loop if EOF(ctrl+D) is detected or if "/exit" command is entered by the user
        if (std::cin.eof() || command == "/exit") {
            std::cout << "Exiting..." << std::endl;
            break;
        }

        // handle /help command
        if (command == "/help") {
            command_handler.printHelp();
            continue;
        }

        // handle /maketx command
        if (command == "/maketx") {
            // check if the user is already in transaction
            if (in_transaction) {
                std::cout << BRED << "[ ERROR    ] " << CRESET
                          << "You are already in transaction. Please finish or abort the current transaction." << std::endl;
            } else {
                std::cout << BGRN << "[ INFO     ] " << CRESET
                          << "You are now in transaction. Please enter operations." << std::endl;
                operations.clear();

                // set in_transaction to true
                in_transaction = true;
            }
            continue;
        }

        // handle /endtx command
        if (command == "/endtx") {
            if (in_transaction) {
                // set in_transaction to false
                in_transaction = false;

                // check if the transaction has at least 1 operation (except BEGIN_TRANSACTION and END_TRANSACTION)
                if (operations.size() > 0) {
                    // create timestamp
                    timespec ts;
                    clock_gettime(CLOCK_REALTIME, &ts);
                    long int timestamp_sec = ts.tv_sec;
                    long int timestamp_nsec = ts.tv_nsec;

                    // create JSON object for the transaction
                    nlohmann::json transaction_json = parse_command(timestamp_sec, timestamp_nsec, client_session_id, operations);

                    // dump json object to string
                    std::string transaction_json_string = transaction_json.dump();

                    // send the transaction to the server
                    send_data(ssl_session, transaction_json_string.c_str(), transaction_json_string.length());

                    // CRESET the operations
                    operations.clear();
                }
            } else {
                std::cout << BRED << "[ ERROR    ] " << CRESET
                          << "You are not in transaction. Please enter '/maketx' to start a new transaction." << std::endl;
            }
            continue;
        }

        // handle /undo command
        if (command == "/undo") {
            if (in_transaction) {
                // check if the transaction has at least 1 operation (except BEGIN_TRANSACTION and END_TRANSACTION)
                if (operations.size() > 0) {
                    std::cout << BGRN << "[ INFO     ] " << CRESET
                              << "Last operation " << GRN << "\"" <<  operations.back() << "\" " << CRESET << "removed from transaction." << std::endl;
                    // remove the last operation
                    operations.pop_back();
                } else {
                    std::cout << BRED << "[ ERROR    ] " << CRESET
                              << "No operation to undo." << std::endl;
                }
            } else {
                std::cout << BRED << "[ ERROR    ] " << CRESET
                          << "You are not in transaction. Please enter '/maketx' to start a new transaction." << std::endl;
            }
            continue;
        }

        if (command == "/test") {
            // create timestamp
            timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            long int timestamp_sec = ts.tv_sec;
            long int timestamp_nsec = ts.tv_nsec;
            std::vector<std::string> op = {"INSERT hoge fuga", "INSERT piyo pao"};
            nlohmann::json test_json = parse_command(timestamp_sec, timestamp_nsec, client_session_id, op);
            std::string test_json_string = test_json.dump();
            send_data(ssl_session, test_json_string.c_str(), test_json_string.length());
        }

        // handle operations if in transaction
        if (in_transaction) {
            // check if the command is a valid operation
            std::pair<bool, std::string> check_syntax_result = command_handler.checkOperationSyntax(command);
            bool is_valid_syntax = check_syntax_result.first;
            std::string operation = check_syntax_result.second;

            if (is_valid_syntax) {
                // if the operation is valid, add it to the transaction
                operations.push_back(command);
                std::cout << BGRN << "[ INFO     ] " << CRESET
                          << "Operation " << GRN << "\"" <<  command << "\" " << CRESET << "added to transaction." << std::endl;
            } else {
                // if the operation is invalid, print the error message
                std::cout << BRED << "[ ERROR    ] " << CRESET
                          << check_syntax_result.second << std::endl;
            }
            continue;
        }

        // handle unknown command because valid command was not entered here
        std::cout << BRED << "[ ERROR    ] " << CRESET
                  << "Unknown command. Please enter '/help' to see available commands." << std::endl;
    }
}

int parse_arguments(int argc, char** argv, char** server_name, char** server_port) {
    int ret = 1;
    const char* option = nullptr;
    unsigned int param_len = 0;

    if (argc != 3)
        goto print_usage;

    option = "-server:";
    param_len = strlen(option);
    if (strncmp(argv[1], option, param_len) != 0)
        goto print_usage;
    *server_name = (char*)(argv[1] + param_len);

    option = "-port:";
    param_len = strlen(option);
    if (strncmp(argv[2], option, param_len) != 0)
        goto print_usage;

    *server_port = (char*)(argv[2] + param_len);
    ret = 0;
    goto done;

print_usage:
    printf(TLS_CLIENT "Usage: %s -server:<name> -port:<port>\n", argv[0]);
done:
    return ret;
}

/**
 * @brief Create a socket and connect to the server_name:server_port
 * 
 * @param server_name The name or IP address of the server.
 * @param server_port The server port number.
 * @return Socket file descriptor on success, -1 on failure.
*/
int create_socket(char* server_name, char* server_port) {
    int sockfd = -1;
    struct addrinfo hints, *dest_info, *curr_di;
    int res;

    // Initialize hints for IPv4 and TCP
    hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    // Resolve the server hostname
    if ((res = getaddrinfo(server_name, server_port, &hints, &dest_info)) != 0) {
        printf(TLS_CLIENT "Error: Cannot resolve hostname %s. %s\n", server_name, gai_strerror(res));
        goto done;
    }

    // Find the first IPv4 address
    curr_di = dest_info;
    while (curr_di) {
        if (curr_di->ai_family == AF_INET) {
            break;
        }

        curr_di = curr_di->ai_next;
    }

    // Check if a valid address was found
    if (!curr_di) {
        printf(TLS_CLIENT "Error: Cannot get address for hostname %s.\n", server_name);
        goto done;
    }

    // Create a socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf(TLS_CLIENT "Error: Cannot create socket %d.\n", errno);
        goto done;
    }

    // Connect to the server
    if (connect(sockfd, (struct sockaddr*)curr_di->ai_addr, sizeof(struct sockaddr)) == -1) {
        printf(TLS_CLIENT "failed to connect to %s:%s (errno=%d)\n", server_name, server_port, errno);
        close(sockfd);
        sockfd = -1;
        goto done;
    }
    printf(TLS_CLIENT "connected to %s:%s\n", server_name, server_port);

done:
    // Clean up
    if (dest_info)
        freeaddrinfo(dest_info);

    return sockfd;
}

int main(int argc, char** argv) {
    int ret = 1;
    SSL_CTX* ctx = nullptr;
    SSL* ssl = nullptr;
    int serversocket = 0;
    char* server_name = nullptr;
    char* server_port = nullptr;
    int error = 0;

    printf("\nStarting" TLS_CLIENT "\n\n\n");
    if ((error = parse_arguments(argc, argv, &server_name, &server_port)) != 0) {
        printf(TLS_CLIENT "TLS client:parse input parmeter failed (%d)!\n", error);
        goto done;
    }

    // initialize openssl library and register algorithms
    OpenSSL_add_all_algorithms();
    ERR_load_crypto_strings();
    SSL_load_error_strings();

    if (SSL_library_init() < 0) {
        printf(TLS_CLIENT
               "TLS client: could not initialize the OpenSSL library !\n");
        goto done;
    }

    if ((ctx = SSL_CTX_new(SSLv23_client_method())) == nullptr) {
        printf(TLS_CLIENT "TLS client: unable to create a new SSL context\n");
        goto done;
    }

    // choose TLSv1.2 by excluding SSLv2, SSLv3 ,TLS 1.0 and TLS 1.1
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
    SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3);
    SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1);
    SSL_CTX_set_options(ctx, SSL_OP_NO_TLSv1_1);
    // specify the verify_callback for custom verification
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, &verify_callback);
    
    if ((ssl = SSL_new(ctx)) == nullptr) {
        printf(TLS_CLIENT
               "Unable to create a new SSL connection state object\n");
        goto done;
    }

    serversocket = create_socket(server_name, server_port);
    if (serversocket == -1) {
        printf(TLS_CLIENT "create a socket and initate a TCP connect to server: %s:%s " "(errno=%d)\n", server_name, server_port, errno);
        goto done;
    }

    printf(TLS_CLIENT "create a socket and initate a TCP connect to server: %s:%s " "\n", server_name, server_port);
    
    // setup ssl socket and initiate TLS connection with TLS server
    SSL_set_fd(ssl, serversocket);
    if ((error = SSL_connect(ssl)) != 1){
        printf(TLS_CLIENT "Error: Could not establish an SSL session ret2=%d " "SSL_get_error()=%d\n", error, SSL_get_error(ssl, error));
        goto done;
    }
    printf(TLS_CLIENT "successfully established TLS channel:%s\n", SSL_get_version(ssl));

    // request session ID from server
    request_session_id(ssl);

    // handle commands
    handle_command(ssl);

    // Free the structures we don't need anymore
    ret = 0;
done:
    if (serversocket != -1)
        close(serversocket);

    if (ssl)
        SSL_free(ssl);

    if (ctx)
        SSL_CTX_free(ctx);

    printf(TLS_CLIENT " %s\n", (ret == 0) ? "success" : "failed");
    return (ret);
}