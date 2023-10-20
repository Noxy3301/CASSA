#include <iostream>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp> // random_generator
#include <chrono>
#include <iomanip> // std::setw, std::setfill

using namespace boost::uuids;

void print_two_uint64_as_hex(uint64_t higher, uint64_t lower) {
    for (int i = 0; i < 8; ++i) {
        uint8_t byte = (higher >> (56 - i * 8)) & 0xFF;
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<unsigned>(byte) << " ";
    }
    for (int i = 0; i < 8; ++i) {
        uint8_t byte = (lower >> (56 - i * 8)) & 0xFF;
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<unsigned>(byte) << " ";
    }
}

int main() {
    std::cout << "\n=== Performace test ===" << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now(); // 時間計測開始
    const uuid id = random_generator()();   // ランダムにユニークIDを生成
    auto end = std::chrono::high_resolution_clock::now();   // 時間計測終了
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::cout << "Generated UUID: " << id << std::endl;
    std::cout << "Time taken to generate UUID: " << duration.count() << " us" << std::endl;



    std::cout << "\n=== UUID to (uint64_t,uint64_t) test ===" << std::endl;
    // UUIDのデータへのポインタを取得
    const uint8_t* data = id.data;

    // uint64_t 2つに分割
    uint64_t high = 0;
    uint64_t low = 0;
    for (int i = 0; i < 8; ++i) {
        high = (high << 8) | data[i];
        low = (low << 8) | data[i + 8];
    }

    std::cout << std::hex;  // 出力を16進数形式に設定
    for (int i = 0; i < 16; ++i) {
        std::cout << std::setw(2) << std::setfill('0') // 2桁で表示し、空いている場所は0で埋める
                  << static_cast<unsigned>(data[i]) << " ";
    }
    std::cout << std::dec << std::endl;  // 出力を10進数形式に戻す

    std::cout << "High uint64_t: " << high << std::endl;
    std::cout << "Low uint64_t: " << low << std::endl;



    std::cout << "\n=== UUID to char array test ===" << std::endl;
    // char 16個に分割
    char chars[16];
    std::copy(data, data + 16, chars);

    for (char c : chars) {
        std::cout << static_cast<int>(c) << " ";
    }
    std::cout << std::endl;



    std::cout << "\n=== UUID consisntency test ===" << std::endl;
    std::cout << "- Generated UUID: " << std::endl;
    std::cout << id << std::endl;

    std::cout << "- hexed UUID: " << std::endl;
    std::cout << std::hex;  // 出力を16進数形式に設定
    for (int i = 0; i < 16; ++i) {
        std::cout << std::setw(2) << std::setfill('0') // 2桁で表示し、空いている場所は0で埋める
                  << static_cast<unsigned>(data[i]) << " ";
    }
    std::cout << std::dec << std::endl;  // 出力を10進数形式に戻す

    std::cout << "- uint64_t to hex(uuid): " << std::endl;
    print_two_uint64_as_hex(high, low);
    std::cout << std::endl;

    return 0;
}
