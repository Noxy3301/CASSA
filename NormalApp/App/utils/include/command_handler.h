#pragma once

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>

class CommandHandler {
public:
    bool isAscii(const std::string& str);

    // コマンドを処理するメソッド
    bool handleCommand(const std::string& command);
};