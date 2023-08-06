#include <time.h>

#include "raspi.h"
#include "sdcard.h"

#include <ff.h>
#include <diskio.h>

const char* VolumeStr[FF_VOLUMES] = { "sd" };

uint64_t disk_read_time = 0;
uint64_t disk_write_time = 0;

// FatFS file system object
FATFS g_fs;

DSTATUS disk_initialize(BYTE pdrv)
{
    return 0;
}

DSTATUS disk_status(BYTE pdrv)
{
    return 0;    
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, LBA_t sector, UINT count)
{
    uint64_t start_time = micros();
    int err = read_sdcard(sector, count, buff);
    disk_read_time += micros() - start_time;
    if (err)
    {
        return RES_ERROR;
    }
    
    return RES_OK;    
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
    uint64_t start_time = micros();
    int err = write_sdcard(sector, count, buff);
    disk_write_time += micros() - start_time;
    if (err)
    {
        return RES_ERROR;
    }
    
    return RES_OK;    
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
    return RES_OK;    
}

uint64_t g_timeBase_micros = 0;

void set_timeBase(uint64_t current_time_micros)
{
    // Store base time at power on
    g_timeBase_micros = current_time_micros - micros();
}

DWORD get_fattime()
{
    // Get current time in seconds
    time_t current_time_secs = (g_timeBase_micros + micros()) / 1000000;

    // Unpack
    struct tm* ptm = gmtime(&current_time_secs);

    // Convert to DOS format
    return (
        (DWORD)(ptm->tm_year + 1900 - 1980) << 25 | 
        (DWORD)(ptm->tm_mon + 1) << 21 | 
        (DWORD)(ptm->tm_mday) << 16 | 
        (DWORD)(ptm->tm_hour) << 11 | 
        (DWORD)(ptm->tm_min) << 5 | 
        (DWORD)(ptm->tm_sec / 2) << 0
        );
}

bool g_bMounted = false;

int mount_sdcard()
{
    if (g_bMounted)
        return 0;

    // Initialize the SD card
    int err = init_sdcard();
    if (err)
        return 1000 + err;

    // Mount it
    err = f_mount(&g_fs, "", 1);
    if (err)
        return err;

    g_bMounted = true;

    return 0;
}
