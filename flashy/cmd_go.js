let commandLineParser = require('./commandLineParser');

async function run(ctx)
{
    // Set default baud
    await ctx.port.switchBaud(115200);

    // Wait for device
    await ctx.layer.ping(true);

    // Send go command
    await ctx.layer.sendGo(ctx.cl.address == null ? 0xFFFFFFFF : ctx.cl.address, ctx.cl.delay);
        
}

module.exports = {
    synopsis: "Start a previously flashed image",
    spec: [
        {
            name: "--delay:<ms>|-d",
            help: "Introduces a delay before starting the flashed image",
            parse: commandLineParser.parse_integer(0),
            default: 300,
        },
    ],
    run,
}