#!/usr/bin/env node

///////////////////////////////////////////////////////////////////////////////////
// Flashy v2.0
//
// See commandLine.js for options, or run "node flashy --help"

let os = require('os');
let child_process = require('child_process');
let path = require('path');
let fs = require('fs');
let unzipper = require('unzipper');

let serial = require('./serial');
let packetLayer = require('./packetLayer');
let intelHex = require('./intelHex');
let cl = require('./commandLine');
let wslUtils = require('./wslUtils');

// If running under WSL2 and trying to use a regular serial port
// relaunch self as a Windows process
if (wslUtils.isWsl2() && cl.serialPortName && cl.serialPortName.startsWith("/dev/ttyS"))
{
    process.exit(wslUtils.runSelfUnderWindows());
}
    
// Send a hex file to device
async function sendHexFile(layer, hexFile)
{
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


(async function() {

    // Extract bootloader images
    if (cl.bootloader)
    {
        await fs.createReadStream(path.join(path.dirname(__filename), "bootloader.zip"))
            .pipe(unzipper.Parse())
            .on('entry', entry => { 
                let target = path.join(cl.bootloader, entry.path);
                console.log(`Extracting ${target}`);
                entry.pipe(fs.createWriteStream(target));
            })
            .promise()
            .then( () => console.log('done'), e => console.log('error',e));
   }

   // Quit if nothing else to do
   if (!cl.serialPortName)
   {
        process.exit(0);
   }

    // Setup serial port
    let port = serial(cl.serialPortName, {
        baudRate: 0,  // delay open until first baud rate switch
        log: (msg) => process.stdout.write(msg),
        logFilename: cl.serialLog,
    });
    await port.open();

    try
    {
        // Send reboot magic
        if (cl.rebootMagic)
        {
            await port.switchBaud(cl.userBaud);
            process.stdout.write(`Sending reboot magic '${cl.rebootMagic}'...`)
            await port.write(cl.rebootMagic);
            process.stdout.write(`ok\n`);
        }

        // Open?
        if (cl.goSwitch || cl.hexFile)
        {
            // Set default baud
            port.switchBaud(115200);
            
            // Open packet layer
            let packetLayerOptions = {
                max_packet_size: Math.max(128, cl.packetSize),
                packet_ack_timeout: cl.packetTimeout,
                ping_attempts: cl.pingAttempts,
            };
            let layer = packetLayer(port, packetLayerOptions);
        
            // Wait for device
            let ping = await layer.ping(true);

            // Send hex file
            let startAddress = cl.goAddress == null ? 0xFFFFFFFF : cl.goAddress;
            if (cl.hexFile)
            {
                // Check the selected packet size is supported by the bootloader
                if (cl.packetSize > ping.maxPacketSize)
                {
                    process.stderr.write(`Packet size ${cl.packetSize} exceeds supported packet size of bootloader ${ping.maxPacketSize}.\n`);
                    process.stderr.write(`Please choose a smaller packet size, or rebuild the bootloader with a larger max_packet_size setting.\n`);
                    process.exit(7);
                }
    
                // Switch baud rate while sending file
                if (cl.flashBaud != 115200)
                {
                    await layer.switchBaud(cl.flashBaud, cl.resetBaudTimeout);
                    await layer.ping();
                }
            
                // Send file
                startAddress = await sendHexFile(layer, cl.hexFile);
            }
        
            // Send go command
            if (cl.goSwitch || (cl.hexFile && !cl.nogoSwitch))
            {
                await layer.sendGo(startAddress, cl.goDelay);
            }
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
    finally
    {
        // Finished!
        port.close();
    }

})();