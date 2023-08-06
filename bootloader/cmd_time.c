#include "common.h"

int cmd_time(struct PROCESS* proc)
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
        perr("too many arguments: '%s'", arg.pszRelative);
        set_enum_args_error(&args, -1);
    }
    int err = end_enum_args(&args);
    if (err)
        return err;

    pout("elapsed: %li\ndisk read: %li\ndisk write: %li\nserial write: %li\n", 
        last_elapsed_time, last_disk_read_time, last_disk_write_time, last_serial_write_time);


    return 0;
}

