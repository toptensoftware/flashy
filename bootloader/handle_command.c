#include <stdarg.h>
#include "common.h"
#include "sdcard.h"
#include <malloc.h>

// From ceeelib
void _vcbprintf(void (*write)(void*, char), void* arg, const char* format, va_list args);


typedef struct PACKED
{
    int32_t  exitcode;     // Error code
    uint8_t  did_exit;     // Was "exit" or "reboot" command invoked
    char     cwd[];        // New working directory
} PACKET_COMMAND_ACK;

static uint32_t stdio_seq;
static uint32_t stdio_buffer_used;
static bool stdio_buffer_is_stdout;
static uint32_t last_flush;

void flush_stdio()
{
    if (stdio_buffer_used)
    {
        sendPacket(stdio_seq, stdio_buffer_is_stdout ? PACKET_ID_STDOUT : PACKET_ID_STDERR, response_buf, stdio_buffer_used);
        stdio_buffer_used = 0;
        last_flush = millis();
    }
}

void send_progress_pings()
{
    // If anything in stdio output, flush it
    if (stdio_buffer_used)
    {
        flush_stdio();
        return;
    }

    // If nothing to send, but it's been more than half as second
    // since anything was sent then send an empty STDOUT packet
    // to notify host we're still alive
    if (millis() - last_flush > 500)
    {
        sendPacket(stdio_seq, PACKET_ID_STDOUT, NULL, 0);
        last_flush = millis();
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

void ffsh_sleep(uint32_t ms)
{
    unsigned int start = millis();
    while(millis() - start < ms)
    {
        send_progress_pings();
    }
}


START_COMMAND_TABLE(flashy_command_table)
    COMMAND(reboot)
    COMMAND(chain)
END_COMMAND_TABLE

bool dispatch_flashy_command(struct PROCESS* proc)
{
    if (process_dispatch(proc, flashy_command_table))
        return true;

    return dispatch_builtin_command(proc);
}

void trace(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	_vcbprintf(write_stderr, NULL, format, args);
	va_end(args);

    flush_stdio();
    delay_millis(10);
}

void handle_command(uint32_t seq, const void* p, uint32_t cb)
{
    // Reset last flush time
    last_flush = millis();

    // Make sure SD card mounted
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
    process_init(&proc, dispatch_flashy_command);
    process_set_cwd(&proc, cwd);
    process_set_stderr(&proc, NULL, write_stderr);
    process_set_stdout(&proc, NULL, write_stdout);
    process_set_progress(&proc, send_progress_pings);

    // Execut command
    process_shell(&proc, pszCommand);

    // Send response
    finish_handle_command(&proc);
}

void finish_handle_command(struct PROCESS* proc)
{
    // Flush stdio
    flush_stdio();

    // Send response
    PACKET_COMMAND_ACK* ack = (PACKET_COMMAND_ACK*)response_buf;
    ack->exitcode = proc->exitcode;
    ack->did_exit = proc->did_exit;
    strcpy(ack->cwd, proc->cwd);

    process_close(proc);

    sendPacket(stdio_seq, PACKET_ID_ACK, ack, sizeof(PACKET_COMMAND_ACK) + strlen(ack->cwd) + 1);
}

