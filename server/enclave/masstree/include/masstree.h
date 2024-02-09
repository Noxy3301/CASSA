#pragma once

#include "masstree_insert.h"
#include "masstree_get.h"
#include "masstree_remove.h"
#include "masstree_scan.h"
#include "../../cassa_common/status.h"

class Masstree {
public:
    Status insert_value(Key &key, Value *value, GarbageCollector &gc);
    Status remove_value(Key &key, GarbageCollector &gc);
    Value *get_value(Key &key);
    Status scan(Key &left_key,
                bool l_exclusive,
                Key &right_key,
                bool r_exclusive,
                std::vector<std::pair<Key, Value*>> &result);

    std::string get_min_key() const {
        if (!min_key.has_value()) {
            return "";
        }
        return min_key.value().to_string();
    }

    std::string get_max_key() const {
        if (!max_key.has_value()) {
            return "";
        }
        return max_key.value().to_string();
    }

private:
    std::atomic<Node *> root{nullptr};
    std::optional<Key> min_key;
    std::optional<Key> max_key;
    std::mutex mutex_;  // min_key と max_key の更新を保護するためのミューテックス
};