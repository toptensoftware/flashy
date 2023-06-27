#include "periph.h"
#include <stddef.h>
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

#if defined (RPI4)
#include "BCM2711.h" /* Raspberry Pi 4 */
#elif defined (RPI2) || AARCH == 64
#include "BCM2836.h" /* Raspberry Pi 2 */
#else
#include "BCM2835.h" /* Original B,A,A+,B+ */
#endif


#define ARM_TIMER_CTL   (PBASE+0x0000B408)
#define ARM_TIMER_CNT   (PBASE+0x0000B420)

#define GPFSEL0	        (PBASE+0x00200000)
#define GPFSEL1         (PBASE+0x00200004)
#define GPSET0          (PBASE+0x0020001C)
#define GPCLR0          (PBASE+0x00200028)
#define GPLEV0          (PBASE+0x00200034)


#define GPPUD           (PBASE+0x00200094)
#define GPPUDCLK0       (PBASE+0x00200098)

#ifdef RPI4
#define GPPINMUXSD	    (PBASE+0x002000D0)
#define GPPUPPDN0	    (PBASE+0x002000E4)
#define GPPUPPDN1	    (PBASE+0x002000E8)
#define GPPUPPDN2	    (PBASE+0x002000EC)
#define GPPUPPDN3	    (PBASE+0x002000F0)
#endif

#define AUX_ENABLES     (PBASE+0x00215004)
#define AUX_MU_IO_REG   (PBASE+0x00215040)
#define AUX_MU_IER_REG  (PBASE+0x00215044)
#define AUX_MU_IIR_REG  (PBASE+0x00215048)
#define AUX_MU_LCR_REG  (PBASE+0x0021504C)
#define AUX_MU_MCR_REG  (PBASE+0x00215050)
#define AUX_MU_LSR_REG  (PBASE+0x00215054)
#define AUX_MU_MSR_REG  (PBASE+0x00215058)
#define AUX_MU_SCRATCH  (PBASE+0x0021505C)
#define AUX_MU_CNTL_REG (PBASE+0x00215060)
#define AUX_MU_STAT_REG (PBASE+0x00215064)
#define AUX_MU_BAUD_REG (PBASE+0x00215068)




//GPIO14  TXD0 and TXD1
//GPIO15  RXD0 and RXD1
//------------------------------------------------------------------------
#define MAILBOX_BASE		(PBASE + 0xB880)

#define MAILBOX0_READ  		(MAILBOX_BASE + 0x00)
#define MAILBOX0_STATUS 	(MAILBOX_BASE + 0x18)
	#define MAILBOX_STATUS_EMPTY	0x40000000
#define MAILBOX1_WRITE		(MAILBOX_BASE + 0x20)
#define MAILBOX1_STATUS 	(MAILBOX_BASE + 0x38)
	#define MAILBOX_STATUS_FULL	0x80000000

#define BCM_MAILBOX_PROP_OUT	8

#define CODE_REQUEST		0x00000000
#define CODE_RESPONSE_SUCCESS	0x80000000
#define CODE_RESPONSE_FAILURE	0x80000001

#define PROPTAG_GET_BOARD_REVISION	0x00010002
#define PROPTAG_GET_BOARD_SERIAL	0x00010004
#define PROPTAG_GET_CLOCK_RATE	0x00030002
#define PROPTAG_GET_MAX_CLOCK_RATE	0x00030004
#define PROPTAG_GET_MIN_CLOCK_RATE	0x00030007
#define PROPTAG_GET_CLOCK_RATE_MEASURED 0x00030047
#define PROPTAG_SET_CLOCK_RATE		0x00038002
#define PROPTAG_END		0x00000000

#define CLOCK_ID_ARM        3
#define CLOCK_ID_CORE		4

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
    // set to zero. VC sets this to (1<<31) | <the length of the message it WANTED to send>
    // note that this may be greater than `buffer_length`!!!
    uint32_t response_length;
    // your response data is placed here
    uint32_t buffer[buffer_length / sizeof(uint32_t)]
    }, /* repeat as many times as you like */
/* tag buffer end */
uint32_t end_of_message_marker
]
#endif

