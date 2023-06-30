# Load Testing Logs

## Pi 4 Model B (AARCH64)

```
C:\Users\Brad\Projects\flashy\flashy>node flashy com3 ../blink/kernel8-rpi4.hex --reboot:ccw_tada --stress:20 --flashBau
d:2000000
Opening com3 at 115,200 baud... ok
Sending reboot magic 'ccw_tada'... ok
Waiting for device..................... ok
Found device:
    - Raspberry Pi 4 Model B
    - Serial: 10000000-314ec179
    - CPU Clock: 600MHz (range: 600-1500MHz)
    - Bootloader: rpi4-aarch64 v2.0.13, max packet size: 4096
Sending request for 2,000,000 baud and 1,500MHz CPU... ok
Closing serial port... ok
Opening com3 at 2,000,000 baud... ok
Waiting for device... ok
Sending '../blink/kernel8-rpi4.hex':
....................................................................................................................................................................................................................................................................................................................................................................................................................................
Transfered 1416980 bytes in 12.2 seconds.
Sending go command 0x524288 with delay 0ms... ok
Closing serial port... ok
```


## Pi 4 Model B (AARCH32)


```
C:\Users\Brad\Projects\flashy\flashy>node flashy com3 ../blink/kernel7l.hex --reboot:ccw_tada --stress:20 --flashBaud:2000000
Opening com3 at 115,200 baud... ok
Sending reboot magic 'ccw_tada'... ok
Waiting for device... ok
Found device:
    - Raspberry Pi 4 Model B
    - Serial: 10000000-314ec179
    - CPU Clock: 600MHz (range: 600-1500MHz)
    - Bootloader: rpi4-aarch32 v2.0.13, max packet size: 4096
Sending request for 2,000,000 baud and 1,500MHz CPU... ok
Closing serial port... ok
Opening com3 at 2,000,000 baud... ok
Waiting for device... ok
Sending '../blink/kernel7l.hex':
............................................................................................................................................................................................................................................................................................................................................................................................
Transfered 1367220 bytes in 11.4 seconds.
Sending go command 0x32768 with delay 0ms... ok
Closing serial port... ok
```



## Pi 3 Model B+ (AARCH32/kernel8-32)

```
C:\Users\Brad\Projects\flashy\flashy>node flashy com3 ../blink/kernel8-32.hex --reboot:ccw_tada --stress:20 --flashBaud:2000000
Opening com3 at 115,200 baud... ok
Sending reboot magic 'ccw_tada'... ok
Waiting for device... ok
Found device:
    - Raspberry Pi 3 Model B+
    - Serial: 00000000-bc1b1209
    - CPU Clock: 600MHz (range: 600-1400MHz)
    - Bootloader: rpi2-aarch32 v2.0.13, max packet size: 4096
Sending request for 2,000,000 baud and 1,400MHz CPU... ok
Closing serial port... ok
Opening com3 at 2,000,000 baud... ok
Waiting for device... ok
Sending '../blink/kernel8-32.hex':
....................................................................................................................................................................................................................................................................................................................................................
Transfered 1188020 bytes in 10.0 seconds.
Sending go command 0x32768 with delay 0ms... ok
Closing serial port... ok
```


## Pi 3 Model B+ (AARCH32/kernel7)

```
C:\Users\Brad\Projects\flashy\flashy>node flashy com3 ../blink/kernel7.hex --reboot:ccw_tada --stress:20 --flashBaud:2000000
Opening com3 at 115,200 baud... ok
Sending reboot magic 'ccw_tada'... ok
Waiting for device......... ok
Found device:
    - Raspberry Pi 3 Model B+
    - Serial: 00000000-bc1b1209
    - CPU Clock: 600MHz (range: 600-1400MHz)
    - Bootloader: rpi2-aarch32 v2.0.13, max packet size: 4096
Sending request for 2,000,000 baud and 1,400MHz CPU... ok
Closing serial port... ok
Opening com3 at 2,000,000 baud... ok
Waiting for device... ok
Sending '../blink/kernel7.hex':
....................................................................................................................................................................................................................................................................................................................................................
Transfered 1187300 bytes in 10.3 seconds.
Sending go command 0x32768 with delay 0ms... ok
Closing serial port... ok
```

