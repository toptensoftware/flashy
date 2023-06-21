
#include <stdbool.h>
#include <string.h>
#include "periph.h"
#include "packenc.h"
#include "crc32.h"

// The default baud rate
#define default_baud 115200

// Maximum supported packet size
#define max_packet_size 4096

// Packet decoder buffer
uint8_t decoder_buf[max_packet_size];

// Currently selected baud rate
unsigned current_baud = default_baud;

// Last packet received time
unsigned last_received_packet_time = 0;

// How long to wait on non-default baud rate with no
// received packets before resetting to the default baud rate
uint32_t reset_baud_timeout_millis = 2500;



// Packets ID
enum PACKET_ID
{
    PACKET_ID_PING,
    PACKET_ID_ACK,
    PACKET_ID_ERROR,
    PACKET_ID_DATA,
    PACKET_ID_GO,
    PACKET_ID_REQUEST_BAUD,
};

// Ping ack packet
// Sent in response to PACKET_ID_PING from host to indicate
// device is alive and well and to report some info about the device
struct PACKET_PING_ACK
{
    uint32_t version;            // Bootloader version
    uint32_t boardrev;           // Board revision
    uint64_t boardserial;        // Board serial number
    uint32_t maxpacketsize;      // Maximum size of accepted packet
    uint32_t rate;               // Currently selected baud rate
};

// Error report packet
// device -> host sent on packet decode error
struct PACKET_ERROR
{
    int code;
};

// Data packet
// host -> device - program data to be loaded
struct PACKET_DATA
{
    uint32_t address;
    uint8_t data[0];
};

// Go command packet
// host -> device - instructs bootloader to transfer control to loaded program
struct PACKET_GO
{
    uint32_t startAddress;      // 0xFFFFFFFF to use default for platform
    uint32_t delayMillis;       // optional delay in millisecond before launching
};

// Request baud packet
// host -> device - requests bootloader switch to a different baud rate
//                  if no packets are received for a `reset_timeout_millis` then
//                  bootloader will revert to `default_baud`.
struct PACKET_REQUEST_BAUD
{
    uint32_t baud;
    uint32_t reset_timeout_millis;
};

// Helper to send a single byte (correct signature for packet encoder callback)
void uart_send_byte(uint8_t byte)
{
    uart_send(byte);
}

// Send a packet to the host
void sendPacket(uint32_t seq, uint32_t id, const void* pData, uint32_t cbData)
{
    packet_encode(uart_send_byte, seq, id, pData, cbData);
}

// Receive a packet from the host
void onPacketReceived(uint32_t seq, uint32_t id, const void* p, uint32_t cbData)
{
    // Flash activity led
    set_activity_led(seq & 1);

    // Store packet time
    last_received_packet_time = timer_tick();

    // Dispatch by id
    switch (id)
    {
        case PACKET_ID_PING:
        {
            // Send ack packet with version and current rate
            struct PACKET_PING_ACK ack;
            ack.version = 1;
            ack.boardrev = get_board_revision ();
            ack.boardserial = get_board_serial ();
            ack.maxpacketsize = sizeof(decoder_buf);
            ack.rate = current_baud;
            sendPacket(seq, PACKET_ID_ACK, &ack, sizeof(ack));
            break;
        }

        case PACKET_ID_DATA:
        {
            // Cast packet
            struct PACKET_DATA* pData = (struct PACKET_DATA*)p;

            // Copy packet data to memory
            /*
            memcpy((void*)pData->address, pData->data, cbData - sizeof(struct PACKET_DATA));
            */
            for (uint32_t i = 0; i<cbData - sizeof(struct PACKET_DATA); i++)
            {
                PUT8(pData->address + i, pData->data[i]);
            }

            // Send ack
            struct PACKET_PING_ACK ack;
            ack.version = 1;
            ack.rate = current_baud;
            sendPacket(seq, PACKET_ID_ACK, &ack, sizeof(ack));

            break;
        }

        case PACKET_ID_GO:
        {
            // Cast packet
            struct PACKET_GO* pGo = (struct PACKET_GO*)p;

            // Send ack
            sendPacket(seq, PACKET_ID_ACK, NULL, 0);

            // Flush
            uart_flush();
            delay_micros(10000);

            // User delay before starting?
            if (pGo->delayMillis)
                delay_micros(pGo->delayMillis * 1000);

            // Use default start address?
            if (pGo->startAddress == 0xFFFFFFFF)
            {
                #if AARCH == 32
                pGo->startAddress = 0x8000;
                #else
                pGo->startAddress = 0x80000;
                #endif
            }

            // Jump to loaded program
            BRANCHTO(pGo->startAddress);
            break;
        }

        case PACKET_ID_REQUEST_BAUD:
        {
            // Cast packet
            struct PACKET_REQUEST_BAUD* pBaud = (struct PACKET_REQUEST_BAUD*)p;

            // Store reset
            reset_baud_timeout_millis = pBaud->reset_timeout_millis;

            // Send ack
            sendPacket(seq, PACKET_ID_ACK, NULL, 0);

            if (pBaud->baud != current_baud)
            {
                // Switch baud rate
                uart_flush();
                delay_micros(10000);
                current_baud = pBaud->baud;
                uart_init(current_baud);
            }

            break;
        }
    }
}

// Callback to handle packet decode errors
// Just pass to host for informational/debugging purposes
void onPacketError(int code)
{
    struct PACKET_ERROR err;
    err.code = code;
    sendPacket(0, PACKET_ID_ERROR, &err, sizeof(err));
}

// Main
int notmain ( void )
{
    // Zero bss section
    extern unsigned char __bss_start;
	extern unsigned char __bss_end;
	memset(&__bss_start, 0, &__bss_end - &__bss_start);

    // Setup packet decoder
    struct decode_context ctx = {0};
    ctx.onError = onPacketError;
    ctx.onPacket = onPacketReceived;
    ctx.pBuf = decoder_buf;
    ctx.cbBuf = sizeof(decoder_buf);

    // Initialize hardware
    uart_init(current_baud);
    timer_init();

    // Main loop
    while (true)
    {
        // Read serial bytes into fifo
        int recv_byte;
        while ((recv_byte = uart_try_recv()) >= 0)
            packet_decode(&ctx, recv_byte);

        unsigned tick = timer_tick();

        // If no packets received at non-standard baud rate, then switch
        // back to the default
        if (current_baud != default_baud && reset_baud_timeout_millis != 0 && 
            tick - last_received_packet_time > (reset_baud_timeout_millis * 1000))
        {
            current_baud = default_baud;
            uart_init(current_baud);
        }

        // When in default baud mode and it's been more than a second since
        // received a packet, flash the alive heart beat
        if (current_baud == default_baud && tick - last_received_packet_time > 500000)
        {
            // Flash LED 2 times every half second
            unsigned insec = (tick / 1000) % 1000;
            set_activity_led(insec < 500 ? ((insec / 125) & 1) : 0);
        }
    }
}

