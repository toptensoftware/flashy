#include "raspi.h"
#include "sdcard.h"

// Referencs:
// https://yannik520.github.io/sdio.html
// https://datasheets.raspberrypi.com/bcm2835/bcm2835-peripherals.pdf

// Trace and Error Logging (disabled)
//#define ERROR(fmt, ...) { }
#define TRACE(fmt, ...) { }
//#define INFO(fmt, ...) {}

void trace(const char* format, ...);
#define INFO trace
#define ERROR trace

// Global variables
static uint32_t g_rca = 0;          // RCA address of initialized card (0 if not initialized)
static bool g_is_sdhc = false;      // True if initialized card supports sdhc

// Time out (milliseconds) for all commands
#define TIMEOUT 500


// EMMC Registers

#define MMIO_32(ofs) ((volatile uint32_t*)(size_t)(PBASE+(ofs)))

#if RASPI <= 3
#define EMMC_BASE		0x300000
#else
#define EMMC_BASE		0x340000
#endif

#define EMMC_ARG2 				MMIO_32(EMMC_BASE + 0x0)
#define EMMC_BLKSIZECNT 		MMIO_32(EMMC_BASE + 0x4)
#define EMMC_ARG1 				MMIO_32(EMMC_BASE + 0x8)
#define EMMC_CMDTM 				MMIO_32(EMMC_BASE + 0xc)
#define EMMC_RESP0 				MMIO_32(EMMC_BASE + 0x10)
#define EMMC_RESP1 				MMIO_32(EMMC_BASE + 0x14)
#define EMMC_RESP2 				MMIO_32(EMMC_BASE + 0x18)
#define EMMC_RESP3 				MMIO_32(EMMC_BASE + 0x1c)
#define EMMC_DATA 				MMIO_32(EMMC_BASE + 0x20)
#define EMMC_STATUS 			MMIO_32(EMMC_BASE + 0x24)
#define EMMC_CONTROL0 			MMIO_32(EMMC_BASE + 0x28)
#define EMMC_CONTROL1 			MMIO_32(EMMC_BASE + 0x2c)
#define EMMC_INTERRUPT 			MMIO_32(EMMC_BASE + 0x30)
#define EMMC_IRPT_MASK 			MMIO_32(EMMC_BASE + 0x34)
#define EMMC_IRPT_EN 			MMIO_32(EMMC_BASE + 0x38)
#define EMMC_CONTROL2 			MMIO_32(EMMC_BASE + 0x3c)
#define EMMC_FORCE_IRPT 		MMIO_32(EMMC_BASE + 0x50)
#define EMMC_BOOT_TIMEOUT 		MMIO_32(EMMC_BASE + 0x70)
#define EMMC_DBG_SEL 			MMIO_32(EMMC_BASE + 0x74)
#define EMMC_EXRDFIFO_CFG 		MMIO_32(EMMC_BASE + 0x80)
#define EMMC_EXRDFIFO_EN 		MMIO_32(EMMC_BASE + 0x84)
#define EMMC_TUNE_STEP 			MMIO_32(EMMC_BASE + 0x88)
#define EMMC_TUNE_STEPS_STD 	MMIO_32(EMMC_BASE + 0x8c)
#define EMMC_TUNE_STEPS_DDR 	MMIO_32(EMMC_BASE + 0x90)
#define EMMC_SPI_INT_SPT 		MMIO_32(EMMC_BASE + 0xf0)
#define EMMC_SLOTISR_VER 		MMIO_32(EMMC_BASE + 0xfc)

// Control 0 register flags
#define CONTROL0_ALT_BOOT_EN    (1 << 22)
#define CONTROL0_BOOT_EN        (1 << 21)
#define CONTROL0_SPI_MODE       (1 << 20)
#define CONTROL0_GAP_IEN        (1 << 19)
#define CONTROL0_READWAIT_EN    (1 << 18)
#define CONTROL0_GAP_RESTART    (1 << 17)
#define CONTROL0_GAP_STOP       (1 << 16)
#define CONTROL0_HCTL_8BIT      (1 << 5)
#define CONTROL0_HCTL_HS_EN     (1 << 2)
#define CONTROL0_HCTL_DWIDTH    (1 << 1)

