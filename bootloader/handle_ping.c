#include "common.h"

typedef struct PACKED
{
    uint8_t major;
    uint8_t minor;
    uint8_t build;
    uint8_t revision;
} VERSION;

// Ping ack packet
// Sent in response to PACKET_ID_PING from host to indicate
// device is alive and well and to report some info about the device
typedef struct PACKED
{
    VERSION version;            // Bootloader version
    uint32_t raspi;              // Bootloader raspi version 1..4
    uint32_t aarch;              // Bootloader AARCH (32 or 64)
    uint32_t boardrev;           // Board revision
    uint64_t boardserial;        // Board serial number
    uint32_t maxpacketsize;      // Maximum size of accepted packet
    uint32_t cpu_freq;           // Current CPU freq
    uint32_t min_cpu_freq;       // Min CPU freq
    uint32_t max_cpu_freq;       // Max CPU freq
} PACKET_PING_ACK;


// Handler
void handle_ping(uint32_t seq, const void* p, uint32_t cb)
{
    VERSION ver = { FLASHY_VERSION };
    // Send ack packet with version and current rate
    PACKET_PING_ACK ack;
    ack.version = ver;
    ack.raspi = RASPI;
    ack.aarch = AARCH;
    ack.boardrev = get_board_revision ();
    ack.boardserial = get_board_serial ();
    ack.maxpacketsize = max_packet_size;
    ack.cpu_freq = get_cpu_freq();
    ack.min_cpu_freq = min_cpu_freq;
    ack.max_cpu_freq = max_cpu_freq;
    sendPacket(seq, PACKET_ID_ACK, &ack, sizeof(ack));
}