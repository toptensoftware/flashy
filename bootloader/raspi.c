// Portions based on code from the Circle project: 
//     https://github.com/rsta2/circle
// Portions based on code from David Welch's bootloader project:
//     https://github.com/dwelch67/raspberrypi

#include <stddef.h>
#include <string.h>

#include "raspi.h"

#include "../lib/ceelib/ceelib/heap.h"

struct HEAP ceelib_malloc_heap = { 0 };


// ------- Startup -------

extern int main();



int start_main()
{
    // Zero bss section
    extern unsigned char __bss_start;
	extern unsigned char __bss_end;
	memset(&__bss_start, 0, &__bss_end - &__bss_start);

    heap_init(&ceelib_malloc_heap, (void*)0x08000000, 0x08000000);

    // Call the real main
    return main();
}


// ------- Timer -------

#define ARM_TIMER_CTL   (PBASE+0x0000B408)
#define ARM_TIMER_CNT   (PBASE+0x0000B420)

void  timer_init()
{
    // Freq is get_core_clock()/(prescale+1)
    // 0x200 = enable free running counter at ARM_TIMER_CNT
    int prescalar = ((get_core_clock() / 1000000) - 1);
    PUT32(ARM_TIMER_CTL, prescalar << 16 | 0x200);
}

unsigned int ticks()
{
    return(GET32(ARM_TIMER_CNT));
}

#define ARM_SYSTIMER_BASE	(PBASE + 0x3000)

#define ARM_SYSTIMER_CS		(ARM_SYSTIMER_BASE + 0x00)
#define ARM_SYSTIMER_CLO	(ARM_SYSTIMER_BASE + 0x04)
#define ARM_SYSTIMER_CHI	(ARM_SYSTIMER_BASE + 0x08)
#define ARM_SYSTIMER_C0		(ARM_SYSTIMER_BASE + 0x0C)
#define ARM_SYSTIMER_C1		(ARM_SYSTIMER_BASE + 0x10)
#define ARM_SYSTIMER_C2		(ARM_SYSTIMER_BASE + 0x14)
#define ARM_SYSTIMER_C3		(ARM_SYSTIMER_BASE + 0x18)


uint64_t micros()
{
    // Important to read LS32 first.  See "64-bit timer read.write" of
    // https://datasheets.raspberrypi.com/bcm2836/bcm2836-peripherals.pdf
    uint32_t lo = GET32(ARM_SYSTIMER_CLO);
    uint32_t hi = GET32(ARM_SYSTIMER_CHI);
    return ((uint64_t)hi << 32) | lo;
}


void delay_micros(uint64_t period)
{
    uint64_t start = micros();
    while (micros() - start < period)
    {
    }
}

uint32_t millis()
{
    return (uint32_t)(micros() / 1000);
}

void delay_millis(uint32_t millis)
{
    delay_micros(((uint64_t)millis) * 1000);
}



// ------- Mailbox -------

#if 0
// FROM: https://raspberrypi.stackexchange.com/a/135282
// See also: https://jsandler18.github.io/extra/mailbox.html
// See also: https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
[
uint32_t message_length,
uint32_t message_type,
/* tag buffer begin */
    /* tag */
    {
    uint32_t tag;
    // the amount of space you ACTUALLY have
    uint32_t buffer_length; 
    // set to zero. VC sets this to(1<<31) | <the length of the message it WANTED to send>
    // note that this may be greater than `buffer_length`!!!
    uint32_t response_length;
    // your response data is placed here
    uint32_t buffer[buffer_length / sizeof(uint32_t)]
    }, /* repeat as many times as you like */
/* tag buffer end */
uint32_t end_of_message_marker
]
#endif

#define MAILBOX_BASE		(PBASE + 0xB880)
#define MAILBOX0_READ  		(MAILBOX_BASE + 0x00)
#define MAILBOX0_STATUS 	(MAILBOX_BASE + 0x18)
#define MAILBOX1_WRITE		(MAILBOX_BASE + 0x20)
#define MAILBOX1_STATUS 	(MAILBOX_BASE + 0x38)
#define MAILBOX_STATUS_EMPTY	0x40000000
#define MAILBOX_STATUS_FULL	0x80000000

