
async function run(ctx)
{
    // Set default baud
    await ctx.port.switchBaud(115200);

    // Wait for device
    await ctx.layer.ping(ctx.cl.verbose);

    let handler = {
        onStdOut: (data) => process.stdout.write(data),
        onStdErr: (data) => process.stderr.write(data),
    };


    // Send command
    let r = await ctx.layer.sendCommand(ctx.cl.cwd, ctx.cl.cmd, handler);
    return r.exitCode;
}

module.exports = {
    synopsis: "Executes a shell command on the target device",
    spec: [
        {
            name: "<cmd>",
            help: "The shell command to execute",
        },
        {
            name: "--cwd:<dir>",
            help: "The working directory on the device in which to execute the command (default = /)",
            default: '/',
        },
    ],
    run,
}