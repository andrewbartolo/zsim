#include <assert.h>

#include <iostream>
#include <fstream>
#include <sstream>

#include "Tool.h"


/*
 * Ctor.
 */
Tool::Tool(const std::string& zsim_output_dir, const std::string& name,
        const std::string& config_path) : zsim_output_dir(zsim_output_dir),
        name(name), config_path(config_path)
{
    stats_text_file_path = zsim_output_dir + "/" + name + "-stats.txt";
    stats_binary_file_path = zsim_output_dir + "/" + name + "-stats.bin";

    parse_config_file();
}

/*
 * Simply parses into a <string, string> dictionary. The Tool subclasses
 * themselves then take (and typecast) what they need out of this dictionary.
 */
void
Tool::parse_config_file()
{
    std::ifstream ifs(config_path);
    assert(ifs);

    std::string line;
    while (std::getline(ifs, line)) {
        // tokenize
        auto iss = std::istringstream(line);
        std::string k, v;
        iss >> k;
        iss >> v;
        config[k] = v;
    }
}
