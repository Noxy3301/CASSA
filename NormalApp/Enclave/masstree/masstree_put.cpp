#include "include/masstree_put.h"

// Layer0がempty(Masstreeが空)の場合、新しいMasstreeを作る
BorderNode *start_new_tree(const Key &key, Value *value) {
    BorderNode *root = new BorderNode();
    root->setIsRoot(true);

    SliceWithSize cursor = key.getCurrentSlice();
    // 現在のスライスが最後のスライスなら
    if (1 <= cursor.size && cursor.size <= 7) {
        root->setKeyLen(0, cursor.size);
        root->setKeySlice(0, cursor.slice);
        root->setLV(0, LinkOrValue(value));
    } else {
        assert(cursor.size == 8);
        if (key.hasNext()) {
            // keyの長さが9byte以上なのでsuffixを持たせる
            root->setKeySlice(0, cursor.slice);
            root->setKeyLen(0, BorderNode::key_len_has_suffix);
            root->setLV(0, LinkOrValue(value));
            root->getKeySuffixes().set(0, key, 1);
        } else {
            // keyの長さが丁度8byte
            root->setKeyLen(0, 8);
            root->setKeySlice(0, cursor.slice);
            root->setLV(0, LinkOrValue(value));
        }
    }
    return root;
}

// BorderNodeにkeyをinsertするときに既存のkeyと競合が発生するかを確認する
// 問題がある場合はtrueIndex、問題がない場合は-1が返ってくる(std::optionalは使い方わからん；；)
// NOTE: ssize_tの関係で異常がない場合に-1が返ってくるので注意
ssize_t check_break_invariant(BorderNode *const borderNode, const Key &key) {
    assert(borderNode->isLocked());
    Permutation permutation = borderNode->getPermutation();
    if (key.hasNext()) {
        SliceWithSize cursor = key.getCurrentSlice();
        for (size_t i = 0; i < permutation.getNumKeys(); i++) {
            uint8_t trueIndex = permutation(i);
            // キーがsuffix or 下位レイヤへのポインタを持っている && 指定したkeyがborderNodeにあるか
            if ((borderNode->getKeyLen(trueIndex) == BorderNode::key_len_has_suffix || 
                 borderNode->getKeyLen(trueIndex) == BorderNode::key_len_layer) && 
                 borderNode->getKeySlice(trueIndex) == cursor.slice) {
                return trueIndex;
            }
        }
    }
    return -1;
}

// invariantを検知した時の処理, §4.6.3
void handle_break_invariant(BorderNode *node, Key &key, size_t old_index, GarbageCollector &gc) {
    assert(node->isLocked());
    if (node->getKeyLen(old_index) == BorderNode::key_len_has_suffix) { // [1] 競合するkey(k2)を含むBorderNodeにkey(k1)をinsertする際に新しいレイヤを作成する
        /*
         * [1] Masstree creates a new layer when inserting a key k1 into a border node that contains a conflicting key k2.
         * [2] It allocates a new empty border node n′, [3] inserts k2’s current value into it under the appropriate key slice, [4] and then replaces k2’s value in n with the next_layer pointer n′.
         * Finally, it unlocks n and continues the attempt to insert k1, now using the newly created layer n′.
         */
        BorderNode *n1 = new BorderNode{};                              // [2] 新しいBorderNodeの作成
        n1->setIsRoot(true);
        n1->setUpperLayer(node);
        Value *k2_value = node->getLV(old_index).value;
        BigSuffix *k2_suffix_copy = new BigSuffix(*node->getKeySuffixes().get(old_index));
        if (k2_suffix_copy->hasNext()) {                                // [3] 適切なkey sliceの下にk2をinsertする
            n1->setKeyLen(0, BorderNode::key_len_has_suffix);
            n1->setKeySlice(0, k2_suffix_copy->getCurrentSlice().slice);
            k2_suffix_copy->next();
            n1->getKeySuffixes().set(0, k2_suffix_copy);
            n1->setLV(0, LinkOrValue(k2_value));
        } else {
            n1->setKeyLen(0,k2_suffix_copy->getCurrentSlice().size);
            n1->setKeySlice(0, k2_suffix_copy->getCurrentSlice().slice);
            n1->setLV(0, LinkOrValue(k2_value));
        }
        /*
         * Since this process only affects a single key, there is no need to update n’s version or permutation.
         * However, readers must reliably distinguish true values from next_layer pointers. Since the pointer and the layer marker are stored separately, this requires a sequence of writes.
         * [5] First, the writer marks the key as UNSTABLE; readers seeing this marker will retry.
         * [6] It then writes the next_layer pointer, and finally marks the key as a LAYER.
         */
        node->setKeyLen(old_index, BorderNode::key_len_unstable);       // [5] next_layerをいじる前にUNSTABLEにしてreaderが観測した際にretryさせる
        node->setLV(old_index, LinkOrValue(n1));                        // [4] node内のk2_valueを次のレイヤへのポインタn1に置き換える
        node->setKeyLen(old_index, BorderNode::key_len_layer);          // [6] next_layerをいじってLAYERに変える
        gc.add(node->getKeySuffixes().get(old_index));
        node->getKeySuffixes().unreferenced(old_index);
    } else {
        // 次のLayerでinsertを行う
        assert(node->getKeyLen(old_index) == BorderNode::key_len_layer);
    }
}

