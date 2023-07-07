#include <malloc.h>

#include "packenc.h"
#include "crc32.h"

// Special bytes
const uint8_t signal_byte = 0xAA;
const uint8_t separator_byte = 0;
const uint8_t stuff_byte = 0;
const uint8_t terminator_byte = 0x55;


// Encoder context
typedef struct 
{
    void (*callback)(uint8_t data);
    uint32_t crc;
    uint8_t signal_bytes;
} encoder_context;

// Helper to write a byte of data to output stream, inserting
// stuffing bytes and calculating CRC
static void write(encoder_context *pctx, uint8_t data)
{
    pctx->callback(data);
    crc32_update_1(&pctx->crc, data);

    if (data == signal_byte)
    {
        if (pctx->signal_bytes == 1)
        {
            // Write stuffing byte (0)
            data = stuff_byte;
            crc32_update_1(&pctx->crc, data);
            pctx->callback(data);
            pctx->signal_bytes = 0;
        }
        else
        {
            pctx->signal_bytes++;
        }
    }
    else
        pctx->signal_bytes = 0;
}

// Helper to variable length encode a value
void write_varlen(encoder_context *pctx, uint32_t value, uint8_t bit)
{
    if (value > 127)
    {
        write_varlen(pctx, value >> 7, 0x80);
    }
    write(pctx, (value & 0x7F) | bit);
}




// Encode a single packet of data
void packet_encode(void (*callback)(uint8_t data), uint32_t seq, uint32_t cmd, const void *pData, uint32_t cbData)
{
    // Write signal
    callback(signal_byte);
    callback(signal_byte);
    callback(signal_byte);

    // Setup context
    encoder_context ctx;
    ctx.callback = callback;
    crc32_start(&ctx.crc);
    ctx.signal_bytes = 0;

    // Write header
    write(&ctx, separator_byte);
    write_varlen(&ctx, seq, 0);
    write_varlen(&ctx, cmd, 0);
    write_varlen(&ctx, cbData, 0);

    // Write data
    for (uint32_t i = 0; i < cbData; i++)
    {
        write(&ctx, ((uint8_t *)pData)[i]);
    }

    // Write crc
    crc32_finish(&ctx.crc);
    uint32_t crc = ctx.crc;
    write(&ctx, (crc >> 24) & 0xFF);
    write(&ctx, (crc >> 16) & 0xFF);
    write(&ctx, (crc >> 8) & 0xFF);
    write(&ctx, (crc >> 0) & 0xFF);

    // Write terminator
    callback(terminator_byte);
}

// Decoder state machine states
enum decode_state
{
    decode_state_waiting_signal,
    decode_state_expect_separator,
    decode_state_expect_seq,
    decode_state_expect_cmd,
    decode_state_expect_length,
    decode_state_expect_data,
    decode_state_expect_crc,
    decode_state_expect_terminator,
};


// Decoder packets
void packet_decode(decode_context* pctx, uint8_t data)
{
    // Monitoring for signal happens even while decoding packets
    // and if seen discards the current packet and starts a new one
    if (data == signal_byte)
    {
        if (pctx->signal_bytes_seen < 3)
            pctx->signal_bytes_seen++;
        if (pctx->signal_bytes_seen == 3)
        {
            if (pctx->state != decode_state_waiting_signal)
            {
                if (pctx->onError)
                    pctx->onError(packet_error_new_packet);
            }

            // Start new packet
            pctx->state = decode_state_expect_separator;
            pctx->count = 0;
            return;
        }
    }
    else
    {
        // Ignore zero byte after two consecutive signal_bytes
        if (pctx->signal_bytes_seen == 2)
        {
            pctx->signal_bytes_seen = 0;

            if (data != stuff_byte)
            {
                if (pctx->onError)
                    pctx->onError(packet_error_invalid_stuff_byte);
                pctx->state = decode_state_waiting_signal;
            }
            else if (pctx->state != decode_state_expect_crc || pctx->count == 0)
                crc32_update_1(&pctx->crcCalc, data);
            return;
        }

        pctx->signal_bytes_seen = 0;
    }

    switch (pctx->state)
    {
    case decode_state_waiting_signal:
        // Still waiting for signal
        break;

    case decode_state_expect_separator:
        if (data != separator_byte)
        {
            if (pctx->onError)
                pctx->onError(packet_error_invalid_separator_byte);
            pctx->state = decode_state_waiting_signal;
        }
        else
        {
            pctx->state = decode_state_expect_seq;
            pctx->seq = 0;
            pctx->cmd = 0;
            pctx->length = 0;
            crc32_start(&pctx->crcCalc);
            crc32_update_1(&pctx->crcCalc, data);
        }
        break;

    case decode_state_expect_seq:
        crc32_update_1(&pctx->crcCalc, data);
        pctx->seq = (pctx->seq << 7) | (data & 0x7f);
        if ((data & 0x80) == 0)
        {
            pctx->state = decode_state_expect_cmd;
        }
        break;

    case decode_state_expect_cmd:
        crc32_update_1(&pctx->crcCalc, data);
        pctx->cmd = (pctx->cmd << 7) | (data & 0x7f);
        if ((data & 0x80) == 0)
        {
            pctx->state = decode_state_expect_length;
        }
        break;

    case decode_state_expect_length:
        crc32_update_1(&pctx->crcCalc, data);
        pctx->length = (pctx->length << 7) | (data & 0x7f);
        if ((data & 0x80) == 0)
        {
            if (pctx->length > pctx->cbBuf)
            {
                if (pctx->onError)
                    pctx->onError(packet_error_too_large);
                pctx->state = decode_state_waiting_signal;
            }
            else
            {
                pctx->state = pctx->length == 0 ? decode_state_expect_crc : decode_state_expect_data;
                pctx->count = 0;
            }
        }
        break;

    case decode_state_expect_data:
        crc32_update_1(&pctx->crcCalc, data);
        pctx->pBuf[pctx->count] = data;
        pctx->count++;
        if (pctx->count == pctx->length)
        {
            pctx->state = decode_state_expect_crc;
            pctx->count = 0;
        }
        break;

    case decode_state_expect_crc:
        pctx->crcRecv = pctx->crcRecv << 8 | data;
        pctx->count++;
        if (pctx->count == 4)
        {
            crc32_finish(&pctx->crcCalc);
            if (pctx->crcCalc != pctx->crcRecv)
            {
                if (pctx->onError)
                    pctx->onError(packet_error_checksum_mismatch);
                pctx->state = decode_state_waiting_signal;
            }
            else
                pctx->state = decode_state_expect_terminator;
        }
        break;

    case decode_state_expect_terminator:
        if (data == terminator_byte)
        {
            pctx->state = decode_state_waiting_signal;
            pctx->onPacket(pctx->seq, pctx->cmd, pctx->length ? pctx->pBuf : NULL, pctx->length);
        }
        else
        {
            pctx->state = decode_state_waiting_signal;
            if (pctx->onError)
                pctx->onError(packet_error_invalid_terminator);
        }
        break;
    }
}
