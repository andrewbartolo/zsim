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

#include "g_std/g_string.h"
#include "memory_hierarchy.h"
#include "stats.h"


class BufferController : public MemObject {
    public:
        BufferController(uint32_t line_size, uint32_t n_lines, uint32_t n_ways,
                uint32_t n_banks, g_string& name, MemObject* mem);
        // NOTE: copy + move ctors + assignment operators *should be* deleted
        BufferController(const BufferController& b) = delete;
        BufferController& operator=(const BufferController& b) = delete;
        BufferController(BufferController&& b) = delete;
        BufferController& operator=(BufferController&& b) = delete;
        ~BufferController();


        void initStats(AggregateStat* parentStat);
        uint64_t access(MemReq& req);

        const char* getName() { return name.c_str(); }
        uint32_t getLineSize() { return line_size; }


    private:
        uint32_t line_size;
        uint32_t n_lines;
        uint32_t n_ways;
        uint32_t n_banks;
        g_string name;

        MemObject* mem = nullptr;
};


#endif  // BUFFER_H_