// BorderNodeのkeyに対応する箇所にvalueを入れる
void insert_to_border(BorderNode *border, const Key &key, Value *value, GarbageCollector &gc) {
    assert(border->isLocked());
    assert(!border->getSplitting());
    assert(!border->getInserting());
    Permutation permutation = border->getPermutation();
    assert(permutation.isNotFull());

    size_t insertion_point_permutationIndex = 0;    // permutationのindex
    size_t num_keys = permutation.getNumKeys();
    SliceWithSize cursor = key.getCurrentSlice();
    // 新しいキーをinsertするための実際の場所を探している([3,5,8,10]で7を入れたいならwhileで探すと5の次みたいな感じ)
    while (insertion_point_permutationIndex < num_keys && border->getKeySlice(permutation(insertion_point_permutationIndex)) < cursor.slice) {
        insertion_point_permutationIndex++;
    }
    std::pair<size_t, bool> pair = border->insertPoint();   // permutationの方でinsertできる場所を探す
    size_t insertion_point_trueIndex = pair.first;
    bool reuse = pair.second;

    if (reuse) {
        // key_len = 0ではないスロット(すなわちremoved slot)を再利用するので、古いValueとSuffixが残っている可能性がある
        // w-w conflitはlockで対処、readerはinsertingをtrueにしておけばretryするのでOKのはず
        border->setInserting(true);
        BigSuffix *suffix = border->getKeySuffixes().get(insertion_point_trueIndex);
        if (suffix != nullptr) gc.add(suffix);  // ぬるぽじゃないならgcに投げておく
        gc.add(border->getLV(insertion_point_trueIndex).value);
    }
    border->getKeySuffixes().set(insertion_point_trueIndex, nullptr);   // suffixをclearしておく

    /*
     * キースライスの長さが1~7の場合はスライスとその長さを直接BorderNodeに保存する
     * キースライスの長さが8の場合は現在のスライスが最後のスライスかどうかを判定して、最後のスライスなら通常通り保存する
     * 最後のスライスじゃないならBigSuffixに保存して、key_len_has_suffixのフラグを立てる
     */
    if (1 <= cursor.size && cursor.size <= 7) {
        border->setKeyLen(insertion_point_trueIndex, cursor.size);
        border->setKeySlice(insertion_point_trueIndex, cursor.slice);
        border->setLV(insertion_point_trueIndex, LinkOrValue(value));
    } else {
        assert(cursor.size == 8);
        if (key.hasNext()) {
            border->setKeySlice(insertion_point_trueIndex, cursor.slice);
            border->setKeyLen(insertion_point_trueIndex, BorderNode::key_len_has_suffix);
            border->getKeySuffixes().set(insertion_point_trueIndex, key, key.cursor + 1);
            border->setLV(insertion_point_trueIndex, LinkOrValue(value));
        } else {
            border->setKeyLen(insertion_point_trueIndex, 8);
            border->setKeySlice(insertion_point_trueIndex, cursor.slice);
            border->setLV(insertion_point_trueIndex, LinkOrValue(value));
        }
    }
    permutation.insert(insertion_point_permutationIndex, insertion_point_trueIndex);
    border->setPermutation(permutation);
}

