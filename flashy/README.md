# Flashy v2

All-In-One Reboot, Flash and Monitor Tool for Raspberry Pi bare metal.



## Features

* **New V2** Strong packet encoding with crc32 and all packets acknowledged by device for rebustness
* **New V2** Stateless device side bootloader for reliable recovery and restart
* **New V2** Dynamic baud-rate switching saves updating kernel image on device to switch baud rate
* **New V2** Ability to boost device CPU frequency during high baud uploads
* **New V2** Automatic idle time baud rate and CPU frequency reset (for recovery)    
* **New V2** Activity LED indicator shows ready status feedback
* **New V2** Bootloader version checks ensures flashy script and bootloader image are compatible
* **New V2** Kernel checks ensure hexfile being uploaded matches the device
* **New V2** Configurable packet size and timeout for flexibility with different devices
* **New V2** Host side .hex file parsing and re-chunking for packet size balancing and simpler
  device side implementation
* **New V2** Includes packaged pre-compiled bootloader images
* **New V2** Supports sending `.img` or `.hex` image files
* Binary mode transfers (faster than sending .hex file as text)
* Supports sending magic reboot strings
* Supports serial port monitoring after flashing
* Delayed image starts (by timeout, or by command)



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



## Manual Uploads

A typical command line to reboot the device, flash an image and start monitoring looks like this:

```
flashy kernel7.hex /dev/ttyS3 --flashBaud:2000000 --reboot:yourmagicstring --monitor
```

Run `flashy --help` for more details, or see below.



## CPU Boost

At high upload rates (> 1M baud) the device's CPU frequency is boosted to the maximum rate
for the period of the upload.  This helps the device keep up with the data rate and often
prevents a failed upload.

CPU boost can be overridden with the `--cpuBoost` command line option:

* `--cpuBoost:no` - disable CPU boost even for > 1M baud uploads
* `--cpuBoost:yes` - boost CPU even for < 1M baud uploads
* `--cpuBoost:auto` - the default boost of > 1M baud upload.



## Activity LED

On devices that have an activity LED, the bootloader provides the following feedback:

* When idle and ready the led flashes in a heartbeat pattern (2 flashes in quick succession every 1 second).
* When receiving data, the activity LED toggles on receipt of every packet (ie: it flashes rapidly, flickers, or appears dim depending on baud rate and packet size)
* Otherwise the LED is off.  

When the indicator is off, this indicates either the bootloader isn't running, or is still in 
in a non-default (ie: high) baud rate mode after a failed or cancelled flash operation.  Normally, 
half a second after a failed flash the bootloader will revert to the default baud rate and the heart
beat indicator will re-appear.



## Idle Time Reset

If the bootloader detects an idle period while in non-default baud rate, or when the CPU frequency
has been boosted (typically after a failed boot) it will automatically reset itself to the default
baud rate and original CPU frequency.

This puts the boot loader back in its original state, ready for a new upload attempt.  This ready
state is indicated by the heartbeat signal on the activity LED.

The idle time period can be tweaked with the `--resetBaudTimeout` command line option.



## Delayed Starts

Sometimes you'll need time between uploading a program image and the program starting.  There are
two ways to do this:

1. Use `--goDelay:NNN` where NNN is a number of milliseconds the bootloaded will stall for after
   the upload, but before starting the program
2. Use `--noGo` - this will cause the bootloader to accept the uploaded program but doesn't start it.
   To later manually start the program, use the `--go` option.

The `--goDelay` is particularly handy for giving time to start a serial monitor program so he start 
of the program's debug log isn't missed.



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
Usage: flashy <serialport> [<hexfile>] [options]

<serialport>               Serial port to write to
<hexfile>                  The .hex file to write (optional)
--flashBaud:<N>            Baud rate for flashing (default=1000000)
--userBaud:<N>             Baud rate for monitor and reboot magic (default=115200)
--reboot:<magic>           Sends a magic reboot string at user baud before flashing
--noGo                     Don't send the go command after flashing
--go[:<addr>]              Send the go command, even if not flashing, using address if specified
                           If address not specified, uses start address from .hex file
--goDelay:<ms>             Sets a delay period for the go command
--packetSize:<N>           Size of data chunks transmitted (default=4096)
--packetTimeout:<N>        Time out to receive packet ack in millis (default=300ms)
--pingAttmempts:<T>        How many times to ping for device before giving up (default=20)
--serialLog:<file>         File to write low level log of serial comms
--resetBaudTimeout:<N>     How long device should wait for packet before resetting
                           to the default baud and CPU frequent boost(default=500ms)
--cpuBoost:<yes|no|auto>   whether to boost CPU clock frequency during uploads
                              auto = yes if flash baud rate > 1M
--bootloader[:<dir>]       Save the bootloader kernel images to directory <dir>
                           or the current directory if <dir> not specified.
                           (note: overwrites existing files without prompting)
--status                   Display device status info without flashing
--noVersionCheck           Don't check bootloader version on device
--noKernelCheck            Don't check the hex filename matches expected kernel type for device
--cwd:<dir>                Change current directory
--stress:<N>               Send data packets N times (for load testing)
--monitor                  Monitor serial port
--help                     Show this help
--version                  Show version info
```



## License

Apache 2.0.  See LICENSE file.
