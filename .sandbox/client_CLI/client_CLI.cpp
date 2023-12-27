#include <iostream>
#include <vector>
#include <string>

// #include "../../client/app/utils/command_handler.hpp"

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

    // CommandHandler command_handler;
    std::vector<std::string> operations;

    while (true) {
        // get user input
        std::cout << "> ";
        std::string command;
        std::getline(std::cin, command);

        // exit the loop if EOF(ctrl+D) is detected or if "/exit" command is entered by the user
        // if (std::cin.eof() || command == "/exit") {
        //     operations.clear();  // 念のため
        //     break;
        // }
        if (command == "/exit") {
            std::cout << "aaaa" << std::endl;
            break;
        }
        if (std::cin.eof()) {
            std::cout << "bbbb" << std::endl;
            break;
        }
    }


}

int main() {
    handle_command();
    return 0;
}