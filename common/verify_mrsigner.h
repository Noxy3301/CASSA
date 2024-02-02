#pragma once

#include <openssl/x509_vfy.h>
#include <cstdio>
#include "common.h"

// SGX Quoteを含む証明書拡張のOID
// Referenced from linux-sgx/sdk/ttls/cert_header.h
static const char SGX_QUOTE_EXTENSION_OID[] = "1.2.840.113741.1.13.1";
const char FILENAME[] = "./client/mrsigner.bin";

// ファイルからMRSIGNERを読み込む関数
int read_mrsigner_from_file(const char* filename, unsigned char* mrsigner, size_t mrsigner_size) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        perror("Failed to open file");
        return -1;
    }

    size_t read_bytes = fread(mrsigner, 1, mrsigner_size, file);
    fclose(file);

    if (read_bytes != mrsigner_size) {
        fprintf(stderr, "Failed to read full MRSIGNER from file\n");
        return -1;
    }

    return 0;
}

void extract_mrsigner_from_sgx_quote(const unsigned char* quote, int quote_len) {
    // QuoteからMRSIGNERを抜き出すためのオフセットを定義
    // この値は、SGX Quoteの構造に基づいて適宜調整する必要があります。
    const int mrsigner_offset = 176; // sgx_quote_tからsgx_measurement_tのmr_signerまでのオフセット
    const int mrsigner_size = 32; // MRSIGNERのサイズ（バイト）

    if (quote_len < (mrsigner_offset + mrsigner_size)) {
        printf("Error: SGX Quote data is too short to contain MRSIGNER\n");
        return;
    }

    // ファイルからMRSIGNERを読み込む
    unsigned char file_mrsigner[mrsigner_size];
    if (read_mrsigner_from_file(FILENAME, file_mrsigner, mrsigner_size) != 0) {
        printf("Error: Failed to read MRSIGNER from file\n");
        return;
    }

    // QuoteからMRSIGNERを抽出
    const unsigned char* quote_mrsigner = quote + mrsigner_offset;

    // MRSIGNERの比較
    if (memcmp(quote_mrsigner, file_mrsigner, mrsigner_size) == 0) {
        printf("MRSIGNER match\n");
    } else {
        printf("MRSIGNER does not match\n");
    }
}


void extract_sgx_quote_from_der_cert(unsigned char* der, int der_len) {
    const unsigned char* p = der;
    X509* cert = d2i_X509(NULL, &p, der_len);
    if (!cert) {
        printf("Failed to parse the certificate\n");
        return;
    }

    // ASN1_OBJECTをOID文字列から作成
    ASN1_OBJECT* sgx_quote_oid_obj = OBJ_txt2obj(SGX_QUOTE_EXTENSION_OID, 1);

    // 証明書から拡張を検索
    int loc = X509_get_ext_by_OBJ(cert, sgx_quote_oid_obj, -1);
    if (loc < 0) {
        printf("SGX Quote extension not found in the certificate\n");
    } else {
        // 拡張からSGX Quoteデータを取得
        X509_EXTENSION* ext = X509_get_ext(cert, loc);
        ASN1_OCTET_STRING* data = X509_EXTENSION_get_data(ext);

        const unsigned char* quote = ASN1_STRING_get0_data(data);
        int quote_len = ASN1_STRING_length(data);

        extract_mrsigner_from_sgx_quote(quote, quote_len);

        // // SGX Quote データを表示（例えばHEX形式で）
        // PRINT("SGX Quote Data (%d bytes):\n", quote_len);
        // for (int i = 0; i < quote_len; i++) {
        //     PRINT("%02X", quote[i]);
        //     if ((i + 1) % 16 == 0) PRINT("\n");
        //     else if (i + 1 != quote_len) PRINT(" ");
        // }
        // PRINT("\n");
    }

    // リソース解放
    X509_free(cert);
    ASN1_OBJECT_free(sgx_quote_oid_obj);
}