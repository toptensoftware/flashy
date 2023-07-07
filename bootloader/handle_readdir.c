#include "common.h"

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


// File system state
DIR readdir;
uint32_t readdir_token = 0;

// Handler
int handle_readdir_internal(uint32_t seq, const void* p, uint32_t cb)
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

void handle_readdir(uint32_t seq, const void* p, uint32_t cb)
{
    int err = handle_readdir_internal(seq, p, cb);
    if (err)
    {
        sendPacket(seq, PACKET_ID_ACK, &err, sizeof(err));
    }
}

