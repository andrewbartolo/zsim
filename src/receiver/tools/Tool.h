/*
 * Generic superclass for all Tool types (Counter, etc.)
 */
#pragma once

#include <string>
#include <unordered_map>

#include "../../bridge_packet.h"


class Tool {
    public:
        Tool(const std::string& zsim_output_dir, const std::string& tool_name,
                const std::string& tool_config_path);
        Tool(const Tool& t) = delete;
        Tool& operator=(const Tool& t) = delete;
        Tool(Tool&& t) = delete;
        Tool& operator=(Tool&& t) = delete;

        // pure virtual functions, must be implemented by Tool subclasses
        virtual void specific_parse_config_file() = 0;
        virtual void access(const RequestPacket& req, ResponsePacket& res) = 0;
        virtual void dump_stats_text() = 0;
        virtual void dump_stats_binary() = 0;

    protected:
        std::string zsim_output_dir;
        std::string tool_name;
        std::string tool_config_path;
        std::string stats_text_file_path;
        std::string stats_binary_file_path;
        std::unordered_map<std::string, std::string> tool_config;

    private:
        void generic_parse_config_file();
};
