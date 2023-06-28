
#include <stdbool.h>
#include <string.h>
#include "raspi.h"
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
uint32_t reset_timeout_millis = 2500;

// Pre throttle up clock rate
uint32_t original_cpu_freq = 0;
uint32_t min_cpu_freq = 0;
uint32_t max_cpu_freq = 0;

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
    uint32_t cpu_freq;
    uint32_t measured_cpu_freq;
    uint32_t min_cpu_freq;
    uint32_t max_cpu_freq;
    uint32_t baudrate;           // Currently selected baud rate
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
    uint32_t cpufreq;
};

#if 0

#define serial_init mini_uart_init
#define serial_try_recv mini_uart_try_recv
#define serial_send mini_uart_send
#define serial_flush mini_uart_flush

#elif 0

#define serial_init(unused) ((void)0)
#define serial_try_recv() (-1)
#define serial_send(byte) ((void)0)
#define serial_flush() ((void)0)

#else

#define serial_init uart_init
#define serial_try_recv uart_try_recv
#define serial_send uart_send
#define serial_flush uart_flush

#endif

void restore_cpu_freq()
{
    if (original_cpu_freq != 0)
    {
        set_cpu_freq(original_cpu_freq);
        original_cpu_freq = 0;
    }
}



// Helper to send a single byte (correct signature for packet encoder callback)
void send_byte(uint8_t byte)
{
    serial_send(byte);
}

// Send a packet to the host
void sendPacket(uint32_t seq, uint32_t id, const void* pData, uint32_t cbData)
{
    packet_encode(send_byte, seq, id, pData, cbData);
}

// Receive a packet from the host
void onPacketReceived(uint32_t seq, uint32_t id, const void* p, uint32_t cbData)
{
    // Store packet time
    last_received_packet_time = micros();

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
            ack.cpu_freq = get_cpu_freq();
            ack.measured_cpu_freq = get_measured_cpu_freq();
            ack.min_cpu_freq = min_cpu_freq;
            ack.max_cpu_freq = max_cpu_freq;
            ack.baudrate = current_baud;
            sendPacket(seq, PACKET_ID_ACK, &ack, sizeof(ack));
            break;
        }

        case PACKET_ID_DATA:
        {
            // Flash activity led
            set_activity_led(1);

            // Cast packet
            struct PACKET_DATA* pData = (struct PACKET_DATA*)p;

            // Copy packet data to memory
            memcpy((void*)(size_t)pData->address, pData->data, cbData - sizeof(struct PACKET_DATA));

            // Send ack
            sendPacket(seq, PACKET_ID_ACK, NULL, 0);

            set_activity_led(0);

            break;
        }

        case PACKET_ID_GO:
        {
            // Cast packet
            struct PACKET_GO* pGo = (struct PACKET_GO*)p;

            // Send ack
            sendPacket(seq, PACKET_ID_ACK, NULL, 0);

            // Restore clock rate
            restore_cpu_freq();

            // Flush
            serial_flush();
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
            reset_timeout_millis = pBaud->reset_timeout_millis;

            // Throttle up
            if (pBaud->cpufreq)
            {
                // Clamp to limits
                if (pBaud->cpufreq < min_cpu_freq)
                    pBaud->cpufreq = min_cpu_freq;
                if (pBaud->cpufreq > max_cpu_freq)
                    pBaud->cpufreq = max_cpu_freq;

                // Save original CPU freq
                if (original_cpu_freq == 0)
                    original_cpu_freq = get_cpu_freq();

                // Set new freq
                set_cpu_freq(pBaud->cpufreq);
            }

            // Send ack
            sendPacket(seq, PACKET_ID_ACK, NULL, 0);

            if (pBaud->baud != current_baud)
            {
                // Switch baud rate
                serial_flush();
                delay_micros(10000);
                current_baud = pBaud->baud;
                serial_init(current_baud);
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
int main()
{
    min_cpu_freq = get_min_cpu_freq();
    max_cpu_freq = get_max_cpu_freq();

    // Boost
    //set_cpu_freq(get_max_cpu_freq());

    // Initialize hardware
    serial_init(current_baud);
    timer_init();

    // Setup packet decoder
    struct decode_context ctx = {0};
    ctx.onError = onPacketError;
    ctx.onPacket = onPacketReceived;
    ctx.pBuf = decoder_buf;
    ctx.cbBuf = sizeof(decoder_buf);

    // Main loop
    while (true)
    {
        // Read serial bytes into fifo
        int recv_byte;
        while ((recv_byte = serial_try_recv()) >= 0)
            packet_decode(&ctx, recv_byte);

        unsigned tick = micros();

        // If no packets received for reset timeout period, then reset
        if (reset_timeout_millis != 0 && 
            tick - last_received_packet_time > (reset_timeout_millis * 1000))
        {
            // Reset baud rate
            if (current_baud != default_baud)
            {
                current_baud = default_baud;
                serial_init(current_baud);
            }
            
            // Reset CPU freq
            restore_cpu_freq();
        }


        // When in default baud mode and it's been more than half a second since
        // received a packet, flash the alive heart beat
        if (current_baud == default_baud && tick - last_received_packet_time > 500000)
        {
            unsigned insec = (tick / 1000) % 1000;
            if (insec < 500)
                set_activity_led(0);
            else
                set_activity_led((insec / 125) & 1);
        }
    }
}

