#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "../bridge_packet.h"
#include "Receiver.h"
#include "tools/Tool.h"
#include "tools/Counter.h"

// maximum struct sockaddr_un path length
#define SUN_PATH_MAX 108

/*
 * Static variable definitions.
 */
std::string Receiver::zsim_output_dir;
std::string Receiver::bridge_sock_path;
int Receiver::bridge_sock_fd;
struct sockaddr_un Receiver::bridge_sock_addr;
size_t Receiver::bridge_sock_addr_len;
bool Receiver::finalized = false;
Tool* Receiver::tool = nullptr;


int
main(int argc, char* argv[])
{
    assert(argc == 4);
    char* const receiver_bin_path = argv[0];
    char* const zsim_output_dir = argv[1];
    char* const tool_name = argv[2];
    char* const tool_config_path = argv[3];

    printf("initiating receiver:\n");
    printf("\ttool: %s\n", tool_name);
    printf("\ttool config: %s\n", tool_config_path);

    Receiver::init(zsim_output_dir, tool_name, tool_config_path);
    Receiver::run();

    return 0;
}

/*
 * Pseudo-constructor for the static Receiver class.
 */
void
Receiver::init(const char* const zsim_output_dir, const char* const tool_name,
        const char* const tool_config_path)
{
    Receiver::zsim_output_dir = zsim_output_dir;
    // by convention, the sock file is "bridge.sock" in the output dir
    Receiver::bridge_sock_path = Receiver::zsim_output_dir + "/bridge.sock";

    establish_signal_handler();
    establish_socket();

    // construct the appropriate form of tool
    if      (std::string(tool_name) == "Counter") {
        tool = new Counter(zsim_output_dir, tool_name, tool_config_path);
    }
    else assert(false);
}

/*
 * The main body of the Receiver module. Listens for packets, processes them,
 * and replies to the bridge.
 */
void
Receiver::run()
{
    RequestPacket req;
    ResponsePacket res;

    size_t n_pkts = 0;
    while (true) {
        ssize_t ret = recv(bridge_sock_fd, &req, sizeof(req), 0);
        assert(ret == (ssize_t) sizeof(req));
        tool->access(req, res);
        ++n_pkts;
    }
}

void
Receiver::establish_signal_handler()
{
    // set up a signal handler to handle termination signal
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = signal_handler;

    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
}

void
Receiver::establish_socket()
{
    assert(bridge_sock_path != "");

    assert((bridge_sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) != -1);

    memset(&bridge_sock_addr, 0, sizeof(bridge_sock_addr));
    bridge_sock_addr.sun_family = AF_UNIX;

    assert(bridge_sock_path.length() < SUN_PATH_MAX);
    strncpy(bridge_sock_addr.sun_path, bridge_sock_path.c_str(), SUN_PATH_MAX);
    bridge_sock_addr_len = SUN_LEN(&bridge_sock_addr);

    assert(bind(bridge_sock_fd, (struct sockaddr*) &bridge_sock_addr,
                bridge_sock_addr_len) == 0);
}

/*
 *  Finish and dump stats.
 */
void
Receiver::finish()
{
    tool->dump_stats_text();
    tool->dump_stats_binary();

    printf("Receiver exiting.\n");
    exit(0);
}

/*
 * signal_handler trampolines into the appropriate function based on the signal.
 */
void
Receiver::signal_handler(int signum, siginfo_t* info, void* ucxt)
{
    if (finalized) return;
    finalized = true;

    if (signum == SIGTERM) {
        printf("Receiver saw zsim finish cleanly. Finalizing...\n");
    }
    else if (signum == SIGHUP) {
        printf("Receiver saw zsim finish unexpectedly! Finalizing...\n");
    }

    finish();
}
