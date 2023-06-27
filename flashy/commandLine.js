///////////////////////////////////////////////////////////////////////////////////
// Commannd line processing

let fs = require('fs');
let path = require('path');

// Help!
function showHelp()
{
    let pkg = JSON.parse(fs.readFileSync(path.join(__dirname, 'package.json')), "utf8");

    console.log(`Flashy ${pkg.version}`)
    console.log(`All-In-One Reboot, Flash and Monitor Tool for Raspberry Pi bare metal`);
    console.log();
    console.log(`Copyright (C) 2023 Topten Software`);
    console.log(`Portions of bootloader Copyright (c) 2012 David Welch dwelch@dwelch.com`);
    console.log();
    console.log(`Usage: flashy <serialport> [<hexfile>] [options]`);
    console.log();
    console.log(`<serialport>            Serial port to write to`);
    console.log(`<hexfile>               The .hex file to write (optional)`);
    console.log(`--flashBaud:<N>         Baud rate for flashing (default=1000000)`);
    console.log(`--userBaud:<N>          Baud rate for monitor and reboot magic (default=115200)`);
    console.log(`--reboot:<magic>        Sends a magic reboot string at user baud before flashing`);
    console.log(`--noGo                  Don't send the go command after flashing`);
    console.log(`--go                    Send the go command, even if not flashing`);
    console.log(`--goDelay:<ms>          Sets a delay period for the go command`);
    console.log("--packetSize:<N>        Size of data chunks transmitted (default=4096)");
    console.log("--packetTimeout:<N>     Time out to receive packet ack in millis (default=300ms)");
    console.log("--pingAttmempts:<T>     How many times to ping for device before giving up (default=10)");
    console.log("--serialLog:<file>      File to write low level log of serial comms");
    console.log("--resetBaudTimeout:<N>  How long device should wait for packet before resetting");
    console.log("                        to the default baud (default=2500ms)");
    console.log("--bootloader[:<dir>]    Save the bootloader kernel images to directory <dir>");
    console.log("                        or the current directory if <dir> not specified.  Requires");
    console.log("                        `unzip` executable installed on path");
    console.log("--cwd:<dir>             Change current directory");
    console.log("--stress:<N>            Send data packets N times (for load testing)");
    console.log(`--monitor               Monitor serial port`);
    console.log(`--help                  Show this help`);
}

// Abort with message
function fail(msg)
{
    console.error(msg);
    console.error(`Run with --help for instructions`);
    process.exit(7);
}

// Parse command line args
function parseCommandLine()
{
    let cl = {
        hexFile: null,
        serialPortName: null,
        flashBaud: 1000000,
        userBaud: 115200,
        goSwitch: false,
        nogoSwitch: false,
        rebootMagic: null,
        monitor: false,
        goDelay: 0,
        packetSize: 4096,
        packetTimeout: 300,
        pingAttempts: 10,
        resetBaudTimeout: 2500,
        serialLog: null,
        bootloader: null,
        stress: 1,
    };

    for (let i=2; i<process.argv.length; i++)
    {   
        let arg = process.argv[i];
        if (arg.startsWith(`--`))
        {
            let parts = arg.substr(2).split(':');
            let sw = parts[0];
            let value = parts[1];
            switch (sw.toLowerCase())
            {
                case `flashbaud`:
                    cl.flashBaud = Number(value);
                    break;

                case `help`:
                    showHelp();
                    process.exit(0);

                case `nogo`:
                    cl.nogoSwitch = true;
                    break;

                case `go`:
                    cl.goSwitch = true;
                    break;

                case `godelay`:
                    cl.goDelay = Number(value);
                    break;

                case `reboot`:
                    cl.rebootMagic = value;
                    break;

                case `monitor`:
                    cl.monitor = true;
                    break;

                case `userbaud`:
                    cl.userBaud = Number(value);
                    break;

                case `packetsize`:
                    cl.packetSize = Number(value);
                    break;
                    
                case `packettimeout`:
                    cl.packetTimeout = Number(value);
                    break;

                case `pingattempts`:
                    cl.pingAttempts = Number(value);
                    break;
                            
                case "resetbaudtimeout":
                    cl.resetBaudTimeout = Number(value);
                    break;
            
                case "seriallog":
                    cl.serialLog = value;
                    break;

                case "bootloader":
                    cl.bootloader = value || ".";
                    break;

                case "cwd":
                    process.chdir(value);
                    break;

                case "showargs":
                    console.log(process.argv);
                    break;

                case "stress":
                    cl.stress = Number(value);
                    break;  

                default:
                    fail(`Unknown switch --${sw}`);
            }
        }
        else
        {
            // First arg is serial port name
            if (cl.serialPortName == null)
            {
                cl.serialPortName = arg;
                continue;
            }
            else if (cl.hexFile == null)
            {
                // Second arg is the .hex file
                cl.hexFile = arg;
                
                // Sanity check
                if (!arg.toLowerCase().endsWith('.hex'))
                {
                    console.error(`Warning: hex file '${arg}' doesn't have .hex extension.`);
                }
            }
            else
            {
                fail(`Too many command line args: '${arg}'`);
            }
        }
    }

    // Can't do anything without a serial port
    if (!cl.serialPortName && !cl.bootloader)
        fail(`No serial port specified`);

    return cl;
}

module.exports = parseCommandLine();