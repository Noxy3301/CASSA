#pragma once

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <random>

#include "../../Include/structures.h"
#include "../../Include/consts.h"
#include "../../Enclave/Enclave.h"

#include "../../Include/third_party/json.hpp"

class CommandHandler {
public:
    std::random_device rnd;

    // 最後に発行したトランザクションのID
    size_t txIDcounter_ = 0;

    /**
     * @brief Print help message
    */
    void printHelp() {
        std::cout 
            << "=== Available commands ===\n"
            << "  - [x] /help: Display available commands and their usage.\n"
            << "  - [x] /exit: Terminate the command handler.\n"
            << "  - [ ] /mktable <name>: Create a new table with the specified name.\n"

            << "  - [x] /mkproc: Create a new procedure.\n"
            << "  - [x] /endproc: End the current procedure and send to the enclave.\n"
            << "  - [ ] /showproclist: Show the list of procedures.\n"
            << "  - [ ] /delproc <name>: Delete the procedure with the specified name.\n"

            << "=== Procedure commands ===\n"
            << "  - [x] BEGIN_TRANSACTION: Begin a new transaction.\n"
            << "  - [x] END_TRANSACTION: End the current transaction.\n"
            << "  - [x] INSERT <key> <value>: Insert a new key-value pair.\n"
            << "  - [ ] DELETE <key>        : Delete the key-value pair associated with the specified key.\n"
            << "  - [x] READ   <key>        : Retrieve the value associated with the specified key.\n"
            << "  - [x] WRITE  <key> <value>: Set a value associated with the specified key.\n"
            << "  - [ ] RMW    <key> <value>: Read the value associated with the specified key, and then write the specified value.\n"
            << "  - [ ] SCAN   <key> <count>: Retrieve the specified number of key-value pairs starting from the specified key.\n"

            << "=== Usage ===\n"
            << "  - All commands are case-sensitive.\n"
            << "  - Commands and their arguments must be separated by spaces.\n"
            << "  - Only ASCII characters are allowed in commands and arguments.\n"
            << "  - Each transaction, enclosed by BEGIN_TRANSACTION and END_TRANSACTION, must include one or more operations.\n"
            << "  - Each procedure must consist of one or more transactions.\n"
            << "  - Each procedure must be enclosed by EXIT.\n"

            << "=== Examples ===\n"
            << "  > BEGIN_TRANSACTION\n"
            << "  > INSERT key1 value1\n"
            << "  > WRITE key2 value2\n"
            << "  > READ key3\n"
            << "  > END_TRANSACTION\n"
            << "  > BEGIN_TRANSACTION\n"
            << "  > READ key4\n"
            << "  > END_TRANSACTION\n"
            << "  > DONE"
        << std::endl;
    }

    /**
     * @brief Check if the string is ASCII
     * @param[in] str string to check
     * @return bool true if the string is ASCII
    */
    bool isAscii(const std::string &str) {
        for(char c: str) {
            if(!isascii(c)) return false;
        }
        return true;
    }

    /**
     * @brief Check if the string is an operation
     * @param[in] str string to check
     * @return bool true if the string is an operation
    */
    bool isOperation(const std::string &str) {
        return str == "INSERT" ||
               str == "DELETE" ||
               str == "READ"   ||
               str == "WRITE"  ||
               str == "RMW"    ||
               str == "SCAN";
    }

