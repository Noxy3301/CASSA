/**
 * This file was created with reference to the following URL.
 * Special thanks to Mr. Hoshino.
 * https://github.com/starpos/oltp-cc-bench/blob/master/include/atomic_wrapper.hpp
 */

#pragma once

/**
 * @brief atomic relaxed load.
 * @details Performs an atomic load operation with relaxed memory order.
 * 
 * @tparam T The type of the value being loaded.
 * @param ptr Reference to the value being loaded.
 * @return The loaded value.
 */
template<typename T>
[[maybe_unused]] static T load(T &ptr) {
    return __atomic_load_n(&ptr, __ATOMIC_RELAXED);
}

template<typename T>
[[maybe_unused]] static T load(T *ptr) {
	return __atomic_load_n(ptr, __ATOMIC_RELAXED);
}

template<typename T>
[[maybe_unused]] static T loadRelaxed(T &ptr) {
	return __atomic_load_n(&ptr, __ATOMIC_RELAXED);
}

template<typename T>
[[maybe_unused]] static T loadRelaxed(T *ptr) {
	return __atomic_load_n(ptr, __ATOMIC_RELAXED);
}

/**
 * @brief atomic acquire load.
 * @details Performs an atomic load operation with acquire memory order.
 * 
 * @tparam T The type of the value being loaded.
 * @param ptr Reference to the value being loaded.
 * @return The loaded value.
 */
template<typename T>
static T loadAcquire(T &ptr) {
	return __atomic_load_n(&ptr, __ATOMIC_ACQUIRE);
}

/**
 * @brief atomic relaxed store.
 * @details Performs an atomic store operation with relaxed memory order.
 * 
 * @tparam T The type of the pointer/reference where the value is stored.
 * @tparam T2 The type of the value being stored.
 * @param ptr Reference or pointer to the location where value is stored.
 * @param val The value to store.
 */
template<typename T, typename T2>
[[maybe_unused]] static void store(T &ptr, T2 val) {
	__atomic_store_n(&ptr, (T) val, __ATOMIC_RELAXED);
}

template<typename T, typename T2>
[[maybe_unused]] static void storeRelaxed(T &ptr, T2 val) {
	__atomic_store_n(&ptr, (T) val, __ATOMIC_RELAXED);  // NOLINT
}

template<typename T, typename T2>
[[maybe_unused]] static void storeRelaxed(T *ptr, T2 val) {
	__atomic_store_n(ptr, (T) val, __ATOMIC_RELAXED);  // NOLINT
}

/**
 * @brief atomic release store.
 * @details Performs an atomic store operation with release memory order.
 * 
 * @tparam T The type of the pointer/reference where the value is stored.
 * @tparam T2 The type of the value being stored.
 * @param ptr Reference to the location where value is stored.
 * @param val The value to store.
 */
template<typename T, typename T2>
static void storeRelease(T &ptr, T2 val) {
	__atomic_store_n(&ptr, (T) val, __ATOMIC_RELEASE);  // NOLINT
}

/**
 * @brief atomic acq-rel compare-and-exchange (CAS).
 * @details Performs an atomic CAS operation with acq-rel memory order.
 * 
 * @tparam T Type of the atomic variable.
 * @tparam T2 Type of the value to compare against and to exchange.
 * @param m Atomic variable.
 * @param before Expected value.
 * @param after New value.
 * @return True if the CAS was successful. False otherwise.
 */
template<typename T, typename T2>
static bool compareExchange(T &m, T &before, T2 after) {
	return __atomic_compare_exchange_n(&m, &before, (T) after, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
}

/**
 * @brief atomic fetch and add.
 * @details Atomically adds a value to an atomic integer and returns its previous value.
 * 
 * @tparam Int1 Type of the atomic integer.
 * @tparam Int2 Type of the value to add.
 * @param m Atomic integer.
 * @param v Value to add.
 * @param memorder Memory order for the operation. Default is acq-rel.
 * @return Previous value of the atomic integer.
 */
template<typename Int1, typename Int2>
static Int1 fetchAdd(Int1 &m, Int2 v, int memorder = __ATOMIC_ACQ_REL) {
	return __atomic_fetch_add(&m, v, memorder);
}