#define BCM_MAILBOX_PROP_OUT	8

#define CODE_REQUEST		    0x00000000
#define CODE_RESPONSE_SUCCESS	0x80000000
#define CODE_RESPONSE_FAILURE	0x80000001

// Property Tags
#define PROPTAG_GET_BOARD_REVISION	0x00010002
#define PROPTAG_GET_BOARD_SERIAL	0x00010004
#define PROPTAG_GET_CLOCK_RATE	    0x00030002
#define PROPTAG_GET_MAX_CLOCK_RATE	0x00030004
#define PROPTAG_GET_MIN_CLOCK_RATE	0x00030007
#define PROPTAG_GET_CLOCK_RATE_MEASURED 0x00030047
#define PROPTAG_SET_CLOCK_RATE		0x00038002
#define PROPTAG_GET_COMMAND_LINE	0x00050001
#define PROPTAG_END		            0x00000000

// Clock IDs
#if RASPI >= 4
#define CLOCK_ID_EMMC		12			// EMMC2
#else
#define CLOCK_ID_EMMC		1
#endif
#define CLOCK_ID_UART       2
#define CLOCK_ID_ARM        3
#define CLOCK_ID_CORE		4


unsigned mbox_writeread(unsigned nData)
{
	while (GET32(MAILBOX1_STATUS) & MAILBOX_STATUS_FULL)
        ;

	PUT32(MAILBOX1_WRITE, BCM_MAILBOX_PROP_OUT | nData);

	unsigned nResult;
	do
	{
		while (GET32(MAILBOX0_STATUS) & MAILBOX_STATUS_EMPTY)
            ;

		nResult = GET32(MAILBOX0_READ);
	}
	while ((nResult & 0xF) != BCM_MAILBOX_PROP_OUT);

	return nResult & ~0xF;
}

struct SinglePropertyTag
{
    uint32_t request_length;
    uint32_t message_type;
    uint32_t tag;
    uint32_t buffer_length;
    uint32_t return_length;
} __attribute__((__packed__));

static size_t align(size_t value, int align)
{
    value = value + align - 1;
    return value - value % align;
}

size_t get_tag(uint32_t tag, void* pBuf, size_t cbBuf)
{
    // Buf must be aligned to sizeof(uint32_t)
    size_t cbBufAligned = align(cbBuf, sizeof(uint32_t));

    // Allocate aligned buffer
    size_t totalSize = sizeof(struct SinglePropertyTag) + cbBufAligned + sizeof(uint32_t);
    void* pTags = __builtin_alloca_with_align(totalSize, 16 * 8);

    // Setup request
    struct SinglePropertyTag* ps = (struct SinglePropertyTag*)pTags;
    ps->request_length = totalSize;
    ps->message_type = CODE_REQUEST;
    ps->tag = tag;
    ps->buffer_length = cbBufAligned;
    ps->return_length = 0;

    // Copy request in
    memcpy(ps + 1, pBuf, cbBuf);

    // Setup end tag
    uint32_t* pEndTag = (uint32_t*)(((uint8_t*)pTags) + totalSize - sizeof(uint32_t));
    *pEndTag = PROPTAG_END;

    // Call it
    mbox_writeread((unsigned)(unsigned long)pTags);

    // Failed?
    if (ps->message_type != CODE_RESPONSE_SUCCESS)
        return 0;

    // Copy response out
    memcpy(pBuf, ps+1, cbBuf);

    // Return the response length
    return ps->return_length & ~0x80000000;
}


