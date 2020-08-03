// Atmega hex file uploader (from SD card)
// Author: Nick Gammon
// Date: 22nd May 2012
// Version: 1.37     // NB update 'Version' variable below!

// Version 1.1: Some code cleanups as suggested on the Arduino forum.
// Version 1.2: Cleared temporary flash area to 0xFF before doing each page
// Version 1.3: Added ability to read from flash and write to disk, also to erase flash
// Version 1.4: Slowed down bit-bang SPI to make it more reliable on slower processors
// Version 1.5: Fixed bug where file "YES" might be saved instead of the correct name
//              Also corrected flash size for Atmega1284P.
// Version 1.6: Echo user input
// Version 1.7: Moved signatures into PROGMEM. Added ability to change fuses/lock byte.
// Version 1.8: Made dates in file list line up. Omit date/time if default (unknown) date used.
//              Added "L" command (list directory)
// Version 1.9: Ensure in programming mode before access flash (eg. if reset removed to test)
//              Added reading of clock calibration byte (note: this cannot be changed)
// Version 1.10: Added signatures for ATtiny2313A, ATtiny4313, ATtiny13
// Version 1.11: Added signature for Atmega8
// Version 1.11: Added signature for Atmega32U4
// Version 1.12: Added option to allow target to run when not being programmed
// Version 1.13: Changed so you can set fuses without an SD card active.
// Version 1.14: Changed SPI writing to have pause before and after setting SCK low
// Version 1.15: Remembers last file name uploaded in EEPROM
// Version 1.16: Allowed for running on the Leonardo, Micro, etc.
// Version 1.17: Added timed writing for Atmega8
// Version 1.18: Added support for running on an Atmega2560
// Version 1.19: Added safety checks for high fuse, so you can't disable SPIEN or enable RSTDISBL etc.
// Version 1.20: Added support to ignore extra Intel Hex record types (4 and 5)
// Version 1.21: Fixed bug in pollUntilReady function
// Version 1.22: Cleaned up _BV() macro to use bit() macro instead for readability
// Version 1.23: Fixed bug regarding checking if you set the SPIEN bit (wrong value used)
// Version 1.24: Display message if cannot enter programming mode.
// Version 1.25: Bug fixes
// Version 1.26: Show Arduino IDE version
// Version 1.27: Added support for Arduino Ethernet shield (different Slave Select pin for SD card)
//                 (Make USE_ETHERNET_SHIELD true below to use this feature)
// Version 1.28: Added support for At90USB82, At90USB162
// Version 1.29: Added support for Atmega1284P as the programming chip
// Version 1.30: Added support for ATmega64rfr2/ATmega128rfr2/ATmega256rfr2 chips
// Version 1.31: Fixed bug regarding the way that USE_ETHERNET_SHIELD was being handled
// Version 1.32: Added preliminary support for high-voltage programming mode for Atmega328 family
// Version 1.33: Major tidy-ups, made code more modular
// Version 1.34: Added include for SPI.h, and various tidy-ups to correct some issues
// Version 1.35: Got rid of compiler warnings in IDE 1.6.7
// Version 1.36: Got rid of warning from cppcheck regarding scope of allFF variable
// Version 1.37: Fixed bug re verifying combined sketch/bootloader on Atmega2560


const bool allowTargetToRun = true;  // if true, programming lines are freed when not programming

#define ALLOW_MODIFY_FUSES true   // make false if this sketch doesn't fit into memory
#define ALLOW_FILE_SAVING true    // make false if this sketch doesn't fit into memory
#define SAFETY_CHECKS true        // check for disabling SPIEN, or enabling RSTDISBL

#define USE_ETHERNET_SHIELD false  // Use the Arduino Ethernet Shield for the SD card

// make true if you have spare pins for the SD card interface
#define SD_CARD_ACTIVE true

// make true to use the high-voltage parallel wiring
#define HIGH_VOLTAGE_PARALLEL false
// make true to use the high-voltage serial wiring
#define HIGH_VOLTAGE_SERIAL false
// make true to use ICSP programming
#define ICSP_PROGRAMMING true

// make true to use bit-banged SPI for programming
#define USE_BIT_BANGED_SPI true

#if HIGH_VOLTAGE_PARALLEL && HIGH_VOLTAGE_SERIAL
  #error Cannot use both high-voltage parallel and serial at the same time
#endif

#if (HIGH_VOLTAGE_PARALLEL || HIGH_VOLTAGE_SERIAL) && ICSP_PROGRAMMING
  #error Cannot use ICSP and high-voltage programming at the same time
#endif

