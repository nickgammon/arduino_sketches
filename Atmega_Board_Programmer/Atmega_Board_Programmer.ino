// Atmega chip programmer
// Author: Nick Gammon
// Date: 22nd May 2012
// Version: 1.37

// IMPORTANT: If you get a compile or verification error, due to the sketch size,
// make some of these false to reduce compile size (the ones you don't want).
// The Atmega328 is always included (Both Uno and Lilypad versions).

#define USE_ATMEGA8 true
#define USE_ATMEGA16U2 true    // Uno USB interface chip
#define USE_ATMEGA32U4 true    // Leonardo
#define USE_ATMEGA168 true
#define USE_ATMEGA1280 true
#define USE_ATMEGA1284 true
#define USE_ATMEGA2560 true
#define USE_ATMEGA256RFR2 false // Pinoccio Scout

/* ----------------------------------------------------------------------------
WARNING: The Arduino Leonardo, Arduino Esplora and the Arduino Micro all use the same chip (ATmega32U4).
They will all have the Leonardo bootloader burnt onto them. This means that if you have a 
Micro or Esplora it will be identified as a Leonardo in the Tools -> Serial Port menu.
This is because the PID (Product ID) in the USB firmware will be 0x0036 (Leonardo).
This only applies during the uploading process. You should still select the correct board in the 
Tools -> Boards menu.
------------------------------------------------------------------------------ */

// For more information including wiring, see: http://www.gammon.com.au/forum/?id=11635

// Version 1.1: Reset foundSig to -1 each time around the loop.
// Version 1.2: Put hex bootloader data into separate files
// Version 1.3: Added verify, and MD5 sums
// Version 1.4: Added signatures for ATmeag8U2/16U2/32U2 (7 May 2012)
// Version 1.5: Added signature for ATmega1284P (8 May 2012)
// Version 1.6: Allow sketches to read bootloader area (lockbyte: 0x2F)
// Version 1.7: Added choice of bootloaders for the Atmega328P (8 MHz or 16 MHz)
// Version 1.8: Output an 8 MHz clock on pin 9
// Version 1.9: Added support for Atmega1284P, and fixed some bugs
// Version 1.10: Corrected flash size for Atmega1284P.
// Version 1.11: Added support for Atmega1280. Removed MD5SUM stuff to make room.
// Version 1.12: Added signatures for ATtiny2313A, ATtiny4313, ATtiny13
// Version 1.13: Added signature for Atmega8A
// Version 1.14: Added bootloader for Atmega8
// Version 1.15: Removed extraneous 0xFF from some files
// Version 1.16: Added signature for Atmega328
// Version 1.17: Allowed for running on the Leonardo, Micro, etc.
// Version 1.18: Added timed writing for Atmega8
// Version 1.19: Changed Atmega1280 to use the Optiboot loader.
// Version 1.20: Changed bootloader for Atmega2560 to fix problems with watchdog timer.
// Version 1.21: Automatically clear "divide by 8" fuse bit
// Version 1.22: Fixed compiling problems under IDE 1.5.8
// Version 1.23: Added support for Leonardo bootloader
// Version 1.24: Added bootloader for Uno Atmega16U2 chip (the USB interface)
// Version 1.25: Fixed bug re verifying uploaded sketch for the Lilypad
// Version 1.26: Turn off programming mode when done (so chip can run)
// Version 1.27: Made bootloaders conditional, so you can omit some to save space
// Version 1.28: Changed _BV () macro to bit () macro.
// Version 1.29: Display message if cannot enter programming mode.
// Version 1.30: Various tidy-ups 
// Version 1.31: Fixed bug in doing second lot of programming under IDE 1.6.0
// Version 1.32: Bug fixes, added support for At90USB82, At90USB162 signatures
// Version 1.33: Added support for ATMEGA256RFR2 (Pinoccio Scout)
// Version 1.34: Added support for high-voltage programming mode for Atmega328 / ATtiny25 family
// Version 1.35: Updated bootloader for Leonardo/Micro to Leonardo-prod-firmware-2012-12-10.hex
// Version 1.36: Got rid of compiler warnings in IDE 1.6.7
// Version 1.37: Got rid of compiler warnings in IDE 1.6.9, added more information about where bootloaders came from

#define VERSION "1.37"

// make true to use the high-voltage parallel wiring
#define HIGH_VOLTAGE_PARALLEL false
// make true to use the high-voltage serial wiring
#define HIGH_VOLTAGE_SERIAL false
// make true to use ICSP programming
#define ICSP_PROGRAMMING true

