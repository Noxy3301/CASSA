#pragma once

#include <string>
#include <sstream>
#include <vector>

#include "../../../common/third_party/json.hpp"

/**
 * @brief Parse commands and generate JSON object
 * @param[in] commands commands to parse
 * @return nlohmann::json JSON object
 * 
 * @details This function accepts an array and converts it into a valid JSON object discribed below
 * Example:
 *   std::vector<std::string> commands = {
 *       "BEGIN_TRANSACTION",
 *       "INSERT key1 value1",
 *       "WRITE key2 value2",
 *       "READ key3",
 *       "END_TRANSACTION"
 *   };
*/
nlohmann::json parse_command(const std::vector<std::string> &commands) {
    nlohmann::json transactions = nlohmann::json::array();
    nlohmann::json current_transaction;

    for (const auto &command : commands) {
        std::istringstream ss(command);
        std::string word;
        while (ss >> word) {
            if (word == "BEGIN_TRANSACTION") {
                current_transaction = nlohmann::json::object();
                current_transaction["timestamp"] = "2023-12-27T12:00:00"; // 仮のtimestamp
                current_transaction["client_sessionID"] = "session1234";  // 仮のclient sessionID
                current_transaction["transaction"] = nlohmann::json::array();
            } else if (word == "END_TRANSACTION") {
                transactions.push_back(current_transaction);
            } else {
                // parse operations
                nlohmann::json operation = nlohmann::json::object();
                std::string key, value;
                ss >> key;

                if (word == "INSERT" || word == "WRITE" || word == "RMW") {
                    ss >> value;
                    operation["operation"] = word;
                    operation["key"] = key;
                    operation["value"] = value;

                    current_transaction["transaction"].push_back(operation);
                } else if (word == "READ" || word == "DELETE") {
                    operation["operation"] = word;
                    operation["key"] = key;

                    current_transaction["transaction"].push_back(operation);
                } // TODO: SCAN
            }
        }
    }
    return transactions;
}