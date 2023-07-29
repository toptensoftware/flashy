
async function run(ctx)
{
    // Wait for device
    await ctx.port.switchBaud(115200);
    await ctx.layer.ping(ctx.cl.verbose);

    let handler = {
        onStdOut: (data) => process.stdout.write(data),
        onStdErr: (data) => process.stderr.write(data),
    };


    // Send command
    let r = await ctx.layer.sendCommand(ctx.cl.rwd, ctx.cl.cmd, handler);
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
            name: "--rwd:<dir>",
            help: "The remote working directory (ie: on the device) in which to execute the command (default = /)",
            default: '/',
        },
    ],
    run,
}