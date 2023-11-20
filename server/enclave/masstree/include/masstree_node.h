#pragma once

#include <atomic>
#include <tuple>
#include <cassert>
#include <utility>
#include <algorithm>
#include <mutex>
#include <optional>
#include <array>

#include "../../cassa_common/atomic_wrapper.h"
#include "../../cassa_common/db_value.h"
#include "masstree_version.h"
#include "masstree_key.h"
#include "permutation.h"

class InteriorNode;
class BorderNode;

class Node {
    public:
        static constexpr size_t ORDER = 16;

        // Node() = default;
        Node() : parent(nullptr), upperLayer(nullptr) {};
        // コピーコンストラクタと代入演算子の削除
        Node(const Node& other) = delete;
        Node &operator=(const Node& other) = delete;
        Node(Node&& other) = delete;
        Node &operator=(Node&& other) = delete;

        // 挿入や分割中でない安定したバージョン((version.inserting || version.splitting) == 0)を取得する
        Version stableVersion() const {
            Version v = getVersion();
            while (v.inserting || v.splitting) v = getVersion();
            return v;
        }
        // ロックを取得する
        void lock() {
            Version expected, desired;
            for (;;) {
                expected = getVersion();
                if (!expected.locked) { // ロックが取れる場合
                    desired = expected;
                    desired.locked = true;
                    if (version.compare_exchange_weak(expected, desired)) return;
                }
            }
        }
        // ロックを解除する
        void unlock() {
            Version v = getVersion();
            assert(v.locked && !(v.inserting && v.splitting));
            if (v.inserting) {  // insertとsplittingは同時に起きないことを明示的に書いてる
                v.v_insert++;
            } else if (v.splitting) {
                v.v_split++;
            }
            v.locked    = false;
            v.inserting = false;
            v.splitting = false;

            setVersion(v);
        }
        // versionの各フィールドのsetterとgetter
        inline void setInserting(bool inserting) {
            Version v = getVersion();
            assert(v.locked);
            v.inserting = inserting;
            setVersion(v);
        }

        inline void setSplitting(bool splitting) {
            Version v = getVersion();
            assert(v.locked);
            v.splitting = splitting;
            setVersion(v);
        }

        inline void setDeleted(bool deleted) {
            Version v = getVersion();
            assert(v.locked);
            v.deleted = deleted;
            setVersion(v);
        }

        inline void setIsRoot(bool is_root) {
            Version v = getVersion();
            v.is_root = is_root;
            setVersion(v);
        }

        inline void setIsBorder(bool is_border) {
            Version v = getVersion();
            v.is_border = is_border;
            setVersion(v);
        }

        inline void setVersion(Version const &v) {
            // storeRelease(version, v);    
            //TODO: wrapperに&v(ポインタ)を渡すと関数内部で二重ポインタになっているっぽい？wrapper作りたい
            version.store(v, std::memory_order_release);
        }


        inline void setUpperLayer(BorderNode *p) {
            if (p != nullptr) assert(reinterpret_cast<Node *>(p)->isLocked()); // setParentをするときはその親をロックしているのが前提, 自分のノードはロックする必要ない
            // storeRelease(upperLayer, p);
            upperLayer.store(p, std::memory_order_release);
        }

        inline void setParent(InteriorNode *p) {
            if (p != nullptr) assert(reinterpret_cast<Node *>(p)->isLocked());
            // storeRelease(parent, p);
            parent.store(p, std::memory_order_release);
        }
        
        inline bool getInserting() const {
            Version v = getVersion();
            return v.inserting;
        }
        
        inline bool getSplitting() const {
            Version v = getVersion();
            return v.splitting;
        }
        
        inline bool getDeleted() const {
            Version v = getVersion();
            return v.deleted;
        }
        
        inline bool getIsRoot() const {
            Version v = getVersion();
            return v.is_root;
        }
        
        inline bool getIsBorder() const {
            Version v = getVersion();
            return v.is_border;
        }
        
