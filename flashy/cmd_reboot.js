
async function run(ctx)
{
    let port = ctx.port;
    let cl = ctx.cl;

    // Send reboot magic
    await port.switchBaud(cl.userBaud);

    if (cl.verbose)
        process.stdout.write(`Sending reboot magic '${cl.magic}'...`)
    await port.write(cl.magic);
    if (cl.verbose)
        process.stdout.write(` ok\n`);    
}

export default {
    synopsis: "Sends a magic reboot string to the device",
    spec: [
        {
            name: "<magic>",
            help: "The magic reboot string",
            default: null,
        },
    ],
    run,
}