## Pi 3 Model B+ (AARCH64)

```
C:\Users\Brad\Projects\flashy\flashy>node flashy com3 ../blink/kernel8.hex --reboot:ccw_tada --stress:20 --flashBaud:2000000
Opening com3 at 115,200 baud... ok
Sending reboot magic 'ccw_tada'... ok
Waiting for device... ok
Found device:
    - Raspberry Pi 3 Model B+
    - Serial: 00000000-bc1b1209
    - CPU Clock: 600MHz (range: 600-1400MHz)
    - Bootloader: rpi3-aarch64 v2.0.13, max packet size: 4096
Sending request for 2,000,000 baud and 1,400MHz CPU... ok
Closing serial port... ok
Opening com3 at 2,000,000 baud... ok
Waiting for device... ok
Sending '../blink/kernel8.hex':
................................................................................................................................................................................................................................................................................................................................................................................................................
Transfered 1324180 bytes in 11.3 seconds.
Sending go command 0x524288 with delay 0ms... ok
Closing serial port... ok
```


## Pi 2 Model B

(NB: flash baud reduced to 1.5M)

```
C:\Users\Brad\Projects\flashy\flashy>node flashy com3 ../blink/kernel7.hex --reboot:ccw_tada --stress:20 --flashBaud:1500000
Opening com3 at 115,200 baud... ok
Sending reboot magic 'ccw_tada'... ok
Waiting for device...
Device packet error: 1
 ok
Found device:
    - Raspberry Pi 2 Model B
    - Serial: 00000000-0a7df9f5
    - CPU Clock: 600MHz (range: 600-900MHz)
    - Bootloader: rpi2-aarch32 v2.0.13, max packet size: 4096
Sending request for 1,500,000 baud and 900MHz CPU... ok
Closing serial port... ok
Opening com3 at 1,500,000 baud... ok
Waiting for device... ok
Sending '../blink/kernel7.hex':
....................................................................................................................................................................................................................................................................................................................................................
Transfered 1187300 bytes in 10.8 seconds.
Sending go command 0x32768 with delay 0ms... ok
Closing serial port... ok
```


## Pi 1 Model B R2

```
C:\Users\Brad\Projects\flashy\flashy>node flashy com3 ../blink/kernel.hex --reboot:ccw_tada --stress:20 --flashBaud:2000000
Opening com3 at 115,200 baud... ok
Sending reboot magic 'ccw_tada'... ok
Waiting for device... ok
Found device:
    - Raspberry Pi Model B R2
    - Serial: 00000000-4a5037d0
    - CPU Clock: 700MHz (range: 700-700MHz)
    - Bootloader: rpi1-aarch32 v2.0.13, max packet size: 4096
Sending request for 2,000,000 baud and 700MHz CPU... ok
Closing serial port... ok
Opening com3 at 2,000,000 baud... ok
Waiting for device... ok
Sending '../blink/kernel.hex':
....................................................................................................................................................................................................................................................................................................................................................
Transfered 1192660 bytes in 10.0 seconds.
Sending go command 0x32768 with delay 0ms... ok
Closing serial port... ok
```


## Pi Zero W

```
C:\Users\Brad\Projects\flashy\flashy>node flashy com3 ../blink/kernel.hex --reboot:ccw_tada --stress:20 --flashBaud:2000000
Opening com3 at 115,200 baud... ok
Sending reboot magic 'ccw_tada'... ok
Waiting for device... ok
Found device:
    - Raspberry Pi Zero W
    - Serial: 00000000-ed3486b0
    - CPU Clock: 700MHz (range: 700-1000MHz)
    - Bootloader: rpi1-aarch32 v2.0.13, max packet size: 4096
Sending request for 2,000,000 baud and 1,000MHz CPU... ok
Closing serial port... ok
Opening com3 at 2,000,000 baud... ok
Waiting for device... ok
Sending '../blink/kernel.hex':
....................................................................................................................................................................................................................................................................................................................................................
Transfered 1192660 bytes in 9.9 seconds.
Sending go command 0x32768 with delay 0ms... ok
Closing serial port... ok
``















