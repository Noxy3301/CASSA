#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <cassert>
#include <openssl/evp.h>   // For OpenSSL 3.0 compatible cryptographic interfaces
#include <openssl/sha.h>   // For SHA-256 specific constants (SHA256_DIGEST_LENGTH)

#include "../../cassa_server_t.h" // for ocall
#include "silor_log_record.h"

#define SHA256_HEXSTR_LEN (SHA256_DIGEST_LENGTH * 2)

inline size_t get_file_size(std::string file_name) {
    size_t file_size = 0;
    sgx_status_t ocall_status = ocall_get_file_size(&file_size, reinterpret_cast<const uint8_t*>(file_name.c_str()), file_name.size());
    assert(ocall_status == SGX_SUCCESS);
    return file_size;
}

inline std::string read_file(std::string file_name, size_t offset, size_t size) {
    std::string file_data;
    file_data.resize(size);
    
    // OCALL関数の戻り値を格納する変数
    int ocall_retval;

    // OCALLを実行してファイルの内容を読み込む
    sgx_status_t ocall_status = ocall_read_file(&ocall_retval, 
                                                reinterpret_cast<const uint8_t*>(file_name.c_str()), 
                                                file_name.size(), 
                                                reinterpret_cast<uint8_t*>(&file_data[0]), 
                                                offset, 
                                                size);
    assert(ocall_status == SGX_SUCCESS);

    return file_data;
}

inline std::string compute_hash_from_log_record(const RecoveryLogRecord &log) {
    // combine all fields into a single string (except prev_hash)
    std::string data = std::to_string(log.tid_) + log.operation_type_ + log.key_ + log.value_;

    // calculate SHA-256 hash
    unsigned char hash[SHA256_DIGEST_LENGTH];
    unsigned int hash_length = 0;

    EVP_MD_CTX* sha256 = EVP_MD_CTX_new();
    EVP_DigestInit_ex(sha256, EVP_sha256(), NULL);
    EVP_DigestUpdate(sha256, data.c_str(), data.size());
    EVP_DigestFinal_ex(sha256, hash, &hash_length);
    EVP_MD_CTX_free(sha256);

    // convert hash to hex string without using stringstream
    char buffer[3]; // 2 characters + null terminator for each byte
    std::string hex_str;
    for (unsigned char i : hash) {
        snprintf(buffer, sizeof(buffer), "%02x", i);
        hex_str += buffer;
    }
    return hex_str;
}

inline std::string compute_hash_from_string(const std::string &data) {
    // calculate SHA-256 hash
    unsigned char hash[SHA256_DIGEST_LENGTH];
    unsigned int hash_length = 0;

    EVP_MD_CTX* sha256 = EVP_MD_CTX_new();
    EVP_DigestInit_ex(sha256, EVP_sha256(), NULL);
    EVP_DigestUpdate(sha256, data.c_str(), data.size());
    EVP_DigestFinal_ex(sha256, hash, &hash_length);
    EVP_MD_CTX_free(sha256);

    // convert hash to hex string without using stringstream
    char buffer[3];
    std::string hex_str;
    for (unsigned char i : hash) {
        snprintf(buffer, sizeof(buffer), "%02x", i);
        hex_str += buffer;
    }
    return hex_str;
}