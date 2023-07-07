
#include <stdbool.h>
#include <string.h>
#include "raspi.h"
#include "packenc.h"
#include "crc32.h"
#include "diskio.h"
#include "./ff15/source/ff.h"


// The default baud rate
#define default_baud 115200

// Maximum supported packet size
#define max_packet_size 4096

// Packet decoder buffer
uint8_t decoder_buf[max_packet_size];
uint8_t response_buf[max_packet_size];

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

// File system state
DIR readdir;
uint32_t readdir_token = 0;

#define PACKED __attribute__((__packed__))

typedef struct PACKED
{
    uint8_t major;
    uint8_t minor;
    uint8_t build;
    uint8_t revision;
} VERSION;

// Packets ID
enum PACKET_ID
{
    PACKET_ID_PING = 0,
    PACKET_ID_ACK = 1,
    PACKET_ID_ERROR = 2,
    PACKET_ID_DATA = 3,
    PACKET_ID_GO = 4,
    PACKET_ID_REQUEST_BAUD = 5,
    PACKET_ID_READDIR = 6,
};

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

// Error report packet
// device -> host sent on packet decode error
typedef struct PACKED
{
    int code;
} PACKET_ERROR;

// Data packet
// host -> device - program data to be loaded
typedef struct PACKED
{
    uint32_t address;
    uint8_t data[0];
} PACKET_DATA;

// Go command packet
// host -> device - instructs bootloader to transfer control to loaded program
typedef struct PACKED
{
    uint32_t startAddress;      // 0xFFFFFFFF to use default for platform
    uint32_t delayMillis;       // optional delay in millisecond before launching
} PACKET_GO;

// Request baud packet
// host -> device - requests bootloader switch to a different baud rate
//                  if no packets are received for a `reset_timeout_millis` then
//                  bootloader will revert to `default_baud`.
typedef struct PACKED
{
    uint32_t baud;
    uint32_t reset_timeout_millis;
    uint32_t cpufreq;
} PACKET_REQUEST_BAUD;

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
    uart_send(byte);
}

// Send a packet to the host
void sendPacket(uint32_t seq, uint32_t id, const void* pData, uint32_t cbData)
{
    packet_encode(send_byte, seq, id, pData, cbData);
}

// Request a directory listing
// host -> device
typedef struct PACKED
{
    uint32_t token;             // Continuation token to continue previous directory listing
    char     szDirectory[];     // Null terminated string with directory to read (ignored if cont_token != 0)
} PACKET_READDIR;

// Directory entry
typedef struct PACKED
{
    uint32_t size;              // In bytes
    uint16_t time;              // In FatFS format
    uint16_t date;              // In FatFS format
    uint8_t  attr;              // In FatFS format
    char     name[0];           // Null terminated
} READDIR_ENTRY;

// Response to PACKET_READDIR
// device -> host
typedef struct PACKED
{
    int32_t  error;             // Error code
    uint32_t token;             // A token to get the next packet of this directory listing
    uint32_t count;             // Number of entries in this packet
    READDIR_ENTRY entries[0];   // The entries
} PACKET_READDIR_ACK;


