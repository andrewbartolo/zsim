#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>

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
socklen_t Receiver::bridge_sock_addr_len;
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
    struct sockaddr_un req_addr;
    socklen_t req_addr_len;
    ResponsePacket res;

    memset(&req, 0, sizeof(req));
    memset(&req_addr, 0, sizeof(req_addr));
    req_addr_len = 0;
    memset(&res, 0, sizeof(res));

    size_t n_pkts = 0;
    while (true) {
        receive_packet(bridge_sock_fd, &req_addr, &req_addr_len, &req);

        if (req.status == BRIDGE_PACKET_STATUS_TERM) finish();
        else tool->access(req, res);

        // reply to the same address we received the Request from
        res = { 0, true };
        send_packet(bridge_sock_fd, &req_addr, req_addr_len, &res);

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
    bridge_sock_addr_len = get_sockaddr_len(&bridge_sock_addr);

    assert(bind(bridge_sock_fd, (struct sockaddr*) &bridge_sock_addr,
                bridge_sock_addr_len) == 0);
}

/*
 * Handles the oddity of abstract socket name lengths.
 */
socklen_t
Receiver::get_sockaddr_len(struct sockaddr_un* addr)
{
    socklen_t len = SUN_LEN(addr);
    if (addr->sun_path[0] == 0x0) len += strlen(addr->sun_path + 1);
    return len;
}

/*
 * Sends a ResponsePacket back to the bridge.
 */
inline void
Receiver::send_packet(const int fd, const struct sockaddr_un* const dest_addr,
        socklen_t dest_addr_len, ResponsePacket* res)
{
    ssize_t ret = sendto(fd, res, sizeof(*res), 0, (struct sockaddr*) dest_addr,
            dest_addr_len);
    assert (ret == (ssize_t) sizeof(*res));
}

/*
 * Receives a RequestPacket from the bridge. Blocks until it sees a packet.
 */
inline void
Receiver::receive_packet(const int fd, struct sockaddr_un* const src_addr,
        socklen_t* src_addr_len, RequestPacket* req)
{
    // recvfrom() requires *src_addr_len to contain the full size of src_addr as
    // a precondition, so set it.
    if (src_addr_len != nullptr) *src_addr_len = sizeof(*src_addr);

    // before calling recvfrom(), we also need to set sun_family
    if (src_addr != nullptr) src_addr->sun_family = AF_UNIX;

    ssize_t ret = recvfrom(fd, req, sizeof(*req), 0, (struct sockaddr*)
            src_addr, src_addr_len);
    assert(ret == (ssize_t) sizeof(*req));
}

/*
 *  Finish and dump stats.
 */
void
Receiver::finish()
{
    printf("Receiver dumping stats...\n");

    tool->dump_stats_text();
    tool->dump_stats_binary();

    exit(0);
}

/*
 * signal_handler trampolines into the appropriate function based on the signal.
 */
void
Receiver::signal_handler(int signum, siginfo_t* info, void* ucxt)
{
    if (signum == SIGHUP) {
        printf("Receiver saw zsim finish unexpectedly!"
                "Exiting without dumping stats...\n");

        exit(1);
    }
    else assert(false);
}
