#include "bithacks.h"
#include "histogram.h"
#include "record_writer.h"
#include "zsim.h"


/*
 * Static variables.
 */
Histogram* HistogramController::histogram = nullptr;



Histogram::Histogram(uint32_t line_size, uint32_t page_size) :
        line_size(line_size), page_size(page_size)
{
    futex_init(&lock);

    if (!isPow2(line_size) or !isPow2(page_size)) {
        panic("line size and page size must both be powers of two");
    }

    line_size_log2 = ilog2(line_size);
    page_size_log2 = ilog2(page_size);

    // register the RecordWriter so that records can be written upon sim. end
    auto bound_write_record =
            std::bind(&Histogram::write_record, this, std::placeholders::_1);
    RecordWriter* rw = new RecordWriter("histogram", bound_write_record);
    zinfo->recordWriters->push_back(rw);
}

Histogram::~Histogram()
{
}

inline Address
Histogram::line_addr_to_page_addr(Address line_addr)
{
    return line_addr >> (page_size_log2 - line_size_log2);
}

// TODO factor in base latency
void
Histogram::access(MemReq& req)
{
    // if it's not a PUTX (dirty writeback), we don't care about it
    if (req.type != PUTX) return;

    Address line_addr = req.lineAddr;
    Address page_addr = line_addr_to_page_addr(line_addr);


    futex_lock(&lock);

    auto it = page_write_ctr.find(page_addr);
    if (it == page_write_ctr.end()) {
        page_write_ctr.emplace(page_addr, 1);
        page_order.emplace_back(page_addr);
    }
    else ++it->second;

    futex_unlock(&lock);
}

// callback that is passed to RecordWriter to be called at sim. end
void
Histogram::write_record(std::ofstream& ofs)
{
    /*
     * output file is of the format:
     * <n_writes>
     * <n_writes>
     * ...
     *
     * in the order that the page was first seen in the program's execution.
     */

    for (Address a : page_order) {
        auto& count = page_write_ctr.at(a);
        ofs.write((char*) &count, sizeof(count));
    }
}



/*
 * Ctor and dtor.
 * NOTE: HistogramControllers are constructed serially, so we don't need to lock
 * around creating the backing Histogram object.
 */
HistogramController::HistogramController(uint32_t line_size, uint32_t page_size,
        g_string& name, MemObject* mem) : name(name), line_size(line_size),
        mem(mem)
{
    if (histogram == nullptr) histogram = new Histogram(line_size, page_size);
}

/*
 * The stock zsim code never cleans up its MemObjects (of which
 * HistogramController is a subtype), so this dtor is never called.
 */
HistogramController::~HistogramController()
{
}

/*
 * For now, just forward to the underlying mem. object.
 * TODO: wire up Histogram
 */
uint64_t
HistogramController::access(MemReq& req)
{
    // record it. NOTE: histogram always has zero access latency
    // (we analytically apply latency later on)
    histogram->access(req);

    // ...and forward it
    return mem->access(req);
}

/*
 * Forwards the initStats() method to the underlying mem. object.
 */
void
HistogramController::initStats(AggregateStat* parentStat)
{
    // histogram has no zsim-style stats of its own, only a RecordWriter

    mem->initStats(parentStat);
}