#if HIGH_VOLTAGE_PARALLEL && HIGH_VOLTAGE_SERIAL
  #error Cannot use both high-voltage parallel and serial at the same time
#endif 

#if (HIGH_VOLTAGE_PARALLEL || HIGH_VOLTAGE_SERIAL) && ICSP_PROGRAMMING
  #error Cannot use ICSP and high-voltage programming at the same time
#endif

#if !(HIGH_VOLTAGE_PARALLEL || HIGH_VOLTAGE_SERIAL || ICSP_PROGRAMMING)
  #error Choose a programming mode: HIGH_VOLTAGE_PARALLEL, HIGH_VOLTAGE_SERIAL or ICSP_PROGRAMMING 
#endif

const int ENTER_PROGRAMMING_ATTEMPTS = 50;

/*

 Copyright 2012 Nick Gammon.


 PERMISSION TO DISTRIBUTE

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
 subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.


 LIMITATION OF LIABILITY

 The software is provided "as is", without warranty of any kind, express or implied,
 including but not limited to the warranties of merchantability, fitness for a particular
 purpose and noninfringement. In no event shall the authors or copyright holders be liable
 for any claim, damages or other liability, whether in an action of contract,
 tort or otherwise, arising from, out of or in connection with the software
 or the use or other dealings in the software.

*/

#include <SPI.h>
#include <avr/pgmspace.h>


const unsigned long BAUD_RATE = 115200;

const byte CLOCKOUT = 9;

#if ICSP_PROGRAMMING

  #ifdef ARDUINO_PINOCCIO
    const byte RESET = SS;  // --> goes to reset on the target board
  #else
    const byte RESET = 10;  // --> goes to reset on the target board
  #endif
  
  #if ARDUINO < 100
    const byte SCK = 13;    // SPI clock
  #endif

#endif // ICSP_PROGRAMMING


#include "HV_Pins.h"
#include "Signatures.h"
#include "General_Stuff.h"


// structure to hold signature and other relevant data about each bootloader
typedef struct {
   byte sig [3];                // chip signature
   unsigned long loaderStart;   // start address of bootloader (bytes)
   const byte * bootloader;     // address of bootloader hex data
   unsigned int loaderLength;   // length of bootloader hex data (bytes)
   byte lowFuse, highFuse, extFuse, lockByte;  // what to set the fuses, lock bits to.
} bootloaderType;


// hex bootloader data

// For simplicity later one, we always include these two
#include "bootloader_lilypad328.h"
#include "bootloader_atmega328.h"

#if USE_ATMEGA168
  #include "bootloader_atmega168.h"
#endif
#if USE_ATMEGA2560
  #include "bootloader_atmega2560_v2.h"
#endif
#if USE_ATMEGA256RFR2
  #include "bootloader_atmega256rfr2_v1.0a.h"
#endif
#if USE_ATMEGA1284
  #include "bootloader_atmega1284.h"
#endif
#if USE_ATMEGA1280
  #include "bootloader_atmega1280.h"
#endif
#if USE_ATMEGA8
  #include "bootloader_atmega8.h"
#endif
#if USE_ATMEGA32U4
  #include "bootloader_atmega32u4.h"
#endif
#if USE_ATMEGA16U2
  #include "bootloader_atmega16u2.h"  // Uno USB interface chip
#endif

