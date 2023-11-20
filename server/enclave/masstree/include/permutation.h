#pragma once

#include <cstdint>
#include <cstddef>
#include <cassert>

struct Permutation {
    /*
    | permutationIndex |   0  |   1  |   2  |   3  |   4  |   5  |   6  |   7  |   8  |   9  |  10  |  11  |  12  |  13  |  14  | keys |
    ├──────────────────┼──────┼──────┼──────┼──────┼──────┼──────┼──────┼──────┼──────┼──────┼──────┼──────┼──────┼──────┼──────┼──────┤
    |        trueIndex | ____ | ____ | ____ | ____ | ____ | ____ | ____ | ____ | ____ | ____ | ____ | ____ | ____ | ____ | ____ | ____ |
    */
    uint64_t body = 0;

    Permutation() = default;
    Permutation(const Permutation &other) = default;
    Permutation &operator=(const Permutation &other) = default;

    // Keyの数(下位4ビット)を取得する
    inline uint8_t getNumKeys() const {
        return body & 0b1111LLU;
    }

    // keyの数(下位4ビット)をセットする
    inline void setNumKeys(size_t num) {
        assert(0 <= num && num <= 15);
        uint64_t temp_num = body >> 4LU;    // 4bit右シフトで消去
        temp_num = temp_num << 4LU;         // 4bit左シフトで残りのデータを元の位置に戻す
        body = temp_num | num;              // ORで結合
    }

    // Keys++
    inline void incrementNumKeys() {
        assert(getNumKeys() <= 14);
        setNumKeys(getNumKeys() + 1);
    }

    // Keys--
    inline void decrementNumKeys() {
        assert(1 <= getNumKeys());
        setNumKeys(getNumKeys() - 1);
    }
    
    // 指定したpermutationIndexのtrueIndexを取得する
    inline uint8_t getKeyIndex(size_t index) const {
        assert(0 <= index && index < getNumKeys());
        uint64_t Rshift = body >> (15 - index)*4;   // 欲しいindexの位置まで右シフト
        uint8_t result = Rshift & 0b1111LLU;        // 0b1111でANDして取り出す
        assert(0 <= result && result <= 14);
        return result;
    }

    // 指定したpermutationIndexのtrueIndexをセットする
    inline void setKeyIndex(size_t permutationIndex, size_t trueIndex) {
        assert(0 <= permutationIndex && permutationIndex <= 14);
        assert(0 <= trueIndex && trueIndex <= 14);
        uint64_t left   = permutationIndex == 0 ? 0 : (body >> (16 - permutationIndex)*4) << (16 - permutationIndex)*4; // Rshift -> Lshiftで左側の欲しい部分だけ取得
        uint64_t right  = body & ((1LLU << ((15 - permutationIndex)*4)) - 1);                                           // 1LLUをLshiftして-1することでマスクを作ってAND(ex. 0b0100 -> 0b0011)
        uint64_t middle = trueIndex * (1LLU << (15 - permutationIndex)*4);                                              // 1LLUをセットしたいtrueIndexの位置までLshiftして、trueIndexを乗算することで指定した位置にtrueIndexを設定
        body = left | middle | right;
        assert(0 <= ((body >> (15 - permutationIndex)*4) & 0b1111LLU) && ((body >> (15 - permutationIndex)*4) & 0b1111LLU) <= 14);    // 0 <= permutationIndex <= 14
    }

    // 指定したpermutationIndexのtrueIndexを消す
    // CHECK: MasstreeってtrueIndexを消した後左シフトするんだっけ？あとpermutationの操作はinsertingとかのdirtyフラグがたってるからatomicに処理する必要がないってこと？
    inline void removeIndex(uint8_t trueIndex) {
        // 指定されたtrueIndexを探して消去する
        uint8_t permutationIndex;
        for (uint8_t i = 0; i < getNumKeys(); i++) {
            if (getKeyIndex(i) == trueIndex) {
                permutationIndex = i;
                break;
            }
        }
        // permutationIndexより後ろのkeysIndexを1つ左シフトする、ケツのやつは重複するけどdecrementNumKeys()するから無視される
        for (uint8_t target = permutationIndex; target + 1 <= getNumKeys() - 1; target++) {
            setKeyIndex(target, getKeyIndex(target + 1));
        }
        decrementNumKeys();
    }

    // Equivalent to calling p.getKeyIndex(2);
    // 指定したpermutationIndexのtrueIndexを取得する
    uint8_t operator() (size_t i) const {
        return getKeyIndex(i);
    }

    bool isNotFull() const {
        uint8_t num = getNumKeys();
        return num != 15;
    }

    bool isFull() const {
        return !isNotFull();
    }

    void insert(size_t permutationIndex, size_t trueIndex) {
        // permutationIndexで指定されたpermutation slotを開けるために、それより後を右シフト
        for (size_t i = getNumKeys(); i > permutationIndex; i--) {
            setKeyIndex(i, getKeyIndex(i - 1));
        }
        // 突っ込む
        setKeyIndex(permutationIndex, trueIndex);
        incrementNumKeys();
    }

    static Permutation sizeOne() {
        Permutation p{};
        p.setNumKeys(1);
        p.setKeyIndex(0, 0);
        return p;
    }

    // NodeをSortするタイミングでpermutationもSortするときに呼ばれる
    static Permutation fromSorted(size_t n_keys) {
        Permutation permutation{};
        for (size_t i = 0; i < n_keys; i++) {
            permutation.setKeyIndex(i, i);
        }
        permutation.setNumKeys(n_keys);
        return permutation;
    }

    // Permutationにvecの中身をセットする
    // NOTE: test/sample.hで使ってる
    static Permutation from(const std::vector<size_t> &vec) {
        Permutation p{};
        for (size_t i = 0; i < vec.size(); i++) {
            p.setKeyIndex(i, vec[i]);
        }
        p.setNumKeys(vec.size());
        return p;
    }
};