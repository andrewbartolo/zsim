#include "buffer.h"


/*
 * Ctor and dtor.
 */
BufferController::BufferController(uint32_t line_size, uint32_t n_lines,
        uint32_t n_ways, uint32_t n_banks, g_string& name, MemObject* mem) :
    line_size(line_size), n_lines(n_lines), n_ways(n_ways), n_banks(n_banks),
    name(name), mem(mem)
{
    // TODO
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
    return mem->access(req);
}

/*
 * Forwards the initStats() method to the underlying mem. object.
 */
void
BufferController::initStats(AggregateStat* parentStat)
{
    mem->initStats(parentStat);
}
