/*
 * Simple counter that demonstrates the mechanics of processing accesses.
 */
#pragma once

#include <stdint.h>

#include <memory>

#include "Tool.h"
#include "basicbuffer/Buffer.h"


class BufferTool : public Tool {
    public:
        BufferTool(const std::string& zsim_output_dir, const std::string& name,
                const std::string& config_path);
        BufferTool(const BufferTool& b) = delete;
        BufferTool& operator=(const BufferTool& b) = delete;
        BufferTool(BufferTool&& b) = delete;
        BufferTool& operator=(BufferTool&& b) = delete;

        // Tool virtual methods implemented here
        void access(const RequestPacket& req, ResponsePacket& res);
        void dump_stats_text();
        void dump_stats_binary();

    private:
        // property of BufferTool and not Buffer itself
        uint64_t access_latency;

        std::unique_ptr<Buffer> buffer;
};
