#pragma once

#include <stdbool.h>
#include <string.h>
#include "raspi.h"
#include "packenc.h"
#include "crc32.h"
#include "diskio.h"
#include <ff.h>

// Packed structure
#define PACKED __attribute__((__packed__))

// Maximum supported packet size
#define max_packet_size 4096

// Shared globals
extern uint8_t response_buf[max_packet_size];
extern uint32_t min_cpu_freq;
extern uint32_t max_cpu_freq;
extern uint32_t reset_timeout_millis;
extern uint32_t original_cpu_freq;
extern unsigned current_baud;

// Packets ID
enum PACKET_ID
{
    PACKET_ID_PING = 0,
    PACKET_ID_ACK = 1,
    PACKET_ID_ERROR = 2,
    PACKET_ID_DATA = 3,
    PACKET_ID_GO = 4,
    PACKET_ID_REQUEST_BAUD = 5,
    PACKET_ID_COMMAND = 6,
    PACKET_ID_STDOUT = 7,
    PACKET_ID_STDERR = 8,
    PACKET_ID_PULL = 9,
    PACKET_ID_PULL_HEADER = 10,
    PACKET_ID_PULL_DATA = 11,
    PACKET_ID_PUSH_DATA = 12,
    PACKET_ID_PUSH_COMMIT = 13,

};

// Shared functions
void sendPacket(uint32_t seq, uint32_t id, const void* pData, uint32_t cbData);
void restore_cpu_freq();

// Handlers
void handle_ping(uint32_t seq, const void* p, uint32_t cb);
void handle_data(uint32_t seq, const void* p, uint32_t cb);
void handle_baud_request(uint32_t seq, const void* p, uint32_t cb);
void handle_go(uint32_t seq, const void* p, uint32_t cb);
void handle_pull(uint32_t seq, const void* p, uint32_t cb);
void handle_push_data(uint32_t seq, const void* p, uint32_t cb);
void handle_push_commit(uint32_t seq, const void* p, uint32_t cb);
void reset_push();
void handle_command(uint32_t seq, const void* p, uint32_t cb);
