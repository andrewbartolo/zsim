/*
 * Simple counter that demonstrates the mechanics of processing accesses.
 */
#pragma once

#include <stdint.h>

#include "Tool.h"


class Counter : public Tool {
    public:
        Counter(const std::string& zsim_output_dir,
                const std::string& tool_name,
                const std::string& tool_config_path);
        Counter(const Counter& c) = delete;
        Counter& operator=(const Counter& c) = delete;
        Counter(Counter&& c) = delete;
        Counter& operator=(Counter&& c) = delete;

        // Tool virtual methods implemented here
        void specific_parse_config_file();
        void access(const RequestPacket& req, ResponsePacket& res);
        void dump_stats_text();
        void dump_stats_binary();

    private:
        uint64_t n_GETS = 0;
        uint64_t n_GETX = 0;
        uint64_t n_PUTS = 0;
        uint64_t n_PUTX = 0;
};
