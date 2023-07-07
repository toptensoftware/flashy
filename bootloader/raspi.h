#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#if RASPI == 1
#define PBASE 0x20000000
#elif RASPI == 4
#define PBASE 0xFE000000
#else
#define PBASE 0x3F000000
#endif


// ASM helpers
extern void PUT32(unsigned int, unsigned int);
extern void PUT16(unsigned int, unsigned int);
extern void PUT8(unsigned int, unsigned int);
extern unsigned int GET32(unsigned int);
extern unsigned int GETPC();
extern void BRANCHTO(unsigned int);
extern void dummy(unsigned int);

// Timer
void timer_init();
unsigned int ticks();

unsigned int micros();
void delay_micros(unsigned int period);

// Mailbox
unsigned mbox_writeread(unsigned nData);
unsigned get_core_clock();
unsigned get_board_revision();
uint64_t get_board_serial();

// Clocks
unsigned get_clock_freq(uint32_t tag, uint32_t clock_id);
unsigned get_cpu_freq();
void set_cpu_freq(uint32_t value);
unsigned get_min_cpu_freq();
unsigned get_max_cpu_freq();
unsigned get_measured_cpu_freq();
uint32_t get_emmc_freq();


// GPIO
#define GPIO_PIN_MODE_INPUT 0
#define GPIO_PIN_MODE_OUTPUT 1
#define GPIO_PIN_MODE_ALT0 4
#define GPIO_PIN_MODE_ALT1 5
#define GPIO_PIN_MODE_ALT2 6
#define GPIO_PIN_MODE_ALT3 7
#define GPIO_PIN_MODE_ALT4 3
#define GPIO_PIN_MODE_ALT5 2

#define GPIO_PULL_MODE_NONE 0
#define GPIO_PULL_MODE_DOWN 1
#define GPIO_PULL_MODE_UP 2

void gpio_to_register(unsigned pin, unsigned* pRegOffset, unsigned* pRegMask);
void write_gpio(unsigned pin, unsigned value);
unsigned read_gpio(unsigned pin);
void set_gpio_pin_mode(unsigned pin, int mode);
void set_gpio_pull_mode(unsigned pin, int mode);

// Activity LED
void set_activity_led(unsigned on);

// Reboot
void reboot();

// Mini UART
void mini_uart_init(unsigned int baud);
unsigned int mini_uart_lcr();
int mini_uart_try_recv();
unsigned int mini_uart_recv();
void mini_uart_send(unsigned int c);
unsigned int mini_uart_check();
void mini_uart_flush();
void mini_uart_send_str(const char* psz);

// UART
void uart_init(unsigned baud);
void uart_init_ex(unsigned baud, int dataBits, int stopBits, int parity);
int uart_try_recv();
uint8_t uart_recv();
void uart_send(unsigned int c);
unsigned int uart_lcr();
unsigned int uart_check();
void uart_flush();
void uart_send_hex32(unsigned int d);
void uart_sendln_hex32(unsigned int d);
void uart_send_hex4(int rc);
void uart_send_hex8(int val);
void uart_send_str(const char* psz);
void uart_send_dec(unsigned int d);

// Register bit manip/polling
void set_register_bits(uint32_t volatile* reg, uint32_t set, uint32_t mask);
bool wait_register_any_set(uint32_t volatile* reg, uint32_t mask, uint32_t timeout_millis);
bool wait_register_all_clear(uint32_t volatile* reg, uint32_t mask, uint32_t timeout_millis);
