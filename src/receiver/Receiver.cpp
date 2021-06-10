#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "../bridge_packet.h"
#include "Receiver.h"

// maximum struct sockaddr_un path length
#define SUN_PATH_MAX 108

/*
 * Static variable definitions.
 */
std::string Receiver::bridge_sock_path;
int Receiver::bridge_sock_fd;
struct sockaddr_un Receiver::bridge_sock_addr;
size_t Receiver::bridge_sock_addr_len;
bool Receiver::finalized = false;


int
main(int argc, char* argv[])
{
    assert(argc == 2);
    char* const receiver_bin_path = argv[0];
    char* const bridge_sock_path = argv[1];

    printf("initiating receiver: %s\n", receiver_bin_path);

    Receiver::init(bridge_sock_path);
    Receiver::run();

    return 0;
}

/*
 * Pseudo-constructor for the static Receiver class.
 */
void
Receiver::init(const char* const bridge_sock_path)
{
    Receiver::bridge_sock_path = bridge_sock_path;

    establish_signal_handler();
    establish_socket();
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

    size_t ctr = 0;
    while (true) {
        ssize_t ret = recv(bridge_sock_fd, &req, sizeof(req), 0);
        ++ctr;
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
 * TODO: dump stats here?
 */
void
Receiver::finish()
{

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
