#pragma once

#include <string.h>
#include <cstdint>
#include <memory>

#include "../../Include/consts.h"

class LogHeader {
    public:
        int chkSum_ = 0;
        unsigned int logRecNum_ = 0;    // log_record_sizeをheaderに情報として残しておく, 恐らくrecoveryでheader読んで回数指定してみたいな感じ
        const std::size_t len_val_ = VAL_SIZE;

        void init() {
            chkSum_ = 0;
            logRecNum_ = 0;
        }

        void convertChkSumIntoComplementOnTwo() {   // chkSumをtwo's complementに変換
            chkSum_ ^= 0xffffffff;
            chkSum_++;
        }
};

class LogRecord {
    public:
        uint64_t tid_;
        uint32_t key_;
        uint32_t val_;

        LogRecord() : tid_(0), key_(0) {}
        LogRecord(uint64_t tid, uint32_t key, uint32_t val) : tid_(tid), key_(key) {
            this->val_ = val;
        }

        int computeChkSum() {
            // compute check_sum
            int chkSum = 0;
            int *itr = (int*) this;
            for (uint32_t i = 0; i < sizeof(LogRecord) / sizeof(int); i++) {
                chkSum += (*itr);
                itr++;
            }
            return chkSum;
        }
};

class LogPackage {
    public:
        LogHeader header_;
        std::unique_ptr<LogRecord[]> log_records_;
};