        inline Version getVersion() const {
            // return loadAcquire(version); // TODO: wrapperの作成
            return version.load(std::memory_order_acquire);
        }

        inline BorderNode *getUpperLayer() const {
            // return loadAcquire(upperLayer); // TODO: wrapperの作成
            return upperLayer.load(std::memory_order_acquire);
        }

        inline InteriorNode *getParent() const {
            // return loadAcquire(parent); // TODO: wrapperの作成
            return parent.load(std::memory_order_acquire);
        }

        // 親ノードのロックを取得する
        InteriorNode *lockedParent() const {
        RETRY:
            Node *p = reinterpret_cast<Node *>(getParent());
            if (p != nullptr) p->lock();
            // pのロック中にpの親が変わってないかの確認((T1)getParent -> (T2)setParent -> (T1)locked)
            if (p != reinterpret_cast<Node *>(getParent())) {
                assert(p != nullptr);
                p->unlock();
                goto RETRY;
            }
            return reinterpret_cast<InteriorNode *>(p);
        }
        // UpperLayer(上層ノード)のロックを取得する
        BorderNode *lockedUpperNode() const {
        RETRY:
            Node *p = reinterpret_cast<Node *>(getUpperLayer());
            if (p != nullptr) p->lock();
            // pのロック中にpのUpperLayerが変わってないかの確認((T1)getUpperLayer -> (T2)setUpperLayer -> (T1)locked)
            if (p != reinterpret_cast<Node *>(getUpperLayer())) {
                assert(p != nullptr);
                p->unlock();
                goto RETRY;
            }
            return reinterpret_cast<BorderNode *>(p);
        }
        // ノードがロックされているか確認する
        inline bool isLocked() const {
            Version version = getVersion();
            return version.locked;
        }
        // CHECK: これいつ使うんだ？
        inline bool isUnlocked() const {
            return !isLocked();
        }
    
    private:
        std::atomic<Version> version = {};
        std::atomic<InteriorNode *> parent;
        std::atomic<BorderNode *> upperLayer;
};

class InteriorNode : public Node {
    public:
        InteriorNode() : n_keys(0) {}
        // 指定されたスライスを持つ子ノードを検索する
        Node *findChild(uint64_t slice) {
            uint8_t num_keys = getNumKeys();
            for (size_t i = 0; i < num_keys; i++) {
                if (slice < key_slice[i]) return getChild(i);
            }
            return getChild(num_keys);
        }
        // ノードが満杯でないか確認
        inline bool isNotFull() const {
            return (getNumKeys() != ORDER - 1);
        }
        // ノードが満杯か確認
        inline bool isFull() const {
            return !isNotFull();
        }
        // bool debug_has_skip
        // void printNode() const {
        //     printf("/");
        //     for(size_t i = 0; i < ORDER - 1; ++i){
        //         printf("%lu/", getKeySlice(i));
        //     }
        //     printf("\\\n");
        // }

        // 指定された子ノードのインデックスを検索
        size_t findChildIndex(Node *arg_child) const {
            size_t index = 0;
            while (index <= getNumKeys() && (getChild(index) != arg_child)) index++;
            assert(getChild(index) == arg_child);
            return index;
        }

        inline uint8_t getNumKeys() const {
            // return loadAcquire(n_keys);  // TODO: wrapperの作成
            return n_keys.load(std::memory_order_acquire);
        }

        inline void setNumKeys(uint8_t nKeys) {
            // storeRelease(n_keys, nKeys);    // TODO: wrapperの作成
            n_keys.store(nKeys, std::memory_order_release);
        }

        inline void incNumKeys() {
            n_keys.fetch_add(1);
        }

        inline void decNumKeys() {
            n_keys.fetch_sub(1);
        }

        inline uint64_t getKeySlice(size_t index) const {
            // return loadAcquire(key_slice[index]);   // TODO: wrapperの作成
            return key_slice[index].load(std::memory_order_acquire);
        }