// void insert_into_border(BorderNode *border, const Key &key, Value *value, GarbageCollector &gc) {}

// size_t cut(size_t len) {}

/**
 * @brief InteriorNodeのparentを分割して、parent1を作成する。
 *
 * @param parent     分割する元のInteriorNode
 * @param parent1    分割後に新たに作成されるInteriorNode
 * @param slice      挿入する新しいキー
 * @param node1      挿入する新しい子ノード
 * @param node_index node1とsliceを挿入する位置
 * @param k_prime    分割後に上位ノードに引き上げるキー、関数内で更新される
 */
void split_keys_among(InteriorNode *parent, InteriorNode *parent1, uint64_t slice, Node *node1, size_t node_index, std::optional<uint64_t> &k_prime) {
    assert(!parent->isNotFull());
    assert(parent->isLocked());
    assert(parent->getSplitting());
    assert(parent1->isLocked());
    assert(parent1->getSplitting());

    uint64_t temp_key_slice[Node::ORDER] = {};
    Node *temp_child[Node::ORDER + 1] = {};

    for (size_t i = 0, j = 0; i < parent->getNumKeys() + 1; i++, j++) {
        if (j == node_index + 1) j++;   // keyをinsertする場所だけ開けておく ([10,20,30]で15をinsertするなら[10,__,20,30]みたいな感じ)
        temp_child[j] = parent->getChild(i);
    }
    for (size_t i = 0, j = 0; i < parent->getNumKeys(); i++, j++) {
        if (j == node_index) j++;
        temp_key_slice[j] = parent->getKeySlice(i);
    }
    temp_child[node_index + 1] = node1;
    temp_key_slice[node_index] = slice;

    // parentとparent1を初期化する、本当はpermutationのnkeysを0にすれば良いけどバグが怖いので一応全部初期化しておく
    parent->setNumKeys(0);
    parent->resetKeySlices();
    parent->resetChildren();

    size_t split = (Node::ORDER%2 == 0) ? Node::ORDER/2 : Node::ORDER/2 + 1;    // InteriorNodeはバランスの関係上ORDERの半分がsplit pointになる、関数で定義されていたけど参照しているのがここしかなかった
    size_t i = 0, j = 0;
    for (i = 0; i < split - 1; i++) {
        parent->setChild(i, temp_child[i]);
        parent->getChild(i)->setParent(parent);
        parent->setKeySlice(i, temp_key_slice[i]);
        parent->incNumKeys();
    }
    parent->setChild(i, temp_child[i]);
    temp_child[i]->setParent(parent);
    k_prime = temp_key_slice[split - 1];
    for (i++, j = 0; i < Node::ORDER; i++, j++) {
        parent1->setChild(j, temp_child[i]);
        parent1->getChild(j)->setParent(parent1);
        parent1->setKeySlice(j, temp_key_slice[i]);
        parent1->incNumKeys();
    }
    parent1->setChild(j, temp_child[i]);
    temp_child[i]->setParent(parent1);

    // for (auto &node : temp_child) {
    //     assert(node->getParent()->debug_contain_child(node));
    // }
}

// BorderNodeのkey-sliceのマップを作る
// CHECK: これ使っている理由がよくわからん
void create_slice_table(BorderNode *const node, std::vector<std::pair<uint64_t, size_t>> &table, std::vector<uint64_t> &found) {
    Permutation permutation = node->getPermutation();
    assert(permutation.isFull());
    for (size_t i = 0; i < Node::ORDER - 1; i++) {
        if (!std::count(found.begin(), found.end(), node->getKeySlice(i))) {    // foundにkeySliceがまだ登録されていない場合
            table.emplace_back(node->getKeySlice(i), i);
            found.push_back(node->getKeySlice(i));
        }
    }
}

