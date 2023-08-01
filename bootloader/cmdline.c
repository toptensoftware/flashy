#include "common.h"

uint32_t cl_autochain_timeout_millis = 10000;
const char* cl_autochain_target = NULL;

void process_cmdline_autochain(char* value)
{    
    char* path = strchr(value, ',');

    // No comma, so it's just a path
    if (path == NULL)
    {
        cl_autochain_target = stralloc(value);
        return;
    }

    *path++ = '\0';

    // Parse time component
    if (parse_millis(value, &cl_autochain_timeout_millis))
    {
        // Store path
        cl_autochain_target = stralloc(path);
    }
}

void process_cmdline()
{
    char sz[2048];
    get_command_line(sz, sizeof(sz));

    // Iniitalize tokenizer
    struct MEMPOOL pool;
    mempool_init(&pool, 4096);
    struct TOKENIZER tokenizer;
    tokenizer_init(&tokenizer, &pool, false, sz);

    // Process args
    while (tokenizer.token != TOKEN_EOF)
    {
        if (tokenizer.token == TOKEN_ARG)
        {
            char* value = strchr(tokenizer.arg, '=');
            if (value)
            {
                *value++ = '\0';
            }
            if (strcmp(tokenizer.arg, "flashy.autochain") == 0 && value)
            {
                process_cmdline_autochain(value);
            }
        }
        tokenizer_next(&tokenizer);
    }

    // Clean up
    tokenizer_close(&tokenizer);
    mempool_release(&pool);
}