    /**
     * @brief Check if the operation is valid
     * @param[in] operation operation to check
     * @return bool true if the operation is valid, false otherwise
     * @note If false, an error message will also be returned
    */
    std::pair<bool, std::string> checkOperationSyntax(const std::string &operation) {
        std::istringstream stream(operation);
        std::string operation_type;

        // get operation type (e.g., INSERT, READ, WRITE, etc.)
        stream >> operation_type;
        if (!isAscii(operation_type)) {
            return std::make_pair(false, "Error: Non-ASCII character detected in: " + operation_type);
        }
        if (!isOperation(operation_type)) {
            return std::make_pair(false, "Unknown operation: " + operation_type);
        }
        
        // check if the operation_type has the correct number of arguments and if the arguments are ASCII
        if (operation_type == "INSERT" || operation_type == "WRITE" || operation_type == "RMW") {
            // INSERT, WRITE, RMW operations require two arguments (key, value)
            std::string key, value;
            if (!(stream >> key >> value)) {
                return std::make_pair(false, "Syntax error: " + operation_type + " operation requires `key` and `value`.");
            } else {
                // check if the arguments are ASCII
                if (!isAscii(key)) {
                    return std::make_pair(false, "Error: Non-ASCII character detected in: " + key);
                }
                if (!isAscii(value)) {
                    return std::make_pair(false, "Error: Non-ASCII character detected in: " + value);
                }

                // check too many arguments
                std::string extra;
                if (stream >> extra) {
                    return std::make_pair(false, "Syntax error: Too many arguments for the " + operation_type + " operation.");
                }
            }
        } else if (operation_type == "READ" || operation_type == "DELETE") {
            // READ, DELETE operations require one argument (key)
            std::string key;
            if (!(stream >> key)) {
                return std::make_pair(false, "Syntax error: READ operation requires a key.");
            } else {
                // check if the argument is ASCII
                if (!isAscii(key)) {
                    return std::make_pair(false, "Error: Non-ASCII character detected in: " + key);
                }

                // check too many arguments
                std::string extra;
                if (stream >> extra) {
                    return std::make_pair(false, "Syntax error: Too many arguments for the " + operation_type + " operation.");
                }
            }
        } else if (operation_type == "SCAN") {
            // TODO: implement
        } else {
            return std::make_pair(false, "Unknown operation type: " + operation_type);
        }

        return std::make_pair(true, "Syntax is correct.");
    }



