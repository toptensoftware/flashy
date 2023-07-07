let commandLineSpecs = require('./commandLineSpecs');

async function run(ctx)
{
    let port = ctx.port;
    let cl = ctx.cl;

    // Send reboot magic
    if (cl.reboot)
    {
        await port.switchBaud(cl.userBaud);
        process.stdout.write(`Sending reboot magic '${cl.reboot}'...`)
        await port.write(cl.reboot);
        process.stdout.write(` ok\n`);
    }

    // Set default baud
    await ctx.port.switchBaud(115200);

    // Wait for device
    await ctx.layer.ping(true);
    
}

module.exports = {
    synopsis: "Display device bootloader status",
    spec: [
        ...commandLineSpecs.serial_port_specs,
        {
            name: "--reboot:<magic>|-r",
            help: "Send a magic reboot string before flashing",
            default: null,
        },
        {
            name: "--user-baud:<n>",
            help: "Baud rate sending reboot magic (default=115200)",
            default: 115200,
        },
        ...commandLineSpecs.serial_port_packet_specs,
        ...commandLineSpecs.common_specs,
    ],
    run,
}