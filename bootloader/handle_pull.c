#include "common.h"


// File info
// device -> host
typedef struct PACKED
{
    uint32_t size;              // The file's size
    uint16_t time;              // In FatFS format
    uint16_t date;              // In FatFS format
    uint8_t attr;               // The file's attributes
    char filename[];            // The file's name
} PACKET_PULL_HEADER;

// File content
// device -> host
typedef struct PACKED
{
    uint32_t offset;            // Data block offset
    uint8_t  data[];            // The file data
} PACKET_PULL_DATA;


// Handler
static int handle_pull_internal(uint32_t seq, const void* p, uint32_t cb)
{
    // Make sure card mounted
    int err = mount_sdcard();
    if (err)
        return err;

    const char* filename = (const char*)p;

    // Stat file
    FILINFO fi;
    err = f_stat(filename, &fi);
    if (err)
        return err;

    // Open file
    FIL file;
    err = f_open(&file, filename, FA_READ);
    if (err)
        return err;

    // Send header
    PACKET_PULL_HEADER* pHeader = (PACKET_PULL_HEADER*)response_buf;
    pHeader->size = fi.fsize;
    pHeader->time = fi.ftime;
    pHeader->date = fi.fdate;
    pHeader->attr = fi.fattrib;
    strcpy(pHeader->filename, fi.fname);
    sendPacket(seq, PACKET_ID_PULL_HEADER, pHeader, sizeof(PACKET_PULL_HEADER) + strlen(fi.fname) + 1);

    PACKET_PULL_DATA* pData = (PACKET_PULL_DATA*)response_buf;
    pData->offset = 0;

    while (true)
    {
        // Read next packet
        uint32_t room = max_packet_size - sizeof(PACKET_PULL_DATA);
        UINT bytes_read;
        set_activity_led(1);
        err = f_read(&file, pData->data, room, &bytes_read);
        set_activity_led(0);
        if (err)
        {
            f_close(&file);
            return err;
        }

        // EOF?
        if (!bytes_read)
            break;

        // Send it
        sendPacket(seq, PACKET_ID_PULL_DATA, pData, sizeof(PACKET_PULL_DATA) + bytes_read);
        pData->offset += bytes_read;
    }

    // Done!
    f_close(&file);
    return 0;
}

void handle_pull(uint32_t seq, const void* p, uint32_t cb)
{
    int err = handle_pull_internal(seq, p, cb);
    sendPacket(seq, PACKET_ID_ACK, &err, sizeof(err));
}

