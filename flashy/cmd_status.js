
async function run(ctx)
{
    // Set default baud
    await ctx.port.switchBaud(115200);

    // Wait for device
    await ctx.layer.ping(true);
}

module.exports = {
    synopsis: "Display device bootloader status",
    spec: [
    ],
    run,
}