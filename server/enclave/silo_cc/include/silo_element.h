#pragma once

#include "../../cassa_common/db_key.h"
#include "../../cassa_common/db_value.h"
#include "../../cassa_common/structures.h"

class OpElement {
public:
    Key key_;
    Value *value_;
    OpType op_;

    OpElement(const Key &key, Value *value, OpType op)
        : key_(key), value_(value), op_(op) {}
};

class ReadElement : public OpElement {
public:
    using OpElement::OpElement;

    ReadElement(const Key &key, Value *value, 
                const TIDword &tidword, OpType op = OpType::READ)
        : OpElement(key, value, op), tidword_(tidword) {}

    TIDword get_tidword() const {
        return tidword_;
    }

    bool operator<(const ReadElement &right) const {
        return key_ < right.key_;
    }

private:
    TIDword tidword_;
};

class WriteElement : public OpElement {
public:
    using OpElement::OpElement;

    WriteElement(const Key &key, Value *value, 
                 std::string new_value_body, OpType op)
        : OpElement(key, value, op), new_value_body_(new_value_body) {}

    std::string get_new_value_body() const {
        return new_value_body_;
    }

    bool operator<(const WriteElement &right) const {
        return key_ < right.key_;
    }

private:
    std::string new_value_body_;
};
