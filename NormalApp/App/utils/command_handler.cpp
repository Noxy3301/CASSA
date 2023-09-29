# include "include/command_handler.h"

bool CommandHandler::isAscii(const std::string& str) {
    for(char c: str) {
        if(!isascii(c)) return false; // ASCIIでない文字が見つかった場合はfalseを返す
    }
    return true; // 全ての文字がASCIIであった場合はtrueを返す
}

bool CommandHandler::handleCommand(const std::string& command) {
    std::istringstream stream(command); // コマンド文字列からストリームを作成
    std::vector<std::string> tokens;    // トークンを格納するベクター
    std::string token;

    // ストリームからトークンを取り出す
    while (stream >> token) {
        if (!isAscii(token)) {
            std::cout << "Error: Non-ASCII character detected in: " << token << std::endl;
            return true; // 非ASCII文字を検出した場合は、エラーメッセージを出力して続行
        }
        tokens.push_back(token);
    }

    // トークンが存在しない場合、trueを返す
    if (tokens.empty()) return true;

    // コマンドマップの作成
    std::unordered_map<std::string, std::function<bool(const std::vector<std::string>&)>> commandMap;
    
    // 各コマンドに対応するラムダ関数をマップに追加
    commandMap["help"] = [](const std::vector<std::string>&) {
        std::cout << "Available commands:\n"
                  << "  - help: Display available commands and their usage.\n"
                  << "  - exit: Terminate the command handler.\n"
                  << "  - write <key> <value>: Set a value associated with the specified key.\n"
                  << "  - read <key>: Retrieve the value associated with the specified key.\n"
                  << "  - mktable <name>: Create a new table with the specified name.\n"
                  << "Usage:\n"
                  << "  - All commands are case-sensitive.\n"
                  << "  - Commands and their arguments must be separated by spaces.\n"
                  << "  - Only ASCII characters are allowed in commands and arguments.\n";
        return true;
    };

    commandMap["exit"] = [](const std::vector<std::string>&) {
        return false;
    };

    commandMap["write"] = [this](const std::vector<std::string>& args) {
        if (args.size() < 3) {
            std::cout << "Error: Too few arguments"  << " (Usage: write <key> <value>)" << std::endl;
            return true;
        } else if (args.size() > 3) {
            std::cout << "Error: Too many arguments" << " (Usage: write <key> <value>)" << std::endl;
            return true;
        }
        // 正常なwriteコマンドが来た場合は、コマンドに対応する処理を実行
        std::vector<char> write_serialized_data = serializeCommand(OpType::WRITE, args[1], args[2]);

        // 0からWORKER_NUM-1までの乱数を生成
        size_t worker_thid = rnd() % WORKER_NUM;

        // Enclaveにトランザクションを渡す、worker_thidはランダムで設定
        ecall_push_tx(worker_thid, txIDcounter_, reinterpret_cast<const uint8_t*>(write_serialized_data.data()), write_serialized_data.size());
        txIDcounter_++;

        // std::cout << "Setting key: " << args[1] << " to value: " << args[2] << std::endl;
        return true;
    };

    commandMap["read"] = [this](const std::vector<std::string>& args) {
        if (args.size() < 2) {
            std::cout << "Error: Too few arguments"  << " (Usage: read <key>)" << std::endl;
            return true;
        } else if (args.size() > 2) {
            std::cout << "Error: Too many arguments" << " (Usage: read <key>)" << std::endl;
            return true;
        }
        // 正常なreadコマンドが来た場合は、コマンドに対応する処理を実行
        std::vector<char> write_serialized_data = serializeCommand(OpType::READ, args[1]);




        std::cout << "Retrieving value for key: " << args[1] << std::endl;
        std::cout << "Value: DUMMY_VALUE" << std::endl; // 仮の値（ダミー）を出力
        return true;
    };
    // 他のコマンドも同様に追加する
    // ユーザーから入力されたコマンドをマップで検索
    auto it = commandMap.find(tokens[0]);
    if (it != commandMap.end()) { // もしマップ内にコマンドが見つかったら
        // 関連するラムダ関数を実行し、その結果を返す
        return it->second(tokens);
    } else { // もしマップ内にコマンドが見つからなかったら
        std::cout << "Unknown command: " << tokens[0] << std::endl;
        return true; // 不明なコマンドが来た場合でも、処理を続行
    }
}

std::vector<char> CommandHandler::serializeCommand(OpType opType, const std::string& key, const std::string &value) {
    std::vector<char> data;
    
    // Serialize OpType
    data.push_back(static_cast<std::uint8_t>(opType));
    
    // Serialize Key
    uint32_t key_size = key.size();
    data.insert(data.end(), reinterpret_cast<const char*>(&key_size), reinterpret_cast<const char*>(&key_size) + sizeof(key_size));
    data.insert(data.end(), key.begin(), key.end());

    // Serialize Value if necessary
    if (opType == OpType::WRITE || opType == OpType::INSERT || opType == OpType::RMW) {
        // Serialize Value
        uint32_t value_size = value.size();
        data.insert(data.end(), reinterpret_cast<const char*>(&value_size), reinterpret_cast<const char*>(&value_size) + sizeof(value_size));
        data.insert(data.end(), value.begin(), value.end());
    }
    
    return data;
}