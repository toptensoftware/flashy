#include "common.h"
#include "cmdline.h"
#include "chainboot.h"

// The default baud rate
#define default_baud 115200

// Packet decoder buffer
uint8_t decoder_buf[max_packet_size];
uint8_t response_buf[max_packet_size];

// Currently selected baud rate
unsigned current_baud = default_baud;

// Last packet received time
unsigned last_received_packet_time_ms = 0;

// How long to wait on non-default baud rate with no
// received packets before resetting to the default baud rate
uint32_t reset_timeout_millis = 2500;

// Pre throttle up clock rate
uint32_t original_cpu_freq = 0;
uint32_t min_cpu_freq = 0;
uint32_t max_cpu_freq = 0;

uint64_t serial_write_time = 0;

// Set when autochain is pending
bool autochain_armed = false;

// Error report packet
// device -> host sent on packet decode error
typedef struct PACKED
{
    int code;
} PACKET_ERROR;


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
    uint64_t start = micros();
    packet_encode(send_byte, seq, id, pData, cbData);
    serial_write_time += micros() - start;
}


// Receive a packet from the host
void onPacketReceived(uint32_t seq, uint32_t id, const void* p, uint32_t cb)
{
    // Disarm autochain once a packet is received
    autochain_armed = false;

    // Dispatch by id
    switch (id)
    {
        case PACKET_ID_PING:
            handle_ping(seq, p, cb);
            break;

        case PACKET_ID_DATA:
            handle_data(seq, p, cb);
            break;

        case PACKET_ID_GO:
            handle_go(seq, p, cb);
            break;


        case PACKET_ID_REQUEST_BAUD:
            handle_baud_request(seq, p, cb);
            break;

        case PACKET_ID_PULL:
            handle_pull(seq, p, cb);
            break;

        case PACKET_ID_PUSH_DATA:
            handle_push_data(seq, p, cb);
            break;

        case PACKET_ID_PUSH_COMMIT:
            handle_push_commit(seq, p, cb);
            break;

        case PACKET_ID_COMMAND:
            handle_command(seq, p, cb);
            break;
    }

    // Store packet time
    last_received_packet_time_ms = millis();
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

    process_cmdline();

    // Setup packet decoder
    decode_context ctx = {0};
    ctx.onError = onPacketError;
    ctx.onPacket = onPacketReceived;
    ctx.pBuf = decoder_buf;
    ctx.cbBuf = sizeof(decoder_buf);

    // Setup activity pattern
    autochain_armed = cl_autochain_target != NULL && cl_autochain_timeout_millis != 0;

    // Capture start time
    uint32_t start_millis = millis();

    // Main loop
    while (true)
    {
        // Read serial bytes
        int recv_byte;
        while ((recv_byte = uart_try_recv()) >= 0)
            packet_decode(&ctx, recv_byte);

        uint32_t tick_ms = millis();

        // If no packets received for reset timeout period, then reset
        if (reset_timeout_millis != 0 && 
            tick_ms - last_received_packet_time_ms > reset_timeout_millis)
        {
            // Reset baud rate
            if (current_baud != default_baud)
            {
                current_baud = default_baud;
                uart_init(current_baud);
            }
            
            // Reset CPU freq
            restore_cpu_freq();

            // Reset other operations
            reset_push();
        }

        // Time to auto chain?
        if (autochain_armed && (tick_ms - start_millis) > cl_autochain_timeout_millis)
        {
            autochain_armed = false;

            // Make sure SD card mounted
            mount_sdcard();

            if (load_chain_image(cl_autochain_target, NULL) == 0)
                run_chain_image();
        }

        // When in default baud mode and it's been more than half a second since
        // received a packet, flash the alive heart beat
        if (current_baud == default_baud && tick_ms - last_received_packet_time_ms > 500)
        {
            const unsigned activity_parity = 1;            // invert flash pattern
            uint32_t activity_pattern = autochain_armed ? 0x140 : 0x280;
            set_activity_led( (((tick_ms - start_millis) & activity_pattern) == activity_pattern) ^ activity_parity);
        }
    }
}

