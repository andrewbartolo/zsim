/*
 * Bridge tool that implements the "online" portion of the ENDUReR simulation.
 * This simply involves building a histogram of counts of writes to pages.
 * Later, this histogram is fed into the offline ENDUReR simulator, so that
 * a memory lifetime can be established.
 */

#ifndef HISTOGRAM_H_
#define HISTOGRAM_H_

#include <stdint.h>
#include <unistd.h>

#include <fstream>
#include <functional>

#include "g_std/g_string.h"
#include "g_std/g_unordered_map.h"
#include "g_std/g_vector.h"
#include "locks.h"
#include "memory_hierarchy.h"
#include "stats.h"

class Histogram : public GlobAlloc {
    public:
        Histogram(uint32_t line_size, uint32_t page_size);
        Histogram(const Histogram& e) = delete;
        Histogram& operator=(const Histogram& e) = delete;
        Histogram(Histogram&& e) = delete;
        Histogram& operator=(Histogram&& e) = delete;
        ~Histogram();

        void access(MemReq& req);

        void write_record(std::ofstream& ofs);

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


class HistogramController : public MemObject {
    public:
        HistogramController(uint32_t line_size, uint32_t page_size, g_string&
                name, MemObject* mem);
        // NOTE: copy + move ctors + assignment operators *should be* deleted
        HistogramController(const HistogramController& ec) = delete;
        HistogramController& operator=(const HistogramController& ec) = delete;
        HistogramController(HistogramController&& ec) = delete;
        HistogramController& operator=(HistogramController&& ec) = delete;
        ~HistogramController();

        void initStats(AggregateStat* parentStat);
        uint64_t access(MemReq& req);

        const char* getName() { return name.c_str(); }
        uint32_t getLineSize() { return line_size; }

    private:
        // name of the hooked MemObject (not the Histogram it links to)
        g_string name;
        uint32_t line_size;
        MemObject* mem = nullptr;

    // static fields + methods
    private:
        static Histogram* histogram;
};


#endif  // HISTOGRAM_H_
