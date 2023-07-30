#include "common.h"

int cmd_chain(struct PROCESS* proc)
{
    // Process options
    ENUM_ARGS args;
    start_enum_args(&args, proc, &proc->args);

    OPT opt;
    while (next_opt(&args, &opt))
    {
        unknown_opt(&args, &opt);
    }
    if (enum_args_error(&args))
        return end_enum_args(&args);

    // Process args (1st pass list files)
    ARG arg;
    while (next_arg(&args, &arg))
    {
        if (arg.pfi == NULL)
        {
            perr("file not found: '%s'", arg.pszRelative);
        }
        else
        {
            uint32_t baseAddress;
            #if AARCH == 32
            baseAddress = 0x8000;
            #else
            baseAddress = 0x80000;
            #endif

            // Open the file
            FIL file;
            int err = f_open(&file, arg.pszAbsolute, FA_READ | FA_OPEN_EXISTING);
            if (err)
            {
                perr("failed to open file '%s', %s (%i)", arg.pszRelative, f_strerror(err), err);
                continue;
            }

            // Read file
            UINT bytes_read;
            err = f_read(&file, (uint8_t*)(size_t)baseAddress, arg.pfi->fsize, &bytes_read);
            f_close(&file);

            if (err)
            {
                perr("failed to read file '%s', %s (%i)", arg.pszRelative, f_strerror(err), err);
                set_enum_args_error(&args, err);
                continue;
            }
            if (bytes_read != arg.pfi->fsize)
            {
                perr("file read size mismatch '%s', %s (%i)");
                set_enum_args_error(&args, -1);
                continue;
            }

            // Send response
            proc->did_exit = true;
            finish_handle_command(proc);
            delay_millis(100);

            // Boot it
            BRANCHTO(baseAddress);
        }
    }
    int err = end_enum_args(&args);
    if (err)
        return err;


    return 0;
}

