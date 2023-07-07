#include "common.h"

// Data packet
// host -> device - program data to be loaded
typedef struct PACKED
{
    uint32_t address;
    uint8_t data[0];
} PACKET_DATA;


void handle_data(uint32_t seq, const void* p, uint32_t cb)
{
    // Flash activity led
    set_activity_led(1);

    // Cast packet
    PACKET_DATA* pData = (PACKET_DATA*)p;

    // Copy packet data to memory
    memcpy((void*)(size_t)pData->address, pData->data, cb - sizeof(PACKET_DATA));

    // Send ack
    sendPacket(seq, PACKET_ID_ACK, NULL, 0);

    set_activity_led(0);
}
