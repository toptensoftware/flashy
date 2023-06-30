///////////////////////////////////////////////////////////////////////////////////
// Commannd line processing

let fs = require('fs');
let path = require('path');

function showVersion()
{
    let pkg = JSON.parse(fs.readFileSync(path.join(__dirname, 'package.json')), "utf8");
    console.log(`Flashy ${pkg.version}`)
    console.log(`All-In-One Reboot, Flash and Monitor Tool for Raspberry Pi bare metal`);
    console.log();
    console.log(`Copyright (C) 2023 Topten Software`);
    console.log(`    https://www.toptensoftware.com`)
    console.log(`Portions based on code from the Circle project: `);
    console.log(`    https://github.com/rsta2/circle`);
    console.log(`Portions based on code from David Welch's bootloader project:`);
    console.log(`    https://github.com/dwelch67/raspberrypi`);
}

// Help!
function showHelp()
{
    showVersion();

    console.log();
    console.log(`Usage: flashy <serialport> [<imagefile>] [options]`);
    console.log();
    console.log(`<serialport>               Serial port to write to`);
    console.log(`<imagefile>                The .hex or .img file to write (optional)`);
    console.log(`--flashBaud:<N>            Baud rate for flashing (default=1000000)`);
    console.log(`--userBaud:<N>             Baud rate for monitor and reboot magic (default=115200)`);
    console.log(`--reboot:<magic>           Sends a magic reboot string at user baud before flashing`);
    console.log(`--noGo                     Don't send the go command after flashing`);
    console.log(`--go[:<addr>]              Send the go command, even if not flashing, using address if specified`);
    console.log(`                           If address not specified, uses start address from .hex file`);
    console.log(`--goDelay:<ms>             Sets a delay period for the go command`);
    console.log("--packetSize:<N>           Size of data chunks transmitted (default=4096)");
    console.log("--packetTimeout:<N>        Time out to receive packet ack in millis (default=300ms)");
    console.log("--pingAttmempts:<T>        How many times to ping for device before giving up (default=20)");
    console.log("--serialLog:<file>         File to write low level log of serial comms");
    console.log("--resetBaudTimeout:<N>     How long device should wait for packet before resetting");
    console.log("                           to the default baud and CPU frequent boost(default=500ms)");
    console.log("--cpuBoost:<yes|no|auto>   whether to boost CPU clock frequency during uploads");
    console.log("                              auto = yes if flash baud rate > 1M");
    console.log("--bootloader[:<dir>]       Save the bootloader kernel images to directory <dir>");
    console.log("                           or the current directory if <dir> not specified.");
    console.log("                           (note: overwrites existing files without prompting)");
    console.log("--status                   Display device status info without flashing")
    console.log("--noVersionCheck           Don't check bootloader version on device")
    console.log("--noKernelCheck            Don't check the image filename matches expected kernel type for device");
    console.log("--cwd:<dir>                Change current directory");
    console.log("--stress:<N>               Send data packets N times (for load testing)");
    console.log(`--monitor                  Monitor serial port`);
    console.log(`--help                     Show this help`);
    console.log(`--version                  Show version info`);
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
        imageFile: null,
        isHexFile: null,
        serialPortName: null,
        flashBaud: 1000000,
        userBaud: 115200,
        goSwitch: false,
        goAddress: null,
        nogoSwitch: false,
        rebootMagic: null,
        monitor: false,
        goDelay: 0,
        packetSize: 4096,
        packetTimeout: 300,
        pingAttempts: 20,
        resetBaudTimeout: 500,
        cpuBoost: "auto",
        serialLog: null,
        bootloader: null,
        stress: 1,
        status: false,
        checkVersion: true,
        checkKernel: true,
    };

    for (let i=2; i<process.argv.length; i++)
    {   
        let arg = process.argv[i];
        if (arg.startsWith(`--`))
        {
            let colonPos = arg.indexOf(':');
            let sw, value;
            if (colonPos >= 0)
            {
                sw = arg.substring(2, colonPos);
                value = arg.substring(colonPos+1);
            }
            else
            {
                sw = arg.substring(2);
                value = "";
            }

            switch (sw.toLowerCase())
            {
                case `flashbaud`:
                    cl.flashBaud = Number(value);
                    break;

                case `help`:
                    showHelp();
                    process.exit(0);

                case `version`:
                    showVersion();
                    process.exit(0);
        
                case `nogo`:
                    cl.nogoSwitch = true;
                    break;

                case `go`:
                    cl.goSwitch = true;
                    if (value)
                        cl.goAddress = Number(value);
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

                case "cpuboost":
                    if (value != 'auto' && value != 'yes' && value != 'no')
                        fail("--cpuboost should be 'yes', 'no' or 'auto'");
                    cl.cpuBoost = value;
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

                case "status":
                    cl.status = true;
                    break;

                case "noversioncheck":
                    cl.checkVersion = false;
                    break;

                case "nokernelcheck":
                    cl.checkKernel = false;
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

            // Hex file?
            if (arg.toLowerCase().endsWith('.hex'))
            {
                if (cl.imageFile)
                    fail(`Too many command line args: '${arg}'`);
                cl.imageFile = arg;
                cl.isHexFile = true;
                continue;
            }

            // Image file?
            if (arg.toLowerCase().endsWith('.img'))
            {
                if (cl.imageFile)
                    fail(`Too many command line args: '${arg}'`);
                cl.imageFile = arg;
                cl.isHexFile = false;
                continue;
            }

            fail(`Too many command line args: '${arg}'`);
        }
    }

    // Can't do anything without a serial port
    if (!cl.serialPortName && !cl.bootloader)
        fail(`No serial port specified`);

    return cl;
}

module.exports = parseCommandLine();