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

// #include "utils/command_handler.hpp"
// #include "utils/parse_command.hpp"

#define TLS_SERVER_NAME "localhost"
#define TLS_SERVER_PORT "12340"

/* Global EID shared by multiple threads */
sgx_enclave_id_t client_global_eid = 0;

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

void handle_command() {
    while (true) {
        std::cout << "Host: Enter command to send to server: ";
        std::string command;
        std::getline(std::cin, command);

        if (command.empty()) {
            command = R"([ { "transactions": [ { "operation": "INSERT", "key": "key1", "value": "value1" }, { "operation": "INSERT", "key": "key2", "value": "value2" }, { "operation": "INSERT", "key": "key3", "value": "value3" }, { "operation": "INSERT", "key": "key4", "value": "value4" }, ] } ])";
        }
    
        ecall_send_data(client_global_eid, command.c_str(), command.length());

        if (command == "/exit") {
            break;
        }
        std::cout << "\n";
    }
}

// void handle_command() {
//     std::cout << "\033[32m" << "[ INFO     ] " << "\033[0m" 
//               << "Awaiting User Commands... (type '/help' for available commands, '/exit' to quit)" << std::endl;

//     CommandHandler handler;
//     std::vector<std::string> procedure;
//     bool in_procedure = false;
//     bool in_transaction = false;

//     while (true) {
//         // show procedure prompt if in procedure
//         if (in_procedure) {
//             std::cout << "\033[32m" << "=== start of procedure ===" << std::endl;
//             for (auto proc : procedure) {
//                 std::cout << proc << std::endl;
//             }
//             std::cout << "===  end of procedure  ===" << "\033[0m" << std::endl;
//         }

//         // get user input
//         std::cout << "> ";
//         std::string command;
//         std::getline(std::cin, command);

//         // exit the loop if EOF is detected or if "/exit" command is entered by the user
//         if (std::cin.eof() || command == "/exit") {
//             procedure.clear();  // 念のため
//             break;
//         }

//         // handle /help command
//         if (command == "/help") {
//             handler.printHelp();
//             continue;
//         }

//         // handle /mkproc command
//         if (command == "/mkproc") {
//             if (in_procedure) {
//                 std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
//                           << "Error: Already in a procedure. Please end the current procedure first." << std::endl;
//             } else {
//                 in_procedure = true;
//                 procedure.clear();
//             }
//             continue;
//         }

//         // handle /endproc command
//         if (command == "/endproc") {
//             if (!in_procedure) {
//                 std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
//                           << "Error: Not in a procedure. Please begin a procedure first." << std::endl;
//             } else {
//                 if (in_transaction) {
//                     std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
//                               << "Error: Still in a transaction. Please end the current transaction first." << std::endl;
//                 } else {
//                     in_procedure = false;
//                     if (procedure.size() > 0) {
//                         // create json object for procedure
//                         nlohmann::json procedure_json = parseCommand(procedure);
                        
//                         // dump json object to string
//                         std::string procedure_json_string = procedure_json.dump();
//                         uint8_t *dumped_json_data = reinterpret_cast<const uint8_t*>(procedure_json_string.data());
//                         size_t dumped_json_size = procedure_json_string.size();

//                         // send procedure to enclave
//                         size_t worker_thid = handler.rnd() % WORKER_NUM;
//                         ecall_send_data(client_global_eid, dumped_json_data, dumped_json_size);

//                         // clear procedure
//                         procedure.clear();
//                     }
//                 }
//             }
//             continue;
//         }

//         if (in_procedure) {
//             if (command == "BEGIN_TRANSACTION") {
//                 if (in_transaction) {
//                     std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
//                               << "Error: Already in a transaction. Please end the current transaction first." << std::endl;
//                 } else {
//                     in_transaction = true;
//                     procedure.push_back(command);
//                 }
//             } else if (command == "END_TRANSACTION") {
//                 if (!in_transaction) {
//                     std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
//                               << "Error: Not in a transaction. Please begin a transaction first." << std::endl;
//                 } else {
//                     in_transaction = false;
//                     procedure.push_back(command);
//                 }
//             } else if (in_transaction) {
//                     std::pair<bool, std::string> check_syntax_result = handler.checkOperationSyntax(command);
//                     if (check_syntax_result.first) {
//                         procedure.push_back(command);
//                     } else {
//                         std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
//                                   << check_syntax_result.second << std::endl;
//                     }
//             } else if (!in_transaction) {
//                     std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
//                               << "Error: Not in a transaction. Please begin a transaction first." << std::endl;
//             } else {
//                 std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
//                           << "Error: Invalid command. Please type '/help' for available commands." << std::endl;
//             }
//             continue;
//         }

//         // show error message because valid command was not entered here
//         std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
//                   << "Error: Invalid command. Please type '/help' for available commands." << std::endl;
//     }
// }

int main(int argc, const char* argv[]) {
    sgx_status_t result = SGX_SUCCESS;
    int ret = 1;
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
