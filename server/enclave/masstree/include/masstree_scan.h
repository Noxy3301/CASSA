#pragma once

#include "masstree_node.h"
#include "../../cassa_common/status.h"

void masstree_scan(Node* root,
                   Status &scan_status,
                   Key &current_key,
                   Key &left_key,
                   bool l_exclusive,
                   Key &right_key,
                   bool r_exclusive,
                   std::vector<std::pair<Key, Value*>> &result);