# Flashy v2

All-In-One Reboot, Flash and Monitor Tool for Raspberry Pi bare metal.



## Features

* Binary mode transfers (faster than sending .hex file as text)
* Strong packet encoding with crc32 and all packets acknowledged by device for rebustness
* Stateless device side bootloader for reliable recovery and restart
* Dynamic baud-rate switching (saves updating kernel image on device to switch baud rate)
* Automatic idle time baud rate reset (for recovery)    
* Configurable packet size and timeout for flexibility with different devices
* Host side .hex file parsing and re-chunking for packet size balancing and simpler
  device side implementation
* Supports sending magic reboot strings
* Supports serial port monitoring after flashing
* Delayed image starts (by timeout, or by command)
* Includes packaged pre-compiled bootloader images
* Activity indicator shows ready status feedback



## Rationale for v2

Flashy v2 is a complete re-write of Flashy v1 and the original bootloader program.  The 
entire project has been cleaned up, refactored and re-engineered with a focus on robustness.

While Flashy v1 could provide very fast flash times it often suffered from reliability and
recovery issues.  It typically just saturated the serial port write buffer and hoped the 
device could keep up.

This version moves to a strongly encoded packet format with 32-bit checksums and ack messages 
for all host to device transmissions.  It also offers several other improvements and new features
(see above).

For simplicity, this version is incompatible with previous versions of the bootloader and 
requires the included bootloader images be installed on the target device. 



## Quick Guide

1. Install Flashy (see notes below if running under WSL2)

        > npm install -g @toptensoftware/flashy

2. Copy the new bootloader kernel images to the Raspberry Pi SD card

        > flashy --bootloader:path_to_your_sd_card

3. Use it!  eg: to reboot, flash and then monitor the serial port:

        > flashy kernel.hex /dev/ttyS3 --reboot:yourmagicstring --monitor



## Installation

Flashy is written in JavaScript and requires NodeJS to be installed.

Assuming node and npm are installed, install flashy as follows;

```
sudo npm install -g @toptensoftware/flashy
```

See notes below on using Flashy under WSL 2.



## Setup - Bootloader

Flashy v2 is incompatible with previous versions and requires a new bootloader image
to be installed on the target device's SD card.

Pre-compiled kernel images are included and can be extracted with the
`--bootloader` option.  

eg: To place a copy of the kernel images in the root of drive D:

```
flashy --bootloader:D:\
```



## Manually Running Flashy

A typical command line to reboot the device, flash an image and start monitoring looks like this:

```
flashy kernel7.hex /dev/ttyS3 --flashBaud:2000000 --reboot:yourmagicstring --monitor
```

Run `flashy --help` for more details, or see below.



## Activity LED

On devices that have an activity indicator, the bootloader provides the following feedback:

* When idle and ready the led flashes in a heartbeat pattern (2 flashes in quick succession every 1 second).
* When receiving data, the activity LED toggles on receipt of every packet (ie: it flashes rapidly)
* Otherwise the LED is off.  

When the indicator is off, this indicates either the bootloader isn't running, or is still in 
in a non-default (ie: high) baud rate mode after a failed or cancelled flash operation.  Normally, 
2.5 seconds after a failed flash the bootloader will revert to the default baud rate and the heart
beat indicator will re-appear.



## Running under WSL-2

WSL2 is a great development environment for bare metal Raspberry Pi projects however its lack
of support for USB serial ports makes it difficult to flash a connected device.

There's a couple of solutions for this:

* Use USBIPD as [described here](https://learn.microsoft.com/en-us/windows/wsl/connect-usb), or
* Install Flashy on both the Windows host and the WSL2 operating system and let it find itself.

When flashy is launched in a WSL-2 environment with a `/dev/ttyS*` serial port name it will
attempt to relaunch itself as a Windows process from where it has access to serial ports. For this 
to work, Flashy needs to be installed as an npm global tool on both the Windows host
machine and the WSL guest operating system.

ie: 

* on Windows run: `npm install -g @toptensoftware/flashy`
* and on WSL/Linux run: `sudo npm install -g @toptensoftware/flashy`

Once installed in both environments, Flashy will detect when you're using a serial port, 
find itself in Windows and relaunch itself automatically.



## Command Line Options

```
Flashy 2.0.4
All-In-One Reboot, Flash and Monitor Tool for Raspberry Pi bare metal

Copyright (C) 2023 Topten Software
Portions of bootloader Copyright (c) 2012 David Welch dwelch@dwelch.com

Usage: flashy <serialport> [<hexfile>] [options]

<serialport>            Serial port to write to
<hexfile>               The .hex file to write (optional)
--flashBaud:<N>         Baud rate for flashing (default=1000000)
--userBaud:<N>          Baud rate for monitor and reboot magic (default=115200)
--reboot:<magic>        Sends a magic reboot string at user baud before flashing
--noGo                  Don't send the go command after flashing
--go                    Send the go command, even if not flashing
--goDelay:<ms>          Sets a delay period for the go command
--packetSize:<N>        Size of data chunks transmitted (default=4096)
--packetTimeout:<N>     Time out to receive packet ack in millis (default=300ms)
--pingAttmempts:<T>     How many times to ping for device before giving up (default=10)
--serialLog:<file>      File to write low level log of serial comms
--resetBaudTimeout:<N>  How long device should wait for packet before resetting
                        to the default baud (default=2500ms)
--bootloader[:<dir>]    Save the bootloader kernel images to directory <dir>
                        or the current directory if <dir> not specified.  Requires
                        `unzip` executable installed on path
--cwd:<dir>             Change current directory
--monitor               Monitor serial port
--help                  Show this help
```



## License

Apache 2.0.  See LICENSE file.