// BorderNodeにてSplitする位置を決める
// CHECK: これの挙動がいまいちわかっていない
size_t split_point(uint64_t new_slice, const std::vector<std::pair<uint64_t, size_t>> &table, const std::vector<uint64_t> &found) {
    uint64_t min_slice = *std::min_element(found.begin(), found.end());
    uint64_t max_slice = *std::max_element(found.begin(), found.end());
    if (new_slice < min_slice) {    // 最小より小さいならsplit pointは1
        return 1;
    } else if (new_slice == min_slice) {    // 最小と同じならsplit pointは最小の次
        return table[1].second + 1;
    } else if (min_slice < new_slice && new_slice < max_slice) {
        if (std::count(found.begin(), found.end(), new_slice)) {
            for (size_t i = 0; i < table.size(); i++) {
                if (table[i].first == new_slice) return table[i].second;
            }
        } else {
            for (size_t i = 0; i < table.size(); i++) {
                if (table[i].first > new_slice) return table[i].second + 1;
            }
        }
    } else if (new_slice == max_slice) {    // 最大と同じならsplit pointは最大と同じ
        for (size_t i = 0; i < table.size(); i++) {
            if (table[i].first == new_slice) return table[i].second;
        }
    } else {    // 最大よりデカいなら一番最後(15)
        assert(new_slice > max_slice);
        return 15;
    }
}

// BorderNodeのnodeを分割してnode1を作成する
void split_keys_among(BorderNode *node, BorderNode *node1, const Key &key, Value *value) {
    Permutation permutation = node->getPermutation();
    assert(permutation.isFull());
    assert(node->isLocked());
    assert(node->getSplitting());
    assert(node1->isLocked());
    assert(node1->getSplitting());

    // permutationが順番を持っているのは面倒なので一度sortする
    node->sort();

    // BorderNodeのサイズに直接入れるとオーバーフローするのでtempを作ってここで一度管理する
    uint8_t temp_key_len[Node::ORDER] = {};
    uint64_t temp_key_slice[Node::ORDER] = {};
    LinkOrValue temp_lv[Node::ORDER] = {};
    BigSuffix *temp_suffix[Node::ORDER] = {};

    size_t insertion_index = 0;
    while (insertion_index < Node::ORDER - 1 && node->getKeySlice(insertion_index) < key.getCurrentSlice().slice) {
        insertion_index++;
    }

    // tempにコピー
    for (size_t i = 0, j = 0; i < permutation.getNumKeys(); i++, j++) {
        if (j == insertion_index) j++;  // keyをinsertする場所だけ開けておく ([10,20,30]で15をinsertするなら[10,__,20,30]みたいな感じ)
        temp_key_len[j] = node->getKeyLen(i);
        temp_key_slice[j] = node->getKeySlice(i);
        temp_lv[j] = node->getLV(i);
        temp_suffix[j] = node->getKeySuffixes().get(i);
    }
    // insertion_indexだけあけてあるtempにinsertする
    SliceWithSize cursor = key.getCurrentSlice();
    if (1 <= cursor.size && cursor.size <= 7) {
        temp_key_len[insertion_index] = cursor.size;
        temp_key_slice[insertion_index] = cursor.slice;
        temp_lv[insertion_index].value = value;
    } else {
        assert(cursor.size == 8);
        if (key.hasNext()) {
            // キーが9byte以上の場合残りはSuffixに保存されるため、キースライスが全て使用されていてもノードを分割する必要がない -> splitは発生しない
            // CHECK: っていう話らしいんだけど、Suffixに保存されるからSplitされないっていうのはわかる、その処理はどこで書いているんだ？
            temp_key_slice[insertion_index] = cursor.size;
            temp_key_len[insertion_index] = BorderNode::key_len_has_suffix;
            temp_suffix[insertion_index] = BigSuffix::from(key, key.cursor + 1);
            temp_lv[insertion_index].value = value;
        } else {    // キースライスの長さが8
            temp_key_len[insertion_index] = 8;
            temp_key_slice[insertion_index] = cursor.slice;
            temp_lv[insertion_index].value = value;
        }
    }

    std::vector<std::pair<uint64_t, size_t>> table{};
    std::vector<uint64_t> found{};
    create_slice_table(node, table, found);
    size_t split = split_point(cursor.slice, table, found);
    // nodeとnode1を初期化する
    node->resetKeyLen();
    node->resetKeySlice();
    node->resetLVs();
    node->getKeySuffixes().reset();

    node1->resetKeyLen();
    node1->resetKeySlice();
    node1->resetLVs();
    node1->getKeySuffixes().reset();

    // tempとsplit pointから各ノードにコピーする
    for (size_t i = 0; i < split; i++) {
        node->setKeyLen(i, temp_key_len[i]);
        node->setKeySlice(i, temp_key_slice[i]);
        node->setLV(i, temp_lv[i]);
        node->getKeySuffixes().set(i, temp_suffix[i]);
        if (temp_key_len[i] == BorderNode::key_len_layer) {
            node->getLV(i).next_layer->setUpperLayer(node); // BorderNodeがvalueじゃなくてnextLayerを持っている場合はそれを設定する
        }
    }
    node->setPermutation(Permutation::fromSorted(split));

    for (size_t i = split, j = 0; i < Node::ORDER; i++, j++) {
        node1->setKeyLen(j, temp_key_len[i]);
        node1->setKeySlice(j, temp_key_slice[i]);
        node1->setLV(j, temp_lv[i]);
        node1->getKeySuffixes().set(j, temp_suffix[i]);
        if (node1->getKeyLen(j) == BorderNode::key_len_layer) {
            node1->getLV(j).next_layer->setUpperLayer(node1);
        }
    }
    node1->setPermutation(Permutation::fromSorted(Node::ORDER - split));

    // nodeとnode1をつないだり、親の更新をする
    node1->setNext(node->getNext());
    node1->setPrev(node);
    node->setNext(node1);
    if (node1->getNext() != nullptr) node1->getNext()->setPrev(node1);
}