unsigned get_core_clock (void);
unsigned div (unsigned nDividend, unsigned nDivisor);
//------------------------------------------------------------------------
unsigned int uart_lcr ( void )
{
    return(GET32(AUX_MU_LSR_REG));
}
//------------------------------------------------------------------------
int uart_try_recv ( void )
{
    if(GET32(AUX_MU_LSR_REG)&0x01)
        return(GET32(AUX_MU_IO_REG)&0xFF);
    else
        return -1;
}
//------------------------------------------------------------------------
unsigned int uart_recv ( void )
{
    while(1)
    {
        if(GET32(AUX_MU_LSR_REG)&0x01) break;
    }
    return(GET32(AUX_MU_IO_REG)&0xFF);
}
//------------------------------------------------------------------------
unsigned int uart_check ( void )
{
    if(GET32(AUX_MU_LSR_REG)&0x01) return(1);
    return(0);
}
//------------------------------------------------------------------------
void uart_send ( unsigned int c )
{
    while(1)
    {
        if(GET32(AUX_MU_LSR_REG)&0x20) break;
    }
    PUT32(AUX_MU_IO_REG,c);
}
//------------------------------------------------------------------------
void uart_flush ( void )
{
    while(1)
    {
        if((GET32(AUX_MU_LSR_REG)&0x100)==0) break;
    }
}
//------------------------------------------------------------------------
void uart_send_hex32 ( unsigned int d )
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
//------------------------------------------------------------------------
void uart_sendln_hex32 ( unsigned int d )
{
    uart_send_hex32(d);
    uart_send(0x0D);
    uart_send(0x0A);
}
//------------------------------------------------------------------------
void uart_init ( unsigned baud )
{
    unsigned int ra;

    PUT32(AUX_ENABLES,1);
    PUT32(AUX_MU_IER_REG,0);
    PUT32(AUX_MU_CNTL_REG,0);
    PUT32(AUX_MU_LCR_REG,3);
    PUT32(AUX_MU_MCR_REG,0);
    PUT32(AUX_MU_IER_REG,0);
    PUT32(AUX_MU_IIR_REG,0xC6);
    PUT32(AUX_MU_BAUD_REG,div(get_core_clock()/8 + baud/2, baud) - 1);
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
//------------------------------------------------------------------------
void  timer_init ( void )
{
    //0xF9+1 = 250
    //250MHz/250 = 1MHz
    PUT32(ARM_TIMER_CTL,0x00F90000);
    PUT32(ARM_TIMER_CTL,0x00F90200);
}
//-------------------------------------------------------------------------
unsigned int timer_tick ( void )
{
    return(GET32(ARM_TIMER_CNT));
}
//-------------------------------------------------------------------------
unsigned mbox_writeread (unsigned nData)
{
	while (GET32 (MAILBOX1_STATUS) & MAILBOX_STATUS_FULL)
	{
		// do nothing
	}

	PUT32 (MAILBOX1_WRITE, BCM_MAILBOX_PROP_OUT | nData);

	unsigned nResult;
	do
	{
		while (GET32 (MAILBOX0_STATUS) & MAILBOX_STATUS_EMPTY)
		{
			// do nothing
		}

		nResult = GET32 (MAILBOX0_READ);
	}
	while ((nResult & 0xF) != BCM_MAILBOX_PROP_OUT);

	return nResult & ~0xF;
}
//-------------------------------------------------------------------------
unsigned get_core_clock (void)
{
	// does not work without a short delay with newer firmware on RPi 1
	for (volatile unsigned i = 0; i < 10000; i++);

	unsigned proptag[] __attribute__ ((aligned (16))) =
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

	mbox_writeread ((unsigned) (unsigned long) &proptag);

	if (proptag[6] != 0)
	{
		return proptag[6];
	}

	unsigned proptag_measured[] __attribute__ ((aligned (16))) =
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

	mbox_writeread ((unsigned) (unsigned long) &proptag_measured);

	return proptag_measured[6];
}
//-------------------------------------------------------------------------
uint64_t get_board_serial (void)
{
	unsigned proptag[] __attribute__ ((aligned (16))) =
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

	mbox_writeread ((unsigned) (unsigned long) &proptag);

    return (((uint64_t)proptag[5]) << 32) | proptag[6];
}
//-------------------------------------------------------------------------
unsigned get_board_revision (void)
{
	unsigned proptag[] __attribute__ ((aligned (16))) =
	{
		7*4,
		CODE_REQUEST,
		PROPTAG_GET_BOARD_REVISION,
		1*4,
		0,

		0,

		PROPTAG_END
	};

	mbox_writeread ((unsigned) (unsigned long) &proptag);

    return proptag[5];
}

static unsigned get_clock_rate(unsigned tag)
{
	unsigned proptag[] __attribute__ ((aligned (16))) =
	{
		8*4,
		CODE_REQUEST,
		tag,
		2*4,
		0,

		CLOCK_ID_ARM,
		0,

		PROPTAG_END
	};

	mbox_writeread ((unsigned) (unsigned long) &proptag);

	return proptag[6];
}

unsigned get_min_clock_rate()
{
    return get_clock_rate(PROPTAG_GET_MIN_CLOCK_RATE);
}

unsigned get_max_clock_rate()
{
    return get_clock_rate(PROPTAG_GET_MAX_CLOCK_RATE);
}

unsigned get_current_clock_rate()
{
    return get_clock_rate(PROPTAG_GET_CLOCK_RATE);
}

unsigned get_measured_clock_rate()
{
    return get_clock_rate(PROPTAG_GET_CLOCK_RATE);
}

void set_clock_rate(uint32_t value)
{
	unsigned proptag[] __attribute__ ((aligned (16))) =
	{
		16*4,
		CODE_REQUEST,
		PROPTAG_SET_CLOCK_RATE,
		3*4,
		0,

		CLOCK_ID_ARM,
		value,
        0, //SKIPSETTINGTURBO

		PROPTAG_END,
        0,0,0,0,0,0,0
	};

	mbox_writeread ((unsigned) (unsigned long) &proptag);
    delay_micros(355); // See: linux/drivers/cpufreq/bcm2835-cpufreq.c
}

//-------------------------------------------------------------------------
unsigned div (unsigned nDividend, unsigned nDivisor)
{
	if (nDivisor == 0)
	{
		return 0;
	}

	unsigned long long ullDivisor = nDivisor;

	unsigned nCount = 1;
	while (nDividend > ullDivisor)
	{
		ullDivisor <<= 1;
		nCount++;
	}

	unsigned nQuotient = 0;
	while (nCount--)
	{
		nQuotient <<= 1;

		if (nDividend >= ullDivisor)
		{
			nQuotient |= 1;
			nDividend -= ullDivisor;
		}

		ullDivisor >>= 1;
	}

	return nQuotient;
}
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


int nibble_from_hex(int ascii)
{
    if (ascii>='0' && ascii<='9')
        return ascii & 0x0F;
    if (ascii>='A' && ascii<='F')
        return (ascii - 7) & 0x0F;
    return -1;
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

void uart_send_str(const char* psz)
{
    while (*psz)
    {
        uart_send(*psz++);
    }
}

void delay_micros(unsigned int period)
{
    unsigned int start = timer_tick();
    while (timer_tick() - start < period)
    {
    }
}

void *memset (void *pBuffer, int nValue, size_t nLength)
{
    uint8_t* p = (uint8_t*)pBuffer;
    uint8_t* pEnd = p + nLength;
    while (p < pEnd)
        *p++ = (uint8_t)nValue;
    return pBuffer;
}

void *memcpy (void *pDest, const void *pSrc, size_t nLength)
{
    uint8_t* ps = (uint8_t*)pSrc;
    uint8_t* pd = (uint8_t*)pDest;
    uint8_t* pse = ps + nLength;
    while (ps < pse)
        *pd++ = *ps++;
    return pDest;

}


// Given a board revision, return which gpio pin has the activity led
// Returns negitive number if active low
// Returns 0 if not supported
int led_pin_from_revision(unsigned revision)
{
    if (revision & (1 << 23))
    {
        // new revision
        switch ((revision >> 4) & 0xFF)
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
        switch (revision)
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

// Get the register offset and mask for a gpio pin
void gpio_to_register(unsigned pin, unsigned* pRegOffset, unsigned* pRegMask)
{
	*pRegOffset = (pin / 32) * 4;
	*pRegMask = 1 << (pin % 32);
}

// Write to an GPIO output pin
void write_gpio(unsigned pin, unsigned value)
{
    // Get register offset and mask
    unsigned regOffset, regMask;
    gpio_to_register(pin, &regOffset, &regMask);

    // Write
	unsigned nSetClrReg = (value ? GPSET0 : GPCLR0) + regOffset;
	PUT32(nSetClrReg, regMask);
}

// Read from a GPIO input pin
unsigned read_gpio(unsigned pin)
{
    // Get register offset and mask
    unsigned regOffset, regMask;
    gpio_to_register(pin, &regOffset, &regMask);

    // Read
	return GET32 (GPLEV0 + regOffset) & regMask ? 1 : 0;
}

// Set a GPIO pin as input or output
void set_gpio_output(unsigned pin, int output)
{
    unsigned nSelReg = GPFSEL0 + (pin / 10) * 4;
	unsigned nShift = (pin % 10) * 3;

	unsigned nValue = GET32 (nSelReg);
	nValue &= ~(7 << nShift);
	nValue |= (output ? 1 : 0) << nShift;
	PUT32 (nSelReg, nValue);
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

	unsigned nModeReg = GPPUPPDN0 + (pin / 16) * 4;
	unsigned nShift = (pin % 16) * 2;
	static const unsigned ModeMap[3] = {0, 2, 1};
	unsigned nValue = GET32 (nModeReg);
	nValue &= ~(3 << nShift);
	nValue |= ModeMap[mode] << nShift;
	PUT32 (nModeReg, nValue);

#else

	unsigned nClkReg = GPPUDCLK0 + regOffset;
	PUT32 (GPPUD, mode);
	delay_micros (5);		// 1us should be enough, but to be sure
	PUT32 (nClkReg, regMask);
	delay_micros (5);		// 1us should be enough, but to be sure
	PUT32 (GPPUD, 0);
	PUT32 (nClkReg, 0);

#endif

}

int g_activityLed = -1;
unsigned g_activityLedPolarity = 0;

void set_activity_led(unsigned on)
{
    // First time?
    if (g_activityLed == -1)
    {
        unsigned rev = get_board_revision();
        g_activityLed = led_pin_from_revision(rev);
        if (g_activityLed == 0)
            return;

        // Handle active low
        if (g_activityLed < 0)
        {
            g_activityLed = -g_activityLed;
            g_activityLedPolarity = 1;
        }

        // Setup pin
        set_gpio_output(g_activityLed, 1);
        set_gpio_pull_mode(g_activityLed, 0);
    }

    // Set it
    write_gpio(g_activityLed, on ^ g_activityLedPolarity);
}


//-------------------------------------------------------------------------
//
// Copyright (c) 2012 David Welch dwelch@dwelch.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//-------------------------------------------------------------------------