unsigned get_core_clock()
{
	// does not work without a short delay with newer firmware on RPi 1
	for(volatile unsigned i = 0; i < 10000; i++);

	unsigned proptag[] __attribute__((aligned(16))) =
	{
		8*4,
		CODE_REQUEST,
		PROPTAG_GET_CLOCK_RATE,
		4*4,
		1*4,
		CLOCK_ID_CORE,
		0,
		PROPTAG_END
	};

	mbox_writeread((unsigned)(unsigned long) &proptag);

	if(proptag[6] != 0)
	{
		return proptag[6];
	}

	unsigned proptag_measured[] __attribute__((aligned(16))) =
	{
		8*4,
		CODE_REQUEST,
		PROPTAG_GET_CLOCK_RATE_MEASURED,
		4*4,
		1*4,
		CLOCK_ID_CORE,
		0,
		PROPTAG_END
	};

	mbox_writeread((unsigned)(unsigned long) &proptag_measured);

	return proptag_measured[6];
}

uint64_t get_board_serial()
{
	unsigned proptag[] __attribute__((aligned(16))) =
	{
	    8*4,
		CODE_REQUEST,
		PROPTAG_GET_BOARD_SERIAL,
		2*4,
		0,
		0,
        0,
		PROPTAG_END
	};

	mbox_writeread((unsigned)(unsigned long) &proptag);

    return (((uint64_t)proptag[5]) << 32) | proptag[6];
}

unsigned get_board_revision()
{
	unsigned proptag[] __attribute__((aligned(16))) =
	{
		7*4,
		CODE_REQUEST,
		PROPTAG_GET_BOARD_REVISION,
		1*4,
		0,

		0,

		PROPTAG_END
	};

	mbox_writeread((unsigned)(unsigned long) &proptag);

    return proptag[5];
}

int get_major_model()
{
    return get_major_model_from_board_revision(get_board_revision());
}

// Work out the major pi model (1-4) based on revision number
int get_major_model_from_board_revision(uint32_t revision)
{
    // Old revision number?
    if ((revision & (1 << 23)) == 0)
        return 1;

    // New revision number
    switch ((revision >> 4) & 0xFF)
    {
        case 0:
        case 1:
        case 2:
        case 3:
        case 6:
        case 9:
        case 12:
            return 1;

        case 4:
            return 2;
        
        case 8:
        case 10:
        case 13:
        case 14:
        case 16:
            return 3;
        
        case 18:
        case 17:
        case 19:
        case 20:
        case 21:
            return 4;

        default:
            return -1;
    }
}


size_t get_command_line(char* pBuf, size_t cbBuf)
{
    size_t length = get_tag(PROPTAG_GET_COMMAND_LINE, pBuf, cbBuf);
    if (length < cbBuf)
        pBuf[length] = '\0';
    return length;
}


// ------- Clocks -------

unsigned get_clock_freq(uint32_t tag, uint32_t clock_id)
{
	unsigned proptag[] __attribute__((aligned(16))) =
	{
		8*4,
		CODE_REQUEST,
		tag,
		2*4,
		1*4,

		clock_id,
		0,

		PROPTAG_END
	};

	mbox_writeread((unsigned)(unsigned long) &proptag);

	return proptag[6];
}


// ------- CPU Freq -------



unsigned get_cpu_freq()
{
    return get_clock_freq(PROPTAG_GET_CLOCK_RATE, CLOCK_ID_ARM);
}

void set_cpu_freq(uint32_t value)
{
	unsigned proptag[] __attribute__((aligned(16))) =
	{
		9*4,
		CODE_REQUEST,
		PROPTAG_SET_CLOCK_RATE,
		3*4,
		3*4,

		CLOCK_ID_ARM,
		value,
        0, //SKIPSETTINGTURBO

		PROPTAG_END,
	};

	mbox_writeread((unsigned)(unsigned long) &proptag);
    delay_micros(355); // See: linux/drivers/cpufreq/bcm2835-cpufreq.c
}

unsigned get_min_cpu_freq()
{
    return get_clock_freq(PROPTAG_GET_MIN_CLOCK_RATE, CLOCK_ID_ARM);
}

