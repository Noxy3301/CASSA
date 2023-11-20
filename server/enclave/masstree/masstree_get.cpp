#include "include/masstree_get.h"

Value *masstree_get(Node *root, Key &key) {
    if (root == nullptr) return nullptr;    // Layer0がemptyの状態でgetが来た場合
RETRY:
    std::pair<BorderNode*, Version> node_version = findBorder(root, key);
    BorderNode *node = node_version.first;
    Version version = node_version.second;
FORWARD:
    if (version.deleted) {
        if (version.is_root) {
            return nullptr; // Layer0がemptyにされた or 下位レイヤに移った場合
        } else {
            goto RETRY;
        }
    }
    std::pair<SearchResult, LinkOrValue> result_lv = node->searchLinkOrValue(key);
    SearchResult result = result_lv.first;
    LinkOrValue lv = result_lv.second;
    if ((node->getVersion() ^ version) > Version::has_locked) {
        version = node->stableVersion();
        BorderNode *next = node->getNext();
        while (!version.deleted && next != nullptr && key.getCurrentSlice().slice >= next->lowestKey()) {
            node = next;
            version = node->stableVersion();
            next = node->getNext();
        }
        goto FORWARD;
    } else if (result == NOTFOUND) {
        return nullptr;
    } else if (result == VALUE) {
        return lv.value;
    } else if (result == LAYER) {
        root = lv.next_layer;
        key.next();
        goto RETRY;
    } else {
        assert(result == UNSTABLE);
        goto FORWARD;
    }
}