# Flashy v2

All-In-One Reboot, Flash and Monitor Tool for Raspberry Pi bare metal.



## Features

* Binary mode transfers (faster than sending .hex file as text)
* Strong packet encoding with crc32 and all packets acknowledged by host for rebustness
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



## Rationale for v2

Flashy v2 is a complete re-write of Flashy v1 and the original bootloader program.  The 
entire project has been cleaned up, refactored and re-engineered with a focus on robustness.

While Flashy v1 could provide very fast flash times it often suffered from reliability and
recovery issues.  It typically saturated the host device serial port write buffer and just
hoped the device could keep up.

This version moves to a strongly encoded packet format with 32-bit checksums and ack messages 
for all packet transmissions.  It also offers several other improvements and new features
(see above).

For simplicity, this version is incompatible with previous versions of the bootloader and 
requires the included bootloader images be installed on the target device. 



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

eg: To place a copy of the kernel images in the current directory just run:

```
flashy --bootloader:.
```



## Manually Running Flashy

A typical command line to reboot the device, flash an image and start monitoring looks like this:

```
flashy kernel.hex /dev/ttyS3 --flashBaud:1000000 --reboot:yourmagicstring --monitor
```

Run `flashy --help` for more details, or see below.



## Running under WSL-2

WSL2 is a great development environment for bare metal Raspberry Pi projects however its lack
or support for USB serial ports makes it difficult to flash a connected device.

There's a couple of solutions for this:

* Use USBIPD as [described here](https://learn.microsoft.com/en-us/windows/wsl/connect-usb), or
* Install Flashy on both Windows host and the WSL2 operating system and let it find itself.

When flashy is launched in a WSL2 environment with a `/dev/ttyS*` serial port name it will
attempt to relaunch itself as a Windows process where it does have access to serial ports.  
For this to work, Flashy needs to be installed as an npm global tool on the Windows host
machine as well as the WSL guest operating system.

ie: 

* on Windows run: `npm install -g @toptensoftware/flashy`
* and on WSL/Linux run: `sudo npm install -g @toptensoftware/flashy`

Once installed in both environments, Flashy will detect when you're using a serial port, 
find itself in Windows and relaunch itself automatically.



## Command Line Options

```
Usage: node flashy <serialport> [<hexfile>] [options]
All-In-One Reboot, Flash and Monitor Tool

<serialport>            Serial port to write to
<hexfile>               The .hex file to write (optional)
--flashBaud:<N>         Baud rate for flashing (default=115200)
--userBaud:<N>          Baud rate for monitor and reboot magic (default=115200)
--noGo                  Don't send the go command after flashing
--go                    Send the go command, even if not flashing
--goDelay:<ms>          Sets a delay period for the go command
--reboot:<magic>        Sends a magic reboot string at user baud before flashing
--packetSize:<N>        Size of data chunks transmitted (default=4096)
--packetTimeout:<N>     Time out to receive packet ack in millis (default=300ms)
--pingAttmempts:<T>     How many times to ping for device before giving up (default=10)
--serialLog:<file>      File to write low level log of serial comms
--resetBaudTimeout:<N>  How long device should wait for packet before resetting to 
                        the default baud (default=2500ms)
--monitor               Monitor serial port
--help                  Show this help
```



## License

Apache 2.0.  See LICENSE file.
