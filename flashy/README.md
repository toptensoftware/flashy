# Flashy v2

All-In-One Reboot, Flash and Monitor Tool for Raspberry Pi bare metal.



## Features

* **New V2** Dynamic baud-rate switching saves updating kernel image on device to switch baud rate
* **New V2** Activity LED indicator shows ready status feedback
* **New V2** Includes packaged pre-compiled bootloader images
* **New V2** Supports sending `.img` or `.hex` image files
* **New V2** Ability to boost device CPU frequency during high baud uploads
* **New V2** Kernel checks ensure hexfile being uploaded matches the device
* **New V2** Strong packet encoding with crc32 and all packets acknowledged by device for rebustness
* **New V2** Stateless device side bootloader for reliable recovery and restart
* **New V2** Automatic idle time baud rate and CPU frequency reset (for recovery)    
* **New V2** Bootloader version checks ensures flashy script and bootloader image are compatible
* **New V2** Configurable packet size and timeout for flexibility with different devices
* **New V2** Host side .hex file parsing and re-chunking for packet size balancing and simpler
  device side implementation
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



## Quick Guide

1. Install it: (requires `sudo` on Linux)

        > npm install -g @toptensoftware/flashy

2. Copy the bootloader kernel images to the Raspberry Pi SD card

        > flashy --bootloader:path_to_your_sd_card

3. Use it! 

        > flashy /dev/ttyS3 kernel.hex



## Installation

Flashy is written in JavaScript and requires NodeJS to be installed. Assuming node and npm are 
installed, install flashy as follows:

```
# Windows
npm install -g @toptensoftware/flashy
```

```
# Linux
sudo npm install -g @toptensoftware/flashy
```

Next you need to copy the bootloader kernel image files to the SD card of your Pi.  The kernel
images are included in the installed package and can be extracted using the `--bootloader` option:

eg: To place a copy of the kernel images in the root of drive D:

```
flashy --bootloader:D:\
```

Note: the `--bootloader` command will overwrite existing files.


## Manual Uploads

Upload an image to the device by specifying the port and image name (either `.hex` or `.img` file):

```
flashy /dev/ttyS3 kernel7.hex 
```


## Activity LED

On devices that have an activity LED, the bootloader provides the following feedback:

* When idle and ready the led flashes in a heartbeat pattern (2 flashes in quick succession 
  every 1 second).
* When receiving data, the activity LED toggles on receipt of every packet (ie: it flashes 
  rapidly, flickers, or appears dim depending on baud rate and packet size)
* Otherwise the LED is off.  

When the indicator is off, this indicates either the bootloader isn't running, or is still in 
in a non-default (ie: high) baud rate mode after a failed or cancelled flash operation.  Normally, 
half a second after a failed flash the bootloader will revert to the default baud rate and the heart
beat indicator will re-appear.



## Flash Baud Rate

Flashy initially connects to the device at 115200 baud.  Once a connection has been established
a command is sent to the device to change to a faster baud rate for the image upload.  

The upload baud rate is controlled by the `--flashBaud:NNN` option and defaults to 1M baud.

Most devices can handle faster rates that this (typically 2M) but the default is 1M to give
some margin for reliability.  Feel free to experiment with this setting to get faster uploads.



## Magic Reboots

Flashy can send a "magic reboot" string.  For this to work the currently running image must be 
monitoring for the supplied string and be configured to reboot when detected.

If you're project uses Circle, use the `CSerialDevice::RegisterMagicReceivedHandler` method to
set this up.

Send reboot strings with the `--reboot:<magic>` command line option:

```
flashy COM3 --reboot:myMagicString
```

By default reboot strings are sent at 115200 baud.  Use the `--userBaud:NNN` option to 
change this.

You can combine reboot strings with sending an image in which case the reboot string will be
sent and the device monitored until it's ready to accept the image upload.


eg: Reboot the device, and then send kernel7.hex
```
flashy COM3 kernel7.hex --reboot:myMagicString
```



## Monitoring

Flashy includes a simple serial port monitor that can show any text output by the device on the
serial port after uploading has completed.  

Monitoring is done at the `--userBaud:NNN` baud rate (by default 115200)

Enable this with the `--monitor` option.



## Sanity Checks

Flashy performs a number of sanity checks to ensure correct behaviour and help prevent silly mistakes:

* The script ensures its own version number matches the bootloader version on the device.
  You can skip this check with the `--noVersionCheck` command line option.  You still get the warning but
  often the upload will still succeed.

* The script checks the image you've selected to upload matches the device and target architecture (aarch32 vs 64)
  you're uploading it to.  For example trying to upload an image named `kernel8.hex` to a Raspberry Pi 4 (which 
  expects `kernel8-rpi4`) will display an error and abort.  You can skip this check with the `--nokernelCheck` option, 
  or by renaming the kernel image file to not contain the string `kernel`.



## Delayed Starts

Sometimes you'll need time between uploading a program image and the program starting.  There are
two ways to do this:

1. Use `--goDelay:NNN` where NNN is a number of milliseconds the bootloaded will stall for after
   the upload, but before starting the program
2. Use `--noGo` - this will cause the bootloader to accept the uploaded program but doesn't start it.
   To later manually start the program, use the `--go` option.

The `--goDelay` is particularly handy for giving time to start a serial monitor program so he start 
of the program's debug log isn't missed.



## CPU Boost

At high upload rates (> 1M baud) the device's CPU frequency is boosted to the maximum rate
for the period of the upload.  This helps the device keep up with the data rate and often
prevents a failed upload.

CPU boost can be overridden with the `--cpuBoost` command line option:

* `--cpuBoost:no` - disable CPU boost even for > 1M baud uploads
* `--cpuBoost:yes` - boost CPU even for < 1M baud uploads
* `--cpuBoost:auto` - the default boost of > 1M baud upload.



## Packet Size

All communications between Flashy and the bootloader is done using binary data packets.  Most
of these packets are quite small except with uploading image data in which case they're 4K (by 
default).

This default was chosen as it gives a good balance between performance, reliability and overhead.

If your having reliability issues you can try reducing either the `--flashBaud:NNN` setting or you
can try using a smaller packet size with the `--packetSize:NNN` option.



## Stress Testing

Flashy includes a `--stress:N` option that simply causes each image upload packet to be sent `N` times.  

This can be handy for checking through put rates, reliability, recoverability etc...



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
Usage: flashy <serialport> [<imagefile>] [options]

<serialport>               Serial port to write to
<imagefile>                The .hex or .img file to write (optional)
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
--noKernelCheck            Don't check the image filename matches expected kernel type for device
--cwd:<dir>                Change current directory
--stress:<N>               Send data packets N times (for load testing)
--monitor                  Monitor serial port
--help                     Show this help
--version                  Show version info
```

## License

Apache 2.0.  See LICENSE file.