// Control 1 register flags
#define CONTROL1_SRST_DATA      (1 << 26)
#define CONTROL1_SRST_CMD       (1 << 25)
#define CONTROL1_SRST_HC        (1 << 24)
#define CONTROL1_CLK_EN         (1 << 2)
#define CONTROL1_CLK_STABLE     (1 << 1)
#define CONTROL1_CLK_INTLEN     (1 << 0)

// Status register flags
#define STATUS_CARD_INSERTED        (1 << 16)
#define STATUS_CMD_INHIBIT          (1 << 0)
#define STATUS_DAT_INHIBIT          (1 << 1)
#define STATUS_DAT_ACTIVE           (1 << 2)
#define STATUS_WRITE_TRANSFER       (1 << 8)
#define STATUS_READ_TRANSFER        (1 << 9)

// Command type flags
// Section 4.9 (p117)
// These flags correspond to the EMMC_CMDTM register bits.
#define CMD_RESPONSE_TYPE_NONE		0		
#define CMD_RESPONSE_TYPE_136		(1 << 16)
#define CMD_RESPONSE_TYPE_48		(2 << 16)
#define CMD_RESPONSE_TYPE_48B		(3 << 16)
#define CMD_RESPONSE_TYPE_MASK  	(3 << 16)
#define CMD_RESPONSE_TYPE_R1        (CMD_RESPONSE_TYPE_48 | CMD_CRCCHK_EN)
#define CMD_RESPONSE_TYPE_R1b       (CMD_RESPONSE_TYPE_48B | CMD_CRCCHK_EN)
#define CMD_RESPONSE_TYPE_R2        (CMD_RESPONSE_TYPE_136 | CMD_CRCCHK_EN)
#define CMD_RESPONSE_TYPE_R3        (CMD_RESPONSE_TYPE_48)
#define CMD_RESPONSE_TYPE_R4        (CMD_RESPONSE_TYPE_136)
#define CMD_RESPONSE_TYPE_R5        (CMD_RESPONSE_TYPE_48 | CMD_CRCCHK_EN)
#define CMD_RESPONSE_TYPE_R5b       (CMD_RESPONSE_TYPE_48B | CMD_CRCCHK_EN)
#define CMD_RESPONSE_TYPE_R6        (CMD_RESPONSE_TYPE_48 | CMD_CRCCHK_EN)
#define CMD_RESPONSE_TYPE_R7        (CMD_RESPONSE_TYPE_48 | CMD_CRCCHK_EN)

// Read/write command?
#define CMD_IS_DATA_WRITE           (1 << 21)
#define CMD_IS_DATA_READ            ((1 << 21) | (1 << 4))

// Command flags
#define CMD_CRCCHK_EN               (1 << 19)
#define CMD_MULTI_BLOCK             (1 << 5)
#define CMD_BLKCNT_EN               (1 << 1)

// Transfer end auto command
#define CMD_AUTO_CMD_EN_NONE        (0 << 2)
#define CMD_AUTO_CMD_EN_CMD12       (1 << 2)
#define CMD_AUTO_CMD_EN_CMD23       (2 << 3)

// Command Type
#define CMD_TYPE_NORMAL             (0 << 22)
#define CMD_TYPE_SUSPEND            (1 << 22)
#define CMD_TYPE_RESUME             (2 << 22)
#define CMD_TYPE_ABORT              (3 << 22)
#define CMD_TYPE_MASK               (3 << 22)

// Psuedo flag used for internal logic
#define CMD_APP_CMD                 (1 << 31)

