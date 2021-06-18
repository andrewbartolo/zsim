/*
 * Defines the Request-Response format for packets sent from Bridge to Receiver
 * (Request) and Receiver back to Bridge (Response).
 */
#ifndef BRIDGE_PACKET_H_
#define BRIDGE_PACKET_H_

#include <stdint.h>


#define BRIDGE_PACKET_STATUS_OK             0x0
#define BRIDGE_PACKET_STATUS_TERM           0x1
// NOTE: access types need to match the AccessType enum in memory_hierarchy.h
#define BRIDGE_PACKET_ACCESS_TYPE_GETS      0x0
#define BRIDGE_PACKET_ACCESS_TYPE_GETX      0x1
#define BRIDGE_PACKET_ACCESS_TYPE_PUTS      0x2
#define BRIDGE_PACKET_ACCESS_TYPE_PUTX      0x3

// TODO -- pad these to cache alignment value?
typedef struct {
    uint8_t status;
    uint64_t line_addr;
    uint8_t access_type;
    uint64_t cycle;
    uint32_t src_id;
} RequestPacket;

typedef struct {
    // extra latency in cycles incurred by just the simulated bridge operation
    uint64_t latency;
    // whether/not to forward the request on to memory
    bool do_forward;
} ResponsePacket;

#endif
