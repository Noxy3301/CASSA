#pragma once

#include "../../utils/db_key.h"
#include "../../utils/db_value.h"

#include "../../../Include/structures.h"

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

    ReadElement(const Key &key, Value *value, const TIDword &tidword, OpType op = OpType::READ) // ReadElementではOpTypeを使わないからREADにしているけどこれでいいのか？
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

    WriteElement(const Key &key, Value *old_value, Value *new_value, OpType op)
        : OpElement(key, old_value, op), new_value_(new_value) {}

    Value* get_new_value() const {
        return new_value_;
    }

    bool operator<(const WriteElement &right) const {
        return key_ < right.key_;
    }

private:
    Value *new_value_;
};
