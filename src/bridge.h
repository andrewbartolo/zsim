/*
 * This class implements the "bridge" between the existing zsim codebase, and
 * the separate facilities for simulating memory-related functionality, such as
 * a write buffer, multi-node NUMA, etc.
 *
 * In essence, instead of attaching a memory controller to the last-level cache,
 * we instead attach the bridge interface to the last-level cache. Then, memory
 * accesses are sent over the bridge to the appropriate tool.
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

#ifndef BRIDGE_H_
#define BRIDGE_H_

#include <stdint.h>
#include <unistd.h>

#include "g_std/g_string.h"
#include "memory_hierarchy.h"
#include "stats.h"


class Bridge : public MemObject {
    public:
        Bridge(uint32_t line_size, g_string& name, g_string& type,
                MemObject* mem);
        // NOTE: copy + move ctors + assignment operators *should be* deleted
        Bridge(const Bridge& b) = delete;
        Bridge& operator=(const Bridge& b) = delete;
        Bridge(Bridge&& b) = delete;
        Bridge& operator=(Bridge&& b) = delete;
        ~Bridge();


        void initStats(AggregateStat* parentStat);
        uint64_t access(MemReq& req);

        const char* getName() { return name.c_str(); }
        uint32_t getLineSize() { return line_size; }


    private:
        uint32_t line_size;
        g_string name;
        g_string type;

        MemObject* mem;
};


#endif  // BRIDGE_H_