InteriorNode *create_root_with_children(Node *left, uint64_t slice, Node *right) {
    assert(left->getIsRoot());
    assert(left->getParent() == nullptr);
    assert(right->getParent() == nullptr);
    assert(left->isLocked());
    assert(right->isLocked());

    // leftとrightの親になるparent nodeを作成して、leftとrightをchildに登録する
    InteriorNode *root = new InteriorNode{};
    root->lock();   // lockする必要ないけどsetParentのassert用にしておく
    BorderNode *upper = left->lockedUpperNode();    // rightは新しく生成されたノードだから上位ノードはないためlockを取得しない、それはそう
    root->setIsRoot(true);
    root->setUpperLayer(upper); // CHECK: これ冗長では？
    if (upper != nullptr) {     // CHECK: これ冗長では？というかassert(left->getIsRoot())がtrueならnodeはroot nodeなのでupperは常にnullptrであるはずでは？
        size_t left_index = upper->findNextLayerIndex(left);
        upper->setLV(left_index, LinkOrValue(root));
    }
    root->setNumKeys(1);
    root->setKeySlice(0, slice);
    root->setChild(0, left);
    root->setChild(1, right);
    // 親をセットしたのでleftとrightのフラグを修正する
    left->setParent(root);
    right->setParent(root);
    left->setUpperLayer(nullptr);   // CHECK: leftってroot nodeだからupperLayerは設定する必要なくない？Layer1とかならupeprLayerが存在するのはわかるけどassert(left->getIsRoot())だからLayer0の話じゃん
    left->setIsRoot(false);
    right->setIsRoot(false);        // CHECK: これ冗長では？
    if (upper != nullptr) upper->unlock();  // CHECK: これ冗長では？
    
    root->unlock();
    assert(left->getUpperLayer() == nullptr);
    return root;
}

