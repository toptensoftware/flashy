#include "common.h"

// Go command packet
// host -> device - instructs bootloader to transfer control to loaded program
typedef struct PACKED
{
    uint32_t startAddress;      // 0xFFFFFFFF to use default for platform
    uint32_t delayMillis;       // optional delay in millisecond before launching
} PACKET_GO;



void handle_go(uint32_t seq, const void* p, uint32_t cb)
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
}