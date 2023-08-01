
async function run(ctx)
{
    // Set default baud
    await ctx.port.switchBaud(115200);

    process.stdout.write("Monitoring...\n");
    await new Promise((resolve, reject) => {});
}

module.exports = {
    synopsis: "Display trace messages from bootloader",
    spec: [
    ],
    run,
}