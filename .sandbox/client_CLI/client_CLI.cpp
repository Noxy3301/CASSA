#include <iostream>
#include <vector>
#include <string>

#include "../../client/app/utils/command_handler.hpp"
#include "../../client/app/utils/parse_command.hpp"

/**
 * 方針メモ
 * - "/"で始まるコマンドかどうかの確認
 * - transactionの中身を表示するコマンドを別途用意する？
 * - transactionの中身は、operationを新規登録した際にも表示する
 * - JSONを直で受け取るモードと、コマンドを受け取るモードを用意する？
 * 
 * 
 * 手順
 * 1. 入力を受け取る
 * 2. 入力を解析する
 *   - コマンドの場合("/maketx"など)
 *     - 有効なコマンドの場合、その結果を表示する
 *     - 無効なコマンドの場合、unknown command errorを表示する
 *   - オペレーションの場合
 *     - /maketxでtransaction operationを受け入れる状態になっているかを確認する
 *     - 有効なオペレーションの場合、追加したうえでtransactionの全体を表示する
 *     - 無効なオペレーションの場合、unknown operation error/too few or too many argumentsを表示する
 * 3. 2に戻る
*/

void handle_command() {
    std::cout << "\033[32m" << "[ INFO     ] " << "\033[0m" 
              << "Awaiting User Commands... (type '/help' for available commands, '/exit' to quit)" << std::endl;

    CommandHandler command_handler;
    std::vector<std::string> operations;
    bool in_transaction = false;

    while (true) {
        if ()

        // if in transaction, print the green prompt
        if (in_transaction) {
            std::cout << "\033[34m" << "> " << "\033[0m";
        } else {
            std::cout << "> ";
        }

        // get command from user
        std::string command;
        std::getline(std::cin, command);

        // exit the loop if EOF(ctrl+D) is detected or if "/exit" command is entered by the user
        if (std::cin.eof() || command == "/exit") {
            std::cout << "Exiting..." << std::endl;
            break;
        }

        // handle /help command
        if (command == "/help") {
            command_handler.printHelp();
            continue;
        }

        // handle /maketx command
        if (command == "/maketx") {
            // check if the user is already in transaction
            if (in_transaction) {
                std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
                          << "You are already in transaction. Please finish or abort the current transaction." << std::endl;
            } else {
                std::cout << "\033[32m" << "[ INFO     ] " << "\033[0m" 
                          << "You are now in transaction. Please enter operations." << std::endl;
                operations.clear();

                // add BEGIN_TRANSACTION and set in_transaction to true
                operations.push_back("BEGIN_TRANSACTION");
                in_transaction = true;
            }
            continue;
        }

        // handle /endtx command
        if (command == "/endtx") {
            if (in_transaction) {
                // add END_TRANSACTION and set in_transaction to false
                operations.push_back("END_TRANSACTION");
                in_transaction = false;

                // check if the transaction has at least 1 operation (except BEGIN_TRANSACTION and END_TRANSACTION)
                if (operations.size() > 2) {
                    // create timestamp


                    // create JSON object for the transaction
                    nlohmann::json transaction_json = parse_command(operations);

                    // dump json object to string
                    std::string transaction_json_string = transaction_json.dump();
                    uint8_t *dumped_json_data = const_cast<uint8_t *>(reinterpret_cast<const uint8_t*>(transaction_json_string.data()));
                    size_t dumped_json_size = transaction_json_string.size();

                    // send the transaction to the server
                    // = debug =
                    std::cout << "\033[34m" << "[ DEBUG    ] " << "\033[0m" 
                              << "Dumped JSON data: " << dumped_json_data << std::endl;

                    // reset the operations
                    operations.clear();
                }
            } else {
                std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
                          << "You are not in transaction. Please enter '/maketx' to start a new transaction." << std::endl;
            }
            continue;
        }

        // handle operations if in transaction
        if (in_transaction) {
            // check if the command is a valid operation
            std::pair<bool, std::string> check_syntax_result = command_handler.checkOperationSyntax(command);
            bool is_valid_syntax = check_syntax_result.first;
            std::string operation = check_syntax_result.second;

            if (is_valid_syntax) {
                // if the operation is valid, add it to the transaction
                operations.push_back(command);
                std::cout << "\033[32m" << "[ INFO     ] " << "\033[0m" 
                          << "Operation added to transaction." << std::endl;
            } else {
                // if the operation is invalid, print the error message
                std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
                          << check_syntax_result.second << std::endl;
            }
            continue;
        }

        // handle unknown command because valid command was not entered here
        std::cout << "\033[31m" << "[ ERROR    ] " << "\033[0m" 
                  << "Unknown command. Please enter '/help' to see available commands." << std::endl;
    }
}

int main() {
    handle_command();
    return 0;
}