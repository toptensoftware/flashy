///////////////////////////////////////////////////////////////////////////////////
// Packet Layer
//
// Handles sending and receiving encoded packets to/from device

let fs = require('fs');
let path = require('path');

let packenc = require('./packetEncoder');
let piModel = require('./piModel');
let RestartableTimeout = require('./restartableTimeout');
let struct = require('./struct');


// Packet ID's
const PACKET_ID_PING = 0;
const PACKET_ID_ACK = 1;
const PACKET_ID_ERROR = 2;
const PACKET_ID_DATA = 3;
const PACKET_ID_GO = 4;
const PACKET_ID_REQUEST_BAUD = 5;
const PACKET_ID_COMMAND = 6;
const PACKET_ID_STDOUT = 7;
const PACKET_ID_STDERR = 8;
const PACKET_ID_PULL = 9;
const PACKET_ID_PULL_HEADER = 10;
const PACKET_ID_PULL_DATA = 11;
const PACKET_ID_PUSH_DATA = 12;
const PACKET_ID_PUSH_COMMIT = 13;

let lib = struct.library();
lib.defineType({
    name: "command_ack",
    fields: [
        "uint32le exitCode",
        "string cwd",
    ]
});
lib.defineType({
    name: "ping",
    fields: [
        "uint32le current_time_secs",
    ]
});
lib.defineType({
    name: "ping_ack",
    fields: [
        "uint8 verMajor",
        "uint8 verMinor",
        "uint8 verBuild",
        "uint8 _unused",
        "uint32le raspi",
        "uint32le aarch",
        "uint32le boardRevision",
        "uint32le boardSerialHi",
        "uint32le boardserialLo",
        "uint32le maxPacketSize",
        "uint32le cpu_freq",
        "uint32le min_cpu_freq",
        "uint32le max_cpu_freq",
    ]    
});
lib.defineType({
    name: "push_commit",
    fields: [
        "uint32le token",
        "uint32le size",
        "uint16le time",
        "uint16le date",
        "uint8 attr",
        "uint8 overwrite",
        "string name",
    ]
});



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
        packet_ack_timeout: 1000,
        ping_ack_timeout: 300,
        ping_attempts: 10,
        check_version: true,
    }, options || {})

    // Get log to local var
    let log = options.log;

    // Callback to be invoked on receipt of ack packet
    let ack_notify = null;

    // Timer, restarted on each packet received
    let timeout = null;

    // Callback for pull handler
    let stdio_handler = null;
    let pull_handler = null;

    // Buffer for output of encoder (will be grown as necessary)
    let encBuffer = Buffer.alloc(1024);

    // Next sequence number
    let next_seq = 101;
    let current_seq = -1;
    
    
    
    // Receive packets
    function onPacket(seq, cmd, data)
    {
        if (timeout)
            timeout.restart();

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

            case PACKET_ID_STDERR:
                if (stdio_handler)
                    stdio_handler.onStdErr(data);
                else
                    process.stderr.write(data);
                break;
        
            case PACKET_ID_STDOUT:
                if (stdio_handler)
                    stdio_handler.onStdOut(data);
                else
                    process.stdout.write(data);
                break;
                
            case PACKET_ID_PULL_HEADER:
                if (pull_handler && seq == current_seq)
                    pull_handler.onHeader(data);
                break;

            case PACKET_ID_PULL_DATA:
                if (pull_handler && seq == current_seq)
                    pull_handler.onData(data);
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

    async function send(cmd, buf)
    {
        // Allocate sequenct number
        current_seq = next_seq++;

        // Time out
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
                timeout = new RestartableTimeout(() => {
                    timeout = null;
                    promise_reject(new Error("timeout awaiting response"));
                }, cmd == PACKET_ID_PING ? options.ping_ack_timeout : options.packet_ack_timeout);
            }
        
            // Wait for ack or timeout
            return await ack_promise;
        }
        finally
        {
            ack_notify = null;
            if (timeout)
            {
                timeout.cancel();
            }
            current_seq = -1;
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

    let last_ping_result;

    // Ping the device and return info from response
    async function ping(showDeviceInfo)
    {
        log && log(`Waiting for device..`);

        for (let i=0; i<options.ping_attempts; i++)
        {
            log && log('.');
            try
            {
                // Setup ping packet with current time in seconds
                let ping = {
                    current_time_secs: (Date.now() - new Date().getTimezoneOffset()*60*1000)/1000
                }

                // Send ping
                let data = await send(PACKET_ID_PING, lib.encode("ping", ping));
        
                // Decode response
                let r = lib.decode("ping_ack", data);
                r.model = piModel.piModelFromRevision(r.boardRevision);
                log && log(" ok\n");

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

                last_ping_result = r;
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

    async function boost(cl)
    {
        let cpufreq = 0;
        if ((cl.cpuBoost == "auto" && cl.baud > 1000000) || cl.cpuBoost == 'yes')
            cpufreq = last_ping_result.max_cpu_freq;
        if (cl.baud != 115200 || cpufreq != 0)
        {
            await switchBaud(cl.baud, cl.resetTimeout, cpufreq);
            await ping();
        }    
    }

    // Sends a request to device to switch baud rate and on success
    // switches the baud rate on the underlying serial connection
    async function switchBaud(baud, reset_timeout_millis, cpu_freq)
    {
        if (log)
        {
            log(`Sending request for ${baud.toLocaleString()} baud`);
            if (cpu_freq) 
                log(` and ${(cpu_freq/1000000).toLocaleString()}MHz CPU...`);
            else
                log("...");
        }

        let packet = Buffer.alloc(12);
        packet.writeUInt32LE(baud, 0);
        packet.writeUInt32LE(reset_timeout_millis, 4);
        packet.writeUInt32LE(cpu_freq, 8);
        await send(PACKET_ID_REQUEST_BAUD, packet);
        log && log(" ok\n");
    
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
        log && log(`Sending go command 0x${startAddress.toString(16)} with delay ${delayMillis}ms...`);

        let packet = Buffer.alloc(8);
        packet.writeUInt32LE(startAddress, 0);
        packet.writeUInt32LE(delayMillis, 4);
        await send(PACKET_ID_GO, packet);

        log && log(" ok\n");
    }


    async function sendCommand(cwd, cmd, handler)
    {
        log && log(`Sending command ${cwd}> ${cmd}...\n`);

        // Format packet
        let buf_cwd = Buffer.from(cwd + "\0", 'utf8');
        let buf_cmd = Buffer.from(cmd + '\0', 'utf8');
        let packet = Buffer.concat([buf_cwd, buf_cmd]);

        // Send it
        stdio_handler = handler;
        let r = await send(PACKET_ID_COMMAND, packet);
        stdio_handler = null;

        // Done
        log && log(" ok\n");
        return lib.decode("command_ack", r);
    }

    async function sendPull(file, handler)
    {
        log && log(`Sending pull request ${file}...\n`);

        // Set pull handler
        pull_handler = handler;

        // Send request
        let buf = Buffer.from(file + "\0", 'utf8');
        let r = await send(PACKET_ID_PULL, buf);

        // Clean up
        pull_handler = null;

        log && log(" ok\n");
        return r;
    }

    async function sendPushData(data)
    {
        return await send(PACKET_ID_PUSH_DATA, data);
    }

    async function sendPushCommit(commit)
    {
        return await send(PACKET_ID_PUSH_COMMIT, lib.encode("push_commit", commit));
    }

    // Parse the output of a "ls -l" output from device
    function parse_ls_output(output)
    {
        let entries = [];
        for (let l of output.split('\n'))
        {
            let m = l.match(/^([-d][-r][-s][-h][-a])\s+(\d\d)\/(\d\d)\/(\d\d\d\d)\s+(\d\d)\:(\d\d)\:(\d\d)\s+(\d+)\s+(.*)$/);
            if (m)
            {
                entries.push({
                    attr: m[1],
                    mtime: new Date(parseInt(m[4]), parseInt(m[3])-1, parseInt(m[2]), parseInt(m[5]), parseInt(m[6]), parseInt(m[7])),
                    size: m[8],
                    name: m[9],
                    isdir: m[1][0] == 'd',
                })
            }
        }

        return entries;

    }

    async function exec_cmd(rwd, cmd, suppressErrors)
    {
        let bufs = [];
        function onStdOut(data)
        {
            bufs.push(Buffer.concat([data]));
        }

        function onStdErr(data)
        {
            if (!suppressErrors)
                process.stderr.write(data);
        }

        let r = await sendCommand(rwd, cmd, {
            onStdOut,
            onStdErr,
        })

        if (r.exitCode != 0)
        {
            if (suppressErrors)
                return "";
            throw new Error(`Failed to exec '${cmd}'`);
        }

        return Buffer.concat(bufs).toString('utf8');
    }


    async function exec_ls(rwd, cmd, emptyOnError)
    {
        let output = await exec_cmd(rwd, cmd, emptyOnError);
        return parse_ls_output(output);
    }


    // Return API
    return {
        send,
        ping,
        boost,
        switchBaud,
        sendData,
        sendGo,
        sendCommand,
        sendPull,   
        sendPushData,
        sendPushCommit,
        exec_cmd,
        exec_ls,
        get options() { return options; },
        get port() { return port; },
    }

}

module.exports = layer;