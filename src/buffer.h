/*
 * This class implements a (write) buffer. The BufferController objects attach
 * to the existing memory controller (in init.cpp, in BuildMemoryController()),
 * and then all accesses are channeled through the Buffer objects. In this way,
 * BufferController (and Buffer) serve as an interposer that comes between the
 * LLC and the memory controller.
 *
 * NOTE: the first pass at a bridge implementation used separate processes
 * communicating over UNIX domain sockets. However, the one-packet-per-
 * send()/recv() incurred a slowdown of ~10X over zsim's existing single-process
 * LLC sim. So, going forward, we do all simulation within the zsim process
 * itself.
 *
 * In the future, it is intended that the functionality resident in the
 * bridge tools can be straightforwardly ported to support a new (non-zsim)
 * frontend. This frontend would:
 *
 * 1. use a more flexible DBI scheme, perhaps based on DynamoRIO,
 * 2. implement core timing and coherent multi-level cache models, similar to
 *    zsim today, and
 * 3. contain advanced importance sampling functionality, enabling
 *    highly-efficient online simulation, as a novel research direction.
 * 4. have support for cluster-level simulation, again, as a novel direction.
 */

#ifndef BUFFER_H_
#define BUFFER_H_

#include <stdint.h>
#include <unistd.h>

// TODO may not be zsim-multi-process compatible
#include <random>

#include "g_std/g_list.h"
#include "g_std/g_string.h"
#include "g_std/g_unordered_map.h"
#include "g_std/g_unordered_set.h"
#include "g_std/g_vector.h"
#include "memory_hierarchy.h"
#include "stats.h"

// some forward declarations
class Buffer;
class Set;
class Bank;


class Buffer : public GlobAlloc {
    public:
        Buffer(uint32_t line_size, g_string& allocation_policy, g_string&
                eviction_policy, uint32_t n_bytes, uint32_t n_ways, uint32_t
                n_banks, uint32_t latency);
        // NOTE: copy + move ctors + assignment operators *should be* deleted
        Buffer(const Buffer& b) = delete;
        Buffer& operator=(const Buffer& b) = delete;
        Buffer(Buffer&& b) = delete;
        Buffer& operator=(Buffer&& b) = delete;
        ~Buffer();

        void init_stats();
        uint64_t access(MemReq& req);
        uint32_t fast_hash(uint64_t line_addr, uint64_t modulo);


    private:
        typedef enum {
            ALLOCATION_POLICY_AORW,
            ALLOCATION_POLICY_AOWO,
        } allocation_policy_t;

        typedef enum {
            EVICTION_POLICY_LRU,
            EVICTION_POLICY_RANDOM,
        } eviction_policy_t;

        typedef enum {
            ACCESS_TYPE_RD,
            ACCESS_TYPE_WR,
        } access_type_t;

        // we can have compount events (e.g., an eviction on a miss), so use
        // a bitset.
        typedef int event_t;
        static constexpr event_t EVENT_RD_HIT =    (1 << 0);
        static constexpr event_t EVENT_WR_HIT =    (1 << 1);
        static constexpr event_t EVENT_RD_MISS =   (1 << 2);
        static constexpr event_t EVENT_WR_MISS =   (1 << 3);
        static constexpr event_t EVENT_EVICTION =  (1 << 4);

        // input and derived parameters
        uint32_t line_size;
        allocation_policy_t allocation_policy;
        eviction_policy_t eviction_policy;
        uint32_t n_bytes;
        uint32_t n_lines;
        uint32_t n_ways;
        uint32_t n_banks;
        uint32_t latency;

        g_vector<Bank> banks;

        // statistics
        AggregateStat* stats;

        Counter n_read_hits;
        Counter n_write_hits;
        Counter n_read_misses;
        Counter n_write_misses;
        Counter n_evictions;

        lock_t lock;

        friend class Bank;
        friend class Set;
};

class Bank {
    public:
        Bank(uint32_t gid, Buffer::allocation_policy_t allocation_policy,
                Buffer::eviction_policy_t eviction_policy, uint32_t
                n_sets_per_bank, uint32_t n_ways);
        ~Bank();

        Buffer::event_t access(Address line_addr, Buffer::access_type_t type);

    private:
        uint32_t gid;
        uint32_t n_sets;
        uint32_t n_ways;

        g_vector<Set> sets;
};

class Set {
    public:
        Set(uint32_t gid, Buffer::allocation_policy_t allocation_policy,
                Buffer::eviction_policy_t eviction_policy, uint32_t n_ways);
        ~Set();

        Buffer::event_t access(Address line_addr, Buffer::access_type_t type);

    private:
        uint32_t gid;
        Buffer::allocation_policy_t allocation_policy;
        Buffer::eviction_policy_t eviction_policy;
        uint32_t n_ways;

        // mechanics
        g_list<Address> lru_list;
        g_unordered_map<Address, g_list<Address>::iterator> lru_map;

        g_vector<Address> rand_vec;
        g_unordered_set<Address> rand_set;
        // TODO is std::mt19937 multi-process compatible?
        std::mt19937 rand_gen;
        std::uniform_int_distribution<uint32_t> rand_dist;

        uint32_t n_ways_active = 0;
};


class BufferController : public MemObject {
    public:
        BufferController(uint32_t line_size, g_string& allocation_policy,
                g_string& eviction_policy, uint32_t n_bytes, uint32_t n_ways,
                uint32_t n_banks, uint32_t latency, g_string& name,
                MemObject* mem);
        // NOTE: copy + move ctors + assignment operators *should be* deleted
        BufferController(const BufferController& bc) = delete;
        BufferController& operator=(const BufferController& bc) = delete;
        BufferController(BufferController&& bc) = delete;
        BufferController& operator=(BufferController&& bc) = delete;
        ~BufferController();


        void initStats(AggregateStat* parentStat);
        uint64_t access(MemReq& req);

        const char* getName() { return name.c_str(); }
        uint32_t getLineSize() { return line_size; }


    private:
        // name of the hooked MemObject (not the Buffer it links to)
        g_string name;
        uint32_t line_size;
        MemObject* mem = nullptr;

    // static fields + methods
    private:
        static Buffer* buffer;
};


#endif  // BUFFER_H_