        void resetKeySlices() {
            for (size_t i = 0; i < ORDER - 1; i++) setKeySlice(i, 0);
        }

        inline void setKeySlice(size_t index, const uint64_t &slice) {
            // storeRelease(key_slice[index], slice);  // TODO: wrapperの作成
            key_slice[index].store(slice, std::memory_order_release);
        }

        inline Node *getChild(size_t index) const {
            // return loadAcquire(child[index]);   // TODO: wrapperの作成
            return child[index].load(std::memory_order_acquire);
        }

        inline void setChild(size_t index, Node *c) {
            assert(0 <= index && index <= 15);
            // storeRelease(child[index], child);  // TODO: wrapperの作成
            child[index].store(c, std::memory_order_release);
        }
        // bool debug_contain_child

        void resetChildren() {
            for (size_t i = 0; i < ORDER; i++) setChild(i, nullptr);
        }

    private:
        std::atomic<uint8_t> n_keys{0};
        std::array<std::atomic<uint64_t>, ORDER - 1> key_slice = {};
        std::array<std::atomic<Node *>, ORDER> child = {}; 
};

// LinkOrValueは、ノードへのリンクまたは値を保持するための共用体
// これにより、同じメモリ領域を使ってNodeへのポインタかValueへのポインタのどちらかを保持することができる
// これは、特定のキーに対して次のレイヤーのノードへのリンクが必要なのか、それとも実際の値へのリンクが必要なのかを効率的に管理するために使用される
union LinkOrValue {
    LinkOrValue() = default;

    explicit LinkOrValue(Node *next) : next_layer(next) {}
    explicit LinkOrValue(Value *value_) : value(value_) {}

    Node *next_layer = nullptr; // 次のレイヤーのノードへのポインタ
    Value *value;               // 実際の値へのポインタ
};

// vectorの先頭要素を消す
template<typename T>
static void pop_front(std::vector<T> &v) {
    if (!v.empty()) v.erase(v.begin());
}

// BigSuffixは、Suffixが8byteよりも大きい場合に使用されるクラス
// 複数のスライス(8byteごと)としてSuffixを保存し、Suffixの長さや現在のスライスの情報、次のSuffixが存在するかどうかを管理する
class BigSuffix {
public:
    BigSuffix() = default;
    // コピーコンストラクタ
    BigSuffix(const BigSuffix &other) : slices(other.slices), lastSliceSize(other.lastSliceSize) {}
    // コピーオペレータ、ムーブコンストラクタ、ムーブオペレータの削除
    BigSuffix &operator=(const BigSuffix &other) = delete;
    BigSuffix(BigSuffix &&other) = delete;
    BigSuffix &operator=(BigSuffix &&other) = delete;
    // slicesとlastSliceSizeを指定して初期化するコンストラクタ
    BigSuffix(std::vector<uint64_t> &&slices_, size_t lastSliceSize_) : slices(std::move(slices_)), lastSliceSize(lastSliceSize_) {}

    // 現在のスライスを取得する(最後のスライスならそのスライスだけ、違うなら8byte)
    SliceWithSize getCurrentSlice() {
        if (hasNext()) {
            std::lock_guard<std::mutex> lock(suffixMutex);
            return SliceWithSize(slices[0], 8);
        } else {
            std::lock_guard<std::mutex> lock(suffixMutex);
            return SliceWithSize(slices[0], lastSliceSize);
        }
    }
    // 残りのスライスの長さを返す
    size_t remainLength() {
        std::lock_guard<std::mutex> lock(suffixMutex);
        return (slices.size() - 1) * 8 + lastSliceSize;
    }
    // 次のスライスが存在するかを返す、slicesが2より大きいなら次のスライスがある(slicesをpop_frontしていくのでここで気づける)
    bool hasNext() {
        std::lock_guard<std::mutex> lock(suffixMutex);
        return slices.size() >= 2;
    }
    // 次のスライスに移動する
    void next() {
        assert(hasNext());
        pop_front(slices);
    }
    // スライスの先頭に新しいスライスを追加する
    // CHECK: これ何に使うんだ？
    void insertTop(uint64_t slice) {
        std::lock_guard<std::mutex> lock(suffixMutex);
        slices.insert(slices.begin(), slice);
    }

