#include "endurer.h"


/*
 * Ctor and dtor.
 */
EndurerController::EndurerController(uint32_t line_size, uint32_t page_size,
        g_string& name, MemObject* mem) : line_size(line_size),
        page_size(page_size), name(name), mem(mem)
{
    printf("EndurerController constructed\n");
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