unsigned get_max_cpu_freq()
{
    return get_clock_freq(PROPTAG_GET_MAX_CLOCK_RATE, CLOCK_ID_ARM);
}

unsigned get_measured_cpu_freq()
{
    return get_clock_freq(PROPTAG_GET_CLOCK_RATE_MEASURED, CLOCK_ID_ARM);
}

uint32_t get_emmc_freq()
{
    return get_clock_freq(PROPTAG_GET_CLOCK_RATE, CLOCK_ID_EMMC);
}




// ------- GPIO -------

#define GPFSEL0	       (PBASE+0x00200000)
#define GPFSEL1        (PBASE+0x00200004)
#define GPSET0         (PBASE+0x0020001C)
#define GPCLR0         (PBASE+0x00200028)
#define GPLEV0         (PBASE+0x00200034)
#define GPPUD          (PBASE+0x00200094)
#define GPPUDCLK0      (PBASE+0x00200098)
#ifdef RASPI4
#define GPPINMUXSD	   (PBASE+0x002000D0)
#define GPPUPPDN0	   (PBASE+0x002000E4)
#define GPPUPPDN1	   (PBASE+0x002000E8)
#define GPPUPPDN2	   (PBASE+0x002000EC)
#define GPPUPPDN3	   (PBASE+0x002000F0)
#endif


// Get the register offset and mask for a gpio pin
void gpio_to_register(unsigned pin, unsigned* pRegOffset, unsigned* pRegMask)
{
	*pRegOffset =(pin / 32) * 4;
	*pRegMask = 1 <<(pin % 32);
}

// Write to an GPIO output pin
void write_gpio(unsigned pin, unsigned value)
{
    // Get register offset and mask
    unsigned regOffset, regMask;
    gpio_to_register(pin, &regOffset, &regMask);

    // Write
	unsigned nSetClrReg =(value ? GPSET0 : GPCLR0) + regOffset;
	PUT32(nSetClrReg, regMask);
}

// Read from a GPIO input pin
unsigned read_gpio(unsigned pin)
{
    // Get register offset and mask
    unsigned regOffset, regMask;
    gpio_to_register(pin, &regOffset, &regMask);

    // Read
	return GET32(GPLEV0 + regOffset) & regMask ? 1 : 0;
}

// Set a GPIO pin as input or output
void set_gpio_pin_mode(unsigned pin, int mode)
{
    unsigned nSelReg = GPFSEL0 +(pin / 10) * 4;
	unsigned nShift =(pin % 10) * 3;

	unsigned nValue = GET32(nSelReg);
	nValue &= ~(7 << nShift);
	nValue |= mode << nShift;
	PUT32(nSelReg, nValue);
}

// Set the pull up mode on a GPIO pin
// 0 = no pullup
// 1 = pull down
// 2 = pull up
void set_gpio_pull_mode(unsigned pin, int mode)
{
    // Get register offset and mask
    unsigned regOffset, regMask;
    gpio_to_register(pin, &regOffset, &regMask);

#ifdef RPI4

	unsigned nModeReg = GPPUPPDN0 +(pin / 16) * 4;
	unsigned nShift =(pin % 16) * 2;
	static const unsigned ModeMap[3] = {0, 2, 1};
	unsigned nValue = GET32(nModeReg);
	nValue &= ~(3 << nShift);
	nValue |= ModeMap[mode] << nShift;
	PUT32(nModeReg, nValue);

#else

	unsigned nClkReg = GPPUDCLK0 + regOffset;
	PUT32(GPPUD, mode);
	delay_micros(5);		// 1us should be enough, but to be sure
	PUT32(nClkReg, regMask);
	delay_micros(5);		// 1us should be enough, but to be sure
	PUT32(GPPUD, 0);
	PUT32(nClkReg, 0);

#endif

}


// ------- Activity LED -------

