#include "../../server/enclave/cassa_common/random.h"   // for Xoroshiro128Plus
#include <string>
#include <iostream>
#include <chrono>

unsigned generateSeed() {
    return static_cast<unsigned>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()
    );
}

class SessionIDGenerator {
public:
    SessionIDGenerator(Xoroshiro128Plus& rnd) : rnd_(rnd) {}

    /**
     * @brief Generate a random session ID
     * @note Xoroshiro128Plus has called 6 times
    */
    std::string generateSessionID() {
        std::string id;
        for (int i = 0; i < 6; ++i) {
            id += numToChar(rnd_.next() % 36);
        }
        return id;
    }

private:
    Xoroshiro128Plus& rnd_;

    /**
     * @brief Convert a number to a character
     * @param[in] num number to convert
     * @return char converted character
    */
    char numToChar(uint64_t num) {
        if (num < 26) {
            return 'A' + num;
        } else {
            return '0' + (num - 26);
        }
    }
};

int main() {
    // SGXではsgx_read_rand()みたいなのを使う
    unsigned seed = generateSeed();
    Xoroshiro128Plus rnd(seed);
    SessionIDGenerator generator(rnd);

    std::string sessionID = generator.generateSessionID();
    std::cout << "Generated Session ID: " << sessionID << std::endl;

    return 0;
}