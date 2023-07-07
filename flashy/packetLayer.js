///////////////////////////////////////////////////////////////////////////////////
// Packet Layer
//
// Handles sending and receiving encoded packets to/from device

let fs = require('fs');
let path = require('path');

let packenc = require('./packetEncoder');
let piModel = require('./piModel');


// Packet ID's
const PACKET_ID_PING = 0;
const PACKET_ID_ACK = 1;
const PACKET_ID_ERROR = 2;
const PACKET_ID_DATA = 3;
const PACKET_ID_GO = 4;
const PACKET_ID_REQUEST_BAUD = 5;


// Check the bootloader version and this script's version number match
function checkVersion(ping, warning)
{
    // Check version of flashy tool matches version of bootloader
    let pkg = JSON.parse(fs.readFileSync(path.join(__dirname, 'package.json')), "utf8");
    let verParts = pkg.version.split('.').map(x => Number(x));
    if (verParts[0] != ping.verMajor || verParts[1] != ping.verMinor || verParts[2] != ping.verBuild)
    {
        console.error("\nBootloader version mismatch:")
        console.error(`    - bootloader version: ${ping.verMajor}.${ping.verMinor}.${ping.verBuild}`);
        console.error(`    - flashy version: ${pkg.version}`);
        if (!warning)
        {
            let error = new Error(`Aborting. Use '--no-version-check' to override or the 'bootloader' command to get the latest images.`);
            error.abort = true;
            throw error;
        }
        else
        {
            console.error(`Use the 'bootloader' command to get the latest images.\n`);
        }
    }
}

// Check the user hasn't selected a packet size larger than
// what the bootloader can support
function checkPacketSize(ping, packet_size)
{
    if (packet_size > ping.maxPacketSize)
    {
        console.error(`\nPacket size too large:`);
        console.error(`    - requested: ${packet_size}`);
        console.error(`    - supported: ${ping.maxPacketSize}`);
        let err = new Error("Aborting. Please choose a smaller packet size.");
        err.abort = true;
        throw err;
    }
}


