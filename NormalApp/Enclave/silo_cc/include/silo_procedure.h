#pragma once

#include "../../utils/db_key.h"

enum class Ope : uint8_t {
    READ,
    WRITE,
    READ_MODIFY_WRITE,
};

class Procedure {
    public:
        Ope ope_;
        Key key_;

        Procedure(Ope ope, Key key) : ope_(ope), key_(key) {}

        bool operator<(const Procedure &right) const {
            if (this->key_ == right.key_ && this->ope_ == Ope::WRITE && right.ope_ == Ope::READ) {
                return true;
            } else if (this->key_ == right.key_ && this->ope_ == Ope::WRITE && right.ope_ == Ope::WRITE) {
                return true;
            }
            // キーが同値なら先に write ope を実行したい．read -> write よりも write -> read.
            // キーが同値で自分が read でここまで来たら，下記の式によって絶対に falseとなり，自分 (read) が昇順で後ろ回しになるので ok
      
            return this->key_ < right.key_;
        }
};