// Given a board revision, return which gpio pin has the activity led
// Returns negitive number if active low
// Returns 0 if not supported
static int led_pin_from_revision(unsigned revision)
{
    if(revision &(1 << 23))
    {
        // new revision
        switch((revision >> 4) & 0xFF)
        {
            case 0: return -16;
            case 1: return -16;
            case 2: return 47;
            case 3: return 47;
            case 4: return 47;
            case 6: return 47;
            case 8: return 0;
            case 9: return -47;
            case 10: return 0;
            case 12: return -47;
            case 13: return 29;
            case 14: return 29;
            case 16: return 0;
            case 17: return 42;
            case 18: return -29;
            case 19: return 42;
            case 20: return 42;
            case 21: return 0;
        }
    }
    else
    {
        switch(revision)
        {
            case 0x02: return -16;
            case 0x03: return -16;
            case 0x04: return -16;
            case 0x05: return -16;
            case 0x06: return -16;
            case 0x07: return -16;
            case 0x08: return -16;
            case 0x09: return -16;
            case 0x0D: return -16;
            case 0x0E: return -16;
            case 0x0F: return -16;
            case 0x10: return 47;
            case 0x11: return 47;
            case 0x12: return 47;
            case 0x13: return 47;
            case 0x14: return 47;
            case 0x15: return 47;
        }
    }

    return 0;
}


int g_activityLed = -1;
unsigned g_activityLedPolarity = 0;

void set_activity_led(unsigned on)
{
    // First time?
    if(g_activityLed == -1)
    {
        unsigned rev = get_board_revision();
        g_activityLed = led_pin_from_revision(rev);
        if(g_activityLed == 0)
            return;

        // Handle active low
        if(g_activityLed < 0)
        {
            g_activityLed = -g_activityLed;
            g_activityLedPolarity = 1;
        }

        // Setup pin
        set_gpio_pin_mode(g_activityLed, GPIO_PIN_MODE_OUTPUT);
        set_gpio_pull_mode(g_activityLed, GPIO_PULL_MODE_NONE);
    }

    // Set it
    write_gpio(g_activityLed, on ^ g_activityLedPolarity);
}



// ------- Reboot -------

#define ARM_PM_BASE		(PBASE + 0x100000)
#define ARM_PM_WDOG		(ARM_PM_BASE + 0x24)
#define ARM_PM_RSTC		(ARM_PM_BASE + 0x1C)
#define ARM_PM_PASSWD		(0x5A << 24)
#define PM_RSTC_WRCFG_FULL_RESET	0x20

void reboot()
{
    PUT32(ARM_PM_WDOG, ARM_PM_PASSWD | 1);	// set some timeout
	PUT32(ARM_PM_RSTC, ARM_PM_PASSWD | PM_RSTC_WRCFG_FULL_RESET);
}




// ------- Mini UART -------

#define AUX_ENABLES    (PBASE+0x00215004)
#define AUX_MU_IO_REG  (PBASE+0x00215040)
#define AUX_MU_IER_REG (PBASE+0x00215044)
#define AUX_MU_IIR_REG (PBASE+0x00215048)
#define AUX_MU_LCR_REG (PBASE+0x0021504C)
#define AUX_MU_MCR_REG (PBASE+0x00215050)
#define AUX_MU_LSR_REG (PBASE+0x00215054)
#define AUX_MU_MSR_REG (PBASE+0x00215058)
#define AUX_MU_SCRATCH (PBASE+0x0021505C)
#define AUX_MU_CNTL_REG (PBASE+0x00215060)
#define AUX_MU_STAT_REG (PBASE+0x00215064)
#define AUX_MU_BAUD_REG (PBASE+0x00215068)

