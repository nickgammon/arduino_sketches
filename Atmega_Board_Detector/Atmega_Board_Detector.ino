// Atmega chip fuse detector
// Author: Nick Gammon
// Date: 22nd May 2012
// Version: 1.19

// Version 1.1 added signatures for Attiny24/44/84 (5 May 2012)
// Version 1.2 added signatures for ATmeag8U2/16U2/32U2 (7 May 2012)
// Version 1.3: Added signature for ATmega1284P (8 May 2012)
// Version 1.4: Output an 8 MHz clock on pin 9
// Version 1.5: Corrected flash size for Atmega1284P.
// Version 1.6: Added signatures for ATtiny2313A, ATtiny4313, ATtiny13
// Version 1.7: Added signature for Atmega8A
// Version 1.8: Allowed for running on the Leonardo, Micro, etc.
// Version 1.9: Fixed bug where wrong fuse was being checked for bootloader in some cases
// Version 1.10: Added database of known signatures
// Version 1.11: Added MD5 sum for Uno Atmega16U2 (USB) bootloader
// Version 1.12: Display message if cannot enter programming mode.
// Version 1.13: Added support for At90USB82, At90USB162
// Version 1.14: Added support for ATmega328
// Version 1.15: Added preliminary support for high-voltage programming mode for Atmega328 family
// Version 1.16: Major tidy-ups, made code more modular
// Version 1.17: Added signature for Leonardo_prod_firmware_2012_12_10 bootloader
// Version 1.18: Got rid of compiler warnings in IDE 1.6.7
// Version 1.19: Added more signatures: ATmega168V, ATmega328PB, ATmega1284


const char Version [] = "1.19";

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

#define USE_BIT_BANGED_SPI false

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
#pragma GCC optimize ("-O0") // avoid GCC memcpy inline

#include <SPI.h>
extern "C"
  {
  #include "md5.h"
  }

const byte CLOCKOUT = 9;
const byte RESET = 10;  // --> goes to reset on the target board

const int ENTER_PROGRAMMING_ATTEMPTS = 50;

#if ARDUINO < 100
  const byte SCK = 13;    // SPI clock
#endif

#include "HV_Pins.h"
#include "Signatures.h"
#include "General_Stuff.h"
#include <string.h>  // needed for memcpy

// for looking up known signatures
typedef struct {
   byte md5sum [16];
   char const * filename;
} deviceDatabaseType;

// These are bootloaders we know about.

const char ATmegaBOOT_168_atmega328             [] PROGMEM = "ATmegaBOOT_168_atmega328";
const char ATmegaBOOT_168_atmega328_pro_8MHz    [] PROGMEM = "ATmegaBOOT_168_atmega328_pro_8MHz";
const char ATmegaBOOT_168_atmega1280            [] PROGMEM = "ATmegaBOOT_168_atmega1280";
const char ATmegaBOOT_168_diecimila             [] PROGMEM = "ATmegaBOOT_168_diecimila";
const char ATmegaBOOT_168_ng                    [] PROGMEM = "ATmegaBOOT_168_ng";
const char ATmegaBOOT_168_pro_8MHz              [] PROGMEM = "ATmegaBOOT_168_pro_8MHz";
const char ATmegaBOOT                           [] PROGMEM = "ATmegaBOOT";
const char ATmegaBOOT_168                       [] PROGMEM = "ATmegaBOOT_168";
const char ATmegaBOOT_168_atmega328_bt          [] PROGMEM = "ATmegaBOOT_168_atmega328_bt";
const char LilyPadBOOT_168                      [] PROGMEM = "LilyPadBOOT_168";
const char optiboot_atmega328_IDE_0022          [] PROGMEM = "optiboot_atmega328_IDE_0022";
const char optiboot_atmega328_pro_8MHz          [] PROGMEM = "optiboot_atmega328_pro_8MHz";
const char optiboot_lilypad                     [] PROGMEM = "optiboot_lilypad";
const char optiboot_luminet                     [] PROGMEM = "optiboot_luminet";
const char optiboot_pro_16MHz                   [] PROGMEM = "optiboot_pro_16MHz";
const char optiboot_pro_20mhz                   [] PROGMEM = "optiboot_pro_20mhz";
const char stk500boot_v2_mega2560               [] PROGMEM = "stk500boot_v2_mega2560";
const char DiskLoader_Leonardo                  [] PROGMEM = "DiskLoader-Leonardo";
const char optiboot_atmega8                     [] PROGMEM = "optiboot_atmega8";
const char optiboot_atmega168                   [] PROGMEM = "optiboot_atmega168";
const char optiboot_atmega328                   [] PROGMEM = "optiboot_atmega328";
const char optiboot_atmega328_Mini              [] PROGMEM = "optiboot_atmega328-Mini";
const char ATmegaBOOT_324P                      [] PROGMEM = "ATmegaBOOT_324P";
const char ATmegaBOOT_644                       [] PROGMEM = "ATmegaBOOT_644";
const char ATmegaBOOT_644P                      [] PROGMEM = "ATmegaBOOT_644P";
const char Mega2560_Original                    [] PROGMEM = "Mega2560_Original";
const char optiboot_atmega1284p                 [] PROGMEM = "optiboot_atmega1284p";
const char Ruggeduino                           [] PROGMEM = "Ruggeduino";
const char Leonardo_prod_firmware_2012_04_26    [] PROGMEM = "Leonardo-prod-firmware-2012-04-26";
const char Leonardo_prod_firmware_2012_12_10    [] PROGMEM = "Leonardo-prod-firmware-2012-12-10";
const char atmega2560_bootloader_wd_bug_fixed   [] PROGMEM = "atmega2560_bootloader_watchdog_bug_fixed";
const char Caterina_Esplora                     [] PROGMEM = "Esplora";
const char Sanguino_ATmegaBOOT_644P             [] PROGMEM = "Sanguino_ATmegaBOOT_644P";
const char Sanguino_ATmegaBOOT_168_atmega644p   [] PROGMEM = "Sanguino_ATmegaBOOT_168_atmega644p";
const char Sanguino_ATmegaBOOT_168_atmega1284p  [] PROGMEM = "Sanguino_ATmegaBOOT_168_atmega1284p";
const char Sanguino_ATmegaBOOT_168_atmega1284p_8m [] PROGMEM = "Sanguino_ATmegaBOOT_168_atmega1284p_8m";
const char Arduino_dfu_usbserial_atmega16u2_Uno_Rev3 [] PROGMEM = "Arduino-dfu-usbserial-atmega16u2-Uno-Rev3";

