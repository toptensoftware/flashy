#pragma once

#include <stdint.h>

extern void PUT32 ( unsigned int, unsigned int );
extern void PUT16 ( unsigned int, unsigned int );
extern void PUT8 ( unsigned int, unsigned int );
extern unsigned int GET32 ( unsigned int );
extern unsigned int GETPC ( void );
extern void BRANCHTO ( unsigned int );
extern void dummy ( unsigned int );

unsigned int uart_lcr ( void );
unsigned int uart_recv ( void );
int uart_try_recv ( void );
unsigned int uart_check ( void );
void uart_send ( unsigned int c );
void uart_flush ( void );
void uart_init ( unsigned int baud );
void  timer_init ( void );
unsigned int timer_tick ( void );
unsigned mbox_writeread (unsigned nData);
unsigned get_core_clock (void);
unsigned get_board_revision (void);
uint64_t get_board_serial (void);
unsigned div (unsigned nDividend, unsigned nDivisor);

int nibble_from_hex(int ascii);

void uart_send_hex32 ( unsigned int d );
void uart_sendln_hex32 ( unsigned int d );
void uart_send_hex4 (int rc);
void uart_send_hex8 (int val);
void uart_send_str(const char* psz);
void delay_micros(unsigned int period);

void gpio_to_register(unsigned pin, unsigned* pRegOffset, unsigned* pRegMask);
void write_gpio(unsigned pin, unsigned value);
unsigned read_gpio(unsigned pin);
void set_gpio_pull_mode(unsigned pin, int mode);

int led_pin_from_revision(unsigned revision);
void set_activity_led(unsigned on);

unsigned get_min_clock_rate();
unsigned get_max_clock_rate();
unsigned get_current_clock_rate();
unsigned get_measured_clock_rate();
void set_clock_rate(uint32_t value);

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------


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