void mini_uart_init(unsigned baud)
{
    unsigned int ra;

    PUT32(AUX_ENABLES,1);
    PUT32(AUX_MU_IER_REG,0);
    PUT32(AUX_MU_CNTL_REG,0);
    PUT32(AUX_MU_LCR_REG,3);
    PUT32(AUX_MU_MCR_REG,0);
    PUT32(AUX_MU_IER_REG,0);
    PUT32(AUX_MU_IIR_REG,0xC6);
    PUT32(AUX_MU_BAUD_REG,(get_core_clock()/8 + baud/2)/baud - 1);
    ra=GET32(GPFSEL1);
    ra&=~(7<<12); //gpio14
    ra|=2<<12;    //alt5
    ra&=~(7<<15); //gpio15
    ra|=2<<15;    //alt5    
    PUT32(GPFSEL1,ra);
    PUT32(GPPUD,0);
    for(ra=0;ra<150;ra++) dummy(ra);
    PUT32(GPPUDCLK0,(1<<14)|(1<<15));
    for(ra=0;ra<150;ra++) dummy(ra);
    PUT32(GPPUDCLK0,0);
    PUT32(AUX_MU_CNTL_REG,3);
}

unsigned int mini_uart_lcr()
{
    return(GET32(AUX_MU_LSR_REG));
}

int mini_uart_try_recv()
{
    if(GET32(AUX_MU_LSR_REG)&0x01)
        return(GET32(AUX_MU_IO_REG)&0xFF);
    else
        return -1;
}

unsigned int mini_uart_recv()
{
    while(1)
    {
        if(GET32(AUX_MU_LSR_REG)&0x01) break;
    }
    return(GET32(AUX_MU_IO_REG)&0xFF);
}

unsigned int mini_uart_check()
{
    if(GET32(AUX_MU_LSR_REG)&0x01) return(1);
    return(0);
}

void mini_uart_send(unsigned int c)
{
    while(1)
    {
        if(GET32(AUX_MU_LSR_REG)&0x20) break;
    }
    PUT32(AUX_MU_IO_REG,c);
}

void mini_uart_flush()
{
    while(1)
    {
        if((GET32(AUX_MU_LSR_REG)&0x100)==0) break;
    }
}

void mini_uart_send_str(const char* psz)
{
    while(*psz)
    {
        mini_uart_send(*psz++);
    }
}


// ------- UART -------

#define UART_BASE (PBASE + 0x201000)

#define ARM_UART_DR		   (UART_BASE + 0x00)
#define ARM_UART_FR     	(UART_BASE + 0x18)
#define ARM_UART_IBRD   	(UART_BASE + 0x24)
#define ARM_UART_FBRD   	(UART_BASE + 0x28)
#define ARM_UART_LCRH   	(UART_BASE + 0x2C)
#define ARM_UART_CR     	(UART_BASE + 0x30)
#define ARM_UART_IFLS   	(UART_BASE + 0x34)
#define ARM_UART_IMSC   	(UART_BASE + 0x38)
#define ARM_UART_RIS    	(UART_BASE + 0x3C)
#define ARM_UART_MIS    	(UART_BASE + 0x40)
#define ARM_UART_ICR    	(UART_BASE + 0x44)

// Definitions from Raspberry PI Remote Serial Protocol.
//     Copyright 2012 Jamie Iles, jamie@jamieiles.com.
//     Licensed under GPLv2
#define DR_OE_MASK		(1 << 11)
#define DR_BE_MASK		(1 << 10)
#define DR_PE_MASK		(1 << 9)
#define DR_FE_MASK		(1 << 8)

#define FR_TXFE_MASK		(1 << 7)
#define FR_RXFF_MASK		(1 << 6)
#define FR_TXFF_MASK		(1 << 5)
#define FR_RXFE_MASK		(1 << 4)
#define FR_BUSY_MASK		(1 << 3)

#define LCRH_SPS_MASK		(1 << 7)
#define LCRH_WLEN8_MASK		(3 << 5)
#define LCRH_WLEN7_MASK		(2 << 5)
#define LCRH_WLEN6_MASK		(1 << 5)
#define LCRH_WLEN5_MASK		(0 << 5)
#define LCRH_FEN_MASK		(1 << 4)
#define LCRH_STP2_MASK		(1 << 3)
#define LCRH_EPS_MASK		(1 << 2)
#define LCRH_PEN_MASK		(1 << 1)
#define LCRH_BRK_MASK		(1 << 0)

