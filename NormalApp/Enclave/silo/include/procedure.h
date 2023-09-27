#pragma once

#include <cstdint>

enum class Ope : uint8_t {
    READ,
    WRITE,
    READ_MODIFY_WRITE,
};

class Procedure {
    public:
        Ope ope_;
        uint64_t key_;

        Procedure() : ope_(Ope::READ), key_(0) {}
        Procedure(Ope ope, uint64_t key) : ope_(ope), key_(key) {}
};