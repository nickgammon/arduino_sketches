arduino_sketches
================

Publicly-released sketches for the Arduino microprocessor.

Update
------

As from 19th January 2015, the various sketches below "time out" if they don't enter programming mode after 50 attempts.

The code displays a dot for every attempt, so if it fails you will see something like this:

```
Attempting to enter programming mode ......................................................
Failed to enter programming mode. Double-check wiring!
```

(There are 53 dots because the initial message is "Attempting to enter programming mode ...").

Wiring
------

For all sketches except for:

* Atmega\_Hex\_Uploader
* Atmega\_Self\_Read_Signature
* Atmega\_Hex\_Uploader\_Fixed\_Filename

... connect hardware SPI pins together as below:

* MISO to MISO
* MOSI to MOSI
* SCK to SCK
* Vcc to Vcc
* Gnd to Gnd
* Reset on target board to D10 on programming board

These pins are generally exposed on the ICSP header of most boards (except "breadboard" boards).

![ICSP header](http://www.gammon.com.au/images/Arduino/ICSP_header.png)

For "breadboard" boards you will need to read the datasheet for the appropriate chip to find which pins to use.

Atmega\_Board\_Detector
---------------------

See forum post: http://www.gammon.com.au/forum/?id=11633

This uses one Arduino to detect the signature, fuses, and bootloader of another one.

Only some Arduinos are supported to run the sketch. It has been tested on a Uno, Mega2560 and Leonardo.

It sumchecks the bootloader so you can quickly see if a particular one is installed. Some bootloader sumchecks are known and the bootloader "name" reported if found.

Example of use:

```
Atmega chip detector.
Written by Nick Gammon.
Version 1.11
Compiled on Nov  4 2014 at 11:10:33
Entered programming mode OK.
Signature = 1E 95 0F 
Processor = ATmega328P
Flash memory size = 32768 bytes.
LFuse = FF 
HFuse = DE 
EFuse = FD 
Lock byte = CF 
Clock calibration = 8D 
Bootloader in use: Yes
EEPROM preserved through erase: No
Watchdog timer always on: No
Bootloader is 512 bytes starting at 7E00

Bootloader:

7E00: 11 24 84 B7 14 BE 81 FF F0 D0 85 E0 80 93 81 00 
7E10: 82 E0 80 93 C0 00 88 E1 80 93 C1 00 86 E0 80 93 
... 
7FE0: 11 50 E9 F7 F2 DF 1F 91 08 95 80 E0 E8 DF EE 27 
7FF0: FF 27 09 94 FF FF FF FF FF FF FF FF FF FF 04 04 

MD5 sum of bootloader = FB F4 9B 7B 59 73 7F 65 E8 D0 F8 A5 08 12 E7 9F 
Bootloader name: optiboot_atmega328

First 256 bytes of program memory:

0: 0C 94 52 05 0C 94 7A 05 0C 94 7A 05 0C 94 7A 05 
10: 0C 94 7A 05 0C 94 7A 05 0C 94 7A 05 0C 94 7A 05 
...
```

Atmega\_Fuse\_Calculator
----------------------

See forum post: http://www.gammon.com.au/forum/?id=11653

Only some Arduinos are supported to run the sketch. It has been tested on a Uno, Mega2560 and Leonardo.

Similar to the Atmega\_Board\_Detector sketch, this reads a target board's fuses, and displays which fuses are set in a nicer interface, for example:

```
External Reset Disable.................. [ ]
Debug Wire Enable....................... [ ]
Enable Serial (ICSP) Programming........ [X]
Watchdog Timer Always On................ [ ]
Preserve EEPROM through chip erase...... [ ]
Boot into bootloader.................... [X]
Divide clock by 8....................... [ ]
Clock output............................ [ ]
```

Atmega\_Self\_Read_Signature
--------------------------

See forum post: http://www.gammon.com.au/forum/?id=11633&reply=2#reply2

Similar to the Atmega\_Board\_Detector sketch this "self-detects" a signature, so an Arduino can report back its own fuses, and bootloader MD5 sum.

Example of use:

```

Signature detector.
Written by Nick Gammon.
Signature = 1E  95  0F 
Fuses
Low = FF High = D6 Ext = FD Lock = CF 

Processor = ATmega328P
Flash memory size = 32768
Bootloader in use: Yes
EEPROM preserved through erase: Yes
Watchdog timer always on: No
Bootloader is 512 bytes starting at 7E00
```

Note: Depending on the fuse settings, it may not be able to read the bootloader.

The Atmega\_Self\_Read_Signature sketch does not require any additional wiring.


Atmega\_Board\_Programmer
-----------------------

See forum post: http://www.gammon.com.au/forum/?id=11635

This will re-flash the bootloader in selected chips.

Only some Arduinos are supported to run the sketch. It has been tested on a Uno, Mega2560 and Leonardo.

Supported target chips are:

* Atmega8 (1024 bytes)
* Atmega168 Optiboot (512 bytes)
* Atmega328 Optiboot (for Uno etc. at 16 MHz) (512 bytes)
* Atmega328 (8 MHz) for Lilypad etc. (2048 bytes)
* Atmega32U4 for Leonardo (4096 bytes)
* Atmega1280 Optiboot (1024 bytes)
* Atmega1284 Optiboot (1024 bytes)
* Atmega2560 with fixes for watchdog timer problem (8192 bytes)
* Atmega16U2 - the bootloader on the USB interface chip of the Uno

You can use that to install or update bootloaders on the above chips (using another Arduino as the programmer).

The bootloader code is built into the sketch, so it is self-contained (it does not require an SD card, PC or anything like that).

Example of use:

```
Atmega chip programmer.
Written by Nick Gammon.
Version 1.25
Compiled on Nov  4 2014 at 07:33:18
Entered programming mode OK.
Signature = 0x1E 0x95 0x0F 
Processor = ATmega328P
Flash memory size = 32768 bytes.
LFuse = 0xFF 
HFuse = 0xDE 
EFuse = 0xFD 
Lock byte = 0xCF 
Clock calibration = 0x8D 
Bootloader address = 0x7E00
Bootloader length = 512 bytes.
Type 'L' to use Lilypad (8 MHz) loader, or 'U' for Uno (16 MHz) loader ...
```

Atmega\_Hex\_Uploader
-------------------

See forum post: http://www.gammon.com.au/forum/?id=11638

This lets you:

* Verify flash memory
* Read from flash and save to disk
* Read from disk and flash a chip
* Check fuses
* Update fuses
* Erase flash memory

Most operations (except changing fuses) require an external SD card, described in the forum post. You can easily connect one by obtaining a Micro SD "breakout" board for around $US 15.

The SD card uses the hardware SPI pins, and thus the programming of the target chip uses bit-banged SPI, which means that the connections to the board to be programmed differs from the above sketches.

Example of use:

```
Attempting to enter programming mode ...
Entered programming mode OK.
Signature = 0x1E 0x95 0x0F 
Processor = ATmega328P
Flash memory size = 32768 bytes.
LFuse = 0xFF 
HFuse = 0xDE 
EFuse = 0xFD 
Lock byte = 0xCF 
Clock calibration = 0x8D 
Actions:
 [E] erase flash
 [F] modify fuses
 [L] list directory
 [R] read from flash (save to disk)
 [V] verify flash (compare to disk)
 [W] write to flash (read from disk)
Enter action:
Programming mode off.
```

Wiring for the Atmega\_Hex\_Uploader sketch:

```
Arduino    SD Card
--------------------------------
SS         CS (chip select) 
MOSI       DI (data in) 
MISO       DO (data out) 
SCK        CLK (clock) 
+5V        5V 
Gnd        Gnd 

Arduino    Target chip/board
-------------------------------------
D6         MISO
D7         MOSI
D4         SCK
D5         Reset
D9         Clock of target (if required)
+5V        5V 
Gnd        Gnd 
```

This sketch uses "bit banged" SPI for programming the target chip, which is why it uses pins D4, D5, D6, D7 instead of the hardware SPI pins.

Atmega\_Hex\_Uploader\_Fixed\_Filename
-------------------

See forum post: http://www.gammon.com.au/forum/?id=11638&reply=5#reply5

This lets you read from disk and flash a chip, with a "fixed" filename (firmware.hex) and no serial port interface. Instead, three LEDs are used to display status, and flash to show errors.

It requires an external SD card, described in the forum post. You can easily connect one by obtaining a Micro SD "breakout" board for around $US 15.

The SD card uses the hardware SPI pins, and thus the programming of the target chip uses bit-banged SPI, which means that the connections to the board to be programmed differs from the above sketches (apart from Atmega\_Hex\_Uploader, which uses the same wiring).


Wiring for the Atmega\_Hex\_Uploader\_Fixed\_Filename sketch:

```
Arduino    SD Card
--------------------------------
SS         CS (chip select) 
MOSI       DI (data in) 
MISO       DO (data out) 
SCK        CLK (clock) 
+5V        5V 
Gnd        Gnd 

Arduino    Target chip/board
-------------------------------------
D6         MISO
D7         MOSI
D4         SCK
D5         Reset
D9         Clock of target (if required)
+5V        5V 
Gnd        Gnd 

Arduino    Switch and LEDs
--------------------------------
D2         Start-programming switch (normally open, other end to Gnd) 
A0         Error LED (red) 
A1         Ready LED (green)
A2         Working LED (yellow)
```

This sketch uses "bit banged" SPI for programming the target chip, which is why it uses pins D4, D5, D6, D7 instead of the hardware SPI pins.

See the source code (and above forum post) for details about the meanings of the different numbers of LED flashes.


High-voltage serial and parallel programming
---------------------------------------

As from 27th May 2015 the sketches Atmega\_Hex\_Uploader, Atmega\_Board\_Detector and Atmega\_Board\_Programmer also optionally support the high-voltage programming mode of various chips.

This is documented in some detail at http://www.gammon.com.au/forum/?id=12898

For connecting your Arduino Uno to an Atmega328 (or similar) chip this is the wiring:

```
Arduino    Target chip
-------------------------------------
  D2       12 (PD6)
  D3       13 (PD7)
  D4       25 (PC2)
  D5       7 and 20 (VCC and AVCC)
  D6       14 (PB0) (data bit 0)
  D7       15 (PB1) (data bit 1)
  D8       16 (PB2) (data bit 2)
  D9       17 (PB3) (data bit 3)
  D10      18 (PB4) (data bit 4)
  D11      19 (PB5) (data bit 5)
  D12      23 (PC0) (data bit 6)
  D13      24 (PC1) (data bit 7)
  A0        3 (PD1)
  A1        4 (PD2)
  A2        5 (PD3)
  A3        6 (PD4)
  A4        9 (XTAL1)
  A5        11 (PD5)

  /RESET (pin 1) on target connected to 12V via a transistor and MOSFET as shown in the schematic.
  Also connect the grounds. Gnd to pins 8 and 22.
  Decoupling capacitors: 0.1 uF between VCC/AVCC (pins 7 and 20) and Gnd.
  Not connected on target: pins 2, 10, 21, 26, 27, 28.
```

###Reset circuitry###


![Reset circuit](http://www.gammon.com.au/images/Arduino/HV_Programmer3.png)


For connecting your Arduino Uno to an ATtiny85 (or similar) chip this is the wiring:

```
Arduino    Target chip
-------------------------------------
  D3         8 (VCC)
  D4         5 (PB0)
  D5         6 (PB1)
  D6         7 (PB2)
  D7         2 (PB3)

 /RESET (pin 1) on target connected to 12V via a transistor and MOSFET as shown in the schematic.
 Also connect the ground. GND to pin 4.
 Decoupling capacitor: 0.1 µF (100 nF) between VCC (pin 8) and Gnd.
 Not connected on target: pin 3.
```


