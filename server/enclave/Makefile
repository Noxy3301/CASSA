#
# Copyright (C) 2011-2021 Intel Corporation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
#   * Redistributions of source code must retain the above copyright
#     notice, this list of conditions and the following disclaimer.
#   * Redistributions in binary form must reproduce the above copyright
#     notice, this list of conditions and the following disclaimer in
#     the documentation and/or other materials provided with the
#     distribution.
#   * Neither the name of Intel Corporation nor the names of its
#     contributors may be used to endorse or promote products derived
#     from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#

include ../../sgxenv.mk

INCDIR := $(SGX_SDK)/include
ENC_CASSA_SERVER_Name := cassa_server_enclave.so
Signed_ENC_CASSA_SERVER_Name := cassa_server_enclave.signed.so
Enclave_Test_Key := private_test_key.pem

.PHONY: all build clean run

# masstreeをビルドするかどうかを指定する変数（デフォルトではビルドする）
BUILD_MASSTREE ?= 1

MASSTREE_SRC_FILES = masstree/masstree_get.cpp \
					 masstree/masstree_insert.cpp \
					 masstree/masstree_node.cpp \
					 masstree/masstree_remove.cpp \
					 masstree/masstree_scan.cpp \
					 masstree/masstree.cpp

MASSTREE_OBJ_FILES = masstree_get.o \
					 masstree_insert.o \
					 masstree_node.o \
					 masstree_remove.o \
					 masstree_scan.o \
					 masstree.o

SILO_CC_SRC_FILES = silo_cc/silo_log_buffer.cpp \
					silo_cc/silo_log_queue.cpp \
					silo_cc/silo_logger.cpp \
					silo_cc/silo_notifier.cpp \
					silo_cc/silo_transaction.cpp \
					silo_cc/silo_util.cpp

SILO_CC_OBJ_FILES = silo_log_buffer.o \
					silo_log_queue.o \
					silo_logger.o \
					silo_notifier.o \
					silo_transaction.o \
					silo_util.o

SILO_R_SRC_FILES = silo_r/silor.cpp \
				   silo_r/silor_log_archive.cpp

SILO_R_OBJ_FILES = silor.o \
				   silor_log_archive.o

TLS_SRC_FILES = openssl_server/tls_server.cpp
TLS_OBJ_FILES = tls_server.o

# source files
SRC_FILES = $(TLS_SRC_FILES) \
			$(SILO_CC_SRC_FILES) \
			$(SILO_R_SRC_FILES) \
			cassa_server.cpp \
			../../sgx_socket/htonl.c \
			../../sgx_socket/htons.c \
			../../sgx_socket/trusted_sgx_socket.cpp \
			../../common/err_msg.cpp \
			../../common/openssl_utility.cpp \
			../../common/openssl_utility_enclave.cpp \
			../../common/tcommon.cpp \
			../../common/utility.cpp \
			../../common/verify_callback.cpp \

# object files
OBJ_FILES = $(MASSTREE_OBJ_FILES) \
			$(TLS_OBJ_FILES) \
			$(SILO_CC_OBJ_FILES) \
			$(SILO_R_OBJ_FILES) \
			cassa_server.o \
			htonl.o \
			htons.o \
			trusted_sgx_socket.o \
			err_msg.o \
			openssl_utility.o \
			openssl_utility_enclave.o \
			tcommon.o \
			utility.o \
			verify_callback.o \
			cassa_server_t.o 

# masstreeのビルド条件
ifeq ($(BUILD_MASSTREE), 1)
    SRC_FILES += $(MASSTREE_SRC_FILES)
endif

all:
	$(MAKE) build
	$(MAKE) sign

build:
	@ echo "Compilers used: $(CC), $(CXX)"
	$(SGX_EDGER8R) ../cassa_server.edl --trusted \
		--search-path . \
		--search-path $(INCDIR) \
		--search-path $(SOCKET_DIR)/include \
		--search-path ../../common \
		--search-path $(SGXSSL_PKG_PATH)/include \
		--search-path ../app

	$(CXX) -c -DM_TLS_SERVER $(Enclave_Cpp_Flags) -I. -std=c++17 -include "tsgxsslio.h" ${SRC_FILES} -DNO_ENCRYPT -DUSE_SGX
	$(CC) -c $(Enclave_C_Flags) -I. cassa_server_t.c
	$(CXX) -o $(ENC_CASSA_SERVER_Name) $(OBJ_FILES) $(Enclave_Link_Flags)  

sign:
ifeq ($(wildcard $(Enclave_Test_Key)),)
	@echo "There is no enclave test key<Enclave_private_test.pem>."
	@echo "The project will generate a key<Enclave_private_test.pem> for test."
	@openssl genrsa -out $(Enclave_Test_Key) -3 3072
endif
	$(SGX_ENCLAVE_SIGNER) sign -key $(Enclave_Test_Key) -enclave $(ENC_CASSA_SERVER_Name) \
		-out $(Signed_ENC_CASSA_SERVER_Name) -config server_enc.config.xml

clean:
	rm -f *.o $(ENC_CASSA_SERVER_Name) $(Signed_ENC_CASSA_SERVER_Name) cassa_server_t.*
