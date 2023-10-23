#include <vector>
#include <string>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <chrono>

#include "../../NormalApp/Enclave/silo_cc/include/silo_procedure.h"

// ... (Your existing code)

std::vector<Procedure> ParseJsonToProcedures(const std::string& jsonString) {
    std::vector<Procedure> procedures;
    
    // Replace JSON-like syntax to more easily parseable syntax
    std::string modifiedString = jsonString;
    std::replace(modifiedString.begin(), modifiedString.end(), '{', ' ');
    std::replace(modifiedString.begin(), modifiedString.end(), '}', ' ');
    std::replace(modifiedString.begin(), modifiedString.end(), '[', ' ');
    std::replace(modifiedString.begin(), modifiedString.end(), ']', ' ');
    std::replace(modifiedString.begin(), modifiedString.end(), ',', ' ');
    std::replace(modifiedString.begin(), modifiedString.end(), ':', ' ');

    std::istringstream stream(modifiedString);
    std::string word;

    // Read and parse modified string word by word
    size_t txIDcounter;
    while (stream >> word) {
        if (word == "\"txIDcounter\"") {
            // Read txIDcounter where contains next word
            stream >> txIDcounter;
        } else if (word == "\"ope\"") {
            // Convert string to OpType
            OpType ope;
            stream >> word;

            if (word == "\"READ\"") {
                ope = OpType::READ;
            } else if (word == "\"WRITE\"") {
                ope = OpType::WRITE;
            } else if (word == "\"INSERT\"") {
                ope = OpType::INSERT;
            } else if (word == "\"DELETE\"") {
                ope = OpType::DELETE;
            } else if (word == "\"SCAN\"") {
                ope = OpType::SCAN;
            } else if (word == "\"RMW\"") {
                ope = OpType::RMW;
            } else {
                assert(false);
            }

            // Skip 'key'
            stream >> word;
            std::string key;
            stream >> key;

            // Skip 'value'
            stream >> word;
            std::string value;
            stream >> value;

            procedures.emplace_back(txIDcounter, ope, key, value);
        }
    }
    
    return procedures;
}

// NOTE: This function assumes that key and value is not contains double quotation(")
std::vector<Procedure> ParseJsonToProcedures2(const std::string& jsonString) {
    std::vector<Procedure> procedures;
    size_t txIDcounter = 0; // Assume 0 if not found
    size_t pos = 0;

    // extract txIDcounter
    if ((pos = jsonString.find("\"txIDcounter\"")) != std::string::npos) {
        // Find the position of the next number
        size_t num_start = jsonString.find_first_of("0123456789", pos);
        size_t num_end = jsonString.find_first_not_of("0123456789", num_start);
        txIDcounter = std::stoi(jsonString.substr(num_start, num_end - num_start));
    }

    // extract operations
    pos = 0;
    while ((pos = jsonString.find("\"ope\"", pos)) != std::string::npos) {
        // Find the block of this operation
        size_t block_start = jsonString.rfind('{', pos);
        size_t block_end = jsonString.find('}', pos);
        std::string block = jsonString.substr(block_start, block_end - block_start);

        // Find operation type ("ope")
        size_t ope_start = block.find_first_of("\"", block.find("\"ope\"") + 5) + 1;
        size_t ope_end = block.find_first_of("\"", ope_start);
        std::string ope_str = block.substr(ope_start, ope_end - ope_start);
        
        // Convert string to OpType here
        OpType ope;
        if (ope_str == "READ") {
            ope = OpType::READ;
        } else if (ope_str == "WRITE") {
            ope = OpType::WRITE;
        } else if (ope_str == "INSERT") {
            ope = OpType::INSERT;
        } else if (ope_str == "DELETE") {
            ope = OpType::DELETE;
        } else if (ope_str == "SCAN") {
            ope = OpType::SCAN;
        } else if (ope_str == "RMW") {
            ope = OpType::RMW;
        } else {
            assert(false);
        }

        // Find key
        size_t key_start = block.find_first_of("\"", block.find("\"key\"") + 5) + 1;
        size_t key_end = block.find_first_of("\"", key_start);
        std::string key = block.substr(key_start, key_end - key_start);

        // Find value
        size_t value_start = block.find_first_of("\"", block.find("\"value\"") + 7) + 1;
        size_t value_end = block.find_first_of("\"", value_start);
        std::string value = block.substr(value_start, value_end - value_start);

        procedures.emplace_back(txIDcounter, ope, key, value);
        
        pos = block_end; // Move to the end of this block for the next iteration
    }
    
    return procedures;
}