// see Atmega328 datasheet page 298
const bootloaderType bootloaders [] PROGMEM =
  {
// Only known bootloaders are in this array.
// If we support it at all, it will have a start address.
// If not compiled into this particular version the bootloader address will be zero.

  // ATmega168PA
  { { 0x1E, 0x94, 0x0B }, 
        0x3E00,               // start address
  #if USE_ATMEGA168
        atmega168_optiboot,   // loader image
        sizeof atmega168_optiboot,
  #else
        0, 0,
  #endif
        0xC6,         // fuse low byte: external full-swing crystal
        0xDD,         // fuse high byte: SPI enable, brown-out detection at 2.7V
        0x04,         // fuse extended byte: boot into bootloader, 512 byte bootloader
        0x2F },       // lock bits: SPM is not allowed to write to the Boot Loader section.

  // ATmega328P
  { { 0x1E, 0x95, 0x0F }, 
        0x7E00,               // start address
        atmega328_optiboot,   // loader image
        sizeof atmega328_optiboot,
        0xFF,         // fuse low byte: external clock, max start-up time
        0xDE,         // fuse high byte: SPI enable, boot into bootloader, 512 byte bootloader
        0x05,         // fuse extended byte: brown-out detection at 2.7V
        0x2F },       // lock bits: SPM is not allowed to write to the Boot Loader section.

  // ATmega328
  { { 0x1E, 0x95, 0x14 }, 
        0x7E00,               // start address
        atmega328_optiboot,   // loader image
        sizeof atmega328_optiboot,
        0xFF,         // fuse low byte: external clock, max start-up time
        0xDE,         // fuse high byte: SPI enable, boot into bootloader, 512 byte bootloader
        0x05,         // fuse extended byte: brown-out detection at 2.7V
        0x2F },       // lock bits: SPM is not allowed to write to the Boot Loader section.

  // ATmega1280
  { { 0x1E, 0x97, 0x03 }, 
        0x1FC00,      // start address
  #if USE_ATMEGA1280
        optiboot_atmega1280_hex,
        sizeof optiboot_atmega1280_hex,
  #else
        0, 0,
  #endif
        0xFF,         // fuse low byte: external clock, max start-up time
        0xDE,         // fuse high byte: SPI enable, boot into bootloader, 1280 byte bootloader
        0xF5,         // fuse extended byte: brown-out detection at 2.7V
        0x2F },       // lock bits: SPM is not allowed to write to the Boot Loader section.

  // ATmega2560
  { { 0x1E, 0x98, 0x01 }, 
        0x3E000,      // start address
  #if USE_ATMEGA2560
        atmega2560_bootloader_hex,// loader image
        sizeof atmega2560_bootloader_hex,
  #else
        0, 0,
  #endif
        0xFF,         // fuse low byte: external clock, max start-up time
        0xD8,         // fuse high byte: SPI enable, boot into bootloader, 8192 byte bootloader
        0xFD,         // fuse extended byte: brown-out detection at 2.7V
        0x2F },       // lock bits: SPM is not allowed to write to the Boot Loader section.

  // ATmega256rfr2
  { { 0x1E, 0xA8, 0x02 },
        0x3E000,      // start address
  #if USE_ATMEGA256RFR2
        atmega256rfr2_bootloader_hex,// loader image
        sizeof atmega256rfr2_bootloader_hex,
  #else
        0, 0,
  #endif
        0xDE,         // fuse low byte: internal transceiver clock, max start-up time
        0xD0,         // fuse high byte: SPI enable, EE save, boot into bootloader, 8192 byte bootloader
        0xFE,         // fuse extended byte: brown-out detection at 1.8V
        0x2F },       // lock bits: SPM is not allowed to write to the Boot Loader section.

  // ATmega16U2
  { { 0x1E, 0x94, 0x89 }, 
        0x3000,      // start address
  #if USE_ATMEGA16U2
        Arduino_COMBINED_dfu_usbserial_atmega16u2_Uno_Rev3_hex,// loader image
        sizeof Arduino_COMBINED_dfu_usbserial_atmega16u2_Uno_Rev3_hex,
  #else
        0, 0,
  #endif
        0xEF,         // fuse low byte: external clock, m
        0xD9,         // fuse high byte: SPI enable, NOT boot into bootloader, 4096 byte bootloader
        0xF4,         // fuse extended byte: brown-out detection at 2.6V
        0xCF },       // lock bits

  // ATmega32U4
  { { 0x1E, 0x95, 0x87 }, 
        0x7000,      // start address
  #if USE_ATMEGA32U4
        leonardo_hex,// loader image
        sizeof leonardo_hex,
  #else
        0, 0,
  #endif
        0xFF,         // fuse low byte: external clock, max start-up time
        0xD8,         // fuse high byte: SPI enable, boot into bootloader, 1280 byte bootloader
        0xCB,         // fuse extended byte: brown-out detection at 2.6V
        0x2F },       // lock bits: SPM is not allowed to write to the Boot Loader section.

  // ATmega1284P family
  
  // ATmega1284P
  { { 0x1E, 0x97, 0x05 }, 
        0x1FC00,      // start address
  #if USE_ATMEGA1284
        optiboot_atmega1284p_hex,
        sizeof optiboot_atmega1284p_hex,
  #else
        0, 0,
  #endif
        0xFF,         // fuse low byte: external clock, max start-up time
        0xDE,         // fuse high byte: SPI enable, boot into bootloader, 1024 byte bootloader
        0xFD,         // fuse extended byte: brown-out detection at 2.7V
        0x2F },       // lock bits: SPM is not allowed to write to the Boot Loader section.

  // Atmega8A family
  
  // ATmega8A
  { { 0x1E, 0x93, 0x07 }, 
        0x1C00,      // start address
  #if USE_ATMEGA8
        atmega8_hex,
        sizeof atmega8_hex,
  #else
        0, 0,
  #endif
        0xE4,         // fuse low byte: external clock, max start-up time
        0xCA,         // fuse high byte: SPI enable, boot into bootloader, 1024 byte bootloader
        0xFD,         // fuse extended byte: brown-out detection at 2.7V
        0x0F  },      // lock bits: SPM is not allowed to write to the Boot Loader section.

  };  // end of bootloaders