    // 指定したキーとこのSuffixが一致するかを確認する
    bool isSame(const Key &key, size_t from) {
        if (key.remainLength(from) != this->remainLength()) return false;
        std::lock_guard<std::mutex> lock(suffixMutex);
        for (size_t i = 0; i < slices.size(); i++) {
            if (key.slices[i + from] != slices[i]) return false;
        }
        return true;
    }

    // 指定したキーと開始位置から新しいBigSuffixを作成
    static BigSuffix *from(const Key &key, size_t from) {
        std::vector<uint64_t> temp{};
        for(size_t i = from; i < key.slices.size(); i++) {
            temp.push_back(key.slices[i]);
        }
        return new BigSuffix(std::move(temp), key.lastSliceSize);
    }

private:
    std::mutex suffixMutex{};
    std::vector<uint64_t> slices = {};
    const size_t lastSliceSize = 0;
};

// KeySuffixはBorderNode内のすべてのkeyのSuffixへの参照を一元管理する
// 各SuffixはBigSuffixオブジェクトへのポインタとして保持される
class KeySuffix {
public:
    KeySuffix() = default;
    
    // 指定されたインデックス位置に、指定されたkeyから取得したSuffixをセットする
    inline void set(size_t i, const Key &key, size_t from) {
        // storeRelease(suffixes[i], BigSuffix::from(key, from));   // TODO: wrapperの作成
        suffixes[i].store(BigSuffix::from(key, from), std::memory_order_release);
    }
    // 指定されたインデックス位置にBigSuffixへのポインタをセットする
    inline void set(size_t i, BigSuffix *const &ptr) {
        // storeRelease(suffixes[i], ptr); // TODO: wrapperの作成
        suffixes[i].store(ptr, std::memory_order_release);
    }
    // 指定されたインデックスのBigSuffixへのポインタを取得する
    inline BigSuffix *get(size_t i) const {
        // loadAcquire(suffixes[i]);    // TODO: wrapperの作成
        return suffixes[i].load(std::memory_order_acquire);
    }
    // 指定されたインデックス(=suffixes[i])のSuffixへの参照を外す
    void unreferenced(size_t i) {
        assert(get(i) != nullptr);
        set(i, nullptr);
    }
    // 指定されたインデックスのSuffixを削除する
    void delete_ptr(size_t i) {
        BigSuffix *ptr = get(i);
        assert(ptr != nullptr);
        delete ptr;
        set(i, nullptr);
    }
    // すべてのSuffixを削除する
    void delete_all() {
        for (size_t i = 0; i < Node::ORDER - 1; i++) {
            BigSuffix *ptr = get(i);
            if (ptr != nullptr) delete_ptr(i);
        }
    }
    // suffixes配列をリセットする(全てnullptrで初期化する)、主にsplit操作などで使用される
    void reset() {
        std::fill(suffixes.begin(), suffixes.end(), nullptr);
    }

private:
    // BigSuffixへのポインタを保持する配列
    // BorderNode内のすべてのkeyのSuffixを一元管理するためのデータ構造
    std::array<std::atomic<BigSuffix*>, Node::ORDER - 1> suffixes = {};
};

enum SearchResult: uint8_t {
    NOTFOUND,
    VALUE,
    LAYER,
    UNSTABLE
};

class BorderNode : public Node {
    public:
        // キーの長さや状態を示すための特殊な値
        static constexpr uint8_t key_len_layer = 255;
        static constexpr uint8_t key_len_unstable = 254;
        static constexpr uint8_t key_len_has_suffix = 9;

