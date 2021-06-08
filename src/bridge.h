/*
 * This class implements the "bridge" between the existing zsim codebase, and
 * the separate facilities for simulating memory-related functionality, such as
 * a write buffer, multi-node NUMA, etc.
 *
 * In essence, instead of attaching a memory controller to the last-level cache,
 * we instead attach the bridge interface to the last-level cache. Then, memory
 * accesses are sent over the bridge to the companion receiver (simulator)
 * process.
 *
 * This design gets around the many annoyances of Pin-injected processes (e.g.,
 * not being able to allocate memory directly using mmap() or, by extension,
 * malloc()). Packets sent by zsim over the bridge are ingested by the receiver
 * simulator process, where the project-specific functionality is simulated.
 *
 * In the future, it is intended that the functionality resident in the
 * receiver process can be straightforwardly ported to support a new (non-zsim)
 * frontend. This frontend would:
 *
 * 1. use a more flexible DBI scheme, perhaps based on DynamoRIO,
 * 2. implement core timing and coherent multi-level cache models, similar to
 *    zsim today, and
 * 3. contain advanced importance sampling functionality, enabling
 *    highly-efficient online simulation, as a novel research direction.
 */

#ifndef BRIDGE_H_
#define BRIDGE_H_

#include <stdint.h>

#include "g_std/g_string.h"
#include "memory_hierarchy.h"


class Bridge : public MemObject {
    public:
        Bridge(uint32_t lineSize, g_string& name);
        // NOTE: copy + move ctors + assignment operators *should be* deleted
        Bridge(const Bridge& b) = delete;
        Bridge& operator=(const Bridge& b) = delete;
        Bridge(Bridge&& b) = delete;
        Bridge& operator=(Bridge&& b) = delete;
        ~Bridge();

        uint64_t access(MemReq& req);

        const char* getName() { return name.c_str(); }
        uint32_t getLineSize() { return lineSize; }

    private:
        uint32_t lineSize;
        g_string name;
};



#endif  // BRIDGE_H_