// Signatures (MD5 sums) for above bootloaders
const deviceDatabaseType deviceDatabase [] PROGMEM = 
  {
  { { 0x0A, 0xAC, 0xF7, 0x16, 0xF4, 0x3C, 0xA2, 0xC9, 0x27, 0x7E, 0x08, 0xB9, 0xD6, 0x90, 0xBC, 0x02,  }, ATmegaBOOT_168_atmega328 }, 
  { { 0x27, 0xEB, 0x87, 0x14, 0x5D, 0x45, 0xD4, 0xD8, 0x41, 0x44, 0x52, 0xCE, 0x0A, 0x2B, 0x8C, 0x5F,  }, ATmegaBOOT_168_atmega328_pro_8MHz }, 
  { { 0x01, 0x24, 0x13, 0x56, 0x60, 0x4D, 0x91, 0x7E, 0xDC, 0xEE, 0x84, 0xD1, 0x19, 0xEF, 0x91, 0xCE,  }, ATmegaBOOT_168_atmega1280 }, 
  { { 0x14, 0x61, 0xCE, 0xDF, 0x85, 0x46, 0x0D, 0x96, 0xCC, 0x41, 0xCB, 0x01, 0x69, 0x40, 0x28, 0x1A,  }, ATmegaBOOT_168_diecimila }, 
  { { 0x6A, 0x22, 0x9F, 0xB4, 0x64, 0x37, 0x3F, 0xA3, 0x0C, 0x68, 0x39, 0x1D, 0x6A, 0x97, 0x2C, 0x40,  }, ATmegaBOOT_168_ng }, 
  { { 0xFF, 0x99, 0xA2, 0xC0, 0xD9, 0xC9, 0xE5, 0x1B, 0x98, 0x7D, 0x9E, 0x56, 0x12, 0xC2, 0xA4, 0xA1,  }, ATmegaBOOT_168_pro_8MHz }, 
  { { 0x98, 0x6D, 0xCF, 0xBB, 0x55, 0xE1, 0x22, 0x1E, 0xE4, 0x3C, 0xC2, 0x07, 0xB2, 0x2B, 0x46, 0xAE,  }, ATmegaBOOT }, 
  { { 0x37, 0xC0, 0xFC, 0x90, 0xE2, 0xA0, 0x5D, 0x8F, 0x62, 0xEB, 0xAE, 0x9C, 0x36, 0xC2, 0x24, 0x05,  }, ATmegaBOOT_168 }, 
  { { 0x29, 0x3E, 0xB3, 0xB7, 0x39, 0x84, 0x2D, 0x35, 0xBA, 0x9D, 0x02, 0xF9, 0xC7, 0xF7, 0xC9, 0xD6,  }, ATmegaBOOT_168_atmega328_bt }, 
  { { 0xFC, 0xAF, 0x05, 0x0E, 0xB4, 0xD7, 0x2D, 0x75, 0x8F, 0x41, 0x8C, 0x85, 0x83, 0x56, 0xAA, 0x35,  }, LilyPadBOOT_168 }, 
  { { 0x55, 0x71, 0xA1, 0x8C, 0x81, 0x3B, 0x9E, 0xD2, 0xE6, 0x3B, 0xC9, 0x3B, 0x9A, 0xB1, 0x79, 0x53,  }, optiboot_atmega328_IDE_0022 }, 
  { { 0x3C, 0x08, 0x90, 0xA1, 0x6A, 0x13, 0xA2, 0xF0, 0xA5, 0x1D, 0x26, 0xEC, 0xF1, 0x4B, 0x0F, 0xB3,  }, optiboot_atmega328_pro_8MHz }, 
  { { 0xAD, 0xBD, 0xA7, 0x4A, 0x4F, 0xAB, 0xA8, 0x65, 0x34, 0x92, 0xF8, 0xC9, 0xCE, 0x58, 0x7D, 0x78,  }, optiboot_lilypad }, 
  { { 0x7B, 0x5C, 0xAC, 0x08, 0x2A, 0x0B, 0x2D, 0x45, 0x69, 0x11, 0xA7, 0xA0, 0xAE, 0x65, 0x7F, 0x66,  }, optiboot_luminet }, 
  { { 0x6A, 0x95, 0x0A, 0xE1, 0xDB, 0x1F, 0x9D, 0xC7, 0x8C, 0xF8, 0xA4, 0x80, 0xB5, 0x1E, 0x54, 0xE1,  }, optiboot_pro_16MHz }, 
  { { 0x2C, 0x55, 0xB4, 0xB8, 0xB5, 0xC5, 0xCB, 0xC4, 0xD3, 0x36, 0x99, 0xCB, 0x4B, 0x9F, 0xDA, 0xBE,  }, optiboot_pro_20mhz }, 
  { { 0x1E, 0x35, 0x14, 0x08, 0x1F, 0x65, 0x7F, 0x8C, 0x96, 0x50, 0x69, 0x9F, 0x19, 0x1E, 0x3D, 0xF0,  }, stk500boot_v2_mega2560 }, 
  { { 0xC2, 0x59, 0x71, 0x5F, 0x96, 0x28, 0xE3, 0xAA, 0xB0, 0x69, 0xE2, 0xAF, 0xF0, 0x85, 0xA1, 0x20,  }, DiskLoader_Leonardo }, 
  { { 0xE4, 0xAF, 0xF6, 0x6B, 0x78, 0xDA, 0xE4, 0x30, 0xFE, 0xB6, 0x52, 0xAF, 0x53, 0x52, 0x18, 0x49,  }, optiboot_atmega8 }, 
  { { 0x3A, 0x89, 0x30, 0x4B, 0x15, 0xF5, 0xBB, 0x11, 0xAA, 0xE6, 0xE6, 0xDC, 0x7C, 0xF5, 0x91, 0x35,  }, optiboot_atmega168 }, 
  { { 0xFB, 0xF4, 0x9B, 0x7B, 0x59, 0x73, 0x7F, 0x65, 0xE8, 0xD0, 0xF8, 0xA5, 0x08, 0x12, 0xE7, 0x9F,  }, optiboot_atmega328 }, 
  { { 0x7F, 0xDF, 0xE1, 0xB2, 0x6F, 0x52, 0x8F, 0xBD, 0x7C, 0xFE, 0x7E, 0xE0, 0x84, 0xC0, 0xA5, 0x6B,  }, optiboot_atmega328_Mini }, 
  { { 0x31, 0x28, 0x0B, 0x06, 0xAD, 0xB5, 0xA4, 0xC9, 0x2D, 0xEF, 0xB3, 0x69, 0x29, 0x22, 0xEA, 0xBF,  }, ATmegaBOOT_324P }, 
  { { 0xE8, 0x93, 0x44, 0x43, 0x37, 0xD3, 0x28, 0x3C, 0x7D, 0x9A, 0xEB, 0x84, 0x46, 0xD5, 0x45, 0x42,  }, ATmegaBOOT_644 }, 
  { { 0x51, 0x69, 0x10, 0x40, 0x8F, 0x07, 0x81, 0xC6, 0x48, 0x51, 0x54, 0x5E, 0x96, 0x73, 0xC2, 0xEB,  }, ATmegaBOOT_644P }, 
  { { 0xB9, 0x49, 0x93, 0x09, 0x49, 0x1A, 0x64, 0x6E, 0xCD, 0x58, 0x47, 0x89, 0xC2, 0xD8, 0xA4, 0x6C,  }, Mega2560_Original }, 
  { { 0x71, 0xDD, 0xC2, 0x84, 0x64, 0xC4, 0x73, 0x27, 0xD2, 0x33, 0x01, 0x1E, 0xFA, 0xE1, 0x24, 0x4B,  }, optiboot_atmega1284p },
  { { 0x0F, 0x02, 0x31, 0x72, 0x95, 0xC8, 0xF7, 0xFD, 0x1B, 0xB7, 0x07, 0x17, 0x85, 0xA5, 0x66, 0x87,  }, Ruggeduino }, 
  { { 0x53, 0xE0, 0x2C, 0xBC, 0x87, 0xF5, 0x0B, 0x68, 0x2C, 0x71, 0x13, 0xE0, 0xED, 0x84, 0x05, 0x34,  }, Leonardo_prod_firmware_2012_04_26 }, 
  { { 0xF3, 0x9D, 0xC5, 0xF5, 0x96, 0x43, 0x85, 0x84, 0x5C, 0xC5, 0x5B, 0x2F, 0x9B, 0x90, 0x6D, 0x38,  }, Leonardo_prod_firmware_2012_12_10 }, 
  { { 0x12, 0xAA, 0x80, 0x07, 0x4D, 0x74, 0xE3, 0xDA, 0xBF, 0x2D, 0x25, 0x84, 0x6D, 0x99, 0xF7, 0x20,  }, atmega2560_bootloader_wd_bug_fixed }, 
  { { 0x32, 0x56, 0xC1, 0xD3, 0xAC, 0x78, 0x32, 0x4D, 0x04, 0x6D, 0x3F, 0x6D, 0x01, 0xEC, 0xAE, 0x09,  }, Caterina_Esplora }, 
  { { 0x39, 0xCC, 0x80, 0xD6, 0xDE, 0xA2, 0xC4, 0x91, 0x6F, 0xBC, 0xE8, 0xDD, 0x70, 0xF2, 0xA2, 0x33,  }, Sanguino_ATmegaBOOT_644P }, 
  { { 0x60, 0x49, 0xC6, 0x0A, 0xE6, 0x31, 0x5C, 0xC1, 0xBA, 0xD7, 0x24, 0xEF, 0x8B, 0x6D, 0xE6, 0xD0,  }, Sanguino_ATmegaBOOT_168_atmega644p }, 
  { { 0xC1, 0x17, 0xE3, 0x5E, 0x9C, 0x43, 0x66, 0x5F, 0x1E, 0x4C, 0x41, 0x95, 0x44, 0x60, 0x47, 0xD5,  }, Sanguino_ATmegaBOOT_168_atmega1284p }, 
  { { 0x27, 0x4B, 0x68, 0x8A, 0x8A, 0xA2, 0x4C, 0xE7, 0x30, 0x7F, 0x97, 0x37, 0x87, 0x16, 0x4E, 0x21,  }, Sanguino_ATmegaBOOT_168_atmega1284p_8m }, 
  { { 0xD8, 0x8C, 0x70, 0x6D, 0xFE, 0x1F, 0xDC, 0x38, 0x82, 0x1E, 0xCE, 0xAE, 0x23, 0xB2, 0xE6, 0xE7,  }, Arduino_dfu_usbserial_atmega16u2_Uno_Rev3 }, 
  };

