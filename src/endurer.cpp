#include "bithacks.h"
#include "endurer.h"


/*
 * Static variables.
 */
Endurer* EndurerController::endu = nullptr;



Endurer::Endurer(uint32_t line_size, uint32_t page_size) : line_size(line_size),
        page_size(page_size)
{
    futex_init(&lock);

    if (!isPow2(line_size) or !isPow2(page_size)) {
        panic("line size and page size must both be powers of two");
    }

    line_size_log2 = ilog2(line_size);
    page_size_log2 = ilog2(page_size);
}

Endurer::~Endurer()
{
}

inline Address
Endurer::line_addr_to_page_addr(Address line_addr)
{
    return line_addr >> (page_size_log2 - line_size_log2);
}

// TODO factor in base latency
void
Endurer::access(MemReq& req)
{
    futex_lock(&lock);

    Address line_addr = req.lineAddr;
    Address page_addr = line_addr_to_page_addr(line_addr);

    auto it = page_write_ctr.find(page_addr);
    if (it == page_write_ctr.end()) {
        page_write_ctr.emplace(page_addr, 1);
        page_order.emplace_back(page_addr);
    }
    else ++it->second;

    futex_unlock(&lock);
}


/*
 * Ctor and dtor.
 * NOTE: EndurerControllers are constructed serially, so we don't need to lock
 * around creating the backing Endurer object.
 */
EndurerController::EndurerController(uint32_t line_size, uint32_t page_size,
        g_string& name, MemObject* mem) : line_size(line_size),
        page_size(page_size), name(name), mem(mem)
{

    if (endu == nullptr) endu = new Endurer(line_size, page_size);
}

/*
 * The stock zsim code never cleans up its MemObjects (of which
 * EndurerController is a subtype), so this dtor is never called.
 */
EndurerController::~EndurerController()
{
}

/*
 * For now, just forward to the underlying mem. object.
 * TODO: wire up Endurer
 */
uint64_t
EndurerController::access(MemReq& req)
{
    // note it...
    endu->access(req);

    // ...and forward it
    return mem->access(req);
}

/*
 * Forwards the initStats() method to the underlying mem. object.
 */
void
EndurerController::initStats(AggregateStat* parentStat)
{
    mem->initStats(parentStat);
}
