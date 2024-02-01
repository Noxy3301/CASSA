#include "include/masstree.h"

/**
 * @brief Insert a record into Masstree.
 * 
 * @param key Key identifying the record to insert.
 * @param value Pointer to a Value object containing data and Transaction ID (TID).
 * @param gc GarbageCollector object to manage the memory of deleted nodes.
 * 
 * @return Status::OK if the operation is successful, 
 *         Status::WARN_ALREADY_EXISTS if the record already exists.
 * 
 * @details This function is employed by Silo's insert operation to add a Value. 
 *          The inserted record is initially marked as absent, and the Value is 
 *          modified to the appropriate data during the commit phase. Concurrent 
 *          read/write operations will either be denied with WARN_ALREADY_EXISTS 
 *          or will observe the absent flag. Concurrent insert operations will 
 *          be denied with WARN_ALREADY_EXISTS. Concurrent delete operations 
 *          navigate through two scenarios:
 *          1. insert -> delete: The delete operation will be denied with 
 *             WARN_NOT_EXIST as the record is marked as absent.
 *          2. delete -> insert: The insert operation will be denied with 
 *             WARN_ALREADY_EXISTS.
 */
Status Masstree::insert_value(Key &key, Value *value, GarbageCollector &gc) {
RETRY:
    Node *old_root = root.load(std::memory_order_acquire);
    std::pair<Status, Node*> resultPair = masstree_insert(old_root, key, value, gc);
    if (resultPair.first == Status::RETRY_FROM_UPPER_LAYER) goto RETRY;
    if (resultPair.first == Status::WARN_ALREADY_EXISTS) return resultPair.first;
    Node *new_root = resultPair.second;
    key.reset();
    // old_rootがぬるぽならrootを作るけど他スレッドと争奪戦が起きるのでCASを使う
    if (old_root == nullptr) {
        bool CAS_success = root.compare_exchange_weak(old_root, new_root);
        if (CAS_success) {
            return Status::OK;
        } else {
            // ハァ...ハァ...敗北者...?(new_rootを消す)
            assert(new_root != nullptr);
            assert(new_root->getIsBorder());
            new_root->setDeleted(true);
            gc.add(reinterpret_cast<BorderNode*>(new_root));
            goto RETRY;
        }
    }
    // masstree_putでrootが更新された場合Masstree自体のrootを更新する
    if (old_root != new_root) {
        assert(old_root != nullptr);
        root.store(new_root, std::memory_order_release);
    }
    
    return Status::OK;
}

/**
 * @brief Delete a record from Masstree.
 * 
 * @param key The key identifying the record to delete.
 * @param gc GarbageCollector object to manage the memory of deleted nodes.
 * 
 * @return Status::OK if the operation is successful, 
 *         Status::WARN_NOT_FOUND if the record does not exist.
 * 
 * @details The method `Masstree::remove_value` immediately deletes the record and, 
 *          if necessary, reconstructs the tree. The deleted record and node, 
 *          once added to the GarbageCollector, will be deleted if no other thread 
 *          is accessing them, as referred to reach the reclamation epoch.

 * 
 * @note Initially, Silo marks the record as absent and, during the commit phase, 
 *       invokes `Masstree::remove_value` to delete the record from the tree.
 *       The GarbageCollector functionality is not yet implemented.
 */

Status Masstree::remove_value(Key &key, GarbageCollector &gc) {
RETRY:
    Node *old_root = root.load(std::memory_order_acquire);
    if (old_root == nullptr) return Status::WARN_NOT_FOUND;

    std::pair<RootChange, Node*> resultPair = remove_at_layer0(old_root, key, gc);
    Node *new_root = resultPair.second;
    key.reset();

    if (resultPair.first == RootChange::DataNotFound) return Status::WARN_NOT_FOUND;

    // remove_at_layer0でrootが更新された場合Masstree自体のrootを更新する
    if (old_root != new_root) {
        bool CAS_success = root.compare_exchange_weak(old_root, new_root);
        if (CAS_success) {
            return Status::OK;
        } else {
            // CHECK: CASが失敗するケースをテストで確認したい
            goto RETRY;
        }
    }
    return Status::OK;
}

/**
 * @brief Retrieve a value from Masstree.
 * 
 * @param key The key identifying the record to retrieve.
 * @return The Value object containing data and TID, or nullptr if not found.
 * 
 * @details This function is used by Silo's read/write operations to retrieve a Value. 
 * Concurrent insert operations will be denied, responding with WARN_ALREADY_EXISTS. 
 * However, for concurrent delete operations, two scenarios need to be considered:
 * 1. read/write -> delete:
 *  In this case, the delete operation marks the absent bit, but internally, 
 *  absent records are treated like present records that must be validated on read.
 * 2. delete -> read/write:
 *  In this case, the delete operation immediately marks the absent bit, 
 *  and the read/write operation will be denied with WARN_NOT_EXIST.
 */
Value *Masstree::get_value(Key &key) {
    Node *root_ = root.load(std::memory_order_acquire);
    Value *v = masstree_get(root_, key);
    key.reset();
    return v;
}


// Scan results will be stored in a vector of <Key, Value> pairs, provided as an argument.
Status Masstree::scan(Key &left_key, bool l_exclusive,
                      Key &right_key, bool r_exclusive,
                      std::vector<std::pair<Key, Value*>> &result) {
    Node *root_ = root.load(std::memory_order_acquire);
    Key current_key = left_key;
    Status scan_status = Status::OK;
    masstree_scan(root_, scan_status, current_key, left_key, l_exclusive, right_key, r_exclusive, result);
    left_key.reset();
    right_key.reset();

    return scan_status;
}