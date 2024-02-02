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

#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/x509_vfy.h>
#include <string.h>
#include "utility.h"
#include "common.h"

#ifdef CLIENT_USE_QVL
#include "verify_mrsigner.h"
#endif

int verify_callback(int preverify_ok, X509_STORE_CTX* ctx) {
    int ret = 0;
    int der_len = 0;
    unsigned char* der = nullptr;
    int sup_data_len = 0;
    time_t current_time;
    unsigned char* sup_data = nullptr;
    unsigned char* buff = nullptr;
    sgx_ql_qv_result_t qv_result;
    quote3_error_t result = SGX_QL_SUCCESS;
    X509* crt = nullptr;
    int err = X509_V_ERR_UNSPECIFIED;

    PRINT(TLS_CLIENT "Verifying Certificate (callback called with  preverify_ok = %d)\n", preverify_ok);
    crt = X509_STORE_CTX_get_current_cert(ctx);
    if (crt == nullptr) {
        PRINT(TLS_CLIENT "Error: failed to retrieve certificate\n");
        goto done;
    }

    if (preverify_ok == 0) {
        err = X509_STORE_CTX_get_error(ctx);
        if (err == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) {
            // A self-signed certificate is expected, return 1 to continue the verification process
            PRINT(TLS_CLIENT "Detected self-signed certificate\n");
            ret = 1;
            goto done;
        }
    }

    // convert a cert into a buffer in DER format
    der_len = i2d_X509(crt, nullptr);
    if (der_len <= 0) {
        PRINT(TLS_CLIENT "Error: i2d_X509 failed(der_len = %d)\n", der_len);
        goto done;
    }

    buff = (unsigned char*)malloc(der_len);
    if (buff == nullptr) {
        PRINT(TLS_CLIENT "Error: malloc failed (der_len = %d)\n", der_len);
        goto done;
    }
    der = buff;
    der_len = i2d_X509(crt, &buff);
    if (der_len < 0) {
        PRINT(TLS_CLIENT "Error: i2d_X509 failed(der_len = %d)\n", der_len);
        goto done;
    }

    // note: i2d_X509() updates the pointer to the buffer so that following the
    // call to i2d_X509(), buff is pointing to the "end" of the data buffer
    // pointed by buff That is, buff = buff + der_len;
    PRINT(
        TLS_CLIENT "der=%p buff=%p buff moved by %d offset der_len=%d\n",
        der,
        buff,
        (int)(buff - der),
        der_len);

    // PRINT("===== Verifying certificate start =====\n");
    // inside enclave, the current_time is acquired by ocall, this is just an example
    // current_time by ocall is untrusted, user please be aware of it.
    GETCURRTIME(&current_time);

    // verify tls certificate
    result = VERIFY_CALLBACK(der, der_len, current_time, &qv_result, &sup_data, (uint32_t *)&sup_data_len);

   // result != SGX_QL_SUCCESS means critical error
    if (result != SGX_QL_SUCCESS) {
        PRINT(TLS_CLIENT "Error: Quote Verification Failed with result(%x) - \n", result);
        p_sgx_tls_qe_err_msg(result);
        goto done;
    } else {
        // We only print the warning info in this sample
        // In your product, we suggest you to check qv_result and supplemental data, define your own verification policy
        if (qv_result != SGX_QL_QV_RESULT_OK) {
            PRINT(TLS_CLIENT "Certificate Verification: Passed (with Non-critical Errors)\n");
            PRINT("You can define your own verification policy based on below info:\n");
            p_sgx_tls_qv_err_msg(qv_result);
        }

        // DER形式の証明書データからSGX Quoteを抽出して、MRSIGNERを比較 (clientのみ)
#ifdef CLIENT_USE_QVL
        if (!extract_sgx_quote_from_der_cert(der, der_len)) {
            FREE_SUPDATA(sup_data);
            ret = 0;
            goto done;
        }
#endif
    }

    FREE_SUPDATA(sup_data);

    // PRINT("===== Verifying certificate end =====\n");
    ret = 1;
done:

    if (der)
        free(der);

    if (err != X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT) {
        PRINT(TLS_CLIENT "Verifying SGX certificate extensions ... %s\n", ret ? "succeeded" : "failed");
    }
    return ret;
}
