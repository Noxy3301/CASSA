#pragma once

// #include <fcntl.h>  // f controlって読むらしい
#include <unistd.h>
#include <iostream>
// #include "Enclave_t.h"
#include "../../cassa_server_t.h" // for ocall_save_logfile
#include "sgx_tseal.h"
// #include "debug.h"

#include <string.h>

class PosixWriter {
    private:
        int fd_;
    
    public:
        void open(const std::string &filePath) {
            // fd_ = ::open(filePath.c_str(), O_WRONLY|O_CREAT, 0644);
            // // std::cout << "fd_ = " << fd_ << " path = " << filePath << std::endl;
            // if (fd_ == -1) {
            //     perror("open failed");
            //     std::abort();
            // }
        }

        void write(void *log_data, size_t size) {
            int ocall_ret;
            sgx_status_t res, ocall_status;
            size_t sealed_size = sizeof(sgx_sealed_data_t) + size;
            uint8_t* sealed_data = (uint8_t*)malloc(sealed_size);
            // sgx_seal_data(0, NULL, plaintext_len, plaintext, ciph_size, (sgx_sealed_data_t *) sealed);
            res = sgx_seal_data(0, NULL, size, (uint8_t*)log_data, sealed_size, (sgx_sealed_data_t*)sealed_data);
            assert(res == SGX_SUCCESS);
            ocall_status = ocall_save_logfile(&ocall_ret, sealed_data, sealed_size);  // TODO: セッションがどのファイルに書き込むかを検討する
            free(sealed_data);

            // printf("%d %d\n", size, sealed_size);

            // if (ocall_ret != 0) printf("ERR! ocall_ret != 0\n");
            // if (ocall_status != SGX_SUCCESS) printf("ERR! ocall_status != SGX_SUCCESS\n");

            // NOTE: for unseal
            // uint8_t* plain_text = (uint8_t*)malloc(size);
            // uint32_t plain_size = size;
            // res = sgx_unseal_data((sgx_sealed_data_t *)sealed_data, NULL, NULL, plain_text, &plain_size);
            // assert (res == SGX_SUCCESS);
        }

        // writeはディスクキャッシュに書き出すだけでディスクに書き出すことを保証していない(kernelが暇なときにやる)から、fsyncでファイル書き込みを保証しているらしい
        // fsyncは同期処理が完了しないと返ってこない
        void sync() {
            // if (::fsync(fd_) == -1) {
            //     perror("fsync failed");
            //     std::abort();
            // }
        }

        void close() {
            // if (::close(fd_) == -1) {
            //     perror("close failed");
            //     std::abort();
            // }
        }
};