// splitでleftの親にright(node1)をinsertするスペース(insertする場所はnodeの次)がある場合に呼ばれる
void insert_into_parent(InteriorNode *parent, Node *node1, uint64_t slice, size_t node_index) {
    assert(parent->isNotFull());
    assert(parent->isLocked());
    assert(parent->getInserting());
    assert(node1->isLocked());

    // node1をinsertするスペースを作るために1つ右にずらす
    for (size_t i = parent->getNumKeys(); i > node_index; i--) {
        parent->setChild(i + 1, parent->getChild(i));
        parent->setKeySlice(i, parent->getKeySlice(i - 1));
    }
    parent->setChild(node_index + 1, node1);
    parent->setKeySlice(node_index, slice);
    parent->incNumKeys();

    node1->setParent(parent);
    // assert(!parent->debug_has_skip());
}

Node *split(Node *node, const Key &key, Value *value) {
    assert(node->isLocked());
    Node *node1 = new BorderNode{};
    node->setSplitting(true);
    node1->setVersion(node->getVersion());
    split_keys_among(reinterpret_cast<BorderNode *>(node), reinterpret_cast<BorderNode *>(node1), key, value);  // nodeとnode1でsplitする
    std::optional<uint64_t> pull_up = std::nullopt; // CHECK: 本当は使いたくないけどuint64_tでエラーを回収するの大変そうだからこっちにしておく  TODO: pairとかでoptionalを回避する
ASCEND:
    // 親ノードが一杯かどうかを調べて、一杯ならsplitしてその親ノードに再帰的にアクセスしに行く
    assert(node->isLocked());
    assert(node1->isLocked());
    InteriorNode *parent = node->lockedParent();
    // if (parent != nullptr) assert(parent->debug_contain_child(node));    // NOTE: debugなので実装してない、必要になったら実装する
    if (parent == nullptr) {
        // nodeがRoot nodeの場合
        uint64_t up;
        if (pull_up) {  // 三項演算子嫌いだからこっちで書いてるけど長いのも嫌だなぁ
            up = pull_up.value();
        } else {
            up = reinterpret_cast<BorderNode *>(node1)->getKeySlice(0);
        }
        parent = create_root_with_children(node, up, node1);
        node->unlock();
        node1->unlock();
        return parent;
    } else if (parent->isNotFull()) {
        // parentに空きがある場合
        parent->setInserting(true);
        size_t node_index = parent->findChildIndex(node);
        uint64_t up;
        if (pull_up) {
            up = pull_up.value();
        } else {
            up = reinterpret_cast<BorderNode *>(node1)->getKeySlice(0);
        }
        insert_into_parent(parent, node1, up, node_index);
        node->unlock();
        node1->unlock();
        parent->unlock();
        return nullptr; // splitの方でnullptrならroot、そうじゃないならmay_new_rootを返すからここではぬるぽを返しておく
    } else {
        // parentに空きがない場合
        assert(parent->isFull());
        parent->setSplitting(true);
        size_t node_index = parent->findChildIndex(node);
        node->unlock();
        Node *parent1 = new InteriorNode{};
        parent1->setVersion(parent->getVersion());
        uint64_t up;    // NOTE: pull_upは一見誰も更新してないから常にstd::nulloptに見えるけどInteriorNodeのsplit_keys_amongで参照渡しされているから更新されているよ
        if (pull_up) {
            up = pull_up.value();
        } else {
            up = reinterpret_cast<BorderNode *>(node1)->getKeySlice(0);
        }
        split_keys_among(reinterpret_cast<InteriorNode *>(parent), reinterpret_cast<InteriorNode *>(parent1), up, node1, node_index, pull_up);
        node1->unlock();
        // assert(node->getParent()->debug_contain_child(node));
        // assert(node1->getParent()->debug_contain_child(node1));
        node = parent;
        node1 = parent1;
        goto ASCEND;
    }
}

