#pragma once

#include "../../utils/db_key.h"
#include "../../utils/db_value.h"

#include "../../../Include/structures.h"

class Procedure {
    public:
        uint64_t uuid_high_;
        uint64_t uuid_low_;
        OpType ope_;
        std::string key_;
        std::string value_;

        Procedure(uint64_t uuid_high, uint64_t uuid_low, 
                  OpType ope, std::string key, std::string value)
            : uuid_high_(uuid_high), uuid_low_(uuid_low), 
              ope_(ope), key_(key), value_(value) {}


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