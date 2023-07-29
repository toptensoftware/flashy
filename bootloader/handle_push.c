#include "common.h"

// Load data packets to a file
// host -> device
typedef struct PACKED
{
    uint32_t token;             // Continuation token to continue file push
    uint32_t offset;            // File offset of data
    uint8_t  data[];            // The file data
} PACKET_PUSH_DATA;

// Commit pushed data packets to a file
typedef struct PACKED
{
    uint32_t token;             // Continuation token
    uint32_t size;              // File size
    uint16_t time;              // In FatFS format
    uint16_t date;              // In FatFS format
    uint8_t  attr;              // The file's attributes
    uint8_t  overwrite;         // Whether to overwrite existing file (fail if exists)
    char     name[];            // Null terminated string with file name to push
} PACKET_PUSH_COMMIT;

#define temp_filename "/push.tmp"

// File system state
static uint32_t push_token = 0;
static FIL file;

// Handler
static int handle_push_data_internal(uint32_t seq, const void* p, uint32_t cb)
{
    // Crack packet
    const PACKET_PUSH_DATA* pPush = (const PACKET_PUSH_DATA*)p;

    // Make sure card mounted
    int err = mount_sdcard();
    if (err)
        return err;

    // New or continued request?
    if (pPush->offset == 0)
    {
        // Close old file
        reset_push();

        // Open the file
        err = f_open(&file, temp_filename, FA_WRITE|FA_CREATE_ALWAYS);
        if (err)
            return err;

        // Store token
        push_token = pPush->token;
    }
    else
    {
        // Check token matches
        if (pPush->token != push_token)
            return -1;

        // Check offset matches
        if (pPush->offset != f_tell(&file))
            return -2;
    }

    // Write packet
    UINT cbWritten;
    err = f_write(&file, pPush->data, cb - sizeof(PACKET_PUSH_DATA), &cbWritten);
    if (err)
        return err;

    // Sanity check
    if (cbWritten != cb - sizeof(PACKET_PUSH_DATA))
        return -3;

    return 0;
}

static int handle_push_commit_internal(uint32_t seq, const void* p, uint32_t cb)
{
    // Crack packet
    const PACKET_PUSH_COMMIT* pPush = (const PACKET_PUSH_COMMIT*)p;

    // Check token matches
    if (pPush->token != push_token)
        return -1;

    // Check size matches
    if (pPush->size != f_tell(&file))
        return -2;

    // Close temp file
    int err = f_close(&file);
    push_token = 0;
    if (err)
        return err;

    // Set file times
    FILINFO fi;
    fi.fdate = pPush->date;
    fi.ftime = pPush->time;
    err = f_utime(temp_filename, &fi);
    if (err)
    {
        f_unlink(temp_filename);
        return err;
    }

    // Unlink existing file if overwriting
    if (pPush->overwrite)
        f_unlink(pPush->name);

    // Move temp file to final location
    err = f_rename(temp_filename, pPush->name);
    if (err)
    {
        f_unlink(temp_filename);
        return err;
    }

    // Send ok response
    return 0;
}

void handle_push_data(uint32_t seq, const void* p, uint32_t cb)
{
    int err = handle_push_data_internal(seq, p, cb);
    sendPacket(seq, PACKET_ID_ACK, &err, sizeof(err));
}

void handle_push_commit(uint32_t seq, const void* p, uint32_t cb)
{
    int err = handle_push_commit_internal(seq, p, cb);
    sendPacket(seq, PACKET_ID_ACK, &err, sizeof(err));
}

void reset_push()
{
    if (push_token != 0)
    {
        f_close(&file);
        f_unlink(temp_filename);
        push_token = 0;
    }
}