void getFuseBytes ()
  {
  Serial.print (F("LFuse = "));
  showHex (readFuse (lowFuse), true);
  Serial.print (F("HFuse = "));
  showHex (readFuse (highFuse), true);
  Serial.print (F("EFuse = "));
  showHex (readFuse (extFuse), true);
  Serial.print (F("Lock byte = "));
  showHex (readFuse (lockByte), true);
  Serial.print (F("Clock calibration = "));
  showHex (readFuse (calibrationByte), true);
  }  // end of getFuseBytes

bootloaderType currentBootloader;


// burn the bootloader to the target device
void writeBootloader ()
  {
  bool foundBootloader = false;
  
  for (unsigned int j = 0; j < NUMITEMS (bootloaders); j++)
    {
      
    memcpy_P (&currentBootloader, &bootloaders [j], sizeof currentBootloader);
    
    if (memcmp (currentSignature.sig, currentBootloader.sig, sizeof currentSignature.sig) == 0)
      {
      foundBootloader = true;
      break;
      }  // end of signature found
    }  // end of for each signature
    
  if (!foundBootloader)
    {
    Serial.println (F("No bootloader support for this device."));
    return;
    }
    
  // if in the table, but with zero length, we need to enable a #define to use it.
  if (currentBootloader.loaderLength == 0)
    {
    Serial.println (F("Bootloader for this device is disabled, edit " __FILE__ " to enable it."));
    return;
    }

  unsigned int i;

  byte lFuse = readFuse (lowFuse);

  byte newlFuse = currentBootloader.lowFuse;
  byte newhFuse = currentBootloader.highFuse;
  byte newextFuse = currentBootloader.extFuse;
  byte newlockByte = currentBootloader.lockByte;


  unsigned long addr = currentBootloader.loaderStart;
  unsigned int  len = currentBootloader.loaderLength;
  unsigned long pagesize = currentSignature.pageSize;
  unsigned long pagemask = ~(pagesize - 1);
  const byte * bootloader = currentBootloader.bootloader;


  byte subcommand = 'U';

  // Atmega328P or Atmega328
  if (currentBootloader.sig [0] == 0x1E &&
      currentBootloader.sig [1] == 0x95 &&
      (currentBootloader.sig [2] == 0x0F || currentBootloader.sig [2] == 0x14)
      )
    {
    Serial.println (F("Type 'L' to use Lilypad (8 MHz) loader, or 'U' for Uno (16 MHz) loader ..."));
    do
      {
      subcommand = toupper (Serial.read ());
      } while (subcommand != 'L' && subcommand != 'U');

    if (subcommand == 'L')  // use internal 8 MHz clock
      {
      Serial.println (F("Using Lilypad 8 MHz loader."));
      bootloader = ATmegaBOOT_168_atmega328_pro_8MHz_hex;
      newlFuse = 0xE2;  // internal 8 MHz oscillator
      newhFuse = 0xDA;  //  2048 byte bootloader, SPI enabled
      addr = 0x7800;
      len = sizeof ATmegaBOOT_168_atmega328_pro_8MHz_hex;
      }  // end of using the 8 MHz clock
    else
      Serial.println (F("Using Uno Optiboot 16 MHz loader."));
     }  // end of being Atmega328P


  Serial.print (F("Bootloader address = 0x"));
  Serial.println (addr, HEX);
  Serial.print (F("Bootloader length = "));
  Serial.print (len);
  Serial.println (F(" bytes."));


  unsigned long oldPage = addr & pagemask;

  Serial.println (F("Type 'Q' to quit, 'V' to verify, or 'G' to program the chip with the bootloader ..."));
  char command;
  do
    {
    command = toupper (Serial.read ());
    } while (command != 'G' && command != 'V' && command != 'Q');

  // let them do nothing
  if (command == 'Q')
    return;

  if (command == 'G')
    {

    // Automatically fix up fuse to run faster, then write to device
    if (lFuse != newlFuse)
      {
      if ((lFuse & 0x80) == 0)
        Serial.println (F("Clearing 'Divide clock by 8' fuse bit."));

      Serial.println (F("Fixing low fuse setting ..."));
      writeFuse (newlFuse, lowFuse);
      delay (1000);
      stopProgramming ();  // latch fuse
      if (!startProgramming ())
        return;
      delay (1000);
      }

    Serial.println (F("Erasing chip ..."));
    eraseMemory ();
    Serial.println (F("Writing bootloader ..."));
    for (i = 0; i < len; i += 2)
      {
      unsigned long thisPage = (addr + i) & pagemask;
      // page changed? commit old one
      if (thisPage != oldPage)
        {
        commitPage (oldPage, true);
        oldPage = thisPage;
        }
      writeFlash (addr + i, pgm_read_byte(bootloader + i));
      writeFlash (addr + i + 1, pgm_read_byte(bootloader + i + 1));
      }  // end while doing each word

    // commit final page
    commitPage (oldPage, true);
    Serial.println (F("Written."));
    }  // end if programming

  Serial.println (F("Verifying ..."));

  // count errors
  unsigned int errors = 0;
  // check each byte
  for (i = 0; i < len; i++)
    {
    byte found = readFlash (addr + i);
    byte expected = pgm_read_byte(bootloader + i);
    if (found != expected)
      {
      if (errors <= 100)
        {
        Serial.print (F("Verification error at address "));
        Serial.print (addr + i, HEX);
        Serial.print (F(". Got: "));
        showHex (found);
        Serial.print (F(" Expected: "));
        showHex (expected, true);
        }  // end of haven't shown 100 errors yet
      errors++;
      }  // end if error
    }  // end of for

  if (errors == 0)
    Serial.println (F("No errors found."));
  else
    {
    Serial.print (errors, DEC);
    Serial.println (F(" verification error(s)."));
    if (errors > 100)
      Serial.println (F("First 100 shown."));
    return;  // don't change fuses if errors
    }  // end if

  if (command == 'G')
    {
    Serial.println (F("Writing fuses ..."));

    writeFuse (newlFuse, lowFuse);
    writeFuse (newhFuse, highFuse);
    writeFuse (newextFuse, extFuse);
    writeFuse (newlockByte, lockByte);

    // confirm them
    getFuseBytes ();
    }  // end if programming

  Serial.println (F("Done."));

  } // end of writeBootloader