std::vector<Procedure> AssignToProcedures() {
    std::vector<Procedure> procedures;
    procedures.emplace_back(100, OpType::READ, "key1", "");
    procedures.emplace_back(100, OpType::WRITE, "key2", "value2");
    procedures.emplace_back(100, OpType::READ, "key3", "value3");
    procedures.emplace_back(100, OpType::READ, "key1", "");
    procedures.emplace_back(100, OpType::WRITE, "key2", "value2");
    procedures.emplace_back(100, OpType::READ, "key3", "value3");
    procedures.emplace_back(100, OpType::READ, "key1", "");
    procedures.emplace_back(100, OpType::WRITE, "key2", "value2");
    procedures.emplace_back(100, OpType::READ, "key3", "value3");
    procedures.emplace_back(100, OpType::READ, "key1", "");

    return procedures;
}


int main() {
    std::string jsonString = R"(
        {
            "txIDcounter": 100,
            "operations": [
                {
                    "ope": "READ",
                    "key": "key1",
                    "value": ""
                },
                {
                    "ope": "WRITE",
                    "key": "unchiburiburi",
                    "value": "pakupakuunko"
                },
                {
                    "ope": "READ",
                    "key": "key3",
                    "value": "value3"
                },
                {
                    "ope": "READ",
                    "key": "key1",
                    "value": ""
                },
                {
                    "ope": "WRITE",
                    "key": "unchiburiburi",
                    "value": "pakupakuunko"
                },
                {
                    "ope": "READ",
                    "key": "key3",
                    "value": "value3"
                },
                {
                    "ope": "READ",
                    "key": "key1",
                    "value": ""
                },
                {
                    "ope": "WRITE",
                    "key": "unchiburiburi",
                    "value": "pakupakuunko"
                },
                {
                    "ope": "READ",
                    "key": "key3",
                    "value": "value3"
                },
                {
                    "ope": "READ",
                    "key": "key1",
                    "value": ""
                }
            ]
        }
    )";

    // Parse JSON string to procedures  
    auto start1 = std::chrono::system_clock::now();
    std::vector<Procedure> procedures = ParseJsonToProcedures(jsonString);
    auto end1 = std::chrono::system_clock::now();
    auto elapsed1 = std::chrono::duration_cast<std::chrono::microseconds>(end1 - start1);
    std::cout << "elapsed time: " << elapsed1.count() << " us (micro sec)" << std::endl;

    // Parse JSON string to procedures  
    auto start2 = std::chrono::system_clock::now();
    std::vector<Procedure> procedures2 = ParseJsonToProcedures2(jsonString);
    auto end2 = std::chrono::system_clock::now();
    auto elapsed2 = std::chrono::duration_cast<std::chrono::microseconds>(end2 - start2);
    std::cout << "elapsed time: " << elapsed2.count() << " us (micro sec)" << std::endl;

    // Assign value to Procedure (for benchmark)
    auto start3 = std::chrono::system_clock::now();
    std::vector<Procedure> procedures_test = AssignToProcedures();
    auto end3 = std::chrono::system_clock::now();
    auto elapsed3 = std::chrono::duration_cast<std::chrono::microseconds>(end3 - start3);
    std::cout << "elapsed time: " << elapsed3.count() << " us (micro sec)" << std::endl;

    for (const auto& procedure : procedures2) {
        std::cout << procedure.txIDcounter_ << " " 
                  << static_cast<int>(procedure.ope_) << " "
                  << procedure.key_ << " " 
                  << procedure.value_ 
                  << std::endl;
    }
}