// Print a string from Program Memory directly to save RAM 
void printProgStr (const char * str)
{
  char c;
  if (!str) 
    return;
  while ((c = pgm_read_byte(str++)))
    Serial.print (c);
} // end of printProgStr

void readBootloader ()
  {
  unsigned long addr;
  unsigned int  len;

  if (currentSignature.baseBootSize == 0)
    {
    Serial.println (F("No bootloader support."));
    return;
    }

  byte fusenumber = currentSignature.fuseWithBootloaderSize;
  byte whichFuse;
  byte hFuse = readFuse (highFuse);

  switch (fusenumber)
    {
    case lowFuse:
    case highFuse:
    case extFuse:
      whichFuse = readFuse (fusenumber);
      break;

    default:
      Serial.println (F("No bootloader fuse."));
      return;

    } // end of switch

  addr = currentSignature.flashSize;
  len = currentSignature.baseBootSize;

  Serial.print (F("Bootloader in use: "));
  showYesNo ((whichFuse & bit (0)) == 0, true);
  Serial.print (F("EEPROM preserved through erase: "));
  showYesNo ((hFuse & bit (3)) == 0, true);
  Serial.print (F("Watchdog timer always on: "));
  showYesNo ((hFuse & bit (4)) == 0, true);

  // work out bootloader size
  // these 2 bits basically give a base bootloader size multiplier
  switch ((whichFuse >> 1) & 3)
    {
    case 0: len *= 8; break;
    case 1: len *= 4; break;
    case 2: len *= 2; break;
    case 3: len *= 1; break;
    }  // end of switch

  // where bootloader starts
  addr -= len;

  Serial.print (F("Bootloader is "));
  Serial.print (len);
  Serial.print (F(" bytes starting at "));
  Serial.println (addr, HEX);
  Serial.println ();
  Serial.println (F("Bootloader:"));
  Serial.println ();

  for (int i = 0; i < len; i++)
    {
    // show address
    if (i % 16 == 0)
      {
      Serial.print (addr + i, HEX);
      Serial.print (F(": "));
      }
    showHex (readFlash (addr + i));
    // new line every 16 bytes
    if (i % 16 == 15)
      Serial.println ();
    }  // end of for

  Serial.println ();
  Serial.print (F("MD5 sum of bootloader = "));

  md5_context ctx;
  byte md5sum [16];
  byte mem;
  bool allFF = true;

  md5_starts( &ctx );

  while (len--)
    {
    mem = readFlash (addr++);
    if (mem != 0xFF)
      allFF = false;
    md5_update( &ctx, &mem, 1);
    }  // end of doing MD5 sum on each byte

  md5_finish( &ctx, md5sum );

  for (int i = 0; i < sizeof md5sum; i++)
    showHex (md5sum [i]);
  Serial.println ();

  if (allFF)
    Serial.println (F("No bootloader (all 0xFF)"));
  else
    {
    bool found = false;
    for (int i = 0; i < NUMITEMS (deviceDatabase); i++)
      {
      deviceDatabaseType dbEntry;
      memcpy_P (&dbEntry, &deviceDatabase [i], sizeof (dbEntry));
      if (memcmp (dbEntry.md5sum, md5sum, sizeof md5sum) != 0)
        continue;
      // found match!  
      Serial.print (F("Bootloader name: "));
      printProgStr (dbEntry.filename);
      Serial.println ();
      found = true;
      break;
      }// end of for
    
    if (!found)
      Serial.println (F("Bootloader MD5 sum not known."));
    }
    
  } // end of readBootloader

