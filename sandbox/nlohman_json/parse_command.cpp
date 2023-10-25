#include <iostream>
#include <string>
#include <sstream>
#include <vector>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp> // random_generator

#include "json.hpp"
#include "../../NormalApp/Enclave/silo_cc/include/silo_procedure.h" // Procedure

#define DEBUG

/**
 * Generate UUID and convert it to uint64_t pair for optimization
*/
std::pair<uint64_t, uint64_t> generateUUID_uint64tPair() {
    boost::uuids::uuid id = boost::uuids::random_generator()();
    const uint8_t* data = id.data;

    uint64_t high = 0;
    uint64_t low = 0;
    for (int i = 0; i < 8; ++i) {
        high = (high << 8) | data[i];
        low = (low << 8) | data[i + 8];
    }

    return std::make_pair(high, low);
}

nlohmann::json parseCommand(const std::vector<std::string> &commands) {
    nlohmann::json transactions = nlohmann::json::array();
    nlohmann::json current_transaction = nlohmann::json::object();

    for (const auto &command : commands) {
        // parse operations which sandwiched by `BEGIN_TRANSACTION` and `END_TRANSACTION`
        std::istringstream ss(command);
        std::string word;
        while (ss >> word) {
            std::cout << word << std::endl;
            if (word == "BEGIN_TRANSACTION") {
                // generate UUID<uint64_t, uint64_t> to identify issued transaction
                std::pair<uint64_t, uint64_t> id_uint64t_pair = generateUUID_uint64tPair();

                current_transaction["uuid"] = nlohmann::json::object();
                current_transaction["uuid"]["high"] = id_uint64t_pair.first;
                current_transaction["uuid"]["low"] = id_uint64t_pair.second;
                current_transaction["transactions"] = nlohmann::json::array();
            } else if (word == "END_TRANSACTION") {
                transactions.push_back(current_transaction);
            } else {
                nlohmann::json operation = nlohmann::json::object();

                std::string key, value;
                ss >> key;

                if (word == "INSERT" || word == "WRITE" || word == "RMW") {
                    ss >> value;
                    operation["operation"] = word;
                    operation["key"] = key;
                    operation["value"] = value;

                    current_transaction["transactions"].push_back(operation);
                } else if (word == "READ" || word == "DELETE") {
                    operation["operation"] = word;
                    operation["key"] = key;

                    current_transaction["transactions"].push_back(operation);
                } // TODO: SCAN
            }
        }
    }
    return transactions;
}

std::string operationTypeToString(OpType ope_type) {
    if (ope_type == OpType::INSERT) {
        return "INSERT";
    } else if (ope_type == OpType::READ) {
        return "READ";
    } else if (ope_type == OpType::WRITE) {
        return "WRITE";
    }

    return "UNKNOWN";
}

std::vector<Procedure> jsonToProcedures(const nlohmann::json &transactions_json) {
    std::vector<Procedure> procedures;
    for (const auto &transaction : transactions_json) {
        uint64_t uuid_high = transaction["uuid"]["high"];
        uint64_t uuid_low = transaction["uuid"]["low"];

        for (const auto &operation : transaction["transactions"]) {
            std::string operation_str = operation["operation"];
            OpType ope;

            if (operation_str == "INSERT") {
                ope = OpType::INSERT;
            } else if (operation_str == "READ") {
                ope = OpType::READ;
            } else if (operation_str == "WRITE") {
                ope = OpType::WRITE;
            } else {
                std::cout << "Unknown operation: " << operation_str << std::endl;
                assert(false);
            }

            std::string key_str = operation["key"];
            std::string value_str = operation.value("value", ""); // If value does not exist (e.g., READ), set empty string

            procedures.emplace_back(uuid_high, uuid_low, ope, key_str, value_str); // TODO: txIDcounter
        }
    }

    return procedures;
}

int main() {
#ifdef DEBUG
    std::vector<std::string> commands = {
        "BEGIN_TRANSACTION",
        "INSERT key1 value1",
        "WRITE key2 value2",
        "READ key3",
        "END_TRANSACTION",

        "BEGIN_TRANSACTION",
        "READ key4",
        "END_TRANSACTION",

        "BEGIN_TRANSACTION",
        "WRITE unchi buri;",
        "END_TRANSACTION"
    };
#else
    std::vector<std::string> commands;
    std::string command;

    std::cout << "Please input commands. (EXIT to finish)" << std::endl;
    while (true) {
        std::getline(std::cin, command);
        if (command == "EXIT") break;
        commands.push_back(command);
    }
#endif

    nlohmann::json transactions = parseCommand(commands);
    std::string json_string = transactions.dump();
    std::cout << transactions.dump() << std::endl;

    // std::stringからnlohmann::jsonに変換して個々の情報を抽出してみる
    nlohmann::json transaction_json = nlohmann::json::parse(json_string);
    std::vector<Procedure> procedures = jsonToProcedures(transaction_json);

    for (auto &procedure : procedures) {
        std::cout << procedure.uuid_high_ << " " << procedure.uuid_low_ << std::endl;
        std::cout << operationTypeToString(procedure.ope_) << std::endl;
        std::cout << procedure.key_ << std::endl;
        std::cout << procedure.value_ << std::endl;
    }

    return 0;
}