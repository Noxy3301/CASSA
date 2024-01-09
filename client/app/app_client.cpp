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

#include "sgx_urts.h"
#include <stdio.h>
#include <netdb.h>
#include "cassa_client_u.h"
#include <sys/socket.h>
#include <sys/types.h>
#include <string>
#include <iostream>
#include <ctime>

// ANSI color codes
#include "../../common/ansi_color_code.h"

// CASSA utilities
#include "utils/command_handler.hpp"
#include "utils/parse_command.hpp"

#define TLS_SERVER_NAME "localhost"
#define TLS_SERVER_PORT "12340"

/* Global EID shared by multiple threads */
sgx_enclave_id_t client_global_eid = 0;

// session ID
std::string client_session_id;

typedef struct _sgx_errlist_t {
    sgx_status_t err;
    const char *msg;
    const char *sug; /* Suggestion */
} sgx_errlist_t;

/* Error code returned by sgx_create_enclave */
static sgx_errlist_t sgx_errlist[] = {
    {SGX_ERROR_UNEXPECTED, "Unexpected error occurred.", NULL},
    {SGX_ERROR_INVALID_PARAMETER, "Invalid parameter.", NULL},
    {SGX_ERROR_OUT_OF_MEMORY, "Out of memory.", NULL},
    {SGX_ERROR_ENCLAVE_LOST, "Power transition occurred.", "Please refer to the sample \"PowerTransition\" for details."},
    {SGX_ERROR_INVALID_ENCLAVE, "Invalid enclave image.", NULL},
    {SGX_ERROR_INVALID_ENCLAVE_ID, "Invalid enclave identification.", NULL},
    {SGX_ERROR_INVALID_SIGNATURE, "Invalid enclave signature.", NULL},
    {SGX_ERROR_OUT_OF_EPC, "Out of EPC memory.", NULL},
    {SGX_ERROR_NO_DEVICE, "Invalid SGX device.", "Please make sure SGX module is enabled in the BIOS, and install SGX driver afterwards."},
    {SGX_ERROR_MEMORY_MAP_CONFLICT, "Memory map conflicted.", NULL},
    {SGX_ERROR_INVALID_METADATA, "Invalid enclave metadata.", NULL},
    {SGX_ERROR_DEVICE_BUSY, "SGX device was busy.", NULL},
    {SGX_ERROR_INVALID_VERSION, "Enclave version was invalid.", NULL},
    {SGX_ERROR_INVALID_ATTRIBUTE, "Enclave was not authorized.", NULL},
    {SGX_ERROR_ENCLAVE_FILE_ACCESS, "Can't open enclave file.", NULL},
};

/* Check error conditions for loading enclave */
void print_error_message(sgx_status_t ret) {
    size_t idx = 0;
    size_t ttl = sizeof sgx_errlist/sizeof sgx_errlist[0];

    for (idx = 0; idx < ttl; idx++) {
        if(ret == sgx_errlist[idx].err) {
            if(NULL != sgx_errlist[idx].sug)
                printf("Info: %s\n", sgx_errlist[idx].sug);
            printf("Error: %s\n", sgx_errlist[idx].msg);
            break;
        }
    }

    if (idx == ttl)
        printf("Error code is 0x%X. Please refer to the \"Intel SGX SDK Developer Reference\" for more details.\n", ret);
}

sgx_status_t initialize_enclave(const char *enclave_path) {
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;

    // the 1st parameter should be CLIENT_ENCLAVE_FILENAME
    ret = sgx_create_enclave(enclave_path, SGX_DEBUG_FLAG, NULL, NULL, &client_global_eid, NULL);
    printf("- [Host] Enclave library: %s\n", enclave_path);

    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        return ret;
    }
    return ret;
}

void terminate_enclave() {
    sgx_destroy_enclave(client_global_eid);
    printf(" -[Host] Enclave successfully terminated.\n");
}

int ocall_set_client_session_id(const uint8_t* session_id_data, size_t session_id_size) {
    client_session_id = std::string((const char*)session_id_data, session_id_size);
    printf("- [Host] Session ID: %s\n", client_session_id.c_str());
    return 0;
}

/**
 * @brief request session ID from server
 * @note This function is utilizing ecall_send_data() 
 *       which non-blockingly fetch the data to receive 
 *       session ID from the server for first.
*/
void request_session_id() {
    std::string command = "/get_session_id";
    ecall_send_data(client_global_eid, command.c_str(), command.length());
}

void handle_command() {
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
                    ecall_send_data(client_global_eid, transaction_json_string.c_str(), transaction_json_string.length());

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
            ecall_send_data(client_global_eid, test_json_string.c_str(), test_json_string.length());
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

int main(int argc, const char* argv[]) {
    // SGX variables
    sgx_status_t result = SGX_SUCCESS;
    int ret = 1;

    // TLS settings
    char* server_name = NULL;
    char* server_port = NULL;

    /* Check argument count */
    if (argc != 4) {
    print_usage:
        printf("Usage: %s TLS_SERVER_ENCLAVE_PATH -server:<name> -port:<port>\n", argv[0]);
        return 1;
    }

    printf("\n[Starting TLS client]\n");
    // read server name  parameter
    {
        const char* option = "-server:";
        int param_len = 0;
        param_len = strlen(option);
        if (strncmp(argv[2], option, param_len) == 0) {
            server_name = (char*)(argv[2] + param_len);
        } else {
            fprintf(stderr, "Unknown option %s\n", argv[2]);
            goto print_usage;
        }
    }
    printf("- [Host] Server Name: %s\n", server_name);

    // read port parameter
    {
        const char* option = "-port:";
        int param_len = 0;
        param_len = strlen(option);
        if (strncmp(argv[3], option, param_len) == 0) {
            server_port = (char*)(argv[3] + param_len);
        } else {
            fprintf(stderr, "Unknown option %s\n", argv[2]);
            goto print_usage;
        }
    }
    printf("- [Host] Server Port: %s\n", server_port);

    // create client enclave
    printf("\n[Creating TLS client enclave]\n");
    result = initialize_enclave(argv[1]);
    if (result != SGX_SUCCESS) {
        printf("- [Host] Status: Failed\n");
        goto exit;
    } else {
        printf("- [Host] Status: Success\n");
    }

    // launch TLS client
    printf("\n[Launching TLS client enclave]\n");
    result = launch_tls_client(client_global_eid, &ret, server_name, server_port);
    if (result != SGX_SUCCESS || ret != 0) {
        printf("- [Host] TLS Channel: Failed to establish\n");
        goto exit;
    } else {
        printf("- [Host] TLS Channel: Successfully established\n");
    }

    printf("\n[Client-Server Communication]\n");
    // request session ID from server
    request_session_id();

    // handle commands
    handle_command();

    ret = 0;
exit:
    printf("\n[Terminating TLS Session]\n");
    result = terminate_ssl_session(client_global_eid);
    if (result != SGX_SUCCESS) {
        printf("- [Host] TLS Session: Failed to terminate\n");
    } else {
        printf("- [Host] TLS Session: Successfully terminated\n");
    }
    terminate_enclave();

    printf("- [Host] %s \n", (ret == 0) ? "succeeded" : "failed");
    return ret;
}
