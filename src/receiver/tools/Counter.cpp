#include <iostream>
#include <fstream>

#include "Counter.h"


Counter::Counter(const std::string& zsim_output_dir,
        const std::string& tool_name, const std::string& tool_config_path) :
        Tool(zsim_output_dir, tool_name, tool_config_path)
{

}

void
Counter::specific_parse_config_file()
{

}

void
Counter::access(const RequestPacket& req, ResponsePacket& res)
{
    switch (req.access_type) {
        case BRIDGE_PACKET_ACCESS_TYPE_GETS:
            ++n_GETS;
            break;
        case BRIDGE_PACKET_ACCESS_TYPE_GETX:
            ++n_GETX;
            break;
        case BRIDGE_PACKET_ACCESS_TYPE_PUTS:
            ++n_PUTS;
            break;
        case BRIDGE_PACKET_ACCESS_TYPE_PUTX:
            ++n_PUTX;
            break;
        default:
            break;
    }

    res.cycle = req.cycle;
}

void
Counter::dump_stats_text()
{
    std::ofstream ofs(stats_text_file_path, std::ios::out);

    std::cout << "GETS: " << n_GETS << std::endl;
    std::cout << "GETX: " << n_GETX << std::endl;
    std::cout << "PUTS: " << n_PUTS << std::endl;
    std::cout << "PUTX: " << n_PUTX << std::endl;

    ofs.close();
}

void
Counter::dump_stats_binary()
{
    std::ofstream ofs(stats_text_file_path, std::ios::out | std::ios::binary);

    // would dump binary stats here, if applicable

    ofs.close();
}