#if !(HIGH_VOLTAGE_PARALLEL || HIGH_VOLTAGE_SERIAL || ICSP_PROGRAMMING)
  #error Choose a programming mode: HIGH_VOLTAGE_PARALLEL, HIGH_VOLTAGE_SERIAL or ICSP_PROGRAMMING
#endif

#if !USE_BIT_BANGED_SPI && SD_CARD_ACTIVE
  #error If you are using an SD card you need to use bit-banged SPI for the programmer
#endif


/*

 For more details, photos, wiring, instructions, see:

    http://www.gammon.com.au/forum/?id=11638


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

// for SDFat library see: https://github.com/greiman/SdFat

#include <SdFat.h>

#include <avr/eeprom.h>

// #include <memdebug.h>

const char Version [] = "1.37";

const unsigned int ENTER_PROGRAMMING_ATTEMPTS = 50;

#include "HV_Pins.h"
#include "Signatures.h"
#include "General_Stuff.h"

// target board reset goes to here
const byte RESET = 5;
// 8 MHz clock on this pin
const byte CLOCKOUT = 9;

#if USE_BIT_BANGED_SPI

  // bit banged SPI pins
  #ifdef __AVR_ATmega2560__
    // Atmega2560
    #if USE_ETHERNET_SHIELD
      const byte MSPIM_SCK = 3;  // port E bit 5
    #else
      const byte MSPIM_SCK = 4;  // port G bit 5
    #endif
    const byte MSPIM_SS  = 5;  // port E bit 3
    const byte BB_MISO   = 6;  // port H bit 3
    const byte BB_MOSI   = 7;  // port H bit 4
  #elif defined(__AVR_ATmega1284P__)
    // Atmega1284P
    const byte MSPIM_SCK = 11;  // port D bit 3
    const byte MSPIM_SS  = 12;  // port D bit 4
    const byte BB_MISO   = 13;  // port D bit 5
    const byte BB_MOSI   = 14;  // port D bit 6
  #else
    // Atmega328
    #if USE_ETHERNET_SHIELD
      const byte MSPIM_SCK = 3;  // port D bit 3
    #else
      const byte MSPIM_SCK = 4;  // port D bit 4
    #endif
    const byte MSPIM_SS  = 5;  // port D bit 5
    const byte BB_MISO   = 6;  // port D bit 6
    const byte BB_MOSI   = 7;  // port D bit 7
  #endif


  /*

  Connect target processor like this:

    D4: (SCK)   --> SCK as per datasheet (If using Ethernet shield, use D3 instead)
    D5: (SS)    --> goes to /RESET on target
    D6: (MISO)  --> MISO as per datasheet
    D7: (MOSI)  --> MOSI as per datasheet

    D9: 8 Mhz clock signal if required by target

  Connect SD card like this:

    D10: SS   (chip select)
    D11: MOSI (DI - data into SD card)
    D12: MISO (DO - data out from SD card)
    D13: SCK  (CLK - clock)

  Both SD card and target processor will need +5V and Gnd connected.

  */

  // for fast port access
  #ifdef __AVR_ATmega2560__
    // Atmega2560
    #define BB_MISO_PORT PINH
    #define BB_MOSI_PORT PORTH
    #if USE_ETHERNET_SHIELD
      #define BB_SCK_PORT PORTE   // Pin D3
    #else
      #define BB_SCK_PORT PORTG   // Pind D4
    #endif
    const byte BB_SCK_BIT = 5;
    const byte BB_MISO_BIT = 3;
    const byte BB_MOSI_BIT = 4;
  #elif defined(__AVR_ATmega1284P__)
    // Atmega1284P
    #define BB_MISO_PORT PIND
    #define BB_MOSI_PORT PORTD
    #define BB_SCK_PORT PORTD
    const byte BB_SCK_BIT = 3;
    const byte BB_MISO_BIT = 5;
    const byte BB_MOSI_BIT = 6;
  #else
    // Atmega328
    #define BB_MISO_PORT PIND
    #define BB_MOSI_PORT PORTD
    #define BB_SCK_PORT PORTD
    #if USE_ETHERNET_SHIELD
      const byte BB_SCK_BIT = 3;  // Pin D3
    #else
      const byte BB_SCK_BIT = 4;  // Pind D4
    #endif
    const byte BB_MISO_BIT = 6;
    const byte BB_MOSI_BIT = 7;
  #endif

  // control speed of programming
  const byte BB_DELAY_MICROSECONDS = 6;



#endif // USE_BIT_BANGED_SPI


