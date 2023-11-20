#pragma once

#include <cstdint>

struct Version {
    static constexpr uint32_t has_locked = 0;

    static bool splitHappened(const Version &before, const Version &after) {
        assert(after.v_split >= before.v_split);
        return before.v_split != after.v_split;
    }

    union {
        uint32_t body;
        struct {
            bool locked :       1;
            bool inserting :    1;
            bool splitting :    1;
            bool deleted :      1;
            bool is_root :      1;
            bool is_border :    1;
            uint16_t v_insert : 16;
            uint8_t v_split :   8;
            bool unused :       2;
        };
    };

    Version() noexcept
        : locked{false}
        , inserting{false}
        , splitting{false}
        , deleted{false}
        , is_root{false}
        , is_border{false}
        , v_insert{0}
        , v_split{0}
    {}

    // Versionが変更されているかをXORで確認する(0x0ならOK)
    uint32_t operator ^(const Version &right) const {
        return (body ^ right.body);
    }
};