#include "include/masstree_node.h"

std::pair<BorderNode*, Version> findBorder(Node *root, const Key &key) {
RETRY:
    Node *node = root;
    Version version = node->stableVersion();

    if (!version.is_root) {
        root = root->getParent();
        goto RETRY;
    }
DESCEND:
    if (node->getIsBorder()) return std::pair<BorderNode*, Version>(reinterpret_cast<BorderNode *>(node), version);
    InteriorNode *interior_node = reinterpret_cast<InteriorNode*>(node);
    Node *next_node = interior_node->findChild(key.getCurrentSlice().slice);
    Version next_version;
    if (next_node != nullptr) {
        next_version = next_node->stableVersion();
    } else {
        next_version = Version();   // CHECK: ここなんでコンストラクタ呼んでるんだ？先の条件で未定義を回避するためか？
    }
    // nodeがロックされていないならそのまま下のノードに降下していく
    if ((next_node->getVersion() ^ next_version) <= Version::has_locked) {
        assert(next_node != nullptr);
        node = next_node;
        version = next_version;
        goto DESCEND;
    }
    // validationを挟んでversionが更新されていないか確認、されてたらRootからRETRY
    Version validation_version = node->stableVersion();
    if (validation_version.v_split != version.v_split) goto RETRY;
    version = validation_version;
    goto DESCEND;
}