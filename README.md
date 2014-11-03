arduino_sketches
================

Publicly-released sketches for the Arduino microprocessor.


Atmega\_Board\_Detector
---------------------

See forum post: http://www.gammon.com.au/forum/?id=11633

This uses one Arduino to detect the signature, fuses, and bootloader of another one.

Only some Arduinos are supported to run the sketch. It has been tested on a Uno, Mega2560 and Leonardo.

It sumchecks the bootloader so you can quickly see if a particular one is installed. Some bootloader sumchecks are known and the bootloader "name" reported if found.


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

