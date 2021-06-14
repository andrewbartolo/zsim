/*
 * Receiver objects read packets from the bridge socket and pass them to higher-
 * level simulation functionality.
 *
 * Receiver is a "static class": we use it as a singleton.
 */
#pragma once

#include <stdint.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include <string>

#include "tools/Tool.h"


class Receiver {
    public:
        static void init(const char* const zsim_output_dir,
                const char* const tool_name, const char* const
                tool_config_path);
        static void run();

    private:
        static void establish_signal_handler();
        static void establish_socket();
        static void finish();
        static void signal_handler(int signum, siginfo_t* info, void* ucxt);

        static std::string zsim_output_dir;
        static std::string bridge_sock_path;
        static int bridge_sock_fd;
        static struct sockaddr_un bridge_sock_addr;
        static size_t bridge_sock_addr_len;
        static bool finalized;

        static Tool* tool;
};

