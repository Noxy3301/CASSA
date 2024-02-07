#pragma once

#include <cstdint>

struct TIDword {
    union {
        uint64_t obj_;
        struct {
            bool lock : 1;
            bool latest : 1;
            bool absent : 1;
            uint64_t TID : 29;
            uint64_t epoch : 32;
        };
    };

    TIDword() : epoch(0), TID(0), absent(false), latest(true), lock(false) {};

    // for insert operation
    void init() {
        this->absent = true;
        this->lock = true;
    }

    bool operator == (const TIDword &right) const { return obj_ == right.obj_; }
    bool operator != (const TIDword &right) const { return !operator == (right); }
    bool operator < (const TIDword &right) const { return this->obj_ < right.obj_; }
};