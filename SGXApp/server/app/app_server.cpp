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
#include "cassa_server_u.h"

// logfile生成用
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream> //filestream
#include <ostream>
#include <string>

#include <vector>
#include <thread>

#define LOOP_OPTION "-server-in-loop"
/* Global EID shared by multiple threads */
sgx_enclave_id_t server_global_eid = 0;


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

// class LoggerNode {
// public:
//     int logger_cpu_;
//     std::vector<int> worker_cpu_;

//     /**
//      * @brief LoggerNodeのコンストラクタ
//      *        logger_cpu_とworker_cpu_をデフォルト初期化する
//      */
//     LoggerNode() : logger_cpu_(0), worker_cpu_() {}
// };

// class LoggerAffinity {
// public:
//     std::vector<LoggerNode> nodes_;
//     unsigned worker_num_ = 0;
//     unsigned logger_num_ = 0;
    
//     /**
//      * @brief worker_numとlogger_numを基にLoggerNodeのリストを初期化する
//      * 
//      * @param worker_num 初期化するworkerの数
//      * @param logger_num 初期化するloggerの数
//      */
//     void init(unsigned worker_num, unsigned logger_num) {
//         unsigned num_cpus = std::thread::hardware_concurrency();
//         if (logger_num > num_cpus || worker_num > num_cpus) {
//             // std::cout << "too many threads" << std::endl;
//             printf("too many threads\n");
//         }
//         // LoggerAffinityのworker_numとlogger_numにコピー
//         worker_num_ = worker_num;
//         logger_num_ = logger_num;
        
//         for (unsigned i = 0; i < logger_num; i++) {
//             nodes_.emplace_back();
//         }
//         unsigned thread_num = logger_num + worker_num;
//         if (thread_num > num_cpus) {
//             for (unsigned i = 0; i < worker_num; i++) {
//                 nodes_[i * logger_num/worker_num].worker_cpu_.emplace_back(i);
//             }
//             for (unsigned i = 0; i < logger_num; i++) {
//                 nodes_[i].logger_cpu_ = nodes_[i].worker_cpu_.back();
//             }
//         } else {
//             for (unsigned i = 0; i < thread_num; i++) {
//                 nodes_[i * logger_num/thread_num].worker_cpu_.emplace_back(i);
//             }
//             for (unsigned i = 0; i < logger_num; i++) {
//                 nodes_[i].logger_cpu_ = nodes_[i].worker_cpu_.back();
//                 nodes_[i].worker_cpu_.pop_back();
//             }
//         }
//     }
// };

void write_sealData(std::string filePath, const uint8_t* sealed_data, const size_t sealed_size) {
    std::ofstream file(filePath, std::ios::out | std::ios::binary);
    if (file.fail()) {
        perror("file open failed");
        abort();
    }
    file.write((const char*)sealed_data, sealed_size);
    file.close();
}

int ocall_save_logfile(const uint8_t* sealed_data, const size_t sealed_size) {
    int thid = 0;   // TODO: セッションがどのファイルに書き込むかを検討する
    std::string filePath = "log/log" + std::to_string(thid) + ".seal";
    write_sealData(filePath, sealed_data, sealed_size);
    return 0;
}

int ocall_save_pepochfile(const uint8_t* sealed_data, const size_t sealed_size) {
    std::string filePath = "log/pepoch.seal";
    write_sealData(filePath, sealed_data, sealed_size);
    return 0;
}

sgx_status_t initialize_enclave(const char *enclave_path) {
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;

    // the 1st parameter should be SERVER_ENCLAVE_FILENAME
    ret = sgx_create_enclave(enclave_path, SGX_DEBUG_FLAG, NULL, NULL, &server_global_eid, NULL);
    printf("- [Host] Enclave library: %s\n", enclave_path);

    if (ret != SGX_SUCCESS){
        print_error_message(ret);
        return ret;
    }
    return ret;
}

// void launch_wogger_thread(size_t w_thid, size_t l_thid) {
//     ecall_wogger_thread_work(server_global_eid, w_thid, l_thid);
// }

