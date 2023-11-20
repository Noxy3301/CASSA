#include "include/masstree_remove.h"

/**
 * @brief Removeの処理において、Layerのdelete処理を行う。
 *        §4.6.3と同じように、下のレイヤを消してから、hand-over-handの要領で上のレイヤの消去処理に移動する。
 * @param borderNode 削除されるBorderNodeへのポインタ。
 * @param gc ガベージコレクタへの参照。
 * @note 指定したBorderNodeが特定のプロパティ(ルートノードであること、単一キーのみを保有していること)を持っていることを前提としている。
 */
void handle_delete_layer_in_remove(BorderNode *borderNode, GarbageCollector &gc) {
    Permutation permutation = borderNode->getPermutation();
    assert(borderNode->getParent() == nullptr);
    assert(permutation.getNumKeys() == 1);
    assert(borderNode->getIsRoot());
    assert(borderNode->getUpperLayer() != nullptr);
    assert(borderNode->isLocked());

    BigSuffix *upper_suffix;
    if (borderNode->getKeyLen(permutation(0)) == BorderNode::key_len_has_suffix) {
        // borderNodeがsuffixを持っているのなら、そのままsuffixを再利用
        BigSuffix *old_suffix = borderNode->getKeySuffixes().get(permutation(0));
        old_suffix->insertTop(borderNode->getKeySlice(permutation(0))); // 1つ上のレイヤに行くのでsliceを回収しておく
        upper_suffix = old_suffix;
    } else {
        // この処理は単一キー(suffixなし)のBorderNode対する処理なので、key_len_layerにはなりえない(key_len_unstableも)
        assert(1 <= borderNode->getKeyLen(permutation(0)) && borderNode->getKeyLen(permutation(0)) <= 8);
        upper_suffix = new BigSuffix({borderNode->getKeySlice(permutation(0))}, borderNode->getKeyLen(permutation(0)));
    }
    // hand-over-handの要領で、下から上に処理を行う
    BorderNode *upper = borderNode->lockedUpperNode();
    // [1] upperをkey_len_unstableにする
    // [2] upper_suffixとして回収したsuffixをupperのsuffixにセットする
    // [3] upperのLinkOrValueを書き換える
    // [4] upperをkey_has_suffixにする
    // [5] borderNodeのLinkOrValueとKeySuffixをunrefする
    size_t nextLayerIndex = upper->findNextLayerIndex(borderNode);
    upper->setKeyLen(nextLayerIndex, BorderNode::key_len_unstable);     // [1]
    assert(upper->getKeySuffixes().get(nextLayerIndex) == nullptr);
    upper->getKeySuffixes().set(nextLayerIndex, upper_suffix);          // [2]
    upper->setLV(nextLayerIndex, borderNode->getLV(permutation(0)));    // [3]
    upper->setKeyLen(nextLayerIndex, BorderNode::key_len_has_suffix);   // [4]
    // [5]
    borderNode->setLV(permutation(0), LinkOrValue{});   // NOTE: これ実体なのが気に食わないな
    borderNode->getKeySuffixes().set(permutation(0), nullptr);
    // GarbageCollectorに渡す
    borderNode->setDeleted(true);
    gc.add(borderNode);
    // 処理が終わったので下からunlockする
    borderNode->unlock();
    upper->unlock();
}

/**
 * @brief 指定されたBorderNodeを削除し、必要に応じてツリーを再構成する。
 * @param borderNode 削除するBorderNodeへのポインタ。
 * @param gc ガベージコレクタへの参照。
 * @return ツリーのルートが変更された場合は新しいルートを、変更されなかった場合はnullptrを返す。
 *         RootChange列挙型で判断する。
 */
