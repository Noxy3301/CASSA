#include <string>
#include <queue>
#include <mutex>
#include <vector>
#include <cassert>
#include <iostream>

class Xoroshiro128Plus {
    public:
        Xoroshiro128Plus() {
            init();
        }

        uint64_t s[2];

        inline void init() {
            unsigned init_seed = 0;
            // sgx_read_rand((unsigned char *) &init_seed, 4);

            s[0] = init_seed;
            s[1] = splitMix64(s[0]);
        }

        uint64_t splitMix64(uint64_t seed) {
            uint64_t z = (seed += 0x9e3779b97f4a7c15);
            z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
            z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
            return z ^ (z >> 31);
        }

        inline uint64_t rotl(const uint64_t x, int k) {
            return (x << k) | (x >> (64 - k));
        }

        uint64_t next(void) {
            const uint64_t s0 = s[0];
            uint64_t s1 = s[1];
            const uint64_t result = s0 + s1;

            s1 ^= s0;
            s[0] = rotl(s0, 24) ^ s1 ^ (s1 << 16);  // a, b
            s[1] = rotl(s1, 37);                    // c

            return result;
        }

        uint64_t operator()() { return next(); }

        /* This is the jump function for the generator. It is equivalent
           to 2^64 calls to next(); it can be used to generate 2^64
           non-overlapping subsequences for parallel computations. */

        void jump(void) {
            static const uint64_t JUMP[] = {0xdf900294d8f554a5, 0x170865df4b3201fc};

            uint64_t s0 = 0;
            uint64_t s1 = 0;
            for (uint64_t i = 0; i < sizeof JUMP / sizeof *JUMP; i++)
                for (int b = 0; b < 64; b++) {
                    if (JUMP[i] & UINT64_C(1) << b) {
                        s0 ^= s[0];
                        s1 ^= s[1];
                    }
                    next();
                }

            s[0] = s0;
            s[1] = s1;
        }

        /* This is the long-jump function for the generator. It is equivalent to
           2^96 calls to next(); it can be used to generate 2^32 starting points,
           from each of which jump() will generate 2^32 non-overlapping
           subsequences for parallel distributed computations. */

        void long_jump(void) {
            static const uint64_t LONG_JUMP[] = {0xd2a98b26625eee7b,
                                                 0xdddf9b1090aa7ac1};

            uint64_t s0 = 0;
            uint64_t s1 = 0;
            for (uint64_t i = 0; i < sizeof LONG_JUMP / sizeof *LONG_JUMP; i++)
                for (int b = 0; b < 64; b++) {
                    if (LONG_JUMP[i] & UINT64_C(1) << b) {
                        s0 ^= s[0];
                        s1 ^= s[1];
                    }
                    next();
                }

            s[0] = s0;
            s[1] = s1;
        }
};

/**
 * @class TransactionQueue
 * @brief Thread-safe transaction queue for each worker
 * 
 * @note  This class offers functionalities to add and retrieve 
 *        transactions (as JSON strings) to/from a queue. 
 *        If the queue is empty, getTransaction returns an empty string.
*/
class TransactionQueue {
public:
    TransactionQueue() = default;

    /**
     * @brief Enqueue transaction to the queue
     * @param json_transaction(std::string) Transaction in JSON format
    */
    void putTransaction(const std::string &json_transaction) {
        transaction_queue_.push(json_transaction);
    }

    /**
     * @brief Dequeue transaction from the queue if exists
     * @return json_transaction(std::string) Transaction in JSON format
    */
    std::string getTransaction() {
        if (transaction_queue_.empty()) {
            return "";  // if queue is empty, return empty string
        }
        std::string json_transaction = transaction_queue_.front();
        transaction_queue_.pop();
        return json_transaction;
    }

private:
    std::queue<std::string> transaction_queue_; // transaction queue
};

/**
 * @class TransactionBalancer
 * @brief Balancer for transactions
 * 
 * @note  This class utilizes the Xoroshiro128Plus random number
 *        generator to randomly select a queue for adding transactions. 
 *        Each worker thread can retrieve a transaction from a specific queue 
 *        based on its ID.
*/
class TransactionBalancer {
public:
    /**
     * @brief Constructor for TransactionBalancer.
     * @param num_workers(size_t) Number of worker queues to manage.
     */
    TransactionBalancer() = default;

    void init(size_t num_workers) {
        transaction_queues_.resize(num_workers);
    }

    /**
     * @brief Enqueue a transaction to a randomly selected queue.
     * @param json_transaction(std::string) Transaction in JSON format
     */
    void putTransaction(const std::string &json_transaction) {
        // select worker queue randomly
        size_t worker_id = rnd_.next() % transaction_queues_.size();
        std::lock_guard<std::mutex> lock(queue_mutexes_[worker_id]);
        transaction_queues_[worker_id].putTransaction(json_transaction);
    }

    /**
     * @brief Dequeue a transaction from the queue of a specific worker.
     * @param worker_id(size_t) The ID of the worker whose queue will be accessed.
     * @return A transaction if available, or an empty string if the queue is empty.
     */
    std::string getTransaction(size_t worker_id) {
        assert(worker_id < transaction_queues_.size()); // validate worker_id
        std::lock_guard<std::mutex> lock(queue_mutexes_[worker_id]);
        return transaction_queues_[worker_id].getTransaction();
    }

private:
    std::vector<TransactionQueue> transaction_queues_;  // transaction queues for workers
    std::vector<std::mutex> queue_mutexes_;  // mutexes for transaction queues
    Xoroshiro128Plus rnd_;  // random number generator
};

int main() {
    TransactionBalancer tb;
    tb.init(10);
    std::string test = "R({\"client_sessionID\":\"KJ9K4B\",\"timestamp_nsec\":154171933,\"timestamp_sec\":1704697050,\"transaction\":[{\"key\":\"hoge\",\"operation\":\"INSERT\",\"value\":\"fuga\"},{\"key\":\"piyo\",\"operation\":\"INSERT\",\"value\":\"pao\"}]})";
    tb.putTransaction(test);

    std::string result = tb.getTransaction(0);
    std::cout << result << std::endl;

    return 0;
}