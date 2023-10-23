#include <iostream>
#include <chrono>

#include "json.hpp"

// for convenience
using json = nlohmann::json;

int main() {
    auto create_json_start = std::chrono::high_resolution_clock::now();

    json j;
    j["pi"] = 3.141;
    j["happy"] = true;
    j["name"] = "Niels";
    j["nothing"] = nullptr;
    j["answer"]["everything"] = 42;  // 存在しないキーを指定するとobjectが構築される
    j["list"] = { 1, 0, 2 };         // [1,0,2]
    j["object"] = { {"currency", "USD"}, {"value", 42.99} };  // {"currentcy": "USD", "value": 42.99}

    auto create_json_end = std::chrono::high_resolution_clock::now();

    std::cout << j << std::endl;  // coutに渡せば出力できる。
    auto create_json_duration = std::chrono::duration_cast<std::chrono::microseconds>(create_json_end - create_json_start);
    std::cout << "create json time: " << create_json_duration.count() << " us" << std::endl;

    auto string_to_json_start = std::chrono::high_resolution_clock::now();

    std::string s = R"({ "happy": "happy_cat", "pi": 3.141 } )";
    json j2 = json::parse(s);

    auto string_to_json_middle = std::chrono::high_resolution_clock::now();

    std::string str = j2["happy"].get<std::string>();;

    auto string_to_json_end = std::chrono::high_resolution_clock::now();

    std::cout << j2["happy"] << std::endl;

    auto string_to_json_time = std::chrono::duration_cast<std::chrono::microseconds>(string_to_json_middle - string_to_json_start);
    auto extract_item_time = std::chrono::duration_cast<std::chrono::microseconds>(string_to_json_end - string_to_json_middle);

    std::cout << "json to string time: " << string_to_json_time.count() << " us" << std::endl;
    std::cout << "extract item time: " << extract_item_time.count() << " us" << std::endl;
    
    // 思ったより処理速度が早くて助かるな
}