std::pair<RootChange, Node*> delete_borderNode_in_remove(BorderNode *borderNode, GarbageCollector &gc) {
    assert(borderNode->isLocked());
    Permutation permutation = borderNode->getPermutation();
    assert(permutation.getNumKeys() == 0);  // borderNodeの中身は既に消去済み
    if (borderNode->getIsRoot()) {  // Layer0のルートノードの場合
        assert(borderNode->getParent() == nullptr);
        assert(borderNode->getUpperLayer() == nullptr);
        borderNode->setDeleted(true);
        gc.add(borderNode);
        borderNode->unlock();
        return std::make_pair(LayerDeleted, nullptr);
    }
    // 親のlockを取る
    InteriorNode *parent = borderNode->getParent();
    parent->lock();
    size_t nextLayerIndex = parent->findChildIndex(borderNode);
    if (parent->getNumKeys() >= 2) {
        parent->setInserting(true); // 左シフトして特定のキーとchild nodeへのリンクを消す
        // nextLayerIndex(要は消したいindex)が0番目の場合、treeの構造的に左側のリンクを消さないといけないからstart_keyIndexとstart_childIndexで判断させる
        size_t startKeyIndex   = (nextLayerIndex == 0) ? 0 : nextLayerIndex - 1;
        size_t startChildIndex = (nextLayerIndex == 0) ? 0 : nextLayerIndex;
        for (size_t i = startKeyIndex; i <= 13; i++) {
            parent->setKeySlice(i, parent->getKeySlice(i+1));
        }
        for (size_t i = startChildIndex; i <= 14; i++) {
            parent->setChild(i, parent->getChild(i+1));
        }
        parent->decNumKeys();
        // hand-over-handの要領に則って下からunlockする
        borderNode->connectPrevAndNext();
        borderNode->setDeleted(true);
        gc.add(borderNode);
        borderNode->unlock();
        parent->unlock();
    } else {
        assert(parent->getNumKeys() == 1);
        size_t pull_up_index = nextLayerIndex == 1 ? 0 : 1;
        Node *pull_up_node = parent->getChild(pull_up_index);
        if (parent->getIsRoot()) {
            assert(parent->getParent() == nullptr);
            // Rootが変わるので、upper_layerからの付け替えが必要になる
            if (parent->getUpperLayer() == nullptr) {
                // Layer0の時、is_rootをtrueにしてからparentをnullptrにする
                pull_up_node->setIsRoot(true);
                pull_up_node->setParent(nullptr);
                pull_up_node->setUpperLayer(nullptr);
                // hand-over-handの要領に則って下からunlockする
                borderNode->connectPrevAndNext();
                borderNode->setDeleted(true);
                gc.add(borderNode);
                borderNode->unlock();
                parent->setDeleted(true);
                gc.add(parent);
                parent->unlock();
                return std::make_pair(NewRoot, pull_up_node);
            } else {
                // Layer1以降の場合、upper_layerを更新、is_rootをtrueにしてからparentをnullptrにする
                BorderNode *upper = parent->lockedUpperNode();
                size_t parentIndex = upper->findNextLayerIndex(parent);
                pull_up_node->setIsRoot(true);
                pull_up_node->setParent(nullptr);
                upper->setLV(parentIndex, LinkOrValue(pull_up_node));
                // hand-over-handの要領に則って下からunlockする
                borderNode->connectPrevAndNext();
                borderNode->setDeleted(true);
                gc.add(borderNode);
                borderNode->unlock();
                parent->setDeleted(true);
                gc.add(parent);
                parent->unlock();
                upper->unlock();
                return std::make_pair(NewRoot, pull_up_node);
            }
        } else {
            // parent->getIsRoot() == false
            InteriorNode *parentOfParent = parent->getParent();
            parentOfParent->lock();
            size_t parentIndex = parentOfParent->findChildIndex(parent);
            parentOfParent->setChild(parentIndex, pull_up_node);
            pull_up_node->setParent(parentOfParent);
            // hand-over-handの要領に則って下からunlockする
            borderNode->connectPrevAndNext();
            borderNode->setDeleted(true);
            gc.add(borderNode);
            borderNode->unlock();
            parent->setDeleted(true);
            gc.add(parent);
            parent->unlock();
            parentOfParent->unlock();
        }
    }
    return std::make_pair(NotChange, nullptr);
}

/**
 * @brief 指定されたキーを持つノードを消去する。
 * @param root Masstreeのルートノード
 * @param key 削除するキー。
 * @param gc ガベージコレクタへの参照。
 * @return ツリーのルートが変更された場合は新しいルートを、変更されなかった場合はnullptrを返す。
 *         RootChange列挙型で判断する。
 */
