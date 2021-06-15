/*
 * This class implements the "bridge" between the existing zsim codebase, and
 * the separate facilities for simulating memory-related functionality, such as
 * a write buffer, multi-node NUMA, etc.
 *
 * In essence, instead of attaching a memory controller to the last-level cache,
 * we instead attach the bridge interface to the last-level cache. Then, memory
 * accesses are sent over the bridge to the companion receiver (simulator)
 * process.
 *
 * This design gets around the many annoyances of Pin-injected processes (e.g.,
 * not being able to allocate memory directly using mmap() or, by extension,
 * malloc()). Packets sent by zsim over the bridge are ingested by the receiver
 * simulator process, where the project-specific functionality is simulated.
 *
 * NOTE: static class variables and functions are called via zsim_harness.cpp,
 * and Bridge member variables and methods are called via init.cpp. The reason
 * for the distinction is that zsim_harness.cpp and init.cpp run in separate
 * processes: the former is the main zsim executable, and the latter is the
 * Pin-injected process.
 *
 * In the future, it is intended that the functionality resident in the
 * receiver process can be straightforwardly ported to support a new (non-zsim)
 * frontend. This frontend would:
 *
 * 1. use a more flexible DBI scheme, perhaps based on DynamoRIO,
 * 2. implement core timing and coherent multi-level cache models, similar to
 *    zsim today, and
 * 3. contain advanced importance sampling functionality, enabling
 *    highly-efficient online simulation, as a novel research direction.
 */
#ifndef BRIDGE_H_
#define BRIDGE_H_

#include <stdint.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <string>

#include "bridge_packet.h"
#include "g_std/g_string.h"
#include "memory_hierarchy.h"


class Bridge : public MemObject {
    public:
        Bridge(const std::string& zsim_output_dir, uint32_t lineSize,
                g_string& name);
        // NOTE: copy + move ctors + assignment operators *should be* deleted
        Bridge(const Bridge& b) = delete;
        Bridge& operator=(const Bridge& b) = delete;
        Bridge(Bridge&& b) = delete;
        Bridge& operator=(Bridge&& b) = delete;
        ~Bridge();

        uint64_t access(MemReq& req);

        const char* getName() { return name.c_str(); }
        uint32_t getLineSize() { return lineSize; }

        static void launch_receiver(const std::string zsim_output_dir,
                const std::string& tool, std::string tool_config_file);
        static void terminate_receiver();


    private:
        static std::string get_receiver_bin_path();
        static std::string get_receiver_sock_path(const std::string&
                zsim_output_dir);
        static void establish_socket_s();
        void establish_socket();
        static void establish_socket_fd_addr_lowlevel(const std::string& path,
                int& fd, struct sockaddr_un& addr, socklen_t& addr_len);
        static socklen_t get_sockaddr_len(struct sockaddr_un* addr);
        static inline void send_packet(const int fd, const struct sockaddr_un*
                const dest_addr, socklen_t dest_addr_len, RequestPacket* req);
        static inline void receive_packet(const int fd, struct sockaddr_un*
                const src_addr, socklen_t* src_addr_len, ResponsePacket* res);
        static std::string gen_uid(const size_t len);

        // static variables used by zsim_harness.cpp
        static pid_t receiver_pid_s;
        static std::string receiver_sock_path_s;
        static int receiver_sock_fd_s;
        static struct sockaddr_un receiver_sock_addr_s;
        static socklen_t receiver_sock_addr_len_s;

        uint32_t lineSize;
        g_string name;

        std::string receiver_sock_path;
        int receiver_sock_fd;
        struct sockaddr_un receiver_sock_addr;
        socklen_t receiver_sock_addr_len;
};


#endif  // BRIDGE_H_