void launch_logger_thread(size_t l_thid) {
    ecall_logger_thread_work(server_global_eid, l_thid);
}

void terminate_enclave() {
    sgx_destroy_enclave(server_global_eid);
    printf("Host: Enclave successfully terminated.\n");
}

int main(int argc, const char* argv[]) {
    sgx_status_t result = SGX_SUCCESS;
    int ret = 1;
    char* server_port = NULL;
    int keep_server_up = 0; // should be bool type, 0 false, 1 true

    // TODO: worker/logger threadの数はハードコーディングしておくけど、後で変更する
    // std::vector<std::thread> worker_threads;
    std::vector<std::thread> logger_threads;
    // size_t worker_num = 1;
    size_t logger_num = 1;

    // LoggerAffinity affin;
    // affin.init(worker_num, logger_num);
    size_t w_thid = 0;  // Workerのthread ID
    size_t l_thid = 0;  // Loggerのthread ID, Workerのgroup IDとしても機能する

    /* Check argument count */
    if (argc == 4) {
        if (strcmp(argv[3], LOOP_OPTION) != 0) {
            printf("Usage: %s TLS_SERVER_ENCLAVE_PATH -port:<port> [%s]\n", argv[0], LOOP_OPTION);
            return 1;
        } else {
            keep_server_up = 1;
        }
    } else if (argc != 3) {
        printf("Usage: %s TLS_SERVER_ENCLAVE_PATH -port:<port> [%s]\n", argv[0], LOOP_OPTION);
        return 1;
    }

    printf("\n[Starting TLS server]\n");
    // read port parameter
    char* option = (char*)"-port:";
    size_t param_len = 0;
    param_len = strlen(option);
    if (strncmp(argv[2], option, param_len) == 0) {
        server_port = (char*)(argv[2] + param_len);
    } else {
        fprintf(stderr, "Unknown option %s\n", argv[2]);
        printf("Usage: %s TLS_SERVER_ENCLAVE_PATH -port:<port> [%s]\n", argv[0], LOOP_OPTION);
        return 1;
    }
    printf("- [Host] Server Port: %s\n", server_port);

    printf("\n[Creating TLS server enclave]\n");
    result = initialize_enclave(argv[1]);
    if (result != SGX_SUCCESS) {
        printf("- [Host] Status: Failed\n");
        goto exit;
    } else {
        printf("- [Host] Status: Success\n");
    }

    // printf("\n[Launching worker/logger thread]\n");
    // for (size_t w_thid = 0; w_thid < worker_num; w_thid++) {
    //     worker_thrads.emplace_back(launch_worker_thread, w_thid);
    // }

    // for (auto itr = affin.nodes_.begin(); itr != affin.nodes_.end(); itr++, l_thid++) {
    //     logger_threads.emplace_back(launch_logger_thread, l_thid);
    //     for (auto wcpu = itr->worker_cpu_.begin(); wcpu != itr->worker_cpu_.end(); wcpu++, w_thid++) {
    //         worker_threads.emplace_back(launch_worker_thread, w_thid, l_thid);
    //     }
    // }

    printf("\n[Initialize CASSA settings]\n");
    ecall_initialize_global_variables(server_global_eid, 1, 1);
    
    printf("\n[Launching logger thread]\n");
    for (size_t l_thid = 0; l_thid < logger_num; l_thid++) {
        logger_threads.emplace_back(launch_logger_thread, l_thid);
    }

    printf("\n[Launching TLS server enclave]\n");
    result = set_up_tls_server(server_global_eid, &ret, server_port, keep_server_up);
    if (result != SGX_SUCCESS || ret != 0) {
        print_error_message(result);
        printf("Host: setup_tls_server failed\n");
        goto exit;
    }

exit:

    // for (auto &thread : worker_threads) thread.join();
    for (auto &thread : logger_threads) thread.join();

    // printf("Host: Terminating enclaves\n");
    printf("\n[Terminating enclaves]\n");
    terminate_enclave();

    printf("- [Host] %s \n", (ret == 0) ? "succeeded" : "failed");
    return ret;
}
