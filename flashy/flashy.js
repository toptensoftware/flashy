#!/usr/bin/env node

///////////////////////////////////////////////////////////////////////////////////
// Flashy v2.0

/*
let os = require('os');
let child_process = require('child_process');
let path = require('path');
let fs = require('fs');
 */

let serial = require('./serial');
let packetLayer = require('./packetLayer');
let commandLineParser = require('./commandLineParser');
let wslUtils = require('./wslUtils');


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
            name: "--version",
            help: "Show version info",
            terminal: false,
        },
        {
            name: "--help",
            help: "Show this help",
            terminal: false,
        },
        {
            name: "--cwd:<dir>",
            help: "Change current directory",
        },
        {
            name: "flash",
            help: "Flash a kernel image to the device"
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
            name: "ls",
            help: "List directories and files on the device's SD card"
        },
    ]
});

// Parse command line
let cl = parser.parse(process.argv.slice(2));
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
            spec: command_handler.spec,
        });
        let command_cl = command_parser.parse(cl.$tail);
        command_parser.handle_help(command_cl);
        command_parser.check(command_cl);

        // Merge base options and command options
        ctx.cl = Object.assign(cl, command_cl);
        delete ctx.cl.$tail;

        // If running under WSL2 and trying to use a regular serial port
        // relaunch self as a Windows process
        if (wslUtils.isWsl2() && cl.serialport && cl.serialport.startsWith("/dev/ttyS"))
        {
            process.exit(wslUtils.runSelfUnderWindows());
        }
        
        // Open serial port if needed 
        if (cl.serialport)
        {
            // Setup serial port
            ctx.port = serial(cl.serialport, {
                baudRate: 0,  // delay open until first baud rate switch
                log: (msg) => process.stdout.write(msg),
                logFilename: cl.serialLog,
            });
            await ctx.port.open();

            // Also create packet layer
            if (cl.packetSize)
            {
                let packetLayerOptions = {
                    max_packet_size: Math.max(128, cl.packetSize),
                    packet_ack_timeout: cl.packetTimeout,
                    ping_attempts: cl.pingAttempts,
                    check_version: !cl.noVersionCheck,
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
