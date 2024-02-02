#!/bin/bash

# エンクレーブ署名ファイルのパス
ENCLAVE_SIGNED_SO="../server/enclave/cassa_server_enclave.signed.so"
# 出力ファイル
OUTPUT_FILE="mrsigner.bin"

# MRSIGNERを抽出
sgx_sign dump -enclave $ENCLAVE_SIGNED_SO -dumpfile out.log

# MRSIGNER値を抽出してバイナリファイルに保存
grep "mrsigner->value:" out.log -A 32 | \
  tail -n 32 | \
  tr -d ' ' | \
  tr -d '\n' | \
  sed 's/0x//g' | \
  sed 's/mrsigner->value://g' | \
  xxd -r -p > $OUTPUT_FILE

# ログファイルを削除
# rm out.log