#define CR_CTSEN_MASK		(1 << 15)
#define CR_RTSEN_MASK		(1 << 14)
#define CR_OUT2_MASK		(1 << 13)
#define CR_OUT1_MASK		(1 << 12)
#define CR_RTS_MASK		(1 << 11)
#define CR_DTR_MASK		(1 << 10)
#define CR_RXE_MASK		(1 << 9)
#define CR_TXE_MASK		(1 << 8)
#define CR_LBE_MASK		(1 << 7)
#define CR_UART_EN_MASK		(1 << 0)

#define IFLS_RXIFSEL_SHIFT	3
#define IFLS_RXIFSEL_MASK	(7 << IFLS_RXIFSEL_SHIFT)
#define IFLS_TXIFSEL_SHIFT	0
#define IFLS_TXIFSEL_MASK	(7 << IFLS_TXIFSEL_SHIFT)
	#define IFLS_IFSEL_1_8		0
	#define IFLS_IFSEL_1_4		1
	#define IFLS_IFSEL_1_2		2
	#define IFLS_IFSEL_3_4		3
	#define IFLS_IFSEL_7_8		4

#define INT_OE			(1 << 10)
#define INT_BE			(1 << 9)
#define INT_PE			(1 << 8)
#define INT_FE			(1 << 7)
#define INT_RT			(1 << 6)
#define INT_TX			(1 << 5)
#define INT_RX			(1 << 4)
#define INT_DSRM		(1 << 3)
#define INT_DCDM		(1 << 2)
#define INT_CTSM		(1 << 1)

static unsigned get_uart_clock()
{
	// does not work without a short delay with newer firmware on RPi 1
	for(volatile unsigned i = 0; i < 10000; i++);

	unsigned proptag[] __attribute__((aligned(16))) =
	{
		8*4,
		CODE_REQUEST,
		PROPTAG_GET_CLOCK_RATE,
		4*4,
		1*4,
		CLOCK_ID_UART,
		0,
		PROPTAG_END
	};

	mbox_writeread((unsigned)(unsigned long) &proptag);

    return proptag[6];
}

void uart_init(unsigned baud)
{
    uart_init_ex(baud, 8, 1, 0);
}

void uart_init_ex(unsigned baud, int dataBits, int stopBits, int parity)
{
    // TX Pin
    set_gpio_pin_mode(14, GPIO_PIN_MODE_ALT0);
    set_gpio_pull_mode(15, GPIO_PULL_MODE_NONE);

    // RX Pin
    set_gpio_pin_mode(15, GPIO_PIN_MODE_ALT0);
    set_gpio_pull_mode(15, GPIO_PULL_MODE_UP);

    // Work out clock fractions
	unsigned nClockRate = get_uart_clock();
	unsigned nBaud16 = baud * 16;
	unsigned nIntDiv = nClockRate / nBaud16;
	unsigned nFractDiv2 =(nClockRate % nBaud16) * 8 / baud;
	unsigned nFractDiv = nFractDiv2 / 2 + nFractDiv2 % 2;


	PUT32(ARM_UART_IMSC, 0);
	PUT32(ARM_UART_ICR,  0x7FF);
	PUT32(ARM_UART_IBRD, nIntDiv);
	PUT32(ARM_UART_FBRD, nFractDiv);

	// line parameters
	uint32_t nLCRH = LCRH_FEN_MASK;
	switch(dataBits)
	{
        case 5:		nLCRH |= LCRH_WLEN5_MASK;	break;
        case 6:		nLCRH |= LCRH_WLEN6_MASK;	break;
        case 7:		nLCRH |= LCRH_WLEN7_MASK;	break;
        case 8:		nLCRH |= LCRH_WLEN8_MASK;	break;
	}


	if(stopBits == 2)
	{
		nLCRH |= LCRH_STP2_MASK;
	}

	switch(parity)
	{
        case 0:
            break;

        case 1:
            nLCRH |= LCRH_PEN_MASK;
            break;

        case 2:
            nLCRH |= LCRH_PEN_MASK | LCRH_EPS_MASK;
            break;
	}

    PUT32(ARM_UART_LCRH, nLCRH);

	PUT32(ARM_UART_CR, CR_UART_EN_MASK | CR_TXE_MASK | CR_RXE_MASK);
}