#if SD_CARD_ACTIVE
  // SD chip select pin
  #if USE_ETHERNET_SHIELD
    const uint8_t chipSelect = 4;   // Ethernet shield uses D4 as SD select
  #else
    const uint8_t chipSelect = SS;  // Otherwise normal slave select
  #endif

  const int MAX_FILENAME = 13;
  void * LAST_FILENAME_LOCATION_IN_EEPROM = 0;

  // file system object
  SdFat sd;
#endif

// actions to take
enum {
    checkFile,
    verifyFlash,
    writeToFlash,
};


// get a line from serial (file name)
//  ignore spaces, tabs etc.
//  forces to upper case
void getline (char * buf, size_t bufsize)
{
byte i;

  // discard any old junk
  while (Serial.available ())
    Serial.read ();

  for (i = 0; i < bufsize - 1; )
    {
    if (Serial.available ())
      {
      int c = Serial.read ();

      if (c == '\n')  // newline terminates
        break;

      if (!isspace (c))  // ignore spaces, carriage-return etc.
        buf [i++] = toupper (c);
      } // end if available
    }  // end of for
  buf [i] = 0;  // terminator
  Serial.println (buf);  // echo what they typed
  }     // end of getline


#if USE_BIT_BANGED_SPI

  // Bit Banged SPI transfer
  byte BB_SPITransfer (byte c)
  {
    byte bit;

    for (bit = 0; bit < 8; bit++)
      {
      // write MOSI on falling edge of previous clock
      if (c & 0x80)
          BB_MOSI_PORT |= bit (BB_MOSI_BIT);
      else
          BB_MOSI_PORT &= ~bit (BB_MOSI_BIT);
      c <<= 1;

      // read MISO
      c |= (BB_MISO_PORT & bit (BB_MISO_BIT)) != 0;

     // clock high
      BB_SCK_PORT |= bit (BB_SCK_BIT);

      // delay between rise and fall of clock
      delayMicroseconds (BB_DELAY_MICROSECONDS);

      // clock low
      BB_SCK_PORT &= ~bit (BB_SCK_BIT);

      // delay between rise and fall of clock
      delayMicroseconds (BB_DELAY_MICROSECONDS);
      }

    return c;
    }  // end of BB_SPITransfer

#endif // USE_BIT_BANGED_SPI

bool haveSDcard;


// convert two hex characters into a byte
//    returns true if error, false if OK
bool hexConv (const char * (& pStr), byte & b)
  {

  if (!isxdigit (pStr [0]) || !isxdigit (pStr [1]))
    {
    Serial.print (F("Invalid hex digits: "));
    Serial.print (pStr [0]);
    Serial.println (pStr [1]);
    return true;
    } // end not hex

  b = *pStr++ - '0';
  if (b > 9)
    b -= 7;

  // high-order nybble
  b <<= 4;

  byte b1 = *pStr++ - '0';
  if (b1 > 9)
    b1 -= 7;

  b |= b1;

  return false;  // OK
  }  // end of hexConv


void verifyData (const unsigned long addr, const byte * pData, const int length)
  {
  // check each byte
  for (int i = 0; i < length; i++)
    {
    unsigned long thisPage = (addr + i) & pagemask;
    // page changed? show progress
    if (thisPage != oldPage && oldPage != NO_PAGE)
      showProgress ();
    // now this is the current page
    oldPage = thisPage;

    byte found = readFlash (addr + i);
    byte expected = pData [i];
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

  }  // end of verifyData

unsigned long lowestAddress;
unsigned long highestAddress;
unsigned long bytesWritten;

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
      return;
      }  // end of signature found
    }  // end of for each signature

  Serial.println (F("Unrecogized signature."));
  }  // end of getSignature

void getFuseBytes ()
  {
  fuses [lowFuse]   = readFuse (lowFuse);
  fuses [highFuse]  = readFuse (highFuse);
  fuses [extFuse]   = readFuse (extFuse);
  fuses [lockByte]  = readFuse (lockByte);
  fuses [calibrationByte]  = readFuse (calibrationByte);

  Serial.print (F("LFuse = "));
  showHex (fuses [lowFuse], true);
  Serial.print (F("HFuse = "));
  showHex (fuses [highFuse], true);
  Serial.print (F("EFuse = "));
  showHex (fuses [extFuse], true);
  Serial.print (F("Lock byte = "));
  showHex (fuses [lockByte], true);
  Serial.print (F("Clock calibration = "));
  showHex (fuses [calibrationByte], true);
  }  // end of getFuseBytes

