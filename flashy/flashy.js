#!/usr/bin/env node

///////////////////////////////////////////////////////////////////////////////////
// Flashy v2.0

let os = require('os');
let path = require('path');
let fs = require('fs');

let serial = require('./serial');
let packetLayer = require('./packetLayer');
let commandLineParser = require('./commandLineParser');
let wslUtils = require('./wslUtils');

// Args for all commands that use the serial port
let serial_arg_specs = [
    {
        name: "--port:<portname>",
        help: "Serial port of target device\n'--port:' prefix not required for COM* or /dev/tty*",
        valuePattern: /^([Cc][Oo][Mm]\d+|\/dev\/tty.+)$/
    },
    {
        name: "--packet-size:<n>",
        help: "Size of data chunks transmitted (default=4096)",
        default: 4096,
    },
    {
        name: "--packet-timeout:<n>",
        help: "Time out to receive packet ack in millis (default=300ms)",
        default: 1000,
    },
    {
        name: "--ping-timeout:<n>",
        help: "Time out to receive ping ack in millis (default=300ms)",
        default: 300,
    },
    {
        name: "--ping-attempts:<n>",
        help: "How many times to ping for device before giving up (default=20)",
        default: 20,
    },
    {
        name: "--serial-log:<file>",
        default: null,
        help: "File to write low level log of serial comms"
    },
    {
        name: "--no-version-check",
        help: "Don't check bootloader version on device",
    },
    {
        name: "--baud:<n>|--flash-baud:<n>|-b",
        help: "Baud rate for flashing and push/pull (default=1000000)",
        default: 1000000,
    },
    {
        name: "--reset-timeout:<ms>",
        help: "How long device should wait for packet before resetting "
            + "to the default baud and CPU frequency (default=500ms)",
        parse: commandLineParser.parse_integer,
        default: 500,
    },
    {
        name: "--cpu-boost:[yes|no|auto]",
        help: "Whether to boost CPU clock frequency during uploads\n"
            + "auto = yes if flash baud rate > 1M",
        default: "auto",
    },
    {
        name: "--verbose|-v",
        help: "Display additional informational messages",
        default: false,
    }
]

// Args common to all commands
let common_arg_specs = [
    {
        name: "--help",
        help: "Show this help",
        terminal: true,
    },
]


// Base command line options
let parser = commandLineParser.parser({
    usagePrefix: "flashy",
    packageDir: __dirname,
    failOnUnknownArg: false,
    copyright: "Copyright (C) 2023 Topten Software\n"
            + "    https://www.toptensoftware.com\n"
            + "Portions based on code from the Circle project:\n"
            + "    https://github.com/rsta2/circle\n"
            + "Portions based on code from David Welch's bootloader project:\n"
            + "    https://github.com/dwelch67/raspberrypi\n",
    spec: [
        {
            name: "flash",
            help: "Flash a kernel image to the device.  (default if no other command specified)"
        },
        {
            name: "bootloader",
            help: "Commands for extracing bootloader images"
        },
        {
            name: "status",
            help: "Display status of the device",
        },
        {
            name: "go",
            help: "Starts a previously flashed image"
        },
        {
            name: "exec",
            help: "Executes a shell command on the target device",
        },
        {
            name: "pull",
            help: "Copies files from the device",
        },
        {
            name: "push",
            help: "Copies files to the device",
        },
        {
            name: "shell",
            help: "Opens an interactive command shell"
        },
        ...serial_arg_specs,
        ...common_arg_specs,
        {
            name: "--version",
            help: "Show version info",
            terminal: true,
        },
        {
            name: "--cwd:<dir>",
            help: "Change current directory",
        },
    ]
});

// Load command line defaults
let defaults = {};
let defaultsFile = path.join(os.homedir(), ".flashy.json");
if (fs.existsSync(defaultsFile))
{
    defaults = JSON.parse(fs.readFileSync(defaultsFile, "utf8"));
}

// Parse command line
let cl = parser.parse(process.argv.slice(2), defaults);
if (!cl.$command)
    parser.handle_help(cl);
    
// If no command specified, assume "flash"
if (!cl.$command)
    cl.$command = "flash";

// Change working directory?
if (cl.cwd)
    process.chdir(cl.cwd);
    
// Main!
(async function() {

    let ctx = {};
    try
    {
        // Load the command handler
        let command_handler = require(`./cmd_${cl.$command}`);

        // Process the command's args
        let command_parser = commandLineParser.parser({
            usagePrefix: `flashy ${cl.$command}`,
            packageDir: __dirname,
            synopsis: command_handler.synopsis,
            spec: [
                ...command_handler.spec,
                ...(command_handler.usesSerialPort !== false ? serial_arg_specs : []), 
                ...common_arg_specs
            ],
        });
        let command_cl = Object.assign(cl, command_parser.parse(cl.$tail));
        command_parser.handle_help(command_cl);
        command_parser.check(command_cl);

        // Merge base options and command options
        ctx.cl = command_cl;
        delete ctx.cl.$tail;

        // If running under WSL2 and trying to use a regular serial port
        // relaunch self as a Windows process
        if (wslUtils.isWsl2() && cl.port && cl.port.startsWith("/dev/ttyS"))
        {
            process.exit(wslUtils.runSelfUnderWindows());
        }
        
        // Open serial port if needed 
        if (cl.port)
        {
            // Setup serial port
            ctx.port = serial(cl.port, {
                baudRate: 0,  // delay open until first baud rate switch
                log: ctx.cl.verbose ? (msg) => process.stdout.write(msg) : null,
                logFilename: cl.serialLog,
            });
            await ctx.port.open();

            // Also create packet layer
            if (cl.packetSize)
            {
                let packetLayerOptions = {
                    max_packet_size: Math.max(128, cl.packetSize),
                    packet_ack_timeout: cl.packetTimeout,
                    ping_ack_timeout: cl.pingTimeout,
                    ping_attempts: cl.pingAttempts,
                    check_version: !cl.noVersionCheck,
                    log: ctx.cl.verbose ? (msg) => process.stdout.write(msg) : null,
                };
                ctx.layer = packetLayer(ctx.port, packetLayerOptions);
            }
        }

        // Run the command
        await command_handler.run(ctx);
    }
    catch (err)
    {
        console.error(err.message);
    }
    finally
    {
        // Clean up
        if (ctx.port)
            ctx.port.close();
    }
})();
