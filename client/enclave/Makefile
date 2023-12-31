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

#
# Copyright (C) 2011-2022 Intel Corporation. All rights reserved.
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

INCDIR := $(SGX_SDK)/include # EDL search path
SOCKETINCDIR = $(SOCKET_DIR)/include # socket EDL search path
SSLINCDIR = $(SGXSSL_PKG_PATH)/include
ENC_CASSA_CLIENT_Name := cassa_client_enclave.so
Signed_ENC_CASSA_CLIENT_Name := cassa_client_enclave.signed.so
Enclave_Test_Key := private_test_key.pem

.PHONY: all build clean run

TLS_SRC_FILES = openssl_client/tls_client.cpp
TLS_OBJ_FILES = tls_client.o

SRC_FILES = $(TLS_SRC_FILES) \
			cassa_client.cpp \
			../../sgx_socket/htonl.c \
			../../sgx_socket/htons.c \
			../../sgx_socket/trusted_sgx_socket.cpp \
			../../common/err_msg.cpp \
			../../common/openssl_utility.cpp \
			../../common/tcommon.cpp \
			../../common/utility.cpp \
			../../common/verify_callback.cpp

OBJ_FILES = $(TLS_OBJ_FILES) \
			cassa_client.o \
			htonl.o \
			htons.o \
			trusted_sgx_socket.o \
			err_msg.o \
			openssl_utility.o \
			tcommon.o \
			utility.o \
			verify_callback.o \
            cassa_client_t.o

all:
	$(MAKE) build
	$(MAKE) sign

build:
	@ echo "Compilers used: $(CC), $(CXX)"
	$(SGX_EDGER8R) --trusted ../cassa_client.edl --search-path . \
		--search-path ../../common \
		--search-path $(INCDIR) \
		--search-path $(SOCKETINCDIR) \
		--search-path $(SSLINCDIR) \
		--search-path ../app

	$(CXX) -c -DM_TLS_CLIENT $(Enclave_Cpp_Flags) -I. -include "tsgxsslio.h" -std=c++17 $(SRC_FILES)
	$(CC) -c $(Enclave_C_Flags) -I. ./cassa_client_t.c
	$(CXX) -o $(ENC_CASSA_CLIENT_Name) $(OBJ_FILES) $(Enclave_Link_Flags)

sign:
ifeq ($(wildcard $(Enclave_Test_Key)),)
	@echo "There is no enclave test key<Enclave_private_test.pem>."
	@echo "The project will generate a key<Enclave_private_test.pem> for test."
	@openssl genrsa -out $(Enclave_Test_Key) -3 3072
endif
	$(SGX_ENCLAVE_SIGNER) sign -key $(Enclave_Test_Key) -enclave $(ENC_CASSA_CLIENT_Name) \
		-out $(Signed_ENC_CASSA_CLIENT_Name) -config client_enc.config.xml

clean:
	rm -f ./*.o $(ENC_CASSA_CLIENT_Name) $(Signed_ENC_CASSA_CLIENT_Name) cassa_client_t.* cassa_client_args.h