// Section 4.7.4 (p102)
#define MAKE_CMD(index, flags)      (((index) << 24) | flags)
#define CMD_GO_IDLE_STATE           MAKE_CMD(0, CMD_RESPONSE_TYPE_NONE)
#define CMD_ALL_SEND_CID            MAKE_CMD(2, CMD_RESPONSE_TYPE_R2)
#define CMD_SEND_RELATIVE_ADDRESS   MAKE_CMD(3, CMD_RESPONSE_TYPE_R6)
#define CMD_SWITCH_FUNCTION         MAKE_CMD(6, CMD_RESPONSE_TYPE_R1 | CMD_IS_DATA_READ)
#define CMD_SELECT_CARD             MAKE_CMD(7, CMD_RESPONSE_TYPE_R1b)
#define CMD_SEND_IF_COND            MAKE_CMD(8, CMD_RESPONSE_TYPE_R7)
#define CMD_STOP_TRANSMISSION       MAKE_CMD(12, CMD_TYPE_ABORT | CMD_RESPONSE_TYPE_R1b)
#define CMD_SET_BLOCKLEN            MAKE_CMD(16, CMD_RESPONSE_TYPE_R1)
#define CMD_READ_SINGLE_BLOCK       MAKE_CMD(17, CMD_RESPONSE_TYPE_R1 | CMD_IS_DATA_READ)
#define CMD_READ_MULTIPLE_BLOCK     MAKE_CMD(18, CMD_RESPONSE_TYPE_R1 | CMD_IS_DATA_READ)
#define CMD_SET_BLOCK_COUNT         MAKE_CMD(23, CMD_RESPONSE_TYPE_R1)
#define CMD_WRITE_SINGLE_BLOCK      MAKE_CMD(24, CMD_RESPONSE_TYPE_R1 | CMD_IS_DATA_WRITE)
#define CMD_WRITE_MULTIPLE_BLOCK    MAKE_CMD(25, CMD_RESPONSE_TYPE_R1 | CMD_IS_DATA_WRITE)
#define CMD_APP                     MAKE_CMD(55, CMD_RESPONSE_TYPE_R1)
#define ACMD_SET_BUS_WIDTH          MAKE_CMD(6, CMD_APP_CMD | CMD_RESPONSE_TYPE_R1)
#define ACMD_SEND_OP_COND           MAKE_CMD(41, CMD_APP_CMD | CMD_RESPONSE_TYPE_R3)
#define ACMD_SEND_SCR               MAKE_CMD(51, CMD_APP_CMD | CMD_RESPONSE_TYPE_R1 | CMD_IS_DATA_READ)
#define ACMD_SET_WR_BLK_ERASE_COUNT MAKE_CMD(23, CMD_APP_CMD | CMD_RESPONSE_TYPE_R1)

// INTERRUPT register flags
#define INTERRUPT_FLAG_ACMD_ERR     (1 << 24)
#define INTERRUPT_FLAG_DEND_ERR     (1 << 22)
#define INTERRUPT_FLAG_DEND_ERR     (1 << 22)
#define INTERRUPT_FLAG_DCRC_ERR     (1 << 21)
#define INTERRUPT_FLAG_DTO_ERR      (1 << 20)
#define INTERRUPT_FLAG_CBAD_ERR     (1 << 19)
#define INTERRUPT_FLAG_CEND_ERR     (1 << 18)
#define INTERRUPT_FLAG_CCRC_ERR     (1 << 17)
#define INTERRUPT_FLAG_CTO_ERR      (1 << 16)
#define INTERRUPT_FLAG_ERR          (1 << 15)
#define INTERRUPT_FLAG_ENDBOOT      (1 << 14)
#define INTERRUPT_FLAG_BOOTACK      (1 << 13)
#define INTERRUPT_FLAG_RETUNE       (1 << 12)
#define INTERRUPT_FLAG_CARD         (1 << 8)
#define INTERRUPT_FLAG_READ_RDY     (1 << 5)
#define INTERRUPT_FLAG_WRITE_RDY    (1 << 4)
#define INTERRUPT_FLAG_BLOCK_GAP    (1 << 2)
#define INTERRUPT_FLAG_DATA_DONE    (1 << 1)
#define INTERRUPT_FLAG_CMD_DONE     (1 << 0)

// Card State
// Table 4-42 (p122)
#define CARD_STATE_IDLE             0
#define CARD_STATE_READY            1
#define CARD_STATE_IDENT            2
#define CARD_STATE_STANDYBY         3
#define CARD_STATE_TRAN             4
#define CARD_STATE_DATA             5
#define CARD_STATE_RECV             6
#define CARD_STATE_PRG              7
#define CARD_STATE_DIS              8

// Extract the Card State from a response
#define R1_CARD_STATE(x)                    ((x >> 9) & 0x0F)

// Flags for the response from RCA
#define R6_RESPONSE_CRC_ERROR              (1 << 15)
#define R6_RESPONSE_ILLEGAL_COMMAND_ERROR  (1 << 14)
#define R6_RESPONSE_ERROR                  (1 << 13)
#define R6_RESPONSE_ALL_ERRORS             (R6_RESPONSE_CRC_ERROR | R6_RESPONSE_ILLEGAL_COMMAND_ERROR | R6_RESPONSE_ERROR)
#define R6_RESPONSE_READY                  (1 << 8) 
#define R6_CARD_STATE(x)                   ((x >> 9) & 0x0F)

