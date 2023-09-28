#pragma once

#include "masstree_node.h"

class GarbageCollector {
    public:
        // コンストラクタ
        GarbageCollector() = default;
        // コピーコンストラクタと代入演算子の削除
        GarbageCollector(GarbageCollector &&other) = delete;
        GarbageCollector(const GarbageCollector &other) = delete;
        GarbageCollector &operator=(GarbageCollector &&other) = delete;
        GarbageCollector &operator=(const GarbageCollector &other) = delete;
        // BorderNodeをGCに追加
        void add(BorderNode *borderNode) {
            assert(!contain(borderNode));
            assert(borderNode->getDeleted());
            borders.push_back(borderNode);
        }
        // InteriorNodeをGCに追加
        void add(InteriorNode *interiorNode) {
            assert(!contain(interiorNode));
            assert(interiorNode->getDeleted());
            interiors.push_back(interiorNode);
        }
        // ValueをGCに追加
        void add(Value *value) {
            assert(!contain(value));
            values.push_back(value);
        }
        // BigSuffixをGCに追加
        void add(BigSuffix *suffix) {
            assert(!contain(suffix));
            suffixes.push_back(suffix);
        }
        // 指定したBorderNodeが格納されているか確認
        bool contain(BorderNode const *borderNode) {
            return std::find(borders.begin(), borders.end(), borderNode) != borders.end();
        }
        // 指定したInteriorNodeが格納されているか確認
        bool contain(InteriorNode const *interiorNode) {
            return std::find(interiors.begin(), interiors.end(), interiorNode) != interiors.end();
        }
        // 指定したValueが格納されているか確認
        bool contain(Value const *value) {
            return std::find(values.begin(), values.end(), value) != values.end();
        }
        // 指定したBigSuffixが格納されているか確認
        bool contain(BigSuffix const *suffix) const {
            return std::find(suffixes.begin(), suffixes.end(), suffix) != suffixes.end();
        }
        // 保持している全てのノードや値を解放する
        void run() {
            for (auto &borderNode : borders) delete borderNode;
            borders.clear();

            for (auto &interiorNode: interiors) delete interiorNode;
            interiors.clear();

            for (auto &value : values) delete value;
            values.clear();

            for (auto &suffix : suffixes) delete suffix;
            suffixes.clear();
        }

    private:
        std::vector<BorderNode *> borders{};        // 削除されたBorderNodeを格納するvector
        std::vector<InteriorNode *> interiors{};    // 削除されたInteriorNodeを格納するvector
        std::vector<Value *> values{};              // 削除されたValueを格納するvector
        std::vector<BigSuffix *> suffixes{};        // 削除されたBigSuffixを格納するvector
};