let commandLineSpecs = require('./commandLineSpecs');
let commandLineParser = require('./commandLineParser');

async function run(ctx)
{
    // Set default baud
    await ctx.port.switchBaud(115200);

    // Wait for device
    await ctx.layer.ping(true);

    // Send go command
    await ctx.layer.sendGo(ctx.cl.address == null ? 0xFFFFFFFF : ctx.cl.address, ctx.cl.goDelay);
        
}

module.exports = {
    synopsis: "Start a previously flashed image",
    spec: [
        ...commandLineSpecs.serial_port_specs,
        {
            name: "<address>",
            help: "Starting address.  If not specified, use the default start address for the device",
            parse: commandLineParser.parse_integer(0),
            default: null,
        },
        {
            name: "--go-delay:<ms>|-d",
            help: "Introduces a delay before starting the flashed image",
            parse: commandLineParser.parse_integer(0),
            default: 300,
        },
        ...commandLineSpecs.serial_port_packet_specs,
        ...commandLineSpecs.common_specs,
    ],
    run,
}