// Bus width flags in SCR response
#define SD_BUS_WIDTH_1  (1 << 0)
#define SD_BUS_WIDTH_4  (1 << 2)

// Find the left most set bit
uint32_t fls(uint32_t val)
{
	for (int i=31; i>=0; i--)
	{
		if (val & (1 << i))
			return i+1;
	}
	return 0;
}

// This function takes a base clock rate and a target clock rate
// and calculates a divisor such that base/divisor <= target
// under the following restrictions:
// - it must be a power of 2 (ie: only 1 bit set)
// - range must be from 0b00000000010 to 0b10000000000
static uint32_t calc_clock_divider(uint32_t base, uint32_t target)
{
	// If the target rate is higher than the base, the use a 
	if (target >= base)
		return 2;	

	// Start with just the most significant bit of the base/target (the ideal divisor)
	uint32_t divisor = 1 << (fls(base / target) - 1); 

	// If that gives a final frequency that's too high then move up one step
	if (base / divisor > target)
		divisor <<= 1;

	// Constrain range
	if (divisor < 0b00000000010)
		divisor = 0b00000000010;
	if (divisor > 0b10000000000)
		divisor = 0b10000000000;

	// Done
	return divisor;
}

// Set the EMMC frequency divider for a specified frequency
// The actual frequency may not match exactly but won't be 
// more than the requested.
static int set_emmc_freq(uint32_t freq)
{
    INFO("Setting EMMC clock to %iHz...\n", freq);

    // Get clock
    uint32_t clock_freq = get_emmc_freq();
    if (clock_freq == 0)
    {
        ERROR("Failed to get emmc clock frequency\n");
        return E_SD_NO_CLOCK;
    }
    INFO("EMMC Clock: %iMHz\n", clock_freq);

    // Calculate divider
    uint32_t divider = calc_clock_divider(clock_freq, freq);
    INFO("EMMC clock divider is %i giving actual frequency of %iHz\n", divider, clock_freq/divider);

    // Rearrange the divider as required by the register
    // value[2..1] => 7..6  value[8..1] => 15..8 value[0] => discarded
    uint32_t divider_shuffled = 
        (((divider >> 1) & 0xFF) << 8) |
        (((divider >> 9) & 0x03) << 6);

    // Disable clock
    set_register_bits(EMMC_CONTROL1, 0, CONTROL1_CLK_EN);
    delay_micros(2000);

    // Set clock bits
    set_register_bits(EMMC_CONTROL1,
        CONTROL1_CLK_INTLEN | divider_shuffled    | (11 << 16), 
        CONTROL1_CLK_INTLEN | 0b1111111111000000 | (0xF << 16)
    );
    delay_micros(2000);

    // Wait for it to stabilize
    if (!wait_register_any_set(EMMC_CONTROL1, CONTROL1_CLK_STABLE, 1000))
    {
        ERROR("Timeout waiting for clock to stabilize\n");
        return E_SD_CLOCK_NOT_STABLE;
    }

    // Enable Clock
    set_register_bits(EMMC_CONTROL1, CONTROL1_CLK_EN, CONTROL1_CLK_EN );
    delay_micros(5000);

    INFO("Clock enabled\n");

    // Done!
    return 0;
}

// Wait until data transfer is done
static int wait_data_done()
{
    // If data inhibit already clear then we're done
    /*
    if ((*EMMC_STATUS & STATUS_DAT_INHIBIT) == 0)
    {
        *EMMC_INTERRUPT = 0xFFFF8000 | INTERRUPT_FLAG_DATA_DONE;
        return 0;
    }
    */

    // Wait for data done flag
    if (!wait_register_any_set(EMMC_INTERRUPT, INTERRUPT_FLAG_DATA_DONE | INTERRUPT_FLAG_ERR, TIMEOUT))
    {
        ERROR("wait data timeout %08x\n", *EMMC_INTERRUPT);
        return E_SD_CMD_TIMEOUT;
    }

    // Alloow DTO error + data done
    if ((*EMMC_INTERRUPT & (0xFFFF0000|INTERRUPT_FLAG_DATA_DONE)) == (INTERRUPT_FLAG_DTO_ERR | INTERRUPT_FLAG_DATA_DONE))
        return 0;

    // Check error
    if ((*EMMC_INTERRUPT & 0xFFFF8000) != 0)
    {
        ERROR("wait data error %08x\n", *EMMC_INTERRUPT);
        return E_SD_RESPONSE_ERROR;
    }

    // Done
    return 0;
}


