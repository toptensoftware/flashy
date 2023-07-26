let struct = require('./struct');

let lib = struct.library();
lib.defineType({
    name: "command_ack",
    fields: [
        "uint32le exitCode",
        "string cwd",
    ]
});

async function run(ctx)
{
    // Set default baud
    await ctx.port.switchBaud(115200);

    // Wait for device
    await ctx.layer.ping(true);

    // Send command
    let r = lib.decode("command_ack", await ctx.layer.sendCommand(ctx.cl.cwd, ctx.cl.cmd));
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