    /**
     * @brief Handle commands
     * @param[in] command command to handle
     * @return bool true if CommandHandler needs to continue
    */
    bool handleCommand(const std::string &command) {
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

        // コマンドマップの作成, 各コマンドに対応するラムダ関数をマップに追加
        std::unordered_map<std::string, std::function<bool(const std::vector<std::string>&)>> command_map;

        command_map["/help"] = [](const std::vector<std::string>&) {
            std::cout 
                << "=== Available commands ===\n"
                << "  - /help: Display available commands and their usage.\n"
                << "  - /exit: Terminate the command handler.\n"
                << "  - /mktable <name>: Create a new table with the specified name.\n"

                << "=== Procedure commands ===\n"
                << " - [x] BEGIN_TRANSACTION: Begin a new transaction.\n"
                << " - [x] END_TRANSACTION: End the current transaction.\n"
                << " - [x] DONE: End the current procedure and send to the enclave.\n\n"
                << " - [x] INSERT <key> <value>: Insert a new key-value pair.\n"
                << " - [ ] DELETE <key>        : Delete the key-value pair associated with the specified key.\n"
                << " - [x] READ   <key>        : Retrieve the value associated with the specified key.\n"
                << " - [x] WRITE  <key> <value>: Set a value associated with the specified key.\n"
                << " - [ ] RMW    <key> <value>: Read the value associated with the specified key, and then write the specified value.\n"
                << " - [ ] SCAN   <key> <count>: Retrieve the specified number of key-value pairs starting from the specified key.\n"

                << "=== Usage ===\n"
                << "  - All commands are case-sensitive.\n"
                << "  - Commands and their arguments must be separated by spaces.\n"
                << "  - Only ASCII characters are allowed in commands and arguments.\n"
                << "  - Each transaction, enclosed by BEGIN_TRANSACTION and END_TRANSACTION, must include one or more operations.\n"
                << "  - Each procedure must consist of one or more transactions.\n"
                << "  - Each procedure must be enclosed by EXIT.\n"

                << "=== Examples ===\n"
                << "  BEGIN_TRANSACTION\n"
                << "  INSERT key1 value1\n"
                << "  WRITE key2 value2\n"
                << "  READ key3\n"
                << "  END_TRANSACTION\n"
                << "  BEGIN_TRANSACTION\n"
                << "  READ key4\n"
                << "  END_TRANSACTION\n"
                << "  DONE\n"
            << std::endl;

            return true;
        };

        command_map["/exit"] = [](const std::vector<std::string>&) {
            return false;
        };

        command_map["insert"] = [this](const std::vector<std::string>& args) {
            if (args.size() < 3) {
                std::cout << "Error: Too few arguments"  << " (Usage: insert <key> <value>)" << std::endl;
                return true;
            } else if (args.size() > 3) {
                std::cout << "Error: Too many arguments" << " (Usage: insert <key> <value>)" << std::endl;
                return true;
            }
            // 正常なinsertコマンドが来た場合は、コマンドに対応する処理を実行
            std::vector<char> serialized_data = serializeCommand(OpType::INSERT, args[1], args[2]);

            // 0からWORKER_NUM-1までの乱数を生成
            size_t worker_thid = rnd() % WORKER_NUM;

            // Enclaveにトランザクションを渡す、worker_thidはランダムで設定
            ecall_push_tx(worker_thid, txIDcounter_, reinterpret_cast<const uint8_t*>(serialized_data.data()), serialized_data.size());
            txIDcounter_++;

            return true;
        };

        command_map["write"] = [this](const std::vector<std::string>& args) {
            if (args.size() < 3) {
                std::cout << "Error: Too few arguments"  << " (Usage: write <key> <value>)" << std::endl;
                return true;
            } else if (args.size() > 3) {
                std::cout << "Error: Too many arguments" << " (Usage: write <key> <value>)" << std::endl;
                return true;
            }
            // 正常なwriteコマンドが来た場合は、コマンドに対応する処理を実行
            std::vector<char> serialized_data = serializeCommand(OpType::WRITE, args[1], args[2]);

            // 0からWORKER_NUM-1までの乱数を生成
            size_t worker_thid = rnd() % WORKER_NUM;

            // Enclaveにトランザクションを渡す、worker_thidはランダムで設定
            ecall_push_tx(worker_thid, txIDcounter_, reinterpret_cast<const uint8_t*>(serialized_data.data()), serialized_data.size());
            txIDcounter_++;

            // std::cout << "Setting key: " << args[1] << " to value: " << args[2] << std::endl;
            return true;
        };

        command_map["read"] = [this](const std::vector<std::string>& args) {
            if (args.size() < 2) {
                std::cout << "Error: Too few arguments"  << " (Usage: read <key>)" << std::endl;
                return true;
            } else if (args.size() > 2) {
                std::cout << "Error: Too many arguments" << " (Usage: read <key>)" << std::endl;
                return true;
            }
            // 正常なreadコマンドが来た場合は、コマンドに対応する処理を実行
            std::vector<char> serialized_data = serializeCommand(OpType::READ, args[1]);

            // 0からWORKER_NUM-1までの乱数を生成
            size_t worker_thid = rnd() % WORKER_NUM;

            // Enclaveにトランザクションを渡す、worker_thidはランダムで設定
            ecall_push_tx(worker_thid, txIDcounter_, reinterpret_cast<const uint8_t*>(serialized_data.data()), serialized_data.size());
            txIDcounter_++;

            return true;
        };
        // 他のコマンドも同様に追加する
        // ユーザーから入力されたコマンドをマップで検索
        auto it = command_map.find(tokens[0]);
        if (it != command_map.end()) { // もしマップ内にコマンドが見つかったら
            // 関連するラムダ関数を実行し、その結果を返す
            return it->second(tokens);
        } else { // もしマップ内にコマンドが見つからなかったら
            std::cout << "Unknown command: " << tokens[0] << std::endl;
            return true; // 不明なコマンドが来た場合でも、処理を続行
        }
    }

    std::vector<char> serializeCommand(OpType opType, const std::string& key, const std::string &value = "");
};