// Issue a command to the SD
static int issue_command(uint32_t command, uint32_t arg)
{
    // If it's an app command, then send the app prefix
    // command with the current card address (g_rca)
    if (command & CMD_APP_CMD)
    {
        // Send app command
        int err = issue_command(CMD_APP, g_rca << 16);
        if (err)
            return err;
    }

    // Remove out pseudo app command flag
    command &= ~CMD_APP_CMD;

    TRACE("IssueCommand(%08x, %08x)\n", command, arg);

    // Send the command
    *EMMC_INTERRUPT = 0xFFFFFFFF;           // Clear interrupt flag
    *EMMC_ARG1 = arg;
    *EMMC_CMDTM = command;

    // Wait for command done, or error
    if (!wait_register_any_set(EMMC_INTERRUPT, INTERRUPT_FLAG_CMD_DONE | INTERRUPT_FLAG_ERR, TIMEOUT))
    {
        ERROR("Timeout waiting for command to complete %08x\n", *EMMC_INTERRUPT);
        return E_SD_CMD_TIMEOUT;
    }

    // Error?
    if ((*EMMC_INTERRUPT & 0xFFFF8000) != 0)
    {
        ERROR("Command error: %08x\n", *EMMC_INTERRUPT);
        return E_SD_CMD_ERROR;
    }

    // Busy command?
    if ((command & CMD_RESPONSE_TYPE_MASK) == CMD_RESPONSE_TYPE_48B)
    {
        int err = wait_data_done();
        if (err)
            return err;
    }

    // Done
    return 0;
}


// Issue a read command
static int issue_read_command(uint32_t command, uint32_t arg, void* pBuffer, uint16_t blockSize, uint16_t blockCount)
{
    int err;

    TRACE("Read: %08x %08x - %ix%i %08x\n", command, arg, blockSize, blockCount);

    // Set block count if multiple
    if (blockCount > 1)
    {
        // Tell emmc controller to do multi-block transfer
        command |= CMD_MULTI_BLOCK | CMD_BLKCNT_EN | CMD_AUTO_CMD_EN_CMD12;
    }

    // Issue command
    *EMMC_BLKSIZECNT = blockSize | (blockCount << 16);
    err = issue_command(command, arg);
    if (err)
        return err;

    TRACE("Starting read transfer...\n");

    // Transfer data
    uint32_t* p = (uint32_t*)pBuffer;
    for (uint16_t i=0; i<blockCount; i++)
    {
        // Wait for read ready
        if (!wait_register_any_set(EMMC_INTERRUPT, INTERRUPT_FLAG_ERR|INTERRUPT_FLAG_READ_RDY, 1000))
        {
            ERROR("Read timeout: %08x\n", *EMMC_INTERRUPT);
            return E_SD_READ_TIMEOUT;
        }

        // Check no errors
        if (*EMMC_INTERRUPT & INTERRUPT_FLAG_ERR)
        {
            ERROR("Read error: %08x\n", *EMMC_INTERRUPT);
            return E_SD_READ_ERROR;
        }

        // Clear the read ready flag
        *EMMC_INTERRUPT = INTERRUPT_FLAG_READ_RDY;

        // Read block
        for (uint16_t j=0; j < blockSize; j += sizeof(uint32_t))
        {
            *p++ = *EMMC_DATA;
        }
    }    

    TRACE("Read transfer finished (interrupt: %08x)\n", *EMMC_INTERRUPT);

    return 0;
}

