/*
 * Defines the Request-Response format for packets sent from Bridge to Receiver
 * (Request) and Receiver back to Bridge (Response).
 */
#include <stdint.h>


// TODO -- pad these to cache alignment value?

typedef struct {
    uint64_t line_addr;
    uint8_t access_type;
    uint64_t cycle;
    uint32_t src_id;
} RequestPacket;

typedef struct {
    // cumulative cycles *after* the event has been simulated
    uint64_t cycle;
} ResponsePacket;
