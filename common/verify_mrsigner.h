#pragma once

#include <openssl/x509_vfy.h>
#include <cstdio>
#include <iostream>
#include <iomanip>

#include "common.h"
#include "ansi_color_code.h"

// SGX Quoteを含む証明書拡張のOID
// Referenced from linux-sgx/sdk/ttls/cert_header.h
#define SGX_QUOTE_EXTENSION_OID "1.2.840.113741.1.13.1"
#define MRSIGNER_FILEPATH "./client/mrsigner.bin"

// ファイルからMRSIGNERを読み込む関数
int read_mrsigner_from_file(const char* filename, unsigned char* mrsigner, size_t mrsigner_size) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        std::cout << BRED << "[ ERROR    ] " << CRESET
                  << "Failed to open file: " << filename << std::endl; 
        return -1;
    }

    size_t read_bytes = fread(mrsigner, 1, mrsigner_size, file);
    fclose(file);

    if (read_bytes != mrsigner_size) {
        std::cout << BRED << "[ ERROR    ] " << CRESET
                  << "Failed to read full MRSIGNER from file, expected " << mrsigner_size << " bytes, read " << read_bytes << " bytes" << std::endl;
        return -1;
    }

    return 0;
}

bool extract_mrsigner_from_sgx_quote(const unsigned char* quote, int quote_len) {
    // QuoteからMRSIGNERを抜き出すためのオフセットを定義 (QuoteGeneration/quote_wrapper/common/inc/sgx_quote_3.h)
    const int mrsigner_offset = 176; // sgx_quote_tからsgx_measurement_tのmr_signerまでのオフセット
    const int mrsigner_size = 32; // MRSIGNERのサイズ

    if (quote_len < (mrsigner_offset + mrsigner_size)) {
        std::cout << BRED << "[ ERROR    ] " << CRESET
                  << "SGX Quote data is too short to contain MRSIGNER" << std::endl;
        return false;
    }

    // ファイルからMRSIGNERを読み込む
    unsigned char file_mrsigner[mrsigner_size];
    if (read_mrsigner_from_file(MRSIGNER_FILEPATH, file_mrsigner, mrsigner_size) != 0) {
        std::cout << BRED << "[ ERROR    ] " << CRESET
                  << "Failed to read MRSIGNER from: " << MRSIGNER_FILEPATH << std::endl;
        return false;
    }

    // QuoteからMRSIGNERを抽出
    const unsigned char* quote_mrsigner = quote + mrsigner_offset;

    std::cout << "MRSIGNER from SGX Quote:" << std::endl;
    for (int i = 0; i < mrsigner_size; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(quote_mrsigner[i]) << " ";
        if ((i + 1) % 16 == 0) std::cout << std::endl;
    }
    std::cout << std::endl;

    std::cout << "MRSIGNER from " << MRSIGNER_FILEPATH << ":" << std::endl;
    for (int i = 0; i < mrsigner_size; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(file_mrsigner[i]) << " ";
        if ((i + 1) % 16 == 0) std::cout << std::endl;
    }
    std::cout << std::endl;

    // MRSIGNERの比較
    if (memcmp(quote_mrsigner, file_mrsigner, mrsigner_size) == 0) {
        std::cout << "Result: " << BGRN << "MRSIGNER match" << CRESET << std::endl;
        return true;
    } else {
        std::cout << "Result: " << BRED << "MRSIGNER does not match" << CRESET << std::endl;
        return false;
    }
}

bool extract_sgx_quote_from_der_cert(unsigned char* der, int der_len) {
    bool result = false;
    X509* cert = nullptr;
    ASN1_OBJECT* sgx_quote_oid_obj = nullptr;

    do {
        const unsigned char* p = der;
        cert = d2i_X509(NULL, &p, der_len);
        if (!cert) {
            printf("Failed to parse the certificate\n");
            break;
        }

        // ASN1_OBJECTをOID文字列から作成
        sgx_quote_oid_obj = OBJ_txt2obj(SGX_QUOTE_EXTENSION_OID, 1);
        if (!sgx_quote_oid_obj) {
            printf("Failed to create ASN1_OBJECT from OID\n");
            break;
        }

        // 証明書から拡張を検索
        int loc = X509_get_ext_by_OBJ(cert, sgx_quote_oid_obj, -1);
        if (loc < 0) {
            printf("SGX Quote extension not found in the certificate\n");
            break;
        }

        // 拡張からSGX Quoteデータを取得
        X509_EXTENSION* ext = X509_get_ext(cert, loc);
        ASN1_OCTET_STRING* data = X509_EXTENSION_get_data(ext);

        const unsigned char* quote = ASN1_STRING_get0_data(data);
        int quote_len = ASN1_STRING_length(data);

        result = extract_mrsigner_from_sgx_quote(quote, quote_len);
    } while (0);

    // リソース解放
    if (cert) X509_free(cert);
    if (sgx_quote_oid_obj) ASN1_OBJECT_free(sgx_quote_oid_obj);

    return result;
}