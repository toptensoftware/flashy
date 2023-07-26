#include <stdarg.h>
#include "common.h"
#include "sdcard.h"
#include "../lib/FFsh/FFsh/src/ffsh.h"

void _vcbprintf(void (*write)(void*, char), void* arg, const char* format, va_list args);


typedef struct PACKED
{
    int32_t  exitcode;     // Error code
    char     cwd[];          // New working directory
} PACKET_COMMAND_ACK;

static uint32_t stdio_seq;
static uint32_t stdio_buffer_used;
static bool stdio_buffer_is_stdout;

void flush_stdio()
{
    if (stdio_buffer_used)
    {
        sendPacket(stdio_seq, stdio_buffer_is_stdout ? PACKET_ID_STDOUT : PACKET_ID_STDERR, response_buf, stdio_buffer_used);
        stdio_buffer_used = 0;
    }
}

void write_stderr(void* user, char ch)
{
    // Flush if switching output handle
    if (stdio_buffer_is_stdout)
    {
        flush_stdio();
        stdio_buffer_is_stdout = false;
    }

    // Store char
    response_buf[stdio_buffer_used++] = ch;

    // Flush if buffer full
    if (stdio_buffer_used >= max_packet_size)
        flush_stdio();
}

void write_stdout(void* user, char ch)
{
    // Flush if switching output handle
    if (!stdio_buffer_is_stdout)
    {
        flush_stdio();
        stdio_buffer_is_stdout = true;
    }

    // Store char
    response_buf[stdio_buffer_used++] = ch;

    // Flush if buffer full
    if (stdio_buffer_used >= max_packet_size)
        flush_stdio();
}

void write(const char* p)
{
    while (*p)
        write_stdout(NULL, *p++);
}

void ffsh_printf(void (*write)(void*, char), void* arg, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	_vcbprintf(write, arg, format, args);
	va_end(args);
}

void ffsh_sleep(uint32_t millis)
{
    delay_micros(millis * 1000);
}



void handle_command(uint32_t seq, const void* p, uint32_t cb)
{
    mount_sdcard();

    // Copy cwd to modifyable working buffer
    char cwd[FF_MAX_LFN];
    strcpy(cwd, (char*)p);

    // The command follows the cwd
    char* pszCommand = (char*)p + strlen(p) + 1;

    // Setup stdio output buffer
    stdio_buffer_is_stdout = true;
    stdio_buffer_used = 0;
    stdio_seq = seq;

    // Setup command context
    struct PROCESS proc;
    process_set_cwd(&proc, cwd);
    process_set_stderr(&proc, NULL, write_stderr);
    process_set_stdout(&proc, NULL, write_stdout);

    // Execut command
    int exitcode = process_shell(&proc, pszCommand);

    process_close(&proc);

    // Flush stdio
    flush_stdio();

    // Send response
    PACKET_COMMAND_ACK* ack = (PACKET_COMMAND_ACK*)response_buf;
    ack->exitcode = exitcode;
    strcpy(ack->cwd, cwd);

    sendPacket(seq, PACKET_ID_ACK, ack, sizeof(PACKET_COMMAND_ACK) + strlen(cwd) + 1);
}

