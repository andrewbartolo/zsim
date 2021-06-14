#include "Tool.h"


/*
 * Ctor.
 */
Tool::Tool(const std::string& zsim_output_dir, const std::string& tool_name,
        const std::string& tool_config_path) : zsim_output_dir(zsim_output_dir), 
        tool_name(tool_name), tool_config_path(tool_config_path)
{
    stats_text_file_path = zsim_output_dir + "/" + tool_name + "-stats.txt";
    stats_binary_file_path = zsim_output_dir + "/" + tool_name + "-stats.bin";

    generic_parse_config_file();
}

/*
 * Simply parses into a <string, string> dictionary. The Tool subclasses
 * themselves then take (and typecast) what they need out of this dictionary.
 */
void
Tool::generic_parse_config_file()
{
    // TODO: read line-by-line
}
