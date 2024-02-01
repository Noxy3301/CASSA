#pragma once

#include <string>
#include <sstream>
#include <vector>

#include "../../common/third_party/json.hpp"

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
nlohmann::json parse_command(long int timestamp_sec, 
                             long int timestamp_nsec, 
                             const std::string &session_id,
                             const std::vector<std::string> &commands) {
    // create JSON object for the transaction
    nlohmann::json transaction = nlohmann::json::object();

    // add timestamp and client sessionID
    transaction["timestamp_sec"] = timestamp_sec;
    transaction["timestamp_nsec"] = timestamp_nsec;
    transaction["client_sessionID"] = session_id;

    // add transaction array
    transaction["transaction"] = nlohmann::json::array();

    // parse operations and add them to the transaction
    for (const auto &command : commands) {
        std::istringstream ss(command);
        std::string operation_type;
        ss >> operation_type;

        if (operation_type == "SCAN") {
            std::string left_key, right_key;
            bool l_exclusive, r_exclusive;

            ss >> left_key >> std::boolalpha >> l_exclusive >> right_key >> std::boolalpha >> r_exclusive;

            nlohmann::json operation = {
                {"operation", "SCAN"},
                {"left_key", left_key},
                {"l_exclusive", l_exclusive},
                {"right_key", right_key},
                {"r_exclusive", r_exclusive}
            };

            transaction["transaction"].push_back(operation);
        } else {
            // Handle other operations (INSERT, READ, WRITE, etc.) as before
            std::string key, value;
            ss >> key;
            if (operation_type == "INSERT" || operation_type == "WRITE" || operation_type == "RMW") {
                ss >> value;
                nlohmann::json operation = {
                    {"operation", operation_type},
                    {"key", key},
                    {"value", value}
                };

                transaction["transaction"].push_back(operation);
            } else if (operation_type == "READ" || operation_type == "DELETE") {
                nlohmann::json operation = {
                    {"operation", operation_type},
                    {"key", key}
                };

                transaction["transaction"].push_back(operation);
            }
        }
    }

    return transaction;
}
