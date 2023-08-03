#include "common.h"
#include "chainboot.h"

int cmd_chain(struct PROCESS* proc)
{
    // Process options
    ENUM_ARGS args;
    start_enum_args(&args, proc, &proc->args);

    bool optRegs = false;

    OPT opt;
    while (next_opt(&args, &opt))
    {
        if (is_switch(&args, &opt, "-r|--regs"))
            optRegs = true;
        else
            unknown_opt(&args, &opt);
    }
    if (enum_args_error(&args))
        return end_enum_args(&args);

    // Just dump registers?
    if (optRegs)
    {
        void** p = (void**)(size_t)0x08000000;
        for (int i=0; i<4; i++)
        {
            p--;
            pout("%p ", *p);
        }
        pout("\n");
        return 0;
    }

    // Process args (1st pass list files)
    ARG arg;
    while (next_arg(&args, &arg))
    {
        // Load image
        int err = load_chain_image(arg.pszAbsolute, proc->progress);
        if (err)
        {
            perr("failed to load image '%s', %s (%i)", arg.pszAbsolute, f_strerror(err), err);
            continue;
        }

        // Send response
        proc->did_exit = true;
        finish_handle_command(proc);
        delay_millis(100);

        // Run chain image
        run_chain_image();
    }
    int err = end_enum_args(&args);
    if (err)
        return err;


    return 0;
}

