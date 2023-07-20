#include "common.h"

// Request a directory listing
// host -> device
typedef struct PACKED
{
    uint32_t token;             // Continuation token to continue file pull
    char     szPath[];          // Null terminated string with file name to pull
} PACKET_PULL;

// Response to PACKET_PULL
// device -> host
typedef struct PACKED
{
    int32_t  error;             // Error code
    uint32_t token;             // A token to get the next packet of this file (zero if end of file)
    uint32_t size;              // The file's size
    uint16_t time;              // In FatFS format
    uint16_t date;              // In FatFS format
    uint8_t attr;               // The file's attributes
    uint32_t offset;            // Data block offset
    uint8_t  data[];            // The file data
} PACKET_PULL_ACK;


// File system state
static uint32_t pull_token = 0;
static FIL file;
static FILINFO fi;

// Handler
static int handle_pull_internal(uint32_t seq, const void* p, uint32_t cb)
{
    // Crack packet
    const PACKET_PULL* pPull = (const PACKET_PULL*)p;

    // Setup response packet
    PACKET_PULL_ACK* ack = (PACKET_PULL_ACK*)p;
    ack->error = 0;
    ack->token = 0;

    // Make sure card mounted
    int err = mount_sdcard();
    if (err)
        return err;

    // New or continued request?
    if (pPull->token)
    {
        if (pPull->token != pull_token)
            return -1;
    }
    else
    {
        // Close old file
        reset_pull();

        // Get file stat
        err = f_stat(pPull->szPath, &fi);
        if (err)
            return err;

        // Open the file
        err = f_open(&file, pPull->szPath, FA_READ);
        if (err)
            return err;

        // Setup token
        pull_token = micros();
    }

    // Setup attributes
    ack->time = fi.ftime;
    ack->date = fi.fdate;
    ack->attr = fi.fattrib;
    ack->size = fi.fsize;
    ack->offset = f_tell(&file);

    // Read next packet
    uint32_t room = max_packet_size - sizeof(*ack);
    UINT bytes_read;
    err = f_read(&file, ack->data, room, &bytes_read);
    if (err)
    {
        reset_pull();
        return err;
    }

    // EOF?
    if (bytes_read < room)
    {
        reset_pull();
        pull_token = 0;
    }

    // Send response
    ack->token = pull_token;
    sendPacket(seq, PACKET_ID_ACK, ack, sizeof(*ack) + bytes_read);
    return 0;
}

void handle_pull(uint32_t seq, const void* p, uint32_t cb)
{
    int err = handle_pull_internal(seq, p, cb);
    if (err)
    {
        sendPacket(seq, PACKET_ID_ACK, &err, sizeof(err));
    }   
}

void reset_pull()
{
    if (pull_token != 0)
    {
        f_close(&file);
        pull_token = 0;
    }
}
