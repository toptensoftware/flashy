let path = require('path');
let fs = require('fs');

let commandLineParser = require('./commandLineParser');
let intelHex = require('./intelHex');

// Send a hex file to device
async function sendHexFile(ctx, hexFile)
{
    let cl = ctx.cl;
    let layer = ctx.layer;

    // Capture start time
    let startTime = new Date().getTime();

    process.stdout.write(`Sending '${hexFile}':\n`)

    // parse and re-chunk hex file and send
    let parser = intelHex.parser(hexFile);
    let chunker = intelHex.chunker(parser, cl.packetSize, 4);
    let segment = 0;
    let eofReceived = false;
    let startAddress = null;
    let programBytesSent = 0;
    while (true)
    {
        // Get next record
        let record = chunker.read();
        if (!record)
            break;

        // Shouldn't have received EOF yet
        if (eofReceived)
            throw new Error("Unexpected data after EOF record in hex file");

        // Handle record
        switch (record.type)
        {
            case '00':
                // DATA

                // Write the full 32-bit address to the header area
                record.data.writeUInt32LE(record.addr + segment, 0);
                
                // Send it
                for (let i=0; i<cl.stress; i++)
                {
                    // Update program byte length
                    programBytesSent += record.data.length - 4;

                    // Send it
                    await layer.sendData(record.data);
                    process.stdout.write('.');
                }
                break;

            case '01':
                // EOF
                eofReceived = true;
                break;

            case '02':
                // Extended segment address
                if (record.data.length != 2)
                    throw new Error("Unexpected length of '02' hex record");
                segment = record.data.readUInt16BE(0) << 4;
                break;
                
            case '03':
                // Start segment address
                if (record.data.length != 4)
                    throw new Error("Unexpected length of '03' hex record");
                if (startAddress !== null)
                    throw new Error("Hex file contains multiple start addresses");
                startAddress = (record.data.readUInt16BE(0) << 4) + 
                                record.data.readUInt16BE(2);
                break;
                    
            case '04':
                // Extended linear address
                if (record.data.length != 2)
                    throw new Error("Unexpected length of '04' hex record");
                segment = record.data.readUInt16BE(0);
                break;

            case '05':
                // Start linear address
                if (record.data.length != 4)
                    throw new Error("Unexpected length of '03' hex record");
                if (startAddress !== null)
                    throw new Error("Hex file contains multiple start addresses");
                startAddress = record.data.readUInt32BE(0);
                break;
        }
    }

    // Close chunker and parser
    chunker.close();
    
    // Check EOF was received
    if (!eofReceived)
        throw new Error("Hex file didn't contain an EOF record");
    
    // Check we got a start address
    if (startAddress == null)
        console.error("WARNING: Hex file didn't report a start address, assuming default");
    
    // Show summary
    let elapsedTime = new Date().getTime() - startTime;
    process.stdout.write(`\nTransfered ${programBytesSent} bytes in ${((elapsedTime / 1000).toFixed(1))} seconds.\n`);

    // Return the start address
    return startAddress == null ? 0xFFFFFFFF : startAddress;
}


// Send a img file to device
async function sendImgFile(ctx, imgFile, aarch)
{
    let cl = ctx.cl;
    let layer = ctx.layer;

    // Capture start time
    let startTime = new Date().getTime();

    process.stdout.write(`Sending '${imgFile}':\n`)

    // Open image file
    let fd = fs.openSync(imgFile, `r`);
    let buf = Buffer.alloc(cl.packetSize);
    let startAddress = (aarch == 64) ? 0x80000 : 0x8000;
    let addr = startAddress;
    let programBytesSent = 0;
    while (true)
    {
        // Read a buffer
        let length = fs.readSync(fd, buf, 4, buf.length - 4);
        if (length == 0)
            break;

        // Write the address to the header area
        buf.writeUInt32LE(addr, 0);

        
        // Send it
        for (let i=0; i<cl.stress; i++)
        {
            // Update program byte length
            programBytesSent += length;

            // Send it
            await layer.sendData(buf.slice(0, length + 4));
            process.stdout.write('.');
        }

        // Update address
        addr += length;
    }

    // Close file
    fs.closeSync(fd);
    
    // Show summary
    let elapsedTime = new Date().getTime() - startTime;
    process.stdout.write(`\nTransfered ${programBytesSent} bytes in ${((elapsedTime / 1000).toFixed(1))} seconds.\n`);

    // Return the start address
    return startAddress;
}

