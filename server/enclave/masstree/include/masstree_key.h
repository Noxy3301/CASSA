#pragma once

#include <cstdint>
#include <vector>
#include <cassert>
#include <string>

// CHECK: KeyWithSliceって何に使うんだ？

struct SliceWithSize {
    uint64_t slice; // スライスの実際のデータ
    uint8_t size;   // スライスのサイズ

    SliceWithSize(uint64_t slice_, uint8_t size_) : slice(slice_), size(size_) {
        assert(1 <= size && size <= 8);
    }
    // 2つのSliceWithSizeオブジェクトが等しいかどうかを確認する
    bool operator==(const SliceWithSize &right) const {
        return slice == right.slice && size == right.size;
    }
    // 2つのSliceWithSizeオブジェクトが異なるかどうかを確認する
    bool operator!=(const SliceWithSize &right) const {
        return !(*this == right);
    }
};

class Key {
    public:
        std::vector<uint64_t> slices;   // スライスのリスト
        size_t lastSliceSize = 0;       // 最後のスライスのサイズ
        size_t cursor = 0;              // 現在のスライスの位置/インデックス

        Key(const std::string &key) {
            std::pair<std::vector<uint64_t>, size_t> slices_lastSliceSize = string_to_uint64t(key);
            slices = std::move(slices_lastSliceSize.first);
            lastSliceSize = slices_lastSliceSize.second;
            assert(1 <= lastSliceSize && lastSliceSize <= 8);
        }
        Key(std::vector<uint64_t> slices_, size_t lastSliceSize_) : slices(std::move(slices_)), lastSliceSize(lastSliceSize_) {
            assert(1 <= lastSliceSize && lastSliceSize <= 8);
        }
        // 次のスライスが存在するかどうかを確認する
        bool hasNext() const {
            if (slices.size() == cursor + 1) return false;
            return true;
        }
        // 指定された位置からの残りのスライスの長さを取得する
        size_t remainLength(size_t from) const {
            assert(from <= slices.size() - 1);
            return (slices.size() - from - 1)*8 + lastSliceSize;
        }
        // 現在のスライスのサイズを取得する
        size_t getCurrentSliceSize() const {
            if (hasNext()) {
                return 8;
            } else {
                return lastSliceSize;
            }
        }
        // 現在のスライスとカーソル(スライスの位置/インデックス)を取得する
        SliceWithSize getCurrentSlice() const {
            return SliceWithSize(slices[cursor], getCurrentSliceSize());
        }
        // 次のスライスに進む
        void next() {
            assert(hasNext());
            cursor++;
        }
        // 前のスライスに戻る
        void back() {
            assert(cursor != 0);
            cursor--;
        }
        // カーソルをリセットする
        void reset() {
            cursor = 0;
        }
        // 2つのKeyオブジェクトが等しいかどうかを確認する
        bool operator==(const Key &right) const {
            return lastSliceSize == right.lastSliceSize && cursor == right.cursor && slices == right.slices;
        }
        // 2つのKeyオブジェクトが異なるかどうかを確認する
        bool operator!=(const Key &right) const {
            return !(*this == right);
        }

        // 演算子 < のオーバーロード
        bool operator<(const Key& right) const {
            // 最小のサイズを取得して、それに基づいてスライスを比較
            size_t minSize = std::min(slices.size(), right.slices.size());
            for (size_t i = 0; i < minSize; i++) {
                // スライスが異なる場合は、そのスライスに基づいて比較結果を返す
                if (slices[i] != right.slices[i]) {
                    return slices[i] < right.slices[i];
                }
            }

            // すべてのスライスが等しい場合は、最後のスライスのサイズで比較
            if (lastSliceSize != right.lastSliceSize) {
                return lastSliceSize < right.lastSliceSize;
            }

            // スライスの数が異なる場合は、スライスの数が少ない方が小さいと判断
            return slices.size() < right.slices.size();
        }

        std::pair<std::vector<uint64_t>, size_t> string_to_uint64t(const std::string &key) {
            std::vector<uint64_t> slices;
            size_t lastSliceSize = 0;
            size_t index = 0;

            while (index < key.size()) {
                uint64_t slice = 0;
                size_t i = 0;
                for (; i < 8 && index < key.size(); i++, index++) {
                    slice = (slice << 8) | static_cast<uint8_t>(key[index]);    // 8bit左シフトしてORを取ることでsliceにkey[index]を挿入する
                }
                slice <<= (8 - i) * 8;    // 残りの部分を0埋め
                slices.push_back(slice);
                lastSliceSize = i;
            }

            return std::make_pair(slices, lastSliceSize);
        }

        std::string uint64t_to_string(const std::vector<uint64_t> &slices, size_t lastSliceSize) {
            std::string result;
            // 最後のスライス以外の処理
            for (size_t i = 0; i < slices.size() - 1; i++) {
                for (int j = 7; j >= 0; j--) {  // jをsize_tにするとj=-1の時にオーバーフローが発生するよ！
                    result.push_back(static_cast<char>((slices[i] >> (j * 8)) & 0xFF)); // AND 0xFFで下位8bitを取得する
                }
            }
            // 最後のスライスの処理(lastSliceSizeより後ろは0埋めされているので)
            for (size_t i = 0; i < lastSliceSize; i++) {
                result.push_back(static_cast<char>((slices.back() >> ((7 - i) * 8)) & 0xFF));
            }
            return result;
        }
};