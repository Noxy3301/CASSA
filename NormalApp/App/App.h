#pragma once

// AppとEnclaveでのEcall/Ocallを疑似的に再現するために相互インクルードする
#include "../Enclave/Enclave.h"

void ocall_print_commit_message(size_t txIDcounter, size_t num_process_record);