std::pair<PutResult, Node*> masstree_put(Node *root, Key &key, Value *value, GarbageCollector &gc) {
    // Layer0がemptyの場合
    if (root == nullptr) return std::make_pair(DONE, start_new_tree(key, value));
RETRY:
    // BorderNodeを探してロックする
    std::pair<BorderNode *, Version> node_version = findBorder(root, key);
    BorderNode *node = node_version.first;
    Version version  = node_version.second;
    node->lock();   // お目当てのnodeを見つけたら即ロック
    // lockした上で最新のversionを取得する、findBorder→lockの間で更新されている可能性があるから
    version = node->getVersion();
FORWARD:
    assert(node->isLocked());
    Permutation permutation = node->getPermutation();
    if (version.deleted) {
        node->unlock();
        if (version.is_root) {
            // Layer0がempty or keyが下位ノードに移動した場合
            // Root nodeが消去されている場合その親ノードが新しいRoot nodeを指している可能性があるから
            // NOTE: is_rootなら上位レイヤからやり直した方がいいのは効率がいいからっていうのは分かる、ただis_rootだけなんで上位レイヤからやり直すの？って言われたらわからん；；
            return std::make_pair(RetryFromUpperLayer, nullptr);
        } else {
            goto RETRY;
        }
    }

    std::tuple<SearchResult, LinkOrValue, size_t> result_lv_index = node->searchLinkOrValueWithIndex(key);
    SearchResult result = std::get<0>(result_lv_index);
    LinkOrValue lv      = std::get<1>(result_lv_index);
    size_t index        = std::get<2>(result_lv_index);
    if (Version::splitHappened(version, node->getVersion())) {
        // findBorder -> lockの間に他スレッドによってsplit処理が起きた場合
        BorderNode *next = node->getNext();
        node->unlock();
        assert(next != nullptr);    // splitが発生した後だからNextは必ず存在するはず

        // BorderNodeが正しい順番で並んでいるんだったらkey.getCurrentSlice().slice < next->lowestKey()になる、それはそう
        // この条件式が成り立たなくなる(version.deleted or next == nullptrも)のがsplitされた場所のはず
        // [5,6,7]で8を入れようとすると[5,6],[7,8]になって、key sliceが8だから(8 < 7)で落ちるみたいな？
        // NOTE: 実はkey.getCurrentSlice().slice >= next->lowestKey()の意味がちゃんと理解できていない
        while (!version.deleted && next != nullptr && key.getCurrentSlice().slice >= next->lowestKey()) {
            node    = next;
            version = node->stableVersion();
            next    = node->getNext();
        }
        node->lock();
        goto FORWARD;
    } else if (result == NOTFOUND) {    // Keyに対応するValueがないのでinsertする
        ssize_t check = check_break_invariant(node, key);
        if (check != -1) {  // BorderNodeにinsertすると違反(競合)が発生する場合
            size_t old_index = check;   // check != -1なのでsize_tとして扱っても問題ない
            handle_break_invariant(node, key, old_index, gc);
            Node *next_layer = node->getLV(old_index).next_layer;
            node->unlock();
            key.next();
            std::pair<PutResult, Node *> pair = masstree_put(next_layer, key, value, gc);
            if (pair.first == RetryFromUpperLayer) {
                key.back();
                goto RETRY;
            }
        } else {    // BorderNodeにinsertすると違反が発生しない場合
            if (permutation.isNotFull()) {
                insert_to_border(node, key, value, gc);
                node->unlock();
            } else {    // permutationが一杯の状態
                Node *may_new_root = split(node, key, value);
                if (may_new_root != nullptr) return std::make_pair(DONE, may_new_root);
            }
        }
    } else if (result == VALUE) {       // Keyに対応するValueがあるのでupdateする
        // updateを行う
        gc.add(node->getLV(index).value);
        node->setLV(index, LinkOrValue(value));
        node->unlock();
    } else if (result == LAYER) {
        node->unlock();
        key.next();
        std::pair<PutResult, Node*> pair = masstree_put(lv.next_layer, key, value, gc);
        if (pair.first == RetryFromUpperLayer) {
            key.back();
            goto RETRY;
        }
    } else {
        // result == UNSTABLE;  ここに来ることはないのでassert(false)で落としておく
        assert(false);
    }
    return std::make_pair(DONE, root);
}