void readProgram ()
  {
  unsigned long addr = 0;
  unsigned int  len = 256;
  Serial.println ();
  Serial.println (F("First 256 bytes of program memory:"));
  Serial.println ();

  for (int i = 0; i < len; i++)
    {
    // show address
    if (i % 16 == 0)
      {
      if ((addr + i) < 16)
        Serial.print (F("0"));
      Serial.print (addr + i, HEX);
      Serial.print (F(": "));
      }
    showHex (readFlash (addr + i));
    // new line every 16 bytes
    if (i % 16 == 15)
      Serial.println ();
    }  // end of for

  Serial.println ();

  } // end of readProgram

void getSignature ()
  {
  foundSig = -1;
  byte sig [3];
  Serial.print (F("Signature = "));
  
  readSignature (sig);
  for (byte i = 0; i < 3; i++)
    showHex (sig [i]);

  Serial.println ();

  for (int j = 0; j < NUMITEMS (signatures); j++)
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

void setup ()
  {
  Serial.begin (115200);
  while (!Serial) ;  // for Leonardo, Micro etc.
  Serial.println ();
  Serial.println (F("Atmega chip detector."));
  Serial.println (F("Written by Nick Gammon."));
  Serial.print   (F("Version "));
  Serial.println (Version);
  Serial.println (F("Compiled on " __DATE__ " at " __TIME__ " with Arduino IDE " xstr(ARDUINO) "."));

  initPins ();

  if (startProgramming ())
    {
    getSignature ();
    getFuseBytes ();
  
    if (foundSig != -1)
      {
      readBootloader ();
      }
  
    readProgram ();
    }   // end of if entered programming mode OK
   
   stopProgramming ();
   
 }  // end of setup

void loop () {}

