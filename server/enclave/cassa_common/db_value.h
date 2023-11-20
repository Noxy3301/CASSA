// masstreeとsiloの両方で使うvalueを定義する

#pragma once

#include <string>

#include "db_tid.h"

#define CACHE_LINE_SIZE 64

// TODO: templateにしてstd::string以外にも対応させる
class Value {
public:
    alignas(CACHE_LINE_SIZE) 
    TIDword tidword_;
    std::string body_;

    Value(std::string body) : body_(body){};

    bool operator==(const Value &right) const {
        return body_ == right.body_;
    }

    bool operator!=(const Value &right) const {
        return !operator==(right);
    }
};