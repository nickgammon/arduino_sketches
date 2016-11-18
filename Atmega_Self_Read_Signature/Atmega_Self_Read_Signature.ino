// Atmega chip self-detector of signature bytes and bootloader
// Author: Nick Gammon
// Date: 23 October 2013
// Version: 1.0


/*

 Copyright 2013 Nick Gammon.


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

#include <avr/boot.h>
#include <avr/pgmspace.h>
#include <string.h>  // for memcpy

extern "C"
  {
  #include "md5.h"
  }

#define SIGRD 5

// number of items in an array
#define NUMITEMS(arg) ((unsigned int) (sizeof (arg) / sizeof (arg [0])))

typedef struct {
   byte sig [3];
   const char * desc;
   unsigned long flashSize;
   unsigned int baseBootSize;
} signatureType;

// stringification for Arduino IDE version
#define xstr(s) str(s)
#define str(s) #s

const unsigned long kb = 1024;

// see Atmega328 datasheet page 298
const signatureType signatures [] =
  {
//     signature          description   flash size  bootloader size

  // Attiny84 family
  { { 0x1E, 0x91, 0x0B }, "ATtiny24",   2 * kb,         0 },
  { { 0x1E, 0x92, 0x07 }, "ATtiny44",   4 * kb,         0 },
  { { 0x1E, 0x93, 0x0C }, "ATtiny84",   8 * kb,         0 },

  // Attiny85 family
  { { 0x1E, 0x91, 0x08 }, "ATtiny25",   2 * kb,         0 },
  { { 0x1E, 0x92, 0x06 }, "ATtiny45",   4 * kb,         0 },
  { { 0x1E, 0x93, 0x0B }, "ATtiny85",   8 * kb,         0 },

  // Atmega328 family
  { { 0x1E, 0x92, 0x0A }, "ATmega48PA",   4 * kb,         0 },
  { { 0x1E, 0x93, 0x0F }, "ATmega88PA",   8 * kb,       256 },
  { { 0x1E, 0x94, 0x0B }, "ATmega168PA", 16 * kb,       256 },
  { { 0x1E, 0x94, 0x06 }, "ATmega168V",  16 * kb,       256 },
  { { 0x1E, 0x95, 0x0F }, "ATmega328P",  32 * kb,       512 },
  { { 0x1E, 0x95, 0x16 }, "ATmega328PB", 32 * kb,       512 },

  // Atmega644 family
  { { 0x1E, 0x94, 0x0A }, "ATmega164P",   16 * kb,      256 },
  { { 0x1E, 0x95, 0x08 }, "ATmega324P",   32 * kb,      512 },
  { { 0x1E, 0x96, 0x0A }, "ATmega644P",   64 * kb,   1 * kb },

  // Atmega2560 family
  { { 0x1E, 0x96, 0x08 }, "ATmega640",    64 * kb,   1 * kb },
  { { 0x1E, 0x97, 0x03 }, "ATmega1280",  128 * kb,   1 * kb },
  { { 0x1E, 0x97, 0x04 }, "ATmega1281",  128 * kb,   1 * kb },
  { { 0x1E, 0x98, 0x01 }, "ATmega2560",  256 * kb,   1 * kb },
  { { 0x1E, 0x98, 0x02 }, "ATmega2561",  256 * kb,   1 * kb },

  // AT90USB family
  { { 0x1E, 0x93, 0x82 }, "At90USB82",    8 * kb, 512,  },
  { { 0x1E, 0x94, 0x82 }, "At90USB162",  16 * kb, 512,  },

  // Atmega32U2 family
  { { 0x1E, 0x93, 0x89 }, "ATmega8U2",    8 * kb,   512 },
  { { 0x1E, 0x94, 0x89 }, "ATmega16U2",  16 * kb,   512 },
  { { 0x1E, 0x95, 0x8A }, "ATmega32U2",  32 * kb,   512 },

  // Atmega32U4 family
  { { 0x1E, 0x94, 0x88 }, "ATmega16U4",  16 * kb,   512 },
  { { 0x1E, 0x95, 0x87 }, "ATmega32U4",  32 * kb,   512 },

  // ATmega1284P family
  { { 0x1E, 0x97, 0x05 }, "ATmega1284P", 128 * kb,   1 * kb },
  { { 0x1E, 0x97, 0x06 }, "ATmega1284",  128 * kb,   1 * kb },

  // ATtiny4313 family
  { { 0x1E, 0x91, 0x0A }, "ATtiny2313A", 2 * kb,   0 },
  { { 0x1E, 0x92, 0x0D }, "ATtiny4313",  4 * kb,   0 },

  // ATtiny13 family
  { { 0x1E, 0x90, 0x07 }, "ATtiny13A",   1 * kb,   0 },

  // Atmega8A family
  { { 0x1E, 0x93, 0x07 }, "ATmega8A",    8 * kb,  256 },

  };  // end of signatures

// if signature found in above table, this is its index
int foundSig = -1;

void showHex (const byte b, const boolean newline = false);
void showHex (const byte b, const boolean newline)
  {
  // try to avoid using sprintf
  char buf [4];
  buf [0] = ((b >> 4) & 0x0F) | '0';
  buf [1] = (b & 0x0F) | '0';
  buf [2] = ' ';
  buf [3] = 0;
  if (buf [0] > '9')
    buf [0] += 7;
  if (buf [1] > '9')
    buf [1] += 7;
  Serial.print (buf);
  if (newline)
    Serial.println ();
  }  // end of showHex

void showYesNo (const boolean b, const boolean newline = false);
void showYesNo (const boolean b, const boolean newline)
  {
  if (b)
    Serial.print (F("Yes"));
  else
    Serial.print (F("No"));
  if (newline)
    Serial.println ();
  }  // end of showYesNo

byte readFlash (unsigned long addr)
  {
#if FLASHEND > 0xFFFF
    return pgm_read_byte_far (addr);
#else
    return pgm_read_byte (addr);
#endif
  }

void readBootloader ()
  {
  unsigned long addr;
  unsigned int  len;
  byte hFuse = boot_lock_fuse_bits_get (GET_HIGH_FUSE_BITS);

  if (signatures [foundSig].baseBootSize == 0)
    {
    Serial.println (F("No bootloader support."));
    return;
    }

  addr = signatures [foundSig].flashSize;
  len = signatures [foundSig].baseBootSize;

  Serial.print (F("Bootloader in use: "));
  showYesNo ((hFuse & bit (0)) == 0, true);
  Serial.print (F("EEPROM preserved through erase: "));
  showYesNo ((hFuse & bit (3)) == 0, true);
  Serial.print (F("Watchdog timer always on: "));
  showYesNo ((hFuse & bit (4)) == 0, true);

  // work out bootloader size
  // these 2 bits basically give a base bootloader size multiplier
  switch ((hFuse >> 1) & 3)
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
      Serial.print (": ");
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

  md5_starts( &ctx );

  while (len--)
    {
    mem = readFlash (addr++);
    md5_update( &ctx, &mem, 1);
    }  // end of doing MD5 sum on each byte

  md5_finish( &ctx, md5sum );

  for (int i = 0; i < sizeof md5sum; i++)
    showHex (md5sum [i]);
  Serial.println ();

  } // end of readBootloader

void getSignature ()
  {
  byte sig [3];

Serial.print (F("Signature = "));

  sig [0] = boot_signature_byte_get (0);
  showHex (sig [0]);
  Serial.print (" ");
  sig [1] = boot_signature_byte_get (2);
  showHex (sig [1]);
  Serial.print (" ");
  sig [2] = boot_signature_byte_get (4);
  showHex (sig [2]);
  Serial.println ();

Serial.println (F("Fuses"));
byte fuse;

  Serial.print (F("Low = "));
  fuse = boot_lock_fuse_bits_get (GET_LOW_FUSE_BITS);
  showHex (fuse);
  Serial.print (F("High = "));
  fuse = boot_lock_fuse_bits_get (GET_HIGH_FUSE_BITS);
  showHex (fuse);
  Serial.print (F("Ext = "));
  fuse = boot_lock_fuse_bits_get (GET_EXTENDED_FUSE_BITS);
  showHex (fuse);
  Serial.print (F("Lock = "));
  fuse = boot_lock_fuse_bits_get (GET_LOCK_BITS);
  showHex (fuse);
  Serial.println ();
  Serial.println ();

  for (int j = 0; j < NUMITEMS (signatures); j++)
    {
    if (memcmp (sig, signatures [j].sig, sizeof sig) == 0)
      {
      foundSig = j;
      Serial.print (F("Processor = "));
      Serial.println (signatures [j].desc);
      Serial.print (F("Flash memory size = "));
      Serial.println (signatures [j].flashSize, DEC);
      return;
      }
    }

  Serial.println (F("Unrecogized signature."));
  }  // end of getSignature

void setup ()
  {
  Serial.begin (115200);
  while (!Serial) {}

  Serial.println ();
  Serial.println (F("Signature detector."));
  Serial.println (F("Written by Nick Gammon."));
  Serial.println (F("Compiled on " __DATE__ " at " __TIME__ " with Arduino IDE " xstr(ARDUINO) "."));

  getSignature ();

  if (foundSig != -1)
    {
    readBootloader ();
    }
  }

void loop () {}

