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

    private:
        std::atomic<Node *> root{nullptr};
};