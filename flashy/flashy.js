#!/usr/bin/env node

///////////////////////////////////////////////////////////////////////////////////
// Flashy v2.0

import os from 'node:os';
import path from 'node:path';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';

import serial from './serial.js';
import packetLayer from './packetLayer.js';
import commandLineParser from './commandLineParser.js';
import wslUtils from './wslUtils.js';

const __dirname = path.dirname(fileURLToPath(import.meta.url));


// Commands
let command_arg_specs = [
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
        name: "monitor",
        help: "Monitors serial port output",
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
        name: "reboot",
        help: "Sends a magic reboot string",
    },
    {
        name: "shell",
        help: "Opens an interactive command shell"
    },
    /*
    {
        name: "trace",
        help: "Display trace messages from bootloader"
    },
    */
];

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
        name: "--baud:<n>|--flashBaud:<n>|-b",
        help: "Baud rate for flashing and push/pull (default=1000000)",
        default: 1000000,
    },
    {
        name: "--user-baud:<n>|--userBaud:<n>",
        help: "Baud rate for monitoring and sending reboot magic (default=115200)",
        default: 115200,
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
    packageDir: path.dirname(__dirname),
    failOnUnknownArg: false,
    copyright: "Copyright (C) 2023 Topten Software\n"
            + "    https://www.toptensoftware.com\n"
            + "Portions based on code from the Circle project:\n"
            + "    https://github.com/rsta2/circle\n"
            + "Portions based on code from David Welch's bootloader project:\n"
            + "    https://github.com/dwelch67/raspberrypi\n",
    spec: [
        ...command_arg_specs,
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
        {
            name: "--ensure-version:<ver>",
            help: "Abort if Flashy version doesn't match",
        }
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

// Check WSL and host versions match
if (cl.ensureVersion)
{
    let version = JSON.parse(fs.readFileSync(path.join(__dirname, 'package.json')), "utf8").version;
    if (version != cl.ensureVersion)
    {
        process.stdout.write(`Flashy version mismatch (requested: ${cl.ensureVersion} actual: ${version})\n`);
        process.stdout.write(`Please ensure you've got the same version of Flashy installed on WSL and\n`);
        process.stdout.write(`on the Windows host.\n`)
        process.exit(7);
    }

}

// Change working directory?
if (cl.cwd)
    process.chdir(cl.cwd);
    
// Main!
let port;
try
{        
    // Parse all commands
    let commands = [];
    let args = cl.$tail;
    let command = cl.$command;
    delete cl.$tail;
    delete cl.$command;
    while (args)
    {
        // Load the command handler
        let command_handler = (await import(`./cmd_${command}.js`)).default;

        // Process the command's args
        let command_parser = commandLineParser.parser({
            usagePrefix: `flashy ${command}`,
            packageDir: path.dirname(__dirname),
            synopsis: command_handler.synopsis,
            spec: [
                ...command_handler.spec,
                ...(command_handler.usesSerialPort !== false ? serial_arg_specs : []), 
                ...common_arg_specs
            ],
        });

        // Setup command context
        let ctx = {
            cl: command_parser.parse(args, cl),
            handler: command_handler,
            usesSerialPort: command_handler.usesSerialPort !== false,
            usesPacketLayer: command_handler.usesSerialPort !== false && command_handler.usesPacketLayer !== false
        };

        // Handle command help and errors
        command_parser.handle_help(ctx.cl);
        command_parser.check(ctx.cl);

        // If running under WSL2 and trying to use a regular serial port
        // relaunch self as a Windows process
        if (wslUtils.isWsl2() && ctx.usesSerialPort && cl.port.match(/^COM\d+/i))
        {
            process.exit(wslUtils.runSelfUnderWindows());
        }

        // Next command
        commands.push(ctx);

        // Process next command
        if (ctx.cl.$tail)
        {
            // Parse next command name
            let tail_parser = commandLineParser.parser({
                spec: command_arg_specs
            });
            let tail_cl = tail_parser.parse(ctx.cl.$tail);
            command = tail_cl.$command;
            args = tail_cl.$tail;
        }
        else
        {
            break;
        }
    }

    // Now execute all commands
    for (let ctx of commands)
    {        
        // Open serial port if needed 
        if (ctx.usesSerialPort)
        {
            // Close old port if different
            if (port && ctx.cl.port != port.portName)
            {
                await port.close();
                port = null;
            }

            // Create port
            if (port == null)
            {
                // Setup serial port
                port = serial(ctx.cl.port, {
                    baudRate: 0,  // delay open until first baud rate switch
                    log: ctx.cl.verbose ? (msg) => process.stdout.write(msg) : null,
                    logFilename: ctx.cl.serialLog,
                });
                await port.open();
            }

            // Attach port to context
            ctx.port = port;
        }

        // Create packet layer
        if (ctx.usesPacketLayer)
        {
            let packetLayerOptions = {
                max_packet_size: Math.max(128, ctx.cl.packetSize),
                packet_ack_timeout: ctx.cl.packetTimeout,
                ping_ack_timeout: ctx.cl.pingTimeout,
                ping_attempts: ctx.cl.pingAttempts,
                check_version: !ctx.cl.noVersionCheck,
                log: ctx.cl.verbose ? (msg) => process.stdout.write(msg) : null,
            };
            ctx.layer = packetLayer(ctx.port, packetLayerOptions);
        }

        // Run the command
        await ctx.handler.run(ctx);

        // Disconnect packet layer from socket
        if (ctx.port)
            ctx.port.read(null);
    }

    if (commands.length == 0 && cl.verbose)
    {
        process.stdout.write("Nothing to do.\n")
    }
}
catch (err)
{
    console.error(err.message);
    //throw err;
}
finally
{
    // Clean up
    if (port)
        await port.close();
}