// Issue a write command
static int issue_write_command(uint32_t command, uint32_t arg, const void* pBuffer, uint16_t blockSize, uint16_t blockCount)
{
    TRACE("Write: %08x %08x - %ix%i\n", command, arg, blockSize, blockCount);

    int err;

    // Set block count if multiple
    if (blockCount > 1)
    {
        // Set block pre-erase count for faster writes
        err = issue_command(ACMD_SET_WR_BLK_ERASE_COUNT, blockCount);
        if (err)
        {
            ERROR("error %i issuing block pre-erase, ignoring\n", err);
        }

        // Tell emmc controller to do multi-block transfer
        command |= CMD_MULTI_BLOCK | CMD_BLKCNT_EN | CMD_AUTO_CMD_EN_CMD12;
    }

    // Setup block size and count
    *EMMC_BLKSIZECNT = blockSize | (blockCount << 16);

    // Issue command
    err = issue_command(command, arg);
    if (err)
        return err;

    TRACE("Starting write transfer...\n");

    // Transfer data
    uint32_t* p = (uint32_t*)pBuffer;
    for (uint16_t i=0; i<blockCount; i++)
    {
        // Wait for write ready
        if (!wait_register_any_set(EMMC_INTERRUPT, INTERRUPT_FLAG_ERR|INTERRUPT_FLAG_WRITE_RDY, 1000))
        {
            ERROR("Write timeout: %08x\n", *EMMC_INTERRUPT);
            return E_SD_WRITE_TIMEOUT;
        }

        // Check no errors
        if (*EMMC_INTERRUPT & INTERRUPT_FLAG_ERR)
        {
            ERROR("Write error: %08x\n", *EMMC_INTERRUPT);
            return E_SD_WRITE_ERROR;
        }

        // Clear write ready flag
        *EMMC_INTERRUPT = INTERRUPT_FLAG_WRITE_RDY;

        // Write block
        for (uint16_t j=0; j < blockSize; j += sizeof(uint32_t))
        {
            *EMMC_DATA = *p++;
        }
    }    

    TRACE("Write transfer finished (interrupt: %08x)\n", *EMMC_INTERRUPT);

    // Wait till finished
    return wait_data_done();
}


// Get SD version from SCR register response
// Section 5.6 (p240)
// Return as 0x<maj><min>
int sd_version_from_scr(uint32_t scr)
{
    // Response is LE, need it in BE
    scr = __builtin_bswap32(scr);
    uint32_t sd_spec = (scr >> (56-32)) & 0x0F;
    if (sd_spec == 0)
        return 0x10;
    if (sd_spec == 1)
        return 0x11;
    if (sd_spec == 2)
    {
        if ((scr & (1 << (47 - 32))) == 0)
            return 0x20;
        if ((scr & (1 << (42 - 32))) == 0)
            return 0x30;
        return 0x40;
    }
    return 0;
}