function layer(port, options)
{
    // Apply default options
    options = Object.assign({
        max_packet_size: 1024,
        packet_ack_timeout: 300,
        ping_attempts: 10,
        check_version: true,
    }, options || {})

    // Callback to be invoked on receipt of ack packet
    let ack_notify = null;
    
    // Receive packets
    function onPacket(seq, cmd, data)
    {
        switch (cmd)
        {
            case PACKET_ID_PING:
                console.log("Ping! (device alive)");
                break;

            case PACKET_ID_ACK:
                //console.log(`Ack: seq${seq}`);
                if (ack_notify)
                    ack_notify(seq, data);
                break;

            case PACKET_ID_ERROR:
                console.error(`\nDevice packet error: ${data.readInt32LE(0)}`);
                break;

            default:
                console.error(`\nUnknown packet: seq#:${seq} cmd:${cmd} len: ${data.length}`);
                break;
        }
    }

    // Log packet decode errors
    function onPacketError(err)
    {
        console.error(`\nPacket decode error: ${err}`);
    }

    // Buffer for output of encoder (will be grown as necessary)
    let encBuffer = Buffer.alloc(1024);
    let next_seq = 101;

    async function send(cmd, buf)
    {
        // Allocate sequenct number
        let current_seq = next_seq++;

        // Time out
        let timeout = null;
        let promise_reject = null;
        let isResolved = false;

        // Setup a promise to receive ack packet callbacks and handle timeouts
        let ack_promise = new Promise((resolve, reject) => {

            // Install packet notification
            ack_notify = function(seq, data) 
            {
                // Ignore if already timed out
                if (isResolved)
                    return;

                // Check correct sequence number
                if (seq == current_seq)
                    resolve(data);
                else
                    reject(new Error("invalid sequence number in ack response"));
            };

            promise_reject = reject;
        });

        // Encode packet
        let enclength = 0;
        packenc.encode(function(encbyte) {
            // Grow buffer?
            if (enclength >= encBuffer.length)
            {
                let newBuffer = Buffer.alloc(encBuffer.length + 1024);
                encBuffer.copy(newBuffer, 0);
                encBuffer = newBuffer;
            }
            // Store byte
            encBuffer[enclength++] = encbyte;

        }, current_seq, cmd, buf);

        try
        {
            ack_promise.then(() => isResolved = true).catch(() => {});

            // Write it and flush
            await port.write(encBuffer.subarray(0, enclength));
            await port.drain();

            // If not yet resolved, setup a timeout
            if (!isResolved)
            {
                // Install timeout
                timeout = setTimeout(() => {
                    timeout = null;
                    promise_reject(new Error("timeout awaiting ack response"));
                }, options.packet_ack_timeout);
            }
        
            // Wait for ack or timeout
            return await ack_promise;
        }
        finally
        {
            ack_notify = null;
            if (timeout)
            {
                clearTimeout(timeout);
            }
        }
    }

    // Create packet decoder
    let decoder = packenc.decode(onPacket, onPacketError, options.max_packet_size);

    // Read data from serial and pump it through the packet decoder
    port.read(function(data) {
        for (let i=0; i<data.length; i++)
        {
            decoder(data[i]);
        }
    });

    function format_hex(val, digits)
    {
        return ("00000000" + val.toString(16)).slice(-digits);
    }

    // Ping the device and return info from response
    async function ping(showDeviceInfo)
    {
        process.stdout.write(`Waiting for device..`);

        for (let i=0; i<options.ping_attempts; i++)
        {
            process.stdout.write('.');
            try
            {
                // Send packet
                let data = await send(PACKET_ID_PING, null);
        
                // Decode response
                let r = {
                    verMajor: data[0],
                    verMinor: data[1],
                    verBuild: data[2],
                    raspi: data.readUInt32LE(4),
                    aarch: data.readUInt32LE(8),
                    boardRevision: data.readUInt32LE(12),
                    boardSerialHi: data.readUInt32LE(16),
                    boardserialLo: data.readUInt32LE(20),
                    maxPacketSize: data.readUInt32LE(24),
                    cpu_freq: data.readUInt32LE(28),
                    min_cpu_freq: data.readUInt32LE(32),
                    max_cpu_freq: data.readUInt32LE(36),
                }
                r.model = piModel.piModelFromRevision(r.boardRevision);
                process.stdout.write(" ok\n");

                // Show device info
                if (showDeviceInfo)
                {
                    process.stdout.write(`Found device: \n`);
                    process.stdout.write(`    - ${r.model.name}\n`);
                    process.stdout.write(`    - Serial: ${format_hex(r.boardSerialHi, 8)}-${format_hex(r.boardserialLo, 8)}\n`);
                    process.stdout.write(`    - CPU Clock: ${r.cpu_freq / 1000000}MHz (range: ${r.min_cpu_freq/1000000}-${r.max_cpu_freq/1000000}MHz)\n`);
                    process.stdout.write(`    - Bootloader: rpi${r.raspi}-aarch${r.aarch} v${r.verMajor}.${r.verMinor}.${r.verBuild}, max packet size: ${r.maxPacketSize}\n`);
                }

                // Do packet size check
                checkPacketSize(r, options.max_packet_size)

                // Do version checks
                checkVersion(r, !options.check_version);

                //process.stdout.write(`cpu_freq: current: ${r.cpu_freq/1000000}, measured:${r.measured_cpu_freq/1000000}, min: ${r.min_cpu_freq/1000000}, max: ${r.max_cpu_freq/1000000}\n`);
                return r;
            }
            catch (err)
            {
                if (err.abort)
                    throw err;
            }
        }

        throw new Error(`Failed to ping device after ${options.ping_attempts} attempts.`)
    }

    // Sends a request to device to switch baud rate and on success
    // switches the baud rate on the underlying serial connection
    async function switchBaud(baud, reset_timeout_millis, cpu_freq)
    {
        process.stdout.write(`Sending request for ${baud.toLocaleString()} baud`);
        if (cpu_freq) 
            process.stdout.write(` and ${(cpu_freq/1000000).toLocaleString()}MHz CPU...`);
        else
            process.stdout.write("...");

        let packet = Buffer.alloc(12);
        packet.writeUInt32LE(baud, 0);
        packet.writeUInt32LE(reset_timeout_millis, 4);
        packet.writeUInt32LE(cpu_freq, 8);
        await send(PACKET_ID_REQUEST_BAUD, packet);
        process.stdout.write(" ok\n");
    
        // Switch underlying serial transport
        await port.switchBaud(baud);
    }

    // Send a data packet
    async function sendData(data)
    {
        await send(PACKET_ID_DATA, data);
    }

    // switches the baud rate on the underlying serial connection
    async function sendGo(startAddress, delayMillis)
    {
        process.stdout.write(`Sending go command 0x${startAddress} with delay ${delayMillis}ms...`);

        let packet = Buffer.alloc(8);
        packet.writeUInt32LE(startAddress, 0);
        packet.writeUInt32LE(delayMillis, 4);
        await send( PACKET_ID_GO, packet);

        process.stdout.write(" ok\n");
    }

    // Return API
    return {
        send,
        ping,
        switchBaud,
        sendData,
        sendGo,
        get options() { return options; },
        get port() { return port; },
    }

}

module.exports = layer;