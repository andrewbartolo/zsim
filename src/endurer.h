/*
 * Bridge tool that implements the ENDUReR module functionality.
 */

#ifndef ENDURER_H_
#define ENDURER_H_

#include <stdint.h>
#include <unistd.h>

#include "g_std/g_string.h"
#include "memory_hierarchy.h"
#include "stats.h"


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
};


#endif  // ENDURER_H_
