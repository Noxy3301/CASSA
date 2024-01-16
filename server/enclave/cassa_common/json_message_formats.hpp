#pragma once

#include <string>
#include <vector>

#include "../../../common/third_party/json.hpp"

/**
 * @brief Creates an message in JSON format.
 * 
 * @param error_code The error code.
 * @param content The content of the message.
 * @param read_values The read values as a vector of key-value pairs.
 *                    Defaults to an empty vector if not specified.
 * @return nlohmann::json The JSON object representing the message.
 */
inline nlohmann::json create_message(int error_code, 
                              const std::string &content, 
                              const std::vector<std::pair<std::string, std::string>> &read_values = {}) {
    nlohmann::json msg_json = nlohmann::json::object();

    // add error code and content
    msg_json["error_code"] = error_code;
    msg_json["content"] = content;

    // add read values if any
    if (!read_values.empty()) {
        nlohmann::json read_values_json = nlohmann::json::array();
        for (const auto &pair : read_values) {
            read_values_json.push_back({{pair.first, pair.second}});
        }
        msg_json["read_values"] = read_values_json;
    }

    return msg_json;
}