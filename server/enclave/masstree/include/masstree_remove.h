#pragma once

#include "masstree_node.h"
#include "masstree_gc.h"

enum RootChange : uint8_t {
    NotChange,
    NewRoot,
    LayerDeleted,
    DataNotFound,
};

void handle_delete_layer_in_remove(BorderNode *borderNode, GarbageCollector &gc);

std::pair<RootChange, Node*> delete_borderNode_in_remove(BorderNode *borderNode, GarbageCollector &gc);

std::pair<RootChange, Node*> remove(Node *root, Key &key, GarbageCollector &gc);

std::pair<RootChange, Node*> remove_at_layer0(Node *root, Key &key, GarbageCollector &gc);