// Reset the SD card
int reset_sdcard_internal()
{
    INFO("Resetting SD card...\n");

    // Clear the card address
    g_rca = 0;

    // Read the controller version
	uint32_t sdVersion = (*EMMC_SLOTISR_VER >> 16) & 0xff;
    INFO("SD version: %i\n", sdVersion);
    if (sdVersion < 2)
    {
        ERROR("Unsupported SD version %i\n", sdVersion);
        return E_SD_VERSION;
    }

    // Set reset flag, turn off clocks
    set_register_bits(EMMC_CONTROL1, CONTROL1_SRST_HC, CONTROL1_SRST_HC | CONTROL1_CLK_EN | CONTROL1_CLK_INTLEN);

    // Wait for reset to finish
    if (!wait_register_all_clear(EMMC_CONTROL1, CONTROL1_SRST_HC|CONTROL1_SRST_CMD|CONTROL1_SRST_DATA, 1000))
    {
        ERROR("Timeout waiting for reset command\n");
        return E_SD_RESET_FAILED;
    }
        

    // Enable VDD1 bus power 3.3V
#if RASPI >= 4
    INFO("Enabling power bus 3.3V\n");
    set_register_bits(EMMC_CONTROL0, 0x0F << 8, 0x0F << 8);
    delay_micros(3000);
#endif

    // Check for valid card
    /*
    INFO("Checking for valid card\n");
    if (!wait_register_any_set(EMMC_STATUS, STATUS_CARD_INSERTED, TIMEOUT))
    {
        ERROR("Timeout waiting for card inserted\n");
        return E_SD_NO_CARD;
    }
    */

    // Clear error bits
    *EMMC_CONTROL2 = 0;

    // Setup initialization clock
    int err = set_emmc_freq(400000);
    if (err)
        return err;

    // No interrupts
    *EMMC_IRPT_EN = 0;
    *EMMC_IRPT_MASK = 0xFFFFFFFF;

    // Clear the interrupt register
    // (Note: when writing to the interrupt register, the written bits
    //  are cleared)
    *EMMC_INTERRUPT = 0xFFFFFFFF;

    // Clear block size and count
    *EMMC_BLKSIZECNT = 0;

    // Go to idle state
    INFO("Sending CMD_GO_IDLE_STATE\n");
    err = issue_command(CMD_GO_IDLE_STATE, 0);
    if (err)
        return err;

    // Send voltage check command
    // 0x100 = low voltage bit
    // 0x0AA = check pattern
    INFO("Sending CMD_SEND_IF_COND\n");
    err = issue_command(CMD_SEND_IF_COND, 0x1AA);
    if (err)
        return err;
    uint32_t response = *EMMC_RESP0;
    /*
    err = wait_data_done();
    if (err)
        return err;
    */
    if ((response & 0xFFF) != 0x1AA)
    {
        ERROR("Incompatible card %08x\n", response);
        return E_SD_INCOMPATIBLE_CARD;
    }

    // Request operating conditions
    INFO("Sending ACMD_SEND_OP_COND\n");
    while (true)
    {
        err = issue_command(ACMD_SEND_OP_COND, 
            (1 << 28)  |     // Power Control = Max Performance
            (1 << 30)  |     // Host supports SDHC
            0x00ff8000       // Supported voltage ranges (OCR register Section 5.1 (p222))
        );
        if (err)
            return err;

        // Ready?
        if (*EMMC_RESP0 & 0x80000000)
            break;

        // Wait a bit
        delay_micros(100000);
    }
    //uint32_t ocr = (*EMMC_RESP0 >> 8) & 0xFFFF;
    g_is_sdhc = ((*EMMC_RESP0 >> 30) & 0x01) != 0;
    //bool can18v = ((*EMMC_RESP0 >> 24) & 0x01) != 0;
    //INFO("OCR: %08x %s %s\n", ocr, g_is_sdhc ? "sdhc" : "sdsc", can18v ? "1.8v" : "not 1.8v");
    INFO("SDHC: %s\n", g_is_sdhc ? "yes" : "no");

    // Switch to 25Mhz
    INFO("Switching to 25Mhz\n");
    err = set_emmc_freq(25000000);
    if (err)
        return err;

    // Request CID
    INFO("Requesting CID\n");
    err = issue_command(CMD_ALL_SEND_CID, 0);
    if (err)
        return err;

    /*
    uint32_t cid0 = *EMMC_RESP0;
    uint32_t cid1 = *EMMC_RESP1;
    uint32_t cid2 = *EMMC_RESP2;
    uint32_t cid3 = *EMMC_RESP3;
    INFO("CID: %08x.%08x.%08x.%08x\n", cid0, cid1, cid2, cid3);
    */

    // Request relative address
    INFO("Requesting Relative Address\n");
    err = issue_command(CMD_SEND_RELATIVE_ADDRESS, 0);
    if (err)
        return err;
    // Check no error bits set, the ready bit is set and card state is "ident"
    if ((*EMMC_RESP0 & R6_RESPONSE_ALL_ERRORS) || 
        (*EMMC_RESP0 & R6_RESPONSE_READY) == 0 ||
        R6_CARD_STATE(*EMMC_RESP0) != CARD_STATE_IDENT )
    {
        ERROR("Failed to get RCA (0x%08x)\n", *EMMC_RESP0);
        return E_SD_RCA_ERROR;
    }
    g_rca = (*EMMC_RESP0 >> 16) & 0xFFFF;
    INFO("Relative address: %04x\n", g_rca);

    // Select Card
    INFO("Selecting Card\n");
    err = issue_command(CMD_SELECT_CARD, g_rca << 16);
    if (err)
        return err;
    uint32_t select_card_response = *(EMMC_RESP0);
    uint32_t card_state = R1_CARD_STATE(select_card_response);
    if (card_state != CARD_STATE_STANDYBY && card_state != CARD_STATE_TRAN)
    {
        ERROR("Invalid state after selecting card: %08x\n", card_state);
        return E_SD_SELECTCARD;
    }

    // Setup block length (non-sdhc cards only)
    if (!g_is_sdhc)
    {
        err = issue_command(CMD_SET_BLOCKLEN, 512);
        if (err)
            return err;
    }

    // Get the cards SCR
    INFO("Getting SCR\n");
    uint32_t scr[2];
    scr[0] = 0xBAADF00D;
    scr[1] = 0xBAADF00D;
    err = issue_read_command(ACMD_SEND_SCR, 0, &scr, 8, 1);
    if (err)
        return err;
    INFO("SCR: %.8x %.8x\n", scr[0], scr[1]);

    // Crack SD version
    uint32_t sdVer = sd_version_from_scr(scr[0]);
    INFO("SD version: %i.%i\n", (sdVer >> 4) & 0x0f, sdVer & 0x0f);

    // Crack bus widths
    uint32_t busWidths = (scr[0] >> (48 - 32)) & 0xf;
    INFO("Supported bus widths: 0x%08x\n", busWidths);

    // Try to switch to high speed mode
    // Section 4.3.10 (p78)
    /*
    if (sdVer >= 0x11)
    {
        INFO("Checking for high-speed mode\n");

        // Get switch data
        uint8_t switch_data[64] __attribute__((aligned(16)));
        int err = issue_read_command(CMD_SWITCH_FUNCTION, 0x00FFFFF0, switch_data, sizeof(switch_data), 1);
        if (err)
            return err;

        // Does card support high speed mode?
        if (switch_data[13] & 0x01)
        {
            INFO("High speed 50MHz mode supported, enabling...\n");
            issue_read_command(CMD_SWITCH_FUNCTION, 0x80FFFFF1, switch_data, sizeof(switch_data), 1);
            if (err)
                return err;   

            // Now change clock rate
            delay_micros(10);
            set_emmc_freq(50000000);

            INFO("Successfully switched to high speed mode\n");
        }
    }
    */

    // Switch to 4-bit bus?
    if (busWidths & SD_BUS_WIDTH_4)
    {
        INFO("Switching to 4-bit bus\n");
        err = issue_command(ACMD_SET_BUS_WIDTH, 2);
        if (err)
            return err;

        // Switch EMMC controller to 4-bit bus
        *EMMC_CONTROL0 |= CONTROL0_HCTL_DWIDTH;
    }

    // Success
    INFO("SD Reset Finished\n");
    return 0;
}

