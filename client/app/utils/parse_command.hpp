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
    // create JSON object for the transaction
    nlohmann::json transaction = nlohmann::json::object();

    // add timestamp and client sessionID
    transaction["timestamp"] = "2023-12-27T12:00:00"; // 仮のtimestamp
    transaction["client_sessionID"] = "session1234";  // 仮のclient sessionID

    // add transaction array
    transaction["transaction"] = nlohmann::json::array();

    // parse operations and add them to the transaction
    for (const auto &command : commands) {
        std::istringstream ss(command);
        std::string word;
        while (ss >> word) {
            nlohmann::json operation = nlohmann::json::object();
            std::string key, value;
            ss >> key;

            if (word == "INSERT" || word == "WRITE" || word == "RMW") {
                ss >> value;
                operation["operation"] = word;
                operation["key"] = key;
                operation["value"] = value;

                transaction["transaction"].push_back(operation);
            } else if (word == "READ" || word == "DELETE") {
                operation["operation"] = word;
                operation["key"] = key;

                transaction["transaction"].push_back(operation);
            } else if (word == "SCAN") {
                // TODO: SCAN
            }
        }
    }
    // return empty json object if the transaction is empty
    return transaction;
}