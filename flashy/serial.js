///////////////////////////////////////////////////////////////////////////////////
// Serial port wrapper
// 
// Handles info logging, serial port data logging, switching baud rates etc.

let os = require('os');
let fs = require('fs');

let FlashyError = require('./FlashyError');

let SerialPort;

// Load the serial port module and display handy message if can't
if (!SerialPort)
{
    try
    {
        SerialPort = require('serialport');
    }
    catch (err)
    {
        console.log(`\nCan't find module 'serialport'.`);
        console.log(`Please run 'npm install':\n`);
        console.log(`   ` + __dirname + `$ npm install`);
        process.exit(7);
    }
}


function serialPort(serialPortName, options)
{
    options = Object.assign({
        baudRate: 0,
        log: function() { },
        logFilename: null,
    }, options);

    // State
    let port;
    let isOpen = false;
    let serialPortOptions = {
        dataBits: 8,
        stopBits: 1,
        parity: 'none',
        baudRate: options.baudRate,
    };
    let readCallback = null;

    log = options.log;

    let fdLogFile = 0;
    if (options.logFilename)
    {
        // Create log file
        fdLogFile = fs.openSync(options.logFilename, "w");
    }

    function logfile(msg)
    {
        if (fdLogFile)
        {
            fs.writeSync(fdLogFile, process.uptime().toString() + ":");
            fs.writeSync(fdLogFile, msg);
            fs.writeSync(fdLogFile, "\n");
        }
    }

    // Remap WSL serial port names to Windows equivalent if appropriate
    if (os.platform() == 'win32' && serialPortName.startsWith(`/dev/ttyS`))
    {
        let remapped = `COM` + serialPortName.substr(9);
        log && log(`Using '${remapped}' instead of WSL port '${serialPortName}'.\n`)
        serialPortName = remapped;
    }

    // Open the serial port
    async function open()
    {
        // Already open?
        if (isOpen)
            return;
        isOpen =true;

        // Delay until baud rate set
        if (serialPortOptions.baudRate == 0)
        {
            return;
        }

        // Open it
        log && log(`Opening ${serialPortName} at ${serialPortOptions.baudRate.toLocaleString()} baud...`)
        logfile(`Opening ${serialPortName} at ${serialPortOptions.baudRate}...`)
        port = new SerialPort(serialPortName, serialPortOptions, function(err) {
            if (err)
            {
                throw new FlashyError(`Failed to open serial port: ${err.message}`);
            }
        });
        log && log(` ok\n`);

        // Listen for data
        port.on('data', function(data) {
            logfile(`recv: ${data.toString("hex")}`);
            if (readCallback)
                readCallback(data);
        });
    }
    
    // Drain
    async function drain()
    {
        // Drain port
        await new Promise((resolve, reject) => {
            port.drain((function(err) {
                if (err)
                    reject(err);
                else
                    resolve();
            }));
        });
    }
    
    
    // Close the serial port (if it's open)
    async function close()
    {
        isOpen = false;
        if (port)
        {
            log && log(`Closing serial port...`)
    
            // Drain
            await drain();
    
            // Close the port
            await new Promise((resolve, reject) => {
                port.close(function(err) { 
                    if (err)
                        reject(err);
                    else
                    {
                        resolve();
                    }
                });
            });

            port = null;
    
            log && log(` ok\n`);
        }
    }

    // Switch baud rate
    async function switchBaud(baud)
    {
        // Log
        logfile(`switchBaud: ${baud}`);

        // Redundant?
        if (serialPortOptions.baudRate == baud && port)
             return;

        // If open, close and re-open
        if (isOpen)
        {
            // Flush and close
            await close();

            // Sometimes opening serial port immediately after closing gives
            // access denied error on Windows.  Small delay to try to alleviate that.
            await new Promise((resolve) => setTimeout(resolve, 20));
    
            // Switch and re-open
            serialPortOptions.baudRate = baud;
            await open();
        }
    }

    // Async delay helper
    function delay(period)
    {
        let start = process.uptime();
        while (process.uptime() - start < period/1000)
        {
            
        }
    }

    async function writeSlow(data)
    {
        for (let i=0; i<data.length; i++)
        {
            await write(data.subarray(i, i+1));
            delay(1);
        }
    }

    async function write(data)
    {
        return new Promise((resolve,reject) => 
        {
            // Log
            logfile(`send: ${data.toString("hex")}`);

            port.write(data, function(err) 
            {
                if (err)
                    reject(err);
                else
                    resolve();
            });
        });
    }

    function read(callback)
    {
        readCallback = callback;
    }

    return {
        open,
        close,
        drain,
        switchBaud,
        write,
        read,
        writeSlow,
    }
    
}

module.exports = serialPort;