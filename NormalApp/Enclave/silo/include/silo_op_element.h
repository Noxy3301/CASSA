#pragma once

#include <memory>

#include "tuple.h"

class OpElement {
    public:
        uint64_t key_;
        Tuple *rcdptr_;

        OpElement() : key_(0), rcdptr_(nullptr) {}
        OpElement(uint64_t key) : key_(key) {}
        OpElement(uint64_t key, Tuple *rcdptr) : key_(key), rcdptr_(rcdptr) {}
};

class ReadElement : public OpElement {
    public:
        using OpElement::OpElement;

        ReadElement(uint64_t key, Tuple *rcdptr, uint32_t val, TIDword tidword) : OpElement::OpElement(key, rcdptr) {
            tidword_.obj_ = tidword.obj_;
            this->val_ = val;
        }

        bool operator < (const ReadElement &right) const {
            return this->key_ < right.key_;
        }
        
        TIDword get_tidword() {
            return tidword_;
        }
    
    private:
        TIDword tidword_;
        uint32_t val_;
};

class WriteElement : public OpElement {
    public:
        using OpElement::OpElement;

        WriteElement(uint64_t key, Tuple *rcdptr, uint32_t val) : OpElement::OpElement(key, rcdptr) {
            this->val_ = val;
        }

        bool operator < (const WriteElement &right) const {
            return this->key_ < right.key_;
        }

        uint32_t get_val() {
            return val_;
        }

    private:
        uint32_t val_;
};