        BorderNode() : permutation(Permutation::sizeOne()) {
            setIsBorder(true);
        }

        ~BorderNode() {
            for (size_t i = 0; i < ORDER - 1; i++) {
                // TODO: assert()
            }
        }

        // BorderNodeの中からkeyに対応するLink or Valueを取得する
        inline std::pair<SearchResult, LinkOrValue> searchLinkOrValue(const Key &key) {
            std::tuple<SearchResult, LinkOrValue, size_t> tuple = searchLinkOrValueWithIndex(key);
            return std::make_pair(std::get<0>(tuple), std::get<1>(tuple));
        }

        // BorderNodeの中からkeyに対応するLink or Valueを取得する、ついでにkeyのindexも返す
        std::tuple<SearchResult, LinkOrValue, size_t> searchLinkOrValueWithIndex(const Key &key) {
            /*
             * 1. キーの次のスライスがない場合は、現在のスライスが最後のものとみなされ、key sliceの長さは1~8になる
             *    このケースでは、現在のレイヤーにValueが存在するはず
             * 2. キーの次のスライスがある場合、key sliceの長さは常に8
             *    このケースでは、現在のレイヤーにはValueが存在せず、次のレイヤーへのLinkを探す必要がある
             * 3. `key_len`が8の場合、残りのkey sliceは1つのみで、次のkey sliceはkey suffixに格納されている
             *    この場合、suffixの中を検索する
             * 4. `key_len`がLAYERを示す場合、次のLayerを検索する
             * 5. `key_len`がUNSTABLEを示す場合、キーの状態は不安定であるとみなされ、UNSTABLEを返す
             * 6. 上記のいずれのケースも該当しない場合、キーは見つからなかったとみなされ、NOTFOUNDを返す
             */
            SliceWithSize current = key.getCurrentSlice();
            Permutation permutation = getPermutation();

            if (!key.hasNext()) {   // 現在のkeyのスライスが最後の場合(current layerにValueがあるはず)
                for (size_t i = 0; i < permutation.getNumKeys(); i++) {
                    uint8_t trueIndex = permutation(i);
                    if (getKeySlice(trueIndex) == current.slice && getKeyLen(trueIndex) == current.size) {
                        return std::make_tuple(VALUE, getLV(trueIndex), trueIndex);
                    }
                }
            } else {    // 次のスライスがある場合(current layerにはvalueがないので下位ノードを辿るためのLinkを探す)
                for (size_t i = 0; i < permutation.getNumKeys(); i++) {
                    uint8_t trueIndex = permutation(i);
                    if (getKeySlice(trueIndex) == current.slice) {
                        if (getKeyLen(trueIndex) == BorderNode::key_len_has_suffix) {
                            // suffixの中を見る
                            BigSuffix *suffix = getKeySuffixes().get(trueIndex);
                            if (suffix != nullptr && suffix->isSame(key, key.cursor + 1)) {
                                return std::make_tuple(VALUE, getLV(trueIndex), trueIndex);
                            }
                        }

                        if (getKeyLen(trueIndex) == BorderNode::key_len_layer) {
                            return std::make_tuple(LAYER, getLV(trueIndex), trueIndex);
                        }
                        if (getKeyLen(trueIndex) == BorderNode::key_len_unstable) {
                            return std::make_tuple(UNSTABLE, LinkOrValue{}, 0);
                        }
                    }
                }
            }
            return std::make_tuple(NOTFOUND, LinkOrValue{}, 0);
        }

        // BorderNode内のキーの中で、最小のキーを返す。
        uint64_t lowestKey() const {
            Permutation permutation = getPermutation();
            return getKeySlice(permutation(0));
        }

