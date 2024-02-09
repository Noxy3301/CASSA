#pragma once

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include <string>
#include <vector>

inline std::string base64_encode(const std::string &data) {
    // BIOオブジェクトを作成
    BIO *bio;
    BIO *b64;
    BUF_MEM *buffer_ptr;

    b64 = BIO_new(BIO_f_base64()); // Base64のフィルタBIOを作成
    bio = BIO_new(BIO_s_mem()); // メモリBIOを作成
    bio = BIO_push(b64, bio); // Base64 BIOをメモリBIOにチェーン

    // Base64の改行を無効にする
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    // データをBase64エンコードしてメモリBIOに書き込む
    BIO_write(bio, data.c_str(), data.length());
    BIO_flush(bio); // バッファをフラッシュ
    BIO_get_mem_ptr(bio, &buffer_ptr); // メモリBIOのバッファへのポインタを取得
    BIO_set_close(bio, BIO_NOCLOSE); // BIO_free_allを呼び出してもメモリを解放しない

    // エンコードされたデータをstd::stringにコピー
    std::string output(buffer_ptr->data, buffer_ptr->length);
    BIO_free_all(bio); // BIOチェーンを解放

    return output;
}

inline std::string base64_decode(const std::string &encoded) {
    // BIOオブジェクトを作成
    BIO *bio;
    BIO *b64;

    int decode_len = encoded.length() * 3 / 4; // デコード後の推定長
    std::vector<char> buffer(decode_len); // デコードされたデータを格納するバッファ

    bio = BIO_new_mem_buf(encoded.c_str(), -1); // エンコードされたデータからメモリBIOを作成
    b64 = BIO_new(BIO_f_base64()); // Base64のフィルタBIOを作成
    bio = BIO_push(b64, bio); // Base64 BIOをメモリBIOにチェーン

    // Base64の改行を無効にする
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);

    // エンコードされたデータをデコードしてバッファに読み込む
    int length = BIO_read(bio, buffer.data(), encoded.length());

    BIO_free_all(bio); // BIOチェーンを解放

    // デコードされたデータをstd::stringに変換して返す
    return std::string(buffer.data(), length);
}
