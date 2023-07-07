#include "raspi.h"

// Error codes sdcard functions
#define E_SD_RESET_FAILED		1
#define E_SD_VERSION			2
#define E_SD_NO_CARD			3
#define E_SD_NO_CLOCK			4
#define E_SD_CLOCK_NOT_STABLE	5
#define E_SD_CMD_TIMEOUT		6
#define E_SD_CMD_ERROR			7
#define E_SD_RESPONSE_ERROR		8
#define E_SD_INCOMPATIBLE_CARD	9
#define E_SD_RCA_ERROR			10
#define E_SD_SELECTCARD			11
#define E_SD_READ_ERROR			12
#define E_SD_READ_TIMEOUT		13
#define E_SD_WRITE_ERROR		14
#define E_SD_WRITE_TIMEOUT		15
#define E_SD_INTERNAL			16
#define E_SD_NOT_INITIALIZED	17

// Initializes the sd card if not already initialized and then
// resets its.  Also sets up pin modes
int init_sdcard();

// Resets the sd card
int reset_sdcard();

// Read 512 byte blocks from sd card
int read_sdcard(uint32_t blockNumber, uint32_t blockCount, void* pData);

// Write 512 byte blocks to sd card
int write_sdcard(uint32_t blockNumber, uint32_t blockCount, const void* pData);
