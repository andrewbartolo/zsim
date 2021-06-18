#include <libgen.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <random>

#include "bridge.h"
#include "bridge_packet.h"

// maximum struct sockaddr_un path length
#define SUN_PATH_MAX 108

/*
 * Static variable definitions.
 */
pid_t Bridge::receiver_pid_s = 0;
std::string Bridge::receiver_sock_path_s;
int Bridge::receiver_sock_fd_s = -1;
struct sockaddr_un Bridge::receiver_sock_addr_s;
socklen_t Bridge::receiver_sock_addr_len_s = 0;


/*
 * Ctor and dtor.
 */
Bridge::Bridge(const std::string& zsim_output_dir, uint32_t line_size) :
        line_size(line_size)
{
    receiver_sock_path = get_receiver_sock_path(zsim_output_dir);
    establish_socket();
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
Bridge::launch_receiver(const std::string zsim_output_dir,
        const std::string& tool, std::string tool_config_file)
{
    // first, validate tool type:

    const char* const valid_tool_types[] = {
        "Counter",
    };
    size_t n_valid_tool_types =
            sizeof(valid_tool_types) / sizeof(valid_tool_types[0]);

    bool found_valid_type = false;
    for (size_t i = 0; i < n_valid_tool_types; ++i) {
        std::string tt = valid_tool_types[i];
        if (tool == tt) {
            found_valid_type = true;
            break;
        }
    }

    if (!found_valid_type) {
        panic("invalid bridge tool type \"%s\" specified", tool.c_str());
    }

    // next, canonicalize tool config file path:
    if (tool_config_file.length() > 0 and tool_config_file[0] != '/') {
        tool_config_file = zsim_output_dir + "/" + tool_config_file;
    }

    // establish the socket
    receiver_sock_path_s = get_receiver_sock_path(zsim_output_dir);
    establish_socket_s();

    // fork and exec
    pid_t pid = fork();
    if (pid < 0) panic("receiver process fork failed");
    if (pid == 0) {
        // child

        // set a catch-all to send SIGHUP to the receiver if zsim dies
        // unexpectedly (SIGTERM is sent and caught beforehand upon clean exit.)
        if (prctl(PR_SET_PDEATHSIG, SIGHUP) != 0) {
            panic("prctl() set on receiver failed")
        }

        std::string receiver_bin_path = get_receiver_bin_path();

        // args to receiver: 1. its own path, 2. the output dir (for sock path),
        // 3. the tool type, and 4. the canonicalized tool config file path
        // ok to cast away constness to fit the execv() signature, as it copies
        // all args.
        char* const execv_argv[] = {
                (char*) receiver_bin_path.c_str(),
                (char*) zsim_output_dir.c_str(),
                (char*) tool.c_str(),
                (char*) tool_config_file.c_str(),
                nullptr
        };

        if (execv(receiver_bin_path.c_str(), execv_argv) == -1) {
            panic("could not execv() receiver process");
        }
    }
    if (pid != 0) {
        // parent

        receiver_pid_s = pid;
    }
}

/*
 * Send a status=TERM packet to the receiver process to trigger it to finalize.
 */
void
Bridge::terminate_receiver()
{
    if (receiver_pid_s == 0) {
        panic("cannot terminate receiver for which we have no record (pid)");
    }

    RequestPacket rp;
    memset(&rp, 0, sizeof(rp));
    rp.status = BRIDGE_PACKET_STATUS_TERM;
    send_packet(receiver_sock_fd_s, &receiver_sock_addr_s,
            receiver_sock_addr_len_s, &rp);
}

/*
 * Reads /proc/self/exe in order to deduce the path to the receiver binary.
 * NOTE: this should only be called from zsim_harness.cpp, not from init.cpp.
 */
std::string
Bridge::get_receiver_bin_path()
{
    char buf[4096];
    memset(buf, 0, sizeof(buf));

    if (readlink("/proc/self/exe", buf, sizeof(buf)) < 0) {
        panic("readlink() to deduce receiver bin path failed");
    }

    // trim off the zsim executable and add the path to receiver instead
    dirname(buf);
    strcat(buf, "/receiver/receiver");

    return std::string(buf);
}


/*
 * Given the absolute path to the zsim output dir, returns an absolute path to
 * the sock.
 * NOTE: this is called from both init.cpp and from zsim_harness.cpp.
 */
std::string
Bridge::get_receiver_sock_path(const std::string& zsim_output_dir)
{
    std::string suffix = "/bridge.sock";
    return zsim_output_dir + suffix;
}

/*
 * The first of the two zsim-side UNIX socket establishment methods.
 * Unlinks the existing socket file (if any), so that the receiver process can
 * re-create it. Then, opens the static socket fd and sets up static sockaddr.
 */
void
Bridge::establish_socket_s()
{
    unlink(receiver_sock_path_s.c_str());

    establish_socket_fd_addr_lowlevel(receiver_sock_path_s, receiver_sock_fd_s,
            receiver_sock_addr_s, receiver_sock_addr_len_s);
}

/*
 * The second of the two zsim-side UNIX socket establishment methods.
 * Spins and waits for the receiver process to create and bind the socket file.
 * (The create-and-bind is a single atomic action, so we can just spin and
 * wait for the file to appear.) After that, sets up the socket file descriptor
 * and sockaddr.
 */
void
Bridge::establish_socket()
{
    while (::access(receiver_sock_path.c_str(), R_OK | W_OK) != 0);

    establish_socket_fd_addr_lowlevel(receiver_sock_path, receiver_sock_fd,
            receiver_sock_addr, receiver_sock_addr_len);
}

/*
 * This needs to be done separately for the static and non-static (Bridge
 * instance) invocations, since they run in separate processes that don't share
 * history. So, we make it a helper fn.
 */
void
Bridge::establish_socket_fd_addr_lowlevel(const std::string& path, int& fd,
        struct sockaddr_un& addr, socklen_t& addr_len)
{
    // set up fd
    if ((fd = socket(AF_UNIX, SOCK_DGRAM, 0)) < 0) {
        panic("could not create socket fd");
    }

    // set up destination sockaddr
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    if (path.length() >= SUN_PATH_MAX) {
        panic("path is too long for sockaddr_un");
    }
    strncpy(addr.sun_path, path.c_str(), SUN_PATH_MAX - 1);
    addr_len = get_sockaddr_len(&addr);

    // bind the socket to an anonymous name so we can get responses on it
    struct sockaddr_un sa_bind;
    memset(&sa_bind, 0, sizeof(sa_bind));
    sa_bind.sun_family = AF_UNIX;

    // get an abstract (anonymous) bind point by using a UID
    constexpr size_t n_uid_chars = 8;
    std::string uid = gen_uid(n_uid_chars);

    sa_bind.sun_path[0] = 0x0;
    strncpy(sa_bind.sun_path + 1, uid.c_str(), n_uid_chars + 1);

    // takes into account the abstract name format
    socklen_t sa_bind_len = get_sockaddr_len(&sa_bind);

    if (bind(fd, (struct sockaddr*) &sa_bind, sa_bind_len) != 0) {
        panic("could not bind client socket for return path");
    }
}

/*
 * Handles the oddity of abstract socket name lengths.
 */
socklen_t
Bridge::get_sockaddr_len(struct sockaddr_un* addr)
{
    socklen_t len = SUN_LEN(addr);
    if (addr->sun_path[0] == 0x0) len += strlen(addr->sun_path + 1);
    return len;
}

/*
 * Sends a RequestPacket to the receiver process via the specified socket.
 */
inline void
Bridge::send_packet(const int fd, const struct sockaddr_un* const dest_addr,
        socklen_t dest_addr_len, RequestPacket* req)
{
    if (fd == -1) {
        panic("Bridge::send_packet() before socket fd initialized");
    }

    ssize_t ret = sendto(fd, req, sizeof(*req), 0, (struct sockaddr*) dest_addr,
            dest_addr_len);

    if (ret != (ssize_t) sizeof(*req)) {
        panic("bridge packet send failed");
    }
}

/*
 * Receives a ResponsePacket from the receiver. Blocks until it sees a packet.
 */
inline void
Bridge::receive_packet(const int fd, struct sockaddr_un* const src_addr,
        socklen_t* src_addr_len, ResponsePacket* res)
{
    // recvfrom() requires *src_addr_len to contain the full size of src_addr as
    // a precondition, so set it.
    if (src_addr_len != nullptr) *src_addr_len = sizeof(*src_addr);

    // before calling recvfrom(), we also need to set sun_family
    if (src_addr != nullptr) src_addr->sun_family = AF_UNIX;

    ssize_t ret = recvfrom(fd, res, sizeof(*res), 0, (struct sockaddr*)
            src_addr, src_addr_len);
    assert(ret == (ssize_t) sizeof(*res));
}

std::string
Bridge::gen_uid(const size_t len)
{
    static const char UID_CHARS[] = "abcdefghijklmnopqrstuvwxyz0123456789";

    char buf[len + 1];

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, strlen(UID_CHARS) - 1);

    for (size_t i = 0; i < len; ++i) {
        buf[i] = UID_CHARS[dis(gen)];
    }
    buf[len] = 0x0;

    return std::string(buf);
}


/*
 * If this were a traditional zsim function, it would return a uint64_t
 * indicating the current cycle upon completion of the access. However, the
 * receiver simulator process keeps track of all its own stats separately
 * (though it does use the zsim start cycle as input).
 * Instead, we return a uint64_t latency value, and also a bool indicating
 * whether/not we should forward the request to the memory.
 */
void
Bridge::access(MemReq& req, uint64_t* latency, bool* do_forward)
{
    /*
     * Send a packet to the reciever process.
     */
    RequestPacket reqp = {
        BRIDGE_PACKET_STATUS_OK,
        req.lineAddr,
        req.type,
        req.cycle,
        req.srcId
    };
    send_packet(receiver_sock_fd, &receiver_sock_addr, receiver_sock_addr_len,
            &reqp);

    /*
     * And wait for the response.
     */
    ResponsePacket resp;
    receive_packet(receiver_sock_fd, nullptr, nullptr, &resp);

    // return values
    *latency = resp.latency;
    *do_forward = resp.do_forward;
}