int reset_sdcard()
{
    int err = reset_sdcard_internal();
    if (err)
    {
        g_rca = 0;
        g_is_sdhc = false;
    }
    return err;
}



int init_sdcard()
{
    // Already initialized?
    if (g_rca != 0)
        return 0;

    // Setup pins
    set_gpio_pin_mode(34, GPIO_PIN_MODE_INPUT);
    set_gpio_pin_mode(35, GPIO_PIN_MODE_INPUT);
    set_gpio_pin_mode(36, GPIO_PIN_MODE_INPUT);
    set_gpio_pin_mode(37, GPIO_PIN_MODE_INPUT);
    set_gpio_pin_mode(38, GPIO_PIN_MODE_INPUT);
    set_gpio_pin_mode(39, GPIO_PIN_MODE_INPUT);
    set_gpio_pin_mode(48, GPIO_PIN_MODE_ALT3);
    set_gpio_pin_mode(49, GPIO_PIN_MODE_ALT3);
    set_gpio_pin_mode(50, GPIO_PIN_MODE_ALT3);
    set_gpio_pin_mode(51, GPIO_PIN_MODE_ALT3);
    set_gpio_pin_mode(52, GPIO_PIN_MODE_ALT3);

    // Reset
    return reset_sdcard();
}

// Read blocks from the SD card
int read_sdcard(uint32_t blockNumber, uint32_t blockCount, void* pData)
{
    // Check initialized
    if (g_rca == 0)
        return E_SD_NOT_INITIALIZED;

    // Redundant?
    if (blockCount == 0)
        return 0;

    // Pre SDHC cards, use bytes offsets, not block numbers
    if (!g_is_sdhc)
    {
        blockNumber *= 512;
    }

    // Work out command
    uint32_t readCommand = blockCount > 1 ? CMD_READ_MULTIPLE_BLOCK : CMD_READ_SINGLE_BLOCK;

    // Read it!
    return issue_read_command(readCommand, blockNumber, pData, 512, blockCount);
}


// Write blocks to the SD card
int write_sdcard(uint32_t blockNumber, uint32_t blockCount, const void* pData)
{
    // Check initialized
    if (g_rca == 0)
        return E_SD_NOT_INITIALIZED;

    // Redundant?
    if (blockCount == 0)
        return 0;

    // Pre SDHC cards, use bytes offsets, not block numbers
    if (!g_is_sdhc)
    {
        blockNumber *= 512;
    }

    // Work out command
    uint32_t writeCommand = blockCount > 1 ? CMD_WRITE_MULTIPLE_BLOCK : CMD_WRITE_SINGLE_BLOCK;

    // Write it!
    return issue_write_command(writeCommand, blockNumber, pData, 512, blockCount);
}