// Check the filename name looks like the right kernel image for the device
function checkKernel(ping, filename, )
{
    // Does the filename name look like kernel name?
    if (filename.indexOf('kernel') < 0)
        return;

    // Work out what image name we expect (can be multiple on pi3)
    let allowedKernelNames=[];
    switch (ping.model.major)
    {
        case 1: 
            allowedKernelNames.push('kernel.'); 
            break;

        case 2: 
            allowedKernelNames.push('kernel7.'); 
            break;

        case 3: 
            if (ping.aarch == 32)
            {
                allowedKernelNames.push('kernel7.'); 
                allowedKernelNames.push('kernel8-32.'); 
            }
            else
                allowedKernelNames.push('kernel8.');
            break;

        case 4: 
            if (ping.aarch == 32)
                allowedKernelNames.push('kernel7l.'); 
            else 
                allowedKernelNames.push('kernel8-rpi4.');
            break;
    }

    // Match?
    for (let en of allowedKernelNames)
    {
        if (filename.indexOf(en) >= 0)
            return;
    }

    // Log and quit
    console.error("\nImage name mismatch");
    console.error(`    - image file: ${path.basename(filename)}`);
    console.error(`    - expected: ${allowedKernelNames.map(x=> x + "[img|hex]").join(" or ")} (for rpi${ping.raspi}-aarch${ping.aarch})`);
    console.error("");
    throw new Error("Aborting.  Use '--no-kernel-check' to override.");
}

async function run(ctx)
{
    let cl = ctx.cl;
    let port = ctx.port;
    let layer = ctx.layer;

    // Send reboot magic
    if (cl.reboot)
    {
        await port.switchBaud(cl.userBaud);
        process.stdout.write(`Sending reboot magic '${cl.reboot}'...`)
        await port.write(cl.reboot);
        process.stdout.write(` ok\n`);
    }

    // Set default baud
    await port.switchBaud(115200);
    
    // Wait for device
    let ping = await layer.ping(true);

    // Send image file
    let startAddress = cl.goAddress == null ? 0xFFFFFFFF : cl.goAddress;

    // Check kernel kind
    if (!cl.noKernelCheck)
        checkKernel(ping, cl.imagefile.filename);
        
    // Switch baud rate and cpu frequency while sending file
    await layer.boost(cl);

    // Send file
    if (cl.imagefile.kind == "hex")
        startAddress = await sendHexFile(ctx, cl.imagefile.filename);
    else
        startAddress = await sendImgFile(ctx, cl.imagefile.filename, ping.aarch);

    // Send go command
    if (!cl.noGo)
    {
        await layer.sendGo(startAddress, cl.goDelay);
    }

    // Open monitor
    if (cl.monitor)
    {
        await port.switchBaud(cl.userBaud);
        console.log("Monitoring....");

        // Setup receive listener
        port.read(function(data) {
    
            process.stdout.write(data.toString(`utf8`));
        });
    
        // Wait for a never delivered promise to keep alive
        await new Promise((resolve) => { });
    }

}




module.exports = {
    synopsis: "Flashes a kernel image to the target device",
    spec: [
        {
            name: "--imagefile:<file>|-i",
            help: "The .hex or .img file to write\n'--imagefile:' prefix not required for *.img or *.hex",
            valuePattern: /\.(hex|img)$/,
            parse: (arg) => {
                if (arg.toLowerCase().endsWith('.hex'))
                    return { filename: arg, kind: "hex" }
                else if (arg.toLowerCase().endsWith('.img'))
                    return { filename: arg, kind: "img" }
                throw new Error("Image file must be a '.hex' or '.img' file");
            }
        },
        {
            name: "--reboot:<magic>|-r",
            help: "Send a magic reboot string before flashing",
            default: null,
        },
        {
            name: "--monitor|-m",
            help: "After flashing monitor the serial port and show output",
            default: false, 
        },
        {
            name: "--user-baud:<n>",
            help: "Baud rate for monitor and reboot magic (default=115200)",
            default: 115200,
        },
        {
            name: "--go-delay:<ms>|-d",
            help: "Introduces a delay before starting the flashed image",
            parse: commandLineParser.parse_integer(0),
            default: 300,
        },
        {
            name: "--no-go",
            help: "Don't start the flashed image",
        },
        {
            name: "--no-kernel-check",
            help: "Don't check the image filename matches expected kernel type for device",
        },
        {
            name: "--stress:<n>",
            help: "Send data packets N times (for load testing)",
            default: 1,
        },
    ],
    run,
}