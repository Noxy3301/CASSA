#pragma once

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <random>

// #include "../../common/cassa/structures.h"
// #include "../../common/cassa/consts.h"

#include "../../common/third_party/json.hpp"

#include "../../common/ansi_color_code.h"

class CommandHandler {
public:
    std::random_device rnd;
    size_t client_id = 0;

    /**
     * @brief Print help message
    */
    void printHelp() {
        std::cout 
            << "=== Available commands ===\n"
            << "  - " BGRN "[x]" CRESET " /help             : Display available commands and their usage.\n"
            << "  - " BGRN "[x]" CRESET " /exit             : Terminate the command handler.\n"
            << "  - " BRED "[ ]" CRESET " /maketable <name> : Create a new table with the specified name.\n"
            << "  - " BGRN "[x]" CRESET " /maketx           : Create a new transaction.\n"
            << "  - " BGRN "[x]" CRESET " /endtx            : End the current transaction and send to the server.\n"
            << "  - " BGRN "[x]" CRESET " /undo             : Undo the last operation. (Only available in a transaction)\n"

            << "=== Transaction operations ===\n"
            << "  - " BGRN "[x]" CRESET " INSERT <key> <value> : Insert a new key-value pair.\n"
            << "  - " BRED "[ ]" CRESET " DELETE <key>         : Delete the key-value pair associated with the specified key.\n"
            << "  - " BGRN "[x]" CRESET " READ   <key>         : Retrieve the value associated with the specified key.\n"
            << "  - " BGRN "[x]" CRESET " WRITE  <key> <value> : Set a value associated with the specified key.\n"
            << "  - " BRED "[ ]" CRESET " RMW    <key> <value> : Read the value associated with the specified key, and then write the specified value.\n"
            << "  - " BGRN "[x]" CRESET " SCAN   <left_key> <l_exclusive> <right_key> <r_exclusive> : Retrieve key-value pairs between left_key and right_key, with exclusivity flags.\n"

            << "=== Examples ===\n"
            << "  > /maketx\n"
            << "  > INSERT key1 value1\n"
            << "  > WRITE key2 value2\n"
            << "  > READ key3\n"
            << "  > /endtx"
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
            std::string left_key, right_key, l_exclusive_str, r_exclusive_str;
            bool l_exclusive, r_exclusive;

            // SCAN operations require four arguments (left_key, l_exclusive, right_key, r_exclusive)
            if (!(stream >> left_key >> l_exclusive_str >> right_key >> r_exclusive_str)) {
                return std::make_pair(false, "Syntax error: SCAN operation requires `<left_key> <l_exclusive> <right_key> <r_exclusive>`.");
            }

            // convert l_exclusive_str and r_exclusive_str to bool
            l_exclusive = (l_exclusive_str == "true");
            r_exclusive = (r_exclusive_str == "true");

            // check if the exclusivity flags are valid
            if (l_exclusive_str != "true" && l_exclusive_str != "false") {
                return std::make_pair(false, "Syntax error: l_exclusive must be 'true' or 'false'.");
            }
            if (r_exclusive_str != "true" && r_exclusive_str != "false") {
                return std::make_pair(false, "Syntax error: r_exclusive must be 'true' or 'false'.");
            }

            // check if the argument is ASCII
            if (!isAscii(left_key) || !isAscii(right_key)) {
                return std::make_pair(false, "Error: Non-ASCII character detected in keys.");
            }

            // check too many arguments
            std::string extra;
            if (stream >> extra) {
                return std::make_pair(false, "Syntax error: Too many arguments for the " + operation_type + " operation.");
            }

        } else {
            return std::make_pair(false, "Unknown operation type: " + operation_type);
        }

        return std::make_pair(true, "Syntax is correct.");
    }
};