void showFuseName (const byte which)
  {
  switch (which)
    {
    case lowFuse:         Serial.print (F("low"));      break;
    case highFuse:        Serial.print (F("high"));     break;
    case extFuse:         Serial.print (F("extended")); break;
    case lockByte:        Serial.print (F("lock"));     break;
    case calibrationByte: Serial.print (F("clock"));    break;
    }  // end of switch
  }  // end of showFuseName

bool updateFuses (const bool writeIt)
  {
  unsigned long addr;
  unsigned int  len;

  byte fusenumber = currentSignature.fuseWithBootloaderSize;

  // if no fuse, can't change it
  if (fusenumber == NO_FUSE)
    {
    Serial.println (F("No bootloader fuse."));
    return false;  // ok return
    }

  addr = currentSignature.flashSize;
  len = currentSignature.baseBootSize;

  if (lowestAddress == 0)
    {
    Serial.println (F("No bootloader."));

    // don't use bootloader
    fuses [fusenumber] |= 1;
    }
  else
    {
    byte newval = 0xFF;

    if (lowestAddress == (addr - len))
      newval = 3;
    else if (lowestAddress == (addr - len * 2))
      newval = 2;
    else if (lowestAddress == (addr - len * 4))
      newval = 1;
    else if (lowestAddress == (addr - len * 8))
      newval = 0;
    else
      {
      Serial.println (F("Start address is not a bootloader boundary."));
      return true;
      }

    if (newval != 0xFF)
      {
      newval <<= 1;
      fuses [fusenumber] &= ~0x07;   // also program (clear) "boot into bootloader" bit
      fuses [fusenumber] |= newval;
      }  // if valid

    }  // if not address 0

  if (writeIt)
    {
    Serial.print (F("Setting "));
    writeFuse (fuses [fusenumber], fusenumber);
    }
  else
    Serial.print (F("Suggest making "));

  showFuseName (fusenumber);
  Serial.print (F(" fuse = "));
  showHex (fuses [fusenumber], true);

  if (writeIt)
    Serial.println (F("Done."));

  return false;
  }  // end of updateFuses

//------------------------------------------------------------------------------
//      SETUP
//------------------------------------------------------------------------------
void setup ()
  {
  Serial.begin(115200);
  while (!Serial) ;  // for Leonardo, Micro etc.

  Serial.println ();
  Serial.println ();
  Serial.println (F("Atmega hex file uploader."));
  Serial.println (F("Written by Nick Gammon."));
  Serial.print   (F("Version "));
  Serial.println (Version);
  Serial.println (F("Compiled on " __DATE__ " at " __TIME__ " with Arduino IDE " xstr(ARDUINO) "."));

  initPins ();

#if SD_CARD_ACTIVE
  initFile ();
#endif // SD_CARD_ACTIVE

}  // end of setup

bool getYesNo ()
  {
  char response [5];
  getline (response, sizeof response);

  return strcmp (response, "YES") == 0;
  }  // end of getYesNo

void eraseFlashContents ()
  {
  Serial.println (F("Erase all flash memory. ARE YOU SURE? Type 'YES' to confirm ..."));

  if (!getYesNo ())
    {
    Serial.println (F("Flash not erased."));
    return;
    }

  // ensure back in programming mode
  if (!startProgramming ())
    return;

  Serial.println (F("Erasing chip ..."));
  eraseMemory ();
  Serial.println (F("Flash memory erased."));

  }  // end of eraseFlashContents

