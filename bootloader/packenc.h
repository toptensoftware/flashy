#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Encodes a packet of data, calling 'callback' with the encoded bytes
void packet_encode(void(*callback)(uint8_t data), uint32_t seq, uint32_t cmd, const void* pData, uint32_t cbData);

enum packet_error
{
    packet_error_none,                          // 0
    packet_error_new_packet,                    // 1
    packet_error_invalid_stuff_byte,            // 2
    packet_error_invalid_separator_byte,        // 3
    packet_error_too_large,                     // 4
    packet_error_checksum_mismatch,             // 5
    packet_error_invalid_terminator,            // 6
};

// Decoder context
struct decode_context
{
    // These should be initialized to zero
    uint8_t state;
    uint8_t signal_bytes_seen;
    uint32_t count;
    uint32_t seq;
    uint32_t cmd;
    uint32_t length;
    uint32_t crcRecv;
    uint32_t crcCalc;

    // These need to be populated
    void (*onPacket)(uint32_t seq, uint32_t cmd, const void *pdata, uint32_t cbData);
    void (*onError)(int code);      // can be null if not interested
    uint8_t* pBuf;
    size_t cbBuf;
};

// Decode packet data (callback passed to constructor will be invoked with decoded packets).
// Invalid data and packets will be discarded
void packet_decode(struct decode_context* pctx, uint8_t data);

#ifdef __cplusplus
}
#endif
