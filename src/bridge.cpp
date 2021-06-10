#include <libgen.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>

#include "bridge.h"
#include "bridge_packet.h"

// maximum struct sockaddr_un path length
#define SUN_PATH_MAX 108

/*
 * Static variable definitions.
 */
pid_t Bridge::receiver_pid = 0;


/*
 * Ctor and dtor.
 */
Bridge::Bridge(const char* const zsim_output_dir, uint32_t lineSize,
        g_string& name) : lineSize(lineSize), name(name)
{
    // find the sock path
    char receiver_sock_path[4096];
    get_receiver_sock_path(zsim_output_dir, receiver_sock_path,
            sizeof(receiver_sock_path));

    establish_socket_post(receiver_sock_path);
}

/*
 * The stock zsim code never cleans up its MemObjects (of which Bridge is a
 * subtype), so this dtor is never called.
 */
Bridge::~Bridge()
{
}

/*
 * Fork off the receiver process and do some signal magic on it, so that it gets
 * a signal when the parent process dies. This function should be called before
 * *any* Pin initialization is done; hence, why it has its own static method
 * (and not just called by Bridge ctor).
 */
void
Bridge::launch_receiver(const char* const zsim_output_dir)
{
    char receiver_sock_path[4096];
    get_receiver_sock_path(zsim_output_dir, receiver_sock_path,
            sizeof(receiver_sock_path));

    establish_socket_pre(receiver_sock_path);

    pid_t pid = fork();
    if (pid < 0) panic("receiver process fork failed");
    if (pid == 0) {
        // child

        // set a catch-all to send SIGHUP to the receiver if zsim dies
        // unexpectedly (SIGTERM is sent and caught beforehand upon clean exit.)
        if (prctl(PR_SET_PDEATHSIG, SIGHUP) != 0) {
            panic("prctl() set on receiver failed")
        }

        char receiver_bin_path[4096];
        get_receiver_bin_path(receiver_bin_path, sizeof(receiver_bin_path));

        // args to receiver: 1. its own path, and 2. the shared sock path
        char* execv_argv[] = { receiver_bin_path, receiver_sock_path, nullptr };

        if (execv(receiver_bin_path, execv_argv) == -1) {
            panic("could not execv() receiver process");
        }
    }
    if (pid != 0) {
        // parent

        receiver_pid = pid;
        // NOTE: socket establishment is done in the Bridge instance instead
    }
}

/*
 * Send a SIGTERM to the receiver process to trigger it to finalize.
 */
void
Bridge::terminate_receiver()
{
    if (kill(receiver_pid, SIGTERM) != 0) {
        panic("could not kill(SIGTERM) receiver process");
    }
}

/*
 * Reads /proc/self/exe in order to deduce the path to the receiver binary.
 * NOTE: this should only be called from zsim_harness.cpp, not from init.cpp.
 */
void
Bridge::get_receiver_bin_path(char* buf, size_t buf_len)
{
    memset(buf, 0, buf_len);

    if (readlink("/proc/self/exe", buf, buf_len) < 0) {
        panic("readlink() to deduce receiver bin path failed");
    }

    // trim off the zsim executable and add the path to receiver instead
    dirname(buf);
    strcat(buf, "/receiver/receiver");
}


/*
 * Given the absolute path to the zsim output dir, returns an absolute path to
 * the sock.
 * NOTE: this function is called by both zsim_harness.cpp and init.cpp.
 */
void
Bridge::get_receiver_sock_path(const char* const zsim_output_dir, char* buf,
        size_t buf_len)
{
    memset(buf, 0, buf_len);

    char suffix[] = "/bridge.sock";
    if (strlen(zsim_output_dir) + strlen(suffix) >= buf_len) {
        panic("socket file path too long");
    }

    strncpy(buf, zsim_output_dir, buf_len);
    strcat(buf, suffix);
}

/*
 * The first of the two zsim-side UNIX socket establishment methods.
 * Unlinks the existing socket file (if any), so that the receiver process can
 * re-create it.
 */
void
Bridge::establish_socket_pre(const char* const receiver_sock_path)
{
    unlink(receiver_sock_path);
}

/*
 * The second of the two zsim-side UNIX socket establishment methods.
 * Spins and waits for the receiver process to create and bind the socket file.
 * (The create-and-bind is a single atomic action, so we can just spin and
 * wait for the file to appear.) After that, sets up the socket file descriptor
 * and sockaddr.
 */
void
Bridge::establish_socket_post(const char* const receiver_sock_path)
{
    while (::access(receiver_sock_path, R_OK | W_OK) != 0);

    // set up fd
    if ((receiver_sock_fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        panic("could not create bridge socket fd");
    }

    // set up sockaddr
    memset(&receiver_sock_addr, 0, sizeof(receiver_sock_addr));
    receiver_sock_addr.sun_family = AF_UNIX;
    if (strlen(receiver_sock_path) >= SUN_PATH_MAX) {
        panic("receiver_sock_path is too long for sockaddr_un");
    }
    strncpy(receiver_sock_addr.sun_path, receiver_sock_path, SUN_PATH_MAX);
    receiver_sock_addr_len = SUN_LEN(&receiver_sock_addr);
}

/*
 * Sends a packet to the receiver process via the pre-established socket.
 */
void
Bridge::send_packet(const void* const buf, const size_t buf_len)
{
    if (receiver_sock_fd == -1) {
        panic("Bridge::send_packet() before socket fd initialized");
    }

    ssize_t ret = sendto(receiver_sock_fd, buf, buf_len, 0,
            (struct sockaddr*) &receiver_sock_addr, receiver_sock_addr_len);

    if (ret != (ssize_t) buf_len) {
        panic("bridge packet send failed");
    }
}


/*
 * If this were a traditional zsim function, it would return a uint64_t
 * indicating the current cycle upon completion of the access. However, the
 * receiver simulator process keeps track of all its own stats separately
 * (though it does use the zsim start cycle as input). We keep the signatures
 * the same and just return the input cycle.
 */
uint64_t
Bridge::access(MemReq& req)
{
    /*
     * NOTE: still need to dereference + set *req.state, because other places in
     * the zsim codebase expect it.
     */
    switch (req.type) {
        case PUTX:
            //Dirty wback
            //Note no break
        case PUTS:
            //Not a real access -- memory must treat clean wbacks as if they never happened.
            *req.state = I;
            break;
        case GETS:
            *req.state = req.is(MemReq::NOEXCL)? S : E;
            break;
        case GETX:
            *req.state = M;
            break;
        default: panic("!?");
    }

    /*
     * Now that we've patched up the correct zsim state, actually send a packet
     * to the receiver process.
     */
    RequestPacket rp = { req.lineAddr, req.type, req.cycle, req.srcId };
    send_packet(&rp, sizeof(rp));

    /*
     * And wait for the response.
     */

    return req.cycle + 0;
}
