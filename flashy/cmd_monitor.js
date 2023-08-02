
async function run(ctx)
{
    let port = ctx.port;
    let cl = ctx.cl;

    await port.switchBaud(cl.userBaud);
    
    console.log("Monitoring....");

    // Setup receive listener
    port.read(function(data) {
        process.stdout.write(data.toString(`utf8`));
    });

    // Wait for a never delivered promise to keep alive
    await new Promise((resolve) => { });
}

module.exports = {
    synopsis: "Monitors serial port output",
    spec: [
    ],
    run,
    usesPacketLayer: false
}