        // このBorderNodeを削除する前に呼び出す、前後のBorderNodeのリンクをつなぐ
        void connectPrevAndNext() const {
        RETRY_PREV_LOCK:
            BorderNode *prev = getPrev();
            if (prev != nullptr) {
                prev->lock();
                // prevをlockしているタイミングで消去/更新されていないことの確認
                if (prev->getDeleted() || prev != getPrev()) {
                    prev->unlock();
                    goto RETRY_PREV_LOCK;
                } else {
                    BorderNode *next = getNext();
                    prev->setNext(next);
                    if (next != nullptr) {
                        assert(!next->getDeleted());
                        next->setPrev(prev);
                    }
                    prev->unlock();
                }
            } else {
                // prev == nullptr
                if (getNext() != nullptr) {
                    getNext()->setPrev(nullptr);
                }
            }
        }

        // 新しいキーを挿入するための適切な位置を検索する
        // 返り値は、挿入する位置と、その位置が再利用されるスロットであるかどうかのブール値
        std::pair<size_t, bool> insertPoint() const {
            assert(getPermutation().isNotFull());
            for (size_t i = 0; i < ORDER - 1; i++) {
                uint8_t len = getKeyLen(i);
                if (len == 0) return std::make_pair(i, false);
                // markKeyRemovedで、assert(1 <= len and len <= key_len_has_suffix) -> setKeyLen(i, len + 9)を行うから、(10 <= len && len <= 18)の範囲である場合は再利用として扱う(v_insertを更新する必要あり)
                if (10 <= len && len <= 18) return std::make_pair(i, true);
            }
            assert(false);
        }

        // void printNode() const {
        // printf("|");
        // for(size_t i = 0; i < ORDER - 1; ++i){
        //     printf("%lu|", getKeySlice(i));
        // }
        // printf("\n");
        // }

        // splitをするとき簡略化のためにsortを行う(BorderNode, permutationどっちも)
        void sort() {
            Permutation permutation = getPermutation();
            assert(permutation.isFull());
            assert(isLocked());
            assert(getSplitting());

            uint8_t temp_key_len[Node::ORDER - 1] = {};
            uint64_t temp_key_slice[Node::ORDER - 1] = {};
            LinkOrValue temp_lv[Node::ORDER - 1] = {};
            BigSuffix *temp_suffix[Node::ORDER - 1] = {};

            for (size_t i = 0; i < ORDER - 1; i++) {
                uint8_t trueIndex = permutation(i);
                temp_key_len[i] = getKeyLen(trueIndex);
                temp_key_slice[i] = getKeySlice(trueIndex);
                temp_lv[i] = getLV(trueIndex);
                temp_suffix[i] = getKeySuffixes().get(trueIndex);
            }
            // forが2回入っていて冗長に見えるけど全部そろえてからpermutationをいじった方が整合性保証的な意味で良い気がする
            for (size_t i = 0; i < ORDER - 1; i++) {
                setKeyLen(i, temp_key_len[i]);
                setKeySlice(i, temp_key_slice[i]);
                setLV(i, temp_lv[i]);
                getKeySuffixes().set(i, temp_suffix[i]);
            }
            setPermutation(Permutation::fromSorted(ORDER - 1));
        }

        /**
         * @brief キーを削除済みとしてマークする。
         *        現在のキーの長さにkey_len_has_suffixの値(9)を加えることで、キーが削除されたことを示す。
         * @param i 削除マークを付けるキーのインデックス
         * @note 消去済みの場合、(10 <= len && len <= 18)となる
         */
        void markKeyRemoved(uint8_t i) {
            uint8_t len = getKeyLen(i);
            assert(1 <= len && len <= key_len_has_suffix);
            setKeyLen(i, len + 9);
        }

        bool isKeyRemoved(uint8_t i) const {
            uint8_t len = getKeyLen(i);
            return (10 <= len and len <= 18);
        }

        size_t findNextLayerIndex(Node *next_layer) const {
            assert(this->isLocked());
            for (size_t i = 0; i < ORDER - 1; i++) {
                if (getLV(i).next_layer == next_layer) return i;
            }
            assert(false);  // ここにはたどり着かないはず
        }
        
