#include "include/masstree_scan.h"

void masstree_scan(Node* root,
                   Status &scan_status,
                   Key &current_key,
                   Key &left_key,
                   bool l_exclusive,
                   Key &right_key,
                   bool r_exclusive,
                   std::vector<std::pair<Key, Value*>> &result) {

    // RootがnullptrまたはステータスがOKでない場合は処理を中断
    if (root == nullptr || scan_status != Status::OK) {
        scan_status = Status::ERROR_CONCURRENT_WRITE_OR_DELETE;
        return;
    }

    // Rootからleft_keyに関連するBorderNodeを探す
    std::pair<BorderNode*, Version> node_version = findBorder(root, left_key);
    BorderNode *border = node_version.first;
    Version version = node_version.second;

    // BorderNode内を走査し、指定された範囲の値を探索する
    while (border != nullptr) {
        Permutation perm = border->getPermutation();
        for (size_t i = 0; i < perm.getNumKeys(); i++) {
            uint8_t trueIndex = perm(i);

            // 削除されたキーは無視する
            if (border->isKeyRemoved(trueIndex)) continue;

            // レイヤが変わるタイミングで不必要なデータが残っている可能性があるので、cursor以降のスライスを削除する
            current_key.slices.resize(current_key.cursor + 1);

            // 新しいスライスとキーの長さを取得
            uint64_t new_slice = border->getKeySlice(trueIndex);
            uint8_t key_len = border->getKeyLen(trueIndex);

            // current_keyのカーソル位置に新しいスライスを設定し、lastSliceSizeを更新
            current_key.slices[current_key.cursor] = new_slice;
            if (key_len == BorderNode::key_len_has_suffix || key_len == BorderNode::key_len_layer) {
                current_key.lastSliceSize = 8;
            } else if (key_len < BorderNode::key_len_has_suffix) {
                current_key.lastSliceSize = key_len;
            }

            // Suffixの処理
            if (key_len == BorderNode::key_len_has_suffix) {
                BigSuffix* suffix = border->getKeySuffixes().get(trueIndex);
                if (suffix != nullptr) {
                    size_t suffixIndex = 0;
                    while (suffix->hasNext()) {
                        current_key.slices.push_back(suffix->getCurrentSlice().slice);
                        suffix->next();
                        suffixIndex++;
                    }
                    // 最後のスライスを追加
                    current_key.slices.push_back(suffix->getCurrentSlice().slice);
                    current_key.lastSliceSize = suffix->getCurrentSlice().size;
                }
            }

            // 範囲外のキーは無視する
            if (current_key < left_key || (l_exclusive && current_key == left_key)) continue;
            if (right_key < current_key || (r_exclusive && current_key == right_key)) return;

            LinkOrValue lv = border->getLV(trueIndex);

            if (lv.value) {
                // Valueを見つけた場合はresultに追加
                result.emplace_back(current_key, lv.value);
            } else if (lv.next_layer) {
                // Linkを見つけた場合は再帰的に探索
                current_key.next(); // レイヤを降下するためカーソルを進める
                masstree_scan(lv.next_layer, scan_status, current_key, left_key, l_exclusive, right_key, r_exclusive, result);
                current_key.back(); // DFSから戻ってきたのでカーソルを戻す
            }
        }

        // 次のBorderNodeへ移動
        border = border->getNext();
    }
}