#if ALLOW_MODIFY_FUSES
void modifyFuses ()
  {
  // display current fuses
  getFuseBytes ();
  byte fusenumber;

  Serial.println (F("Choose fuse (LOW/HIGH/EXT/LOCK) ..."));

  // get which fuse
  char response [6];
  getline (response, sizeof response);

  // turn into number
  if (strcmp (response, "LOW") == 0)
    fusenumber = lowFuse;
  else if (strcmp (response, "HIGH") == 0)
    fusenumber = highFuse;
  else if (strcmp (response, "EXT") == 0)
    fusenumber = extFuse;
  else if (strcmp (response, "LOCK") == 0)
    fusenumber = lockByte;
  else
    {
    Serial.println (F("Unknown fuse name."));
    return;
    }  // end if

  // show current value
  Serial.print (F("Current value of "));
  showFuseName (fusenumber);
  Serial.print (F(" fuse = "));
  showHex (fuses [fusenumber], true);

  // get new value
  Serial.print (F("Enter new value for "));
  showFuseName (fusenumber);
  Serial.println (F(" fuse (2 hex digits) ..."));

  getline (response, sizeof response);

  if (strlen (response) == 0)
    return;

  if (strlen (response) != 2)
    {
    Serial.println (F("Enter exactly two hex digits."));
    return;
    }

  const char * pResponse = response;
  byte newValue;
  if (hexConv (pResponse, newValue))
    return;  // bad hex value

  // check if no change
  if (newValue == fuses [fusenumber])
    {
    Serial.println (F("Same as original. No change requested."));
    return;
    }  // end if no change to fuse

#if SAFETY_CHECKS
  if (fusenumber == highFuse && (newValue & 0xC0) != 0xC0)
    {
    Serial.println (F("Activating RSTDISBL/DWEN/OCDEN/JTAGEN not permitted."));
    return;
    }  // end safety check

  if (fusenumber == highFuse && (newValue & 0x20) != 0)
    {
    Serial.println (F("Disabling SPIEN not permitted."));
    return;
    }  // end safety check
#endif // SAFETY_CHECKS

  // get confirmation
  Serial.println (F("WARNING: Fuse changes may make the processor unresponsive."));
  Serial.print (F("Confirm change "));
  showFuseName (fusenumber);
  Serial.print (F(" fuse from "));
  showHex (fuses [fusenumber], false, true);
  Serial.print (F("to "));
  showHex (newValue, false, true);
  Serial.println (F(". Type 'YES' to confirm ..."));

  // has to be "YES" (not case sensitive)
  if (!getYesNo ())
    {
    Serial.println (F("Cancelled."));
    return;
    }  // if cancelled

  // ensure back in programming mode
  if (!startProgramming ())
    return;

  // tell them what we are doing
  Serial.print (F("Changing "));
  showFuseName (fusenumber);
  Serial.println (F(" fuse ..."));

  // change it
  writeFuse (newValue, fusenumber);

  // confirm and show new value
  Serial.println (F("Fuse written."));
  getFuseBytes ();

  }  // end of modifyFuses
#endif

//------------------------------------------------------------------------------
//      LOOP
//------------------------------------------------------------------------------
void loop ()
{
  Serial.println ();
  Serial.println (F("--------- Starting ---------"));
  Serial.println ();

  if (!startProgramming ())
    {
    Serial.println (F("Halted."));
    stopProgramming ();
    while  (true)
      {}
    }  // end of could not enter programming mode

  getSignature ();
  getFuseBytes ();

  // don't have signature? don't proceed or try recovery
  if (foundSig == -1)
    {
    Serial.println (F("Halted.Are you trying to recover a chip with an external clock set?"));
    char response = Serial.read();
    switch (response) {
      case 'y':
        fuseRecovery ();
        break;
      default: 
        stopProgramming ();
        while  (true)
        {}
     }

    }  // end of no signature

 // ask for verify or write
  Serial.println (F("Actions:"));
  Serial.println (F(" [E] erase flash"));
#if ALLOW_MODIFY_FUSES
  Serial.println (F(" [F] modify fuses"));
#endif

#if SD_CARD_ACTIVE
  if (haveSDcard)
    {
    Serial.println (F(" [L] list directory"));
#if ALLOW_FILE_SAVING
    Serial.println (F(" [R] read from flash (save to disk)"));
#endif
    Serial.println (F(" [V] verify flash (compare to disk)"));
    Serial.println (F(" [W] write to flash (read from disk)"));
    }  // end of if SD card detected
#endif // SD_CARD_ACTIVE

  Serial.println (F("Enter action:"));

  // discard any old junk
  while (Serial.available ())
    Serial.read ();

  // turn off programming outputs if required
  if (allowTargetToRun)
    stopProgramming ();

  char command;
  do
    {
    command = toupper (Serial.read ());
    } while (!isalpha (command));

  Serial.println (command);  // echo their input

  // re-start programming mode if required
  if (allowTargetToRun)
    if (!startProgramming ())
      {
      Serial.println (F("Halted."));
      while  (true)
        {}
      } // end of could not enter programming mode

  switch (command)
    {
#if SD_CARD_ACTIVE

  #if ALLOW_FILE_SAVING
      case 'R':
        readFlashContents ();
        break;
  #endif

      case 'W':
        writeFlashContents ();
        break;

      case 'V':
        verifyFlashContents ();
        break;
#endif // SD_CARD_ACTIVE

    case 'E':
      eraseFlashContents ();
      break;
#if ALLOW_MODIFY_FUSES
    case 'F':
      modifyFuses ();
      break;
#endif

#if SD_CARD_ACTIVE
    case 'L':
      showDirectory ();
      break;
#endif // SD_CARD_ACTIVE

    default:
      Serial.println (F("Unknown command."));
      break;
    }  // end of switch on command

}  // end of loop