int uart_try_recv()
{
    // Available?
    if(GET32(ARM_UART_FR) & FR_RXFE_MASK)
        return -1;

    // Read byte and check for errors
    uint32_t nDR = GET32(ARM_UART_DR);

    // Fast check for no error
    if ((nDR & (DR_BE_MASK|DR_OE_MASK|DR_FE_MASK|DR_PE_MASK)) == 0)
        return nDR & 0xFF;

    // Convert mask to error code
    if(nDR & DR_BE_MASK)
        return -2;
    if(nDR & DR_OE_MASK)
        return -3;
    if(nDR & DR_FE_MASK)
        return -4;
    if(nDR & DR_PE_MASK)
        return -5;

    // Unknown error
    return -6;
}

uint8_t uart_recv()
{
    int ch;
    while((ch = uart_try_recv()) < 0)
        ;

    return(char)ch;
}

unsigned int uart_check()
{
    if(GET32(ARM_UART_FR) & FR_RXFE_MASK)
        return 0;
    else
        return 1;
}

void uart_send(unsigned int c)
{
    while(GET32(ARM_UART_FR) & FR_TXFF_MASK)
    {
        // do nothing
    }

    PUT32(ARM_UART_DR, c);
}

void uart_flush()
{
	while(GET32(ARM_UART_FR) & FR_BUSY_MASK)
	{
		// just wait
	}
}


void uart_send_hex4(int rc)
{
    if(rc>9) rc+=0x37; else rc+=0x30;
    uart_send(rc);
}

void uart_send_hex8(int val)
{
    uart_send_hex4((val >> 4) & 0x0f);
    uart_send_hex4(val & 0x0f);
}

void uart_send_hex32(unsigned int d)
{
    //unsigned int ra;
    unsigned int rb;
    unsigned int rc;

    rb=32;
    while(1)
    {
        rb-=4;
        rc=(d>>rb)&0xF;
        if(rc>9) rc+=0x37; else rc+=0x30;
        uart_send(rc);
        if(rb==0) break;
    }
    uart_send(0x20);
}

void uart_send_dec(unsigned int d)
{
    if (d > 10)
    {
        uart_send_dec(d / 10);
    }
    uart_send('0' + (d % 10));
}

void uart_sendln_hex32(unsigned int d)
{
    uart_send_hex32(d);
    uart_send(0x0D);
    uart_send(0x0A);
}

void uart_send_str(const char* psz)
{
    while(*psz)
    {
        uart_send(*psz++);
    }
}


// Set some bits in a register
void set_register_bits(uint32_t volatile* reg, uint32_t set, uint32_t mask)
{
    uint32_t r = *reg;
    r &= ~mask;
    r |= set;
    *reg = r;
}


// Wait until one or more bits in a register are set
bool wait_register_any_set(uint32_t volatile* reg, uint32_t mask, uint32_t timeout_millis)
{
    for (uint32_t i = 0; i<timeout_millis; i++)
    {
        if ((*reg & mask) != 0)
            return true;
        delay_micros(1000);
    }
    return false;
}


// Wait until all mask bits in a register are clear
bool wait_register_all_clear(uint32_t volatile* reg, uint32_t mask, uint32_t timeout_millis)
{
    for (uint32_t i = 0; i<timeout_millis; i++)
    {
        if ((*reg & mask) == 0)
            return true;
        delay_micros(1000);
    }
    return false;
}

