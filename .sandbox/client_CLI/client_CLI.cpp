#include <iostream>
#include <vector>
#include <string>
#include <iomanip>

#include "../../client/app/utils/command_handler.hpp"
#include "../../client/app/utils/parse_command.hpp"

#include "../../common/ansi_color_code.h"

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

void handle_command(std::string sesison_id) {
    std::cout << BGRN << "[ INFO     ] " << reset 
              << "Awaiting User Commands... (type '/help' for available commands, '/exit' to quit)" << std::endl;

    CommandHandler command_handler;
    std::vector<std::string> operations;
    bool in_transaction = false;

    while (true) {
        // if in transaction, print the current operations
        if (in_transaction) {
            for (size_t i = 0; i < operations.size(); i++) {
                std::cout << CYN << std::setw(4) << std::right << i + 1 << " " << reset 
                          << operations[i] << std::endl;
            }
        }

        // if in transaction, print the green prompt
        if (in_transaction) {
            std::cout << CYN << "> " << reset;
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
                std::cout << BRED << "[ ERROR    ] " << reset
                          << "You are already in transaction. Please finish or abort the current transaction." << std::endl;
            } else {
                std::cout << BGRN << "[ INFO     ] " << reset
                          << "You are now in transaction. Please enter operations." << std::endl;
                operations.clear();

                // set in_transaction to true
                in_transaction = true;
            }
            continue;
        }

        // handle /endtx command
        if (command == "/endtx") {
            if (in_transaction) {
                // set in_transaction to false
                in_transaction = false;

                // check if the transaction has at least 1 operation (except BEGIN_TRANSACTION and END_TRANSACTION)
                if (operations.size() > 0) {
                    // create timestamp


                    // create JSON object for the transaction
                    nlohmann::json transaction_json = parse_command(operations);

                    // dump json object to string
                    std::string transaction_json_string = transaction_json.dump();
                    uint8_t *dumped_json_data = const_cast<uint8_t *>(reinterpret_cast<const uint8_t*>(transaction_json_string.data()));
                    size_t dumped_json_size = transaction_json_string.size();

                    // send the transaction to the server
                    // = debug =
                    std::cout << BBLU << "[ DEBUG    ] " << reset
                              << "Dumped JSON data: " << dumped_json_data << std::endl;

                    // reset the operations
                    operations.clear();
                }
            } else {
                std::cout << BRED << "[ ERROR    ] " << reset
                          << "You are not in transaction. Please enter '/maketx' to start a new transaction." << std::endl;
            }
            continue;
        }

        // handle /undo command
        if (command == "/undo") {
            if (in_transaction) {
                // check if the transaction has at least 1 operation (except BEGIN_TRANSACTION and END_TRANSACTION)
                if (operations.size() > 0) {
                    std::cout << BGRN << "[ INFO     ] " << reset
                              << "Last operation " << GRN << "\"" <<  operations.back() << "\" " << reset << "removed from transaction." << std::endl;
                    // remove the last operation
                    operations.pop_back();
                } else {
                    std::cout << BRED << "[ ERROR    ] " << reset
                              << "No operation to undo." << std::endl;
                }
            } else {
                std::cout << BRED << "[ ERROR    ] " << reset
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
                std::cout << BGRN << "[ INFO     ] " << reset
                          << "Operation " << GRN << "\"" <<  command << "\" " << reset << "added to transaction." << std::endl;
            } else {
                // if the operation is invalid, print the error message
                std::cout << BRED << "[ ERROR    ] " << reset
                          << check_syntax_result.second << std::endl;
            }
            continue;
        }

        // handle unknown command because valid command was not entered here
        std::cout << BRED << "[ ERROR    ] " << reset
                  << "Unknown command. Please enter '/help' to see available commands." << std::endl;
    }
}

int main() {
    std::string session_id = "ABC123";
    handle_command(session_id);
    return 0;
}