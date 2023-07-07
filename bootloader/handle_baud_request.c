#include "common.h"


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


void handle_baud_request(uint32_t seq, const void* p, uint32_t cb)
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
}