        inline uint8_t getKeyLen(size_t i) const {
            // return loadAcquire(key_len[i]); // TODO: wrapperの作成
            return key_len[i].load(std::memory_order_acquire);
        }
        
        // key_len[i]にkeyの長さをセットする
        inline void setKeyLen(size_t i, const uint8_t &len) {
            key_len[i].store(len, std::memory_order_release);
        }

        void resetKeyLen() {
            for (size_t i = 0; i < ORDER - 1; i++) setKeyLen(i, 0);
        }
        
        inline uint64_t getKeySlice(size_t i) const {
            // return loadAcquire(key_slice[i]); // TODO: wrapperの作成
            return key_slice[i].load(std::memory_order_acquire); 
        }
        
        inline void setKeySlice(size_t i, const uint64_t &slice) {
            key_slice[i].store(slice, std::memory_order_release);   // TODO: wrapperの作成
        }

        void resetKeySlice() {
            for (size_t i = 0; i < ORDER - 1; i++) setKeySlice(i, 0);
        }
        
        inline LinkOrValue getLV(size_t i) const {
            // return loadAcquire(lv[i]);   // TODO: wrapperの作成
            return lv[i].load(std::memory_order_acquire); 
        }
        // lv[i]に指定されたLinkOrValueをセットする
        inline void setLV(size_t i, const LinkOrValue &lv_) {
            lv[i].store(lv_, std::memory_order_release);    // TODO: wrapperの作成
        }

        void resetLVs() {
            for (size_t i = 0; i < ORDER - 1; i++) setLV(i, LinkOrValue{});
        }

        inline BorderNode *getNext() const {
            return next.load(std::memory_order_acquire);    // TODO: wrapperの作成
        }

        inline void setNext(BorderNode *next_) {
            next.store(next_, std::memory_order_release);    // TODO: wrapperの作成
        }

        inline BorderNode *getPrev() const {
            return prev.load(std::memory_order_acquire);    // TODO: wrapperの作成
        }
        
        inline void setPrev(BorderNode *prev_) {
            prev.store(prev_, std::memory_order_release);    // TODO: wrapperの作成
        }

        // inline bool CASNext(BorderNode *expected, BorderNode *desired){}
        
        inline KeySuffix &getKeySuffixes() {
            return key_suffixes;
        }

        // inline const KeySuffix& getKeySuffixes() const{} // CHECK: こっち何に使うんだ？
        
        inline Permutation getPermutation() const {
            // return loadAcquire(permutation);    // TODO: wrapperの作成
            return permutation.load(std::memory_order_acquire); 
        }

        inline void setPermutation(const Permutation &p) {
            permutation.store(p, std::memory_order_release);
        }



    private:
        std::array<std::atomic<uint8_t>, ORDER - 1> key_len = {};       // 各キーの長さを保持する配列、キーの長さは255まで
        // CHECK: permutation::sizeOne()をここで呼ぶことはできない(コピー代入をサポートしてないから)からborderNodeのコンストラクタでpermutationのコンストラクタを呼び出す
        std::atomic<Permutation> permutation;                           // BorderNode内のキーの順序を管理するためのPermutationオブジェクト
        std::array<std::atomic<uint64_t>, ORDER - 1> key_slice = {};    // キーのスライスを保持する配列
        std::array<std::atomic<LinkOrValue>, ORDER - 1> lv = {};        // キーに関連付けられたLinkまたはValueを保持する配列
        std::atomic<BorderNode*> next{nullptr};                         // 隣接するBorderNodeへのリンク(next)
        std::atomic<BorderNode*> prev{nullptr};                         // 隣接するBorderNodeへのリンク(previous)
        KeySuffix key_suffixes = {};                                    // BorderNode内のすべてのキーのSuffixを一元管理するKeySuffixオブジェクト
};

std::pair<BorderNode*, Version> findBorder(Node *root, const Key &key);