int handle_readdir(uint32_t seq, const void* p)
{
    // Crack packet
    const PACKET_READDIR* pReadDir = (const PACKET_READDIR*)p;

    // Setup response packet
    PACKET_READDIR_ACK* ack = (PACKET_READDIR_ACK*)response_buf;
    ack->error = 0;
    ack->token = 0;
    ack->count = 0;

    // Make sure card mounted
    int err = mount_sdcard();
    if (err)
        return err;

    // Continue?
    if (pReadDir->token)
    {
        // Check token matches
        if (pReadDir->token != readdir_token)
            return -1;
    }
    else
    {
        // Close old readdir
        if (readdir_token)
        {
            f_closedir(&readdir);
            readdir_token = 0;
        }

        // Open the directory
        err = f_opendir(&readdir, pReadDir->szDirectory);
        if (err)
            return err;

        readdir_token = micros();
    }

    // Read the directory
    READDIR_ENTRY* pEntry = (READDIR_ENTRY*)&ack->entries[0];
    while (true)
    {
        FILINFO fi;
        err = f_readdir(&readdir, &fi);

        // Error?
        if (err != FR_OK)
        {
            f_closedir(&readdir);
            readdir_token = 0;
            return err;
        }

        // End of directory?
        if (fi.fname[0] == '\0')
        {
            f_closedir(&readdir);
            readdir_token = 0;
            break;
        }

        // Copy entry
        pEntry->size = fi.fsize;
        pEntry->time = fi.ftime;
        pEntry->date = fi.fdate;
        pEntry->attr = fi.fattrib;
        strcpy(pEntry->name, fi.fname);

        // Bump count
        ack->count++;

        // Move to next
        pEntry = (READDIR_ENTRY*)(pEntry->name + strlen(pEntry->name) + 1);

        // Quit if out of room (must have room for 1x max length record)
        if (((uint8_t*)pEntry + sizeof(READDIR_ENTRY) + FF_LFN_BUF + 4) > (response_buf + sizeof(response_buf)))
            break;
    }

    // Send response
    ack->token = readdir_token;
    sendPacket(seq, PACKET_ID_ACK, ack, (uint8_t*)pEntry - (uint8_t*)ack);
    return 0;
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
            VERSION ver = { FLASHY_VERSION };
            // Send ack packet with version and current rate
            PACKET_PING_ACK ack;
            ack.version = ver;
            ack.raspi = RASPI;
            ack.aarch = AARCH;
            ack.boardrev = get_board_revision ();
            ack.boardserial = get_board_serial ();
            ack.maxpacketsize = sizeof(decoder_buf);
            ack.cpu_freq = get_cpu_freq();
            ack.min_cpu_freq = min_cpu_freq;
            ack.max_cpu_freq = max_cpu_freq;
            sendPacket(seq, PACKET_ID_ACK, &ack, sizeof(ack));
            break;
        }

        case PACKET_ID_DATA:
        {
            // Flash activity led
            set_activity_led(1);

            // Cast packet
            PACKET_DATA* pData = (PACKET_DATA*)p;

            // Copy packet data to memory
            memcpy((void*)(size_t)pData->address, pData->data, cbData - sizeof(PACKET_DATA));

            // Send ack
            sendPacket(seq, PACKET_ID_ACK, NULL, 0);

            set_activity_led(0);

            break;
        }

        case PACKET_ID_GO:
        {
            // Cast packet
            PACKET_GO* pGo = (PACKET_GO*)p;

            // Send ack
            sendPacket(seq, PACKET_ID_ACK, NULL, 0);

            // Restore clock rate
            restore_cpu_freq();

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
            PACKET_REQUEST_BAUD* pBaud = (PACKET_REQUEST_BAUD*)p;

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
                uart_flush();
                delay_micros(10000);
                current_baud = pBaud->baud;
                uart_init(current_baud);
            }

            break;
        }

        case PACKET_ID_READDIR:
        {
            int err = handle_readdir(seq, p);
            if (err)
            {
                sendPacket(seq, PACKET_ID_ACK, &err, sizeof(err));
            }
            break;
        }
    }
}

// Callback to handle packet decode errors
// Just pass to host for informational/debugging purposes
void onPacketError(int code)
{
    PACKET_ERROR err;
    err.code = code;
    sendPacket(0, PACKET_ID_ERROR, &err, sizeof(err));
}

// Main
int main()
{
    // Initialize hardware
    timer_init();
    uart_init(current_baud);
    min_cpu_freq = get_min_cpu_freq();
    max_cpu_freq = get_max_cpu_freq();

    // Setup packet decoder
    decode_context ctx = {0};
    ctx.onError = onPacketError;
    ctx.onPacket = onPacketReceived;
    ctx.pBuf = decoder_buf;
    ctx.cbBuf = sizeof(decoder_buf);

    // Main loop
    while (true)
    {
        // Read serial bytes into fifo
        int recv_byte;
        while ((recv_byte = uart_try_recv()) >= 0)
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
                uart_init(current_baud);
            }
            
            // Reset CPU freq
            restore_cpu_freq();
        }


        // When in default baud mode and it's been more than half a second since
        // received a packet, flash the alive heart beat
        if (current_baud == default_baud && tick - last_received_packet_time > 500000)
        {
            const unsigned flash_parity = 1;            // invert flash pattern
            unsigned insec = (tick / 1000) % 1000;
            if (insec < 500)
                set_activity_led(flash_parity);
            else
                set_activity_led(((insec / 125) & 1) ^ flash_parity);
        }
    }
}