std::pair<RootChange, Node*> remove(Node *root, Key &key, GarbageCollector &gc) {
    if (root == nullptr) {
        // 無いもんは消せねえ(´・ω・`)
        assert(key.cursor == 0);
        return std::make_pair(NotChange, nullptr);
    }
RETRY:
    std::pair<BorderNode*, Version> borderNode_version = findBorder(root, key);
    BorderNode *borderNode = borderNode_version.first;
    Version version = borderNode_version.second;
    borderNode->lock();
    version = borderNode->getVersion();
    // 該当するborderNodeを見つけたら速攻Lockを取得することで、同一キーに対するremoveとかの事故を防いでいる
FORWARD:
    assert(borderNode->isLocked());
    if (version.deleted) {
        borderNode->unlock();
        if (version.is_root) {
            // 並列で走っていた同じキーに対するremove処理が代わりに消してくれた(RETRYに戻ったところで、borderNodeがRootなら処理は同じな)ので、終わり
            return std::make_pair(NotChange, root);
        } else {
            // Rootじゃないなら再走する必要がある
            goto RETRY;
        }
    }
    std::tuple<SearchResult, LinkOrValue, size_t> result_lv_index = borderNode->searchLinkOrValueWithIndex(key);
    SearchResult result = std::get<0>(result_lv_index);
    LinkOrValue lv      = std::get<1>(result_lv_index);
    size_t index        = std::get<2>(result_lv_index);
    if (Version::splitHappened(version, borderNode->getVersion())) {
        // findBorderとlockの間でsplit処理が起きた場合
        BorderNode *next = borderNode->getNext();
        borderNode->unlock();
        assert(next != nullptr);    // splitが発生したのならnextが必ずあるはず
        while (!version.deleted && next != nullptr && key.getCurrentSlice().slice >= next->lowestKey()) {
            borderNode  = next;
            version     = borderNode->stableVersion();
            next        = borderNode->getNext();
        }
        borderNode->lock();
        goto FORWARD;
    } else if (result == NOTFOUND) {
        // 無いもんは消せねえ(´・ω・`)
        // NOTE: ところで、無い場合はその旨を通知するべきなのか？
        borderNode->unlock();
        return std::make_pair(DataNotFound, root);
    } else if (result == VALUE) {
        /**
         * [1] - もし要素が1つしかないBorderNodeがRootの場合
         *         - Layerを消去して、上のレイヤのSuffixにセットする
         *         - 上のレイヤのsuffixにセットして、そのKeyのremoveをやり直す
         * [2] - それ以外の場合
         *         - BorderNodeからキーを消去
         *         - BorderNode内部でキーを左シフトして隙間を埋める
         * [3]     - BorderNode内のキーの数を確認
         *             - BorderNodeにキーが残っている場合
         *                 - それで終わり
         * [4]         - BorderNodeが空になる場合
         *                 - 親ノード(InteriorNode)での変更が必要になる
         *                     - parentの残りが2以上の場合
         *                         - 左シフトで埋める
         *                     - parentの残りが1つの場合
         *                         - Rootの近くの場合
         *                             - upper_layerから繋ぎなおして、is_rootを更新
         *                         - そうでない場合
         *                             - parentのparentから直接リンクを張る
         */
        Permutation permutation = borderNode->getPermutation();
        // [1]
        if (borderNode->getIsRoot() && permutation.getNumKeys() == 1 && key.cursor != 0) {
            // Layer0の時以外で、BorderNodeがRootで要素が1つだけの場合は、要素数は変化しない
            handle_delete_layer_in_remove(borderNode, gc);
            return std::make_pair(LayerDeleted, nullptr);
        }
        // [2]
        borderNode->markKeyRemoved(index);
        permutation.removeIndex(index);
        borderNode->setPermutation(permutation);
        // [3]
        uint8_t currentNumKeys = permutation.getNumKeys();
        if (currentNumKeys == 0) {
            // [4]
            std::pair<RootChange, Node*> pair = delete_borderNode_in_remove(borderNode, gc);
            if (pair.first != NotChange) {
                return pair;
            }
        } else {
            borderNode->unlock();
        }
    } else if (result == LAYER) {
        borderNode->unlock();
        key.next();
        std::pair<RootChange, Node*> pair = remove(lv.next_layer, key, gc);
        if (pair.first == LayerDeleted) {   // すでに消えているのであればカーソルを一個戻してRETRY
            key.back();
            goto RETRY;
        }
    } else {
        // result == UNSTABLE
        assert(false);  // ここには来ないはずなので
    }
    return std::make_pair(NotChange, root);
}

std::pair<RootChange, Node*> remove_at_layer0(Node *root, Key &key, GarbageCollector &gc) {
    return remove(root, key, gc);
}