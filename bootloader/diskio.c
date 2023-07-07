#include "raspi.h"
#include "sdcard.h"

#include "./ff15/source/ff.h"
#include "./ff15/source/diskio.h"

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
    int err = read_sdcard(sector, count, buff);
    if (err)
    {
        return RES_ERROR;
    }
    
    return RES_OK;    
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, LBA_t sector, UINT count)
{
    int err = write_sdcard(sector, count, buff);
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

DWORD get_fattime()
{
    int year = 2023;
    int month = 1;          // 1 - 12
    int day = 1;            // 1 - 31
    int hour = 9;           // 0 - 23    
    int minute = 0;         // 0 - 59
    int seconds = 0;        // 0 - 59
    return (
        (DWORD)(year - 1980) << 25 | 
        (DWORD)month << 21 | 
        (DWORD)day << 16 | 
        (DWORD)hour << 11 | 
        (DWORD)minute << 5 | 
        (DWORD)(seconds / 2) << 0
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
