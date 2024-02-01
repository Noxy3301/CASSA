#pragma once

#include "../../cassa_common/db_key.h"
#include "../../cassa_common/db_value.h"
#include "../../cassa_common/structures.h"

class Procedure {
public:
    OpType ope_;
    std::string key_;   // Key for WRITE, READ, DELETE, INSERT
    std::string value_; // Value for WRITE, INSERT

    // for SCAN
    std::string left_key_;
    std::string right_key_;
    bool l_exclusive_;
    bool r_exclusive_;

    // Default constructor
    Procedure(OpType ope, std::string key, std::string value)
        : ope_(ope), key_(key), value_(value), l_exclusive_(false), r_exclusive_(false) {}

    // SCAN constructor
    Procedure(OpType ope,
              std::string left_key, bool l_exclusive,
              std::string right_key, bool r_exclusive)
        : ope_(ope), left_key_(left_key), l_exclusive_(l_exclusive),
          right_key_(right_key), r_exclusive_(r_exclusive) {}


    // bool operator<(const Procedure &right) const {
    //     if (this->key_ == right.key_ && this->ope_ == Ope::WRITE && right.ope_ == Ope::READ) {
    //         return true;
    //     } else if (this->key_ == right.key_ && this->ope_ == Ope::WRITE && right.ope_ == Ope::WRITE) {
    //         return true;
    //     }
    //     // キーが同値なら先に write ope を実行したい．read -> write よりも write -> read.
    //     // キーが同値で自分が read でここまで来たら，下記の式によって絶対に falseとなり，自分 (read) が昇順で後ろ回しになるので ok
    
    //     return this->key_ < right.key_;
    // }
};