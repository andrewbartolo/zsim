#include <assert.h>

#include <iostream>
#include <fstream>

#include "BufferTool.h"


BufferTool::BufferTool(const std::string& zsim_output_dir,
        const std::string& name, const std::string& config_path) :
        Tool(zsim_output_dir, name, config_path)
{
    // pull relevant parameters out of the config dict
    allocation_policy_t allocation_policy;
    eviction_policy_t eviction_policy;
    size_t line_size;
    size_t n_lines;
    size_t n_banks;
    size_t n_ways;

    try {
        if (config.at("ALLOCATION_POLICY") == "AORW")
                allocation_policy = BUFFER_ALLOCATION_POLICY_AORW;
        else if (config.at("ALLOCATION_POLICY") == "AOWO")
                allocation_policy = BUFFER_ALLOCATION_POLICY_AOWO;
        else assert(false);

        if (config.at("EVICTION_POLICY") == "LRU")
                eviction_policy = BUFFER_EVICTION_POLICY_LRU;
        else if (config.at("EVICTION_POLICY") == "RANDOM")
                eviction_policy = BUFFER_EVICTION_POLICY_RANDOM;
        else assert(false);

        line_size = std::stoul(config.at("LINE_SIZE"));
        n_lines = std::stoul(config.at("N_LINES"));
        n_banks = std::stoul(config.at("N_BANKS"));
        n_ways = std::stoul(config.at("N_WAYS"));

        access_latency = std::stoul(config.at("ACCESS_LATENCY"));
    }
    catch (...) {
        std::cerr << "ERROR: could not parse tool config file" << std::endl;
        assert(false);
    }

    // construct the Buffer object
    buffer = std::make_unique<Buffer>(allocation_policy, eviction_policy,
            line_size, n_lines, n_banks, n_ways);
}

void
BufferTool::access(const RequestPacket& req, ResponsePacket& res)
{

    access_type_t type;

    switch (req.access_type) {
        case BRIDGE_PACKET_ACCESS_TYPE_GETS:
            type = BUFFER_ACCESS_TYPE_RD;
            break;
        case BRIDGE_PACKET_ACCESS_TYPE_GETX:
            type = BUFFER_ACCESS_TYPE_RD;
            break;
        case BRIDGE_PACKET_ACCESS_TYPE_PUTS:
            // clean writebacks don't incur any cost
            res.latency = 0;
            res.do_forward = false;
            return;
        case BRIDGE_PACKET_ACCESS_TYPE_PUTX:
            type = BUFFER_ACCESS_TYPE_WR;
            break;
        default:
            assert(false);
            break;
    }

    line_addr_t line_addr = (line_addr_t) req.line_addr;
    bool was_hit;

    buffer->access(line_addr, type, &was_hit);

    res.latency = access_latency;
    res.do_forward = !was_hit;
}

void
BufferTool::dump_stats_text()
{
    std::cout << "dumping text stats..." << std::endl;

    buffer->aggregate_stats();
    buffer->print_stats();

    std::ofstream ofs(stats_text_file_path, std::ios::out);

    ofs.close();
}

void
BufferTool::dump_stats_binary()
{
    std::ofstream ofs(stats_text_file_path, std::ios::out | std::ios::binary);

    // would dump binary stats here, if applicable

    ofs.close();
}
