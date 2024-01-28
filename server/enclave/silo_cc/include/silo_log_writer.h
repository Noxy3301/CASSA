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
public:
    void write_log(size_t thid, void *log_data, size_t log_size) {
        int ocall_ret;
        sgx_status_t res, ocall_status;
#ifdef NO_ENCRYPT
        ocall_status = ocall_save_logfile(&ocall_ret, thid, (uint8_t*)log_data, log_size);
#else
        size_t sealed_size = sizeof(sgx_sealed_data_t) + log_size;
        uint8_t* sealed_data = (uint8_t*)malloc(sealed_size);
        // sgx_seal_data(0, NULL, plaintext_len, plaintext, ciph_size, (sgx_sealed_data_t *) sealed);
        res = sgx_seal_data(0, NULL, log_size, (uint8_t*)log_data, sealed_size, (sgx_sealed_data_t*)sealed_data);
        assert(res == SGX_SUCCESS);
        ocall_status = ocall_save_logfile(&ocall_ret, thid, sealed_data, sealed_size);  // TODO: セッションがどのファイルに書き込むかを検討する
        free(sealed_data);
#endif
    }

    /**
     * @brief Writes the tail log hash to the file.
     * 
     * @param thid The thread ID.
     * @param tail_log_hash The tail log hash to write.
     * @param hash_size The size of the hash.
     * 
     * @note Since this is a hash, encryption may not be necessary.
     */
    void write_tail_log_hash(size_t thid, void *tail_log_hash, size_t hash_size) {
        int ocall_ret;
        sgx_status_t ocall_status = ocall_save_tail_log_hash(&ocall_ret, thid, (uint8_t*)tail_log_hash, hash_size);
        assert(ocall_ret == 0);
    }

private:
    int fd_;
};