#!/usr/bin/env node

///////////////////////////////////////////////////////////////////////////////////
// Flashy v2.0
//
// See commandLine.js for options, or run "node flashy --help"

let os = require('os');
let child_process = require('child_process');
let path = require('path');
let fs = require('fs');
 
let serial = require('./serial');
let packetLayer = require('./packetLayer');
let intelHex = require('./intelHex');
let cl = require('./commandLine');
let FlashyError = require('./FlashyError');


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
            throw new FlashyError("Unexpected data after EOF record in hex file");

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
                    throw new FlashyError("Unexpected length of '02' hex record");
                segment = record.data.readUInt16BE(0) << 4;
                break;
                
            case '03':
                // Start segment address
                if (record.data.length != 4)
                    throw new FlashyError("Unexpected length of '03' hex record");
                if (startAddress !== null)
                    throw new FlashyError("Hex file contains multiple start addresses");
                startAddress = (record.data.readUInt16BE(0) << 4) + 
                                record.data.readUInt16BE(2);
                break;
                    
            case '04':
                // Extended linear address
                if (record.data.length != 2)
                    throw new FlashyError("Unexpected length of '04' hex record");
                segment = record.data.readUInt16BE(0);
                break;

            case '05':
                // Start linear address
                if (record.data.length != 4)
                    throw new FlashyError("Unexpected length of '03' hex record");
                if (startAddress !== null)
                    throw new FlashyError("Hex file contains multiple start addresses");
                startAddress = record.data.readUInt32BE(0);
                break;
        }
    }

    // Close chunker and parser
    chunker.close();
    
    // Check EOF was received
    if (!eofReceived)
        throw new FlashyError("Hex file didn't contain an EOF record");
    
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
async function sendImgFile(layer, imgFile, aarch)
{
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

// Check the bootloader version and this script's version number match
function checkVersion(ping)
{
    // Check version of flashy tool matches version of bootloader
    let pkg = JSON.parse(fs.readFileSync(path.join(__dirname, 'package.json')), "utf8");
    let verParts = pkg.version.split('.').map(x => Number(x));
    if (verParts[0] != ping.verMajor || verParts[1] != ping.verMinor || verParts[2] != ping.verBuild)
    {
        console.error("\nBootloader version mismatch:")
        console.error(`    - bootloader version: ${ping.verMajor}.${ping.verMinor}.${ping.verBuild}`);
        console.error(`    - flashy version: ${pkg.version}`);
        if (cl.checkVersion)
        {
            console.error(`Aborting. Use '--noVersionCheck' to override or '--bootloader:<sdcard>' to get the latest images.\n`);
            throw new FlashyError();
        }
        else
        {
            console.error(`Use '--bootloader:<sdcard>' to get the latest images.\n`);
        }
    }
}

// Check the filename name looks like the right kernel image for the device
function checkKernel(ping, filename)
{
    // Disabled?
    if (!cl.checkKernel)
        return;

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
    console.error("Aborting.  Use '--noVersionCheck' to override.\n");
    throw new FlashyError();
}

// Check the user hasn't selected a packet size larger than
// what the bootloader can support
function checkPacketSize(ping)
{
    if (cl.packetSize > ping.maxPacketSize)
    {
        console.error(`\nPacket size too large:`);
        console.error(`    - requested: ${cl.packetSize}`);
        console.error(`    - supported: ${ping.maxPacketSize}`);
        console.error(`Aborting. Please choose a smaller packet size.\n`);
        throw new FlashyError();
    }
}


// Main!
(async function() {

    // Extract bootloader images
    if (cl.bootloader)
    {
        try
        {
            let sourceDir = path.join(path.dirname(__filename), "bootloader_images") 
            let entries = fs.readdirSync(sourceDir, { withFileTypes: true });
            for (let entry of entries)
            {
                let sourcePath = path.join(sourceDir, entry.name);
                let destinationPath = path.join(cl.bootloader, entry.name);
                if (!entry.isDirectory())
                {
                    console.log(`Copying to ${destinationPath}`);
                    fs.copyFileSync(sourcePath, destinationPath);
                }
            };
        }
        catch (err)
        {
            console.log(err.message);
            process.exit(7);
        }
    }

    // Print the bootloader path
    if (cl.printBootloaderPath)
    {
        process.stdout.write(path.join(path.dirname(__filename), "bootloader_images"));
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
            process.stdout.write(` ok\n`);
        }

        // Open?
        if (cl.goSwitch || cl.imageFile || cl.status)
        {
            // Set default baud
            await port.switchBaud(115200);
            
            // Open packet layer
            let packetLayerOptions = {
                max_packet_size: Math.max(128, cl.packetSize),
                packet_ack_timeout: cl.packetTimeout,
                ping_attempts: cl.pingAttempts,
            };
            let layer = packetLayer(port, packetLayerOptions);
        
            // Wait for device
            let ping = await layer.ping(true);

            // Check bootloader version
            checkVersion(ping);

            // Send image file
            let startAddress = cl.goAddress == null ? 0xFFFFFFFF : cl.goAddress;
            if (cl.imageFile)
            {
                // Check kernel kind
                checkKernel(ping, cl.imageFile);
                
                // Check the selected packet size is supported by the bootloader
                checkPacketSize(ping);
                
                // Switch baud rate and cpu frequency while sending file
                let cpufreq = 0;
                if ((cl.cpuBoost == "auto" && cl.flashBaud > 1000000) || cl.cpuBoost == 'yes')
                    cpufreq = ping.max_cpu_freq;
                if (cl.flashBaud != 115200 || cpufreq != 0)
                {
                    await layer.switchBaud(cl.flashBaud, cl.resetBaudTimeout, cpufreq);
                    await layer.ping();
                }
            
                // Send file
                if (cl.isHexFile)
                    startAddress = await sendHexFile(layer, cl.imageFile);
                else
                    startAddress = await sendImgFile(layer, cl.imageFile, ping.aarch);
            }
        
            // Send go command
            if (cl.goSwitch || (cl.imageFile && !cl.nogoSwitch))
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
    catch (err)
    {
        if (err instanceof FlashyError)
        {
            if (err.message)
                console.log(err.message);
        }
        else
            throw err;
    }
    finally
    {
        // Finished!
        port.close();
    }

})();