void getSignature ()
  {
  foundSig = -1;

  byte sig [3];
  Serial.print (F("Signature = "));
  readSignature (sig);
  for (byte i = 0; i < 3; i++)
    showHex (sig [i]);
  
  Serial.println ();

  for (unsigned int j = 0; j < NUMITEMS (signatures); j++)
    {
      
    memcpy_P (&currentSignature, &signatures [j], sizeof currentSignature);
    
    if (memcmp (sig, currentSignature.sig, sizeof sig) == 0)
      {
      foundSig = j;
      Serial.print (F("Processor = "));
      Serial.println (currentSignature.desc);
      Serial.print (F("Flash memory size = "));
      Serial.print (currentSignature.flashSize, DEC);
      Serial.println (F(" bytes."));
      if (currentSignature.timedWrites)
        Serial.println (F("Writes are timed, not polled."));
      return;
      }  // end of signature found
    }  // end of for each signature

  Serial.println (F("Unrecogized signature."));
  }  // end of getSignature

void setup ()
  {
  Serial.begin (BAUD_RATE);
  while (!Serial) ;  // for Leonardo, Micro etc.
  Serial.println ();
  Serial.println (F("Atmega chip programmer."));
  Serial.println (F("Written by Nick Gammon."));
  Serial.println (F("Version " VERSION));
  Serial.println (F("Compiled on " __DATE__ " at " __TIME__ " with Arduino IDE " xstr(ARDUINO) "."));

  initPins ();

 }  // end of setup

void loop ()
  {

  if (startProgramming ())
    {
    getSignature ();
    getFuseBytes ();
  
    // if we found a signature try to write a bootloader
    if (foundSig != -1)
      writeBootloader ();
    stopProgramming ();
    }   // end of if entered programming mode OK
    

  Serial.println (F("Type 'C' when ready to continue with another chip ..."));
  while (toupper (Serial.read ()) != 'C')
    {}

  }  // end of loop

