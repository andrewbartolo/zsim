/*
 * Bridge tool that implements the "online" portion of the ENDUReR simulation.
 * This simply involves building a histogram of counts of writes to pages.
 * Later, this histogram is fed into the offline ENDUReR simulator, so that
 * a memory lifetime can be established.
 */

#ifndef ENDURER_H_
#define ENDURER_H_

#include <stdint.h>
#include <unistd.h>

#include "g_std/g_string.h"
#include "g_std/g_unordered_map.h"
#include "g_std/g_vector.h"
#include "locks.h"
#include "memory_hierarchy.h"
#include "stats.h"

class Endurer : public GlobAlloc {
    public:
        Endurer(uint32_t line_size, uint32_t page_size);
        Endurer(const Endurer& e) = delete;
        Endurer& operator=(const Endurer& e) = delete;
        Endurer(Endurer&& e) = delete;
        Endurer& operator=(Endurer&& e) = delete;
        ~Endurer();


        // TODO add base latency
        void access(MemReq& req);

    private:
        inline Address line_addr_to_page_addr(Address line_addr);

        uint32_t line_size;
        uint32_t page_size;
        uint32_t line_size_log2;
        uint32_t page_size_log2;

        lock_t lock;

        g_unordered_map<Address, uint64_t> page_write_ctr;
        // not strictly necessary, but keep track of the order in which we see
        // pages, just because we can.
        g_vector<Address> page_order;
};


class EndurerController : public MemObject {
    public:
        EndurerController(uint32_t line_size, uint32_t page_size, g_string&
                name, MemObject* mem);
        // NOTE: copy + move ctors + assignment operators *should be* deleted
        EndurerController(const EndurerController& ec) = delete;
        EndurerController& operator=(const EndurerController& ec) = delete;
        EndurerController(EndurerController&& ec) = delete;
        EndurerController& operator=(EndurerController&& ec) = delete;
        ~EndurerController();

        void initStats(AggregateStat* parentStat);
        uint64_t access(MemReq& req);

        const char* getName() { return name.c_str(); }
        uint32_t getLineSize() { return line_size; }

    private:
        uint32_t line_size;
        uint32_t page_size;
        g_string name;

        MemObject* mem = nullptr;

    // static fields + methods
    private:
        static Endurer* endu;
};


#endif  // ENDURER_H_
