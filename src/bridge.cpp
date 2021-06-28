#include "bridge.h"


/*
 * Ctor and dtor.
 */
Bridge::Bridge(uint32_t line_size, g_string& name, g_string& type,
        MemObject* mem) : line_size(line_size), name(name), type(type), mem(mem)
{
    printf("created bridge w/tool %s\n", type.c_str());
}

/*
 * The stock zsim code never cleans up its MemObjects (of which Bridge is a
 * subtype), so this dtor is never called.
 */
Bridge::~Bridge()
{
}

/*
 * For now, just forward to the underlying mem. object.
 */
uint64_t
Bridge::access(MemReq& req)
{
    return mem->access(req);
}

/*
 * Forwards the initStats() method to the underlying mem. object.
 */
void
Bridge::initStats(AggregateStat* parentStat)
{
    mem->initStats(parentStat);
}
