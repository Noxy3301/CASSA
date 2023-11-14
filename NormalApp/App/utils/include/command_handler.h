#pragma once

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <random>

#include "../../../Include/structures.h"
#include "../../../Include/consts.h"
#include "../../../Enclave/Enclave.h"

#include "../../../Include/third_party/json.hpp"

class CommandHandler {
public:
    std::random_device rnd;

    // 最後に発行したトランザクションのID
    size_t txIDcounter_ = 0;

    bool isAscii(const std::string& str);

    // コマンドを処理するメソッド
    bool handleCommand(const std::string& command);

    std::vector<char> serializeCommand(OpType opType, const std::string& key, const std::string &value = "");
};