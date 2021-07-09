#include <algorithm>

#include "buffer.h"
#include "zsim.h"

/*
 * Static variables.
 */
Buffer* BufferController::buffer = nullptr;


Buffer::Buffer(uint32_t line_size, g_string& allocation_policy, g_string&
        eviction_policy, uint32_t n_bytes, uint32_t n_ways, uint32_t n_banks,
        uint32_t latency) : line_size(line_size), n_bytes(n_bytes),
        n_ways(n_ways), n_banks(n_banks)
{
    futex_init(&lock);

    // convert policy strings to canonical (lower) case
    std::transform(allocation_policy.begin(), allocation_policy.end(),
            allocation_policy.begin(), ::tolower);
    std::transform(eviction_policy.begin(), eviction_policy.end(),
            eviction_policy.begin(), ::tolower);

    if (allocation_policy == "aorw")
            this->allocation_policy = ALLOCATION_POLICY_AORW;
    else if (allocation_policy == "aowo")
            this->allocation_policy = ALLOCATION_POLICY_AOWO;
    else panic("allocation policy must be one of 'aorw' or 'aowo'");

    if (eviction_policy == "lru")
            this->eviction_policy = EVICTION_POLICY_LRU;
    else if (eviction_policy == "random")
            this->eviction_policy = EVICTION_POLICY_RANDOM;
    else panic("eviction policy must be one of 'lru' or 'random'");


    if (n_bytes % line_size != 0)
            panic("size in bytes must be a multiple of line size");
    n_lines = n_bytes / line_size;

    uint32_t n_lines_per_bank = n_lines / n_banks;
    uint32_t n_sets_per_bank = n_lines_per_bank / n_ways;

    // construct Banks
    for (uint32_t i = 0; i < n_banks; ++i) {
        uint32_t bank_gid = i;
        // still invokes move constructor
        banks.emplace_back(bank_gid, this->allocation_policy,
                this->eviction_policy, n_sets_per_bank, n_ways);
    }
}

Buffer::~Buffer()
{
    delete stats;
}

void
Buffer::init_stats()
{
    if (stats == nullptr) {
        stats = new AggregateStat(true);

        stats->init("buffer", "Write buffer stats");

        n_read_hits.init("RDh", "Read hits"); stats->append(&n_read_hits);
        n_write_hits.init("WRh", "Write hits"); stats->append(&n_write_hits);
        n_read_misses.init("RDm", "Read misses"); stats->append(&n_read_misses);
        n_write_misses.init("WRm", "Write misses");
                stats->append(&n_write_misses);
        n_evictions.init("ev", "Evictions"); stats->append(&n_evictions);

        zinfo->rootStat->append(stats);
    }
}

uint64_t
Buffer::access(MemReq& req)
{
    // translate some fields into their Buffer internal representations
    Address line_addr = req.lineAddr;
    access_type_t type;
    if (req.type == GETS or req.type == GETX) type = ACCESS_TYPE_RD;
    if (req.type == PUTS) return 0;
    if (req.type == PUTX) type = ACCESS_TYPE_WR;


    futex_lock(&lock);

    uint32_t bank_idx = fast_hash(line_addr, n_banks);
    event_t res = banks[bank_idx].access(line_addr, type);

    if (res & EVENT_RD_HIT) n_read_hits.inc();
    if (res & EVENT_WR_HIT) n_write_hits.inc();
    if (res & EVENT_RD_MISS) n_read_misses.inc();
    if (res & EVENT_WR_MISS) n_write_misses.inc();
    if (res & EVENT_EVICTION) n_evictions.inc();

    futex_unlock(&lock);

    return latency;
}

inline uint32_t
Buffer::fast_hash(uint64_t line_addr, uint64_t modulo)
{
    uint32_t res = 0;
    uint64_t tmp = line_addr;
    for (uint32_t i = 0; i < 4; ++i) {
        res ^= (uint32_t) ( ((uint64_t) 0xffff) & tmp);
        tmp = tmp >> 16;
    }
    return res % modulo;
}


Bank::Bank(uint32_t gid, Buffer::allocation_policy_t allocation_policy,
        Buffer::eviction_policy_t eviction_policy, uint32_t n_sets_per_bank,
        uint32_t n_ways) : gid(gid), n_sets(n_sets_per_bank), n_ways(n_ways)
{
    for (uint32_t i = 0; i < n_sets; ++i) {
        uint32_t set_gid = gid * n_sets + i;
        // still invokes move constructor
        sets.emplace_back(set_gid, allocation_policy, eviction_policy, n_ways);
    }

}

Bank::~Bank()
{
}

Buffer::event_t
Bank::access(Address line_addr, Buffer::access_type_t type)
{
    // look up the correct Set within the Bank
    uint32_t set_idx = line_addr % n_sets;

    return sets[set_idx].access(line_addr, type);
}

Set::Set(uint32_t gid, Buffer::allocation_policy_t allocation_policy,
        Buffer::eviction_policy_t eviction_policy, uint32_t n_ways) :
        gid(gid), allocation_policy(allocation_policy),
        eviction_policy(eviction_policy), n_ways(n_ways), rand_gen(gid),
        rand_dist(0, n_ways - 1)
{
    if (eviction_policy == Buffer::EVICTION_POLICY_LRU) {
        // reserve some space in the vector
        rand_vec.reserve(n_ways);
    }
    else if (eviction_policy == Buffer::EVICTION_POLICY_RANDOM) {
        // RAII: already initialized in initializer list
    }
    else { }

    n_ways_active = 0;
}

Set::~Set()
{
}


Buffer::event_t
Set::access(Address line_addr, Buffer::access_type_t type)
{
    Buffer::event_t ret = 0;

    if (eviction_policy == Buffer::EVICTION_POLICY_LRU) {
        // is it a hit?
        auto it = lru_map.find(line_addr);
        if (it != lru_map.end()) {
            // it was a hit
            if (type == Buffer::ACCESS_TYPE_RD)
                    ret |= Buffer::EVENT_RD_HIT;
            else if (type == Buffer::ACCESS_TYPE_WR)
                    ret |= Buffer::EVENT_WR_HIT;

            // reset the last-used time by removing and appending
            lru_list.erase(it->second);
            auto new_it = lru_list.emplace(lru_list.end(), line_addr);
            lru_map[line_addr] = new_it;
        }
        else {
            // it was a miss
            if (type == Buffer::ACCESS_TYPE_RD)
                    ret |= Buffer::EVENT_RD_MISS;
            else if (type == Buffer::ACCESS_TYPE_WR)
                    ret |= Buffer::EVENT_WR_MISS;

            // based on our allocation policy, do we want to allocate or not?
            bool do_allocate = (type == Buffer::ACCESS_TYPE_WR) or
                    ((type == Buffer::ACCESS_TYPE_RD) and (allocation_policy ==
                    Buffer::ALLOCATION_POLICY_AORW));

            if (do_allocate) {
                if (n_ways_active == n_ways) {
                    // need to evict least-recent element (map erase first!)
                    auto to_evict = lru_list.begin();
                    lru_map.erase(*to_evict);
                    lru_list.erase(to_evict);

                    // insert the new element
                    auto new_it = lru_list.emplace(lru_list.end(), line_addr);
                    lru_map[line_addr] = new_it;

                    ret |= Buffer::EVENT_EVICTION;
                }
                else {
                    // don't need to evict
                    // insert the new element
                    auto new_it = lru_list.emplace(lru_list.end(), line_addr);
                    lru_map[line_addr] = new_it;

                    ++n_ways_active;
                }
            }
        }
    }

    else if (eviction_policy == Buffer::EVICTION_POLICY_RANDOM) {
        // is it a hit?
        auto it = rand_set.find(line_addr);
        if (it != rand_set.end()) {
            // it was a hit
            if (type == Buffer::ACCESS_TYPE_RD)
                    ret |= Buffer::EVENT_RD_HIT;
            else if (type == Buffer::ACCESS_TYPE_WR)
                    ret |= Buffer::EVENT_WR_HIT;
        }
        else {
            // it was a miss
            if (type == Buffer::ACCESS_TYPE_RD)
                    ret |= Buffer::EVENT_RD_MISS;
            else if (type == Buffer::ACCESS_TYPE_WR)
                    ret |= Buffer::EVENT_WR_MISS;

            // based on our allocation policy, do we want to allocate or not?
            bool do_allocate = (type == Buffer::ACCESS_TYPE_WR) or
                    ((type == Buffer::ACCESS_TYPE_RD) and (allocation_policy ==
                    Buffer::ALLOCATION_POLICY_AORW));

            if (do_allocate) {
                if (n_ways_active == n_ways) {
                    // choose a random element to evict
                    uint32_t vec_idx = rand_dist(rand_gen);
                    Address to_evict = rand_vec[vec_idx];
                    rand_vec[vec_idx] = line_addr;

                    rand_set.erase(to_evict);
                    rand_set.emplace(line_addr);

                    ret |= Buffer::EVENT_EVICTION;
                }
                else {
                    // append to the vector
                    rand_vec.emplace_back(line_addr);
                    rand_set.emplace(line_addr);

                    ++n_ways_active;
                }
            }
        }
    }

    else { }

    return ret;
}




/*
 * Ctor and dtor.
 * NOTE: BufferControllers are constructed serially, so we don't need to lock
 * around creating the backing Buffer object.
 */
BufferController::BufferController(uint32_t line_size, g_string&
        allocation_policy, g_string& eviction_policy, uint32_t n_bytes,
        uint32_t n_ways, uint32_t n_banks, uint32_t latency, g_string& name,
        MemObject* mem) : name(name), line_size(line_size), mem(mem)
{
    if (buffer == nullptr) buffer = new Buffer(line_size, allocation_policy,
            eviction_policy, n_bytes, n_ways, n_banks, latency);
}

/*
 * The stock zsim code never cleans up its MemObjects (of which BufferController
 * is a subtype), so this dtor is never called.
 */
BufferController::~BufferController()
{
}

/*
 * For now, just forward to the underlying mem. object.
 * TODO: wire up Buffer
 */
uint64_t
BufferController::access(MemReq& req)
{
    uint64_t buffer_latency = buffer->access(req);
    uint64_t fwd_latency = mem->access(req);

    return buffer_latency + fwd_latency;
}

/*
 * Forwards the initStats() method to the underlying mem. object.
 */
void
BufferController::initStats(AggregateStat* parentStat)
{
    buffer->init_stats();
    mem->initStats(parentStat);
}
