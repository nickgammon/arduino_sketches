// Atmega chip fuse detector
// Author: Nick Gammon
// Date: 22nd May 2012
// Version: 1.8

// Version 1.1 added signatures for Attiny24/44/84 (5 May 2012)
// Version 1.2 added signatures for ATmeag8U2/16U2/32U2 (7 May 2012)
// Version 1.3: Added signature for ATmega1284P (8 May 2012)
// Version 1.4: Output an 8 MHz clock on pin 9
// Version 1.5: Corrected flash size for Atmega1284P.
// Version 1.6: Added signatures for ATtiny2313A, ATtiny4313, ATtiny13
// Version 1.7: Added signature for Atmega8A
// Version 1.8: Allowed for running on the Leonardo, Micro, etc.

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
extern "C" 
  {
  #include "md5.h"
  }

const byte CLOCKOUT = 9;
const byte RESET = 10;  // --> goes to reset on the target board

#if ARDUINO < 100
  const byte SCK = 13;    // SPI clock
#endif

// number of items in an array
#define NUMITEMS(arg) ((unsigned int) (sizeof (arg) / sizeof (arg [0])))

// programming commands to send via SPI to the chip
enum {
    progamEnable = 0xAC,
    programAcknowledge = 0x53,
    
    readSignatureByte = 0x30,
    readCalibrationByte = 0x38,
    
    readLowFuseByte = 0x50,       readLowFuseByteArg2 = 0x00,
    readExtendedFuseByte = 0x50,  readExtendedFuseByteArg2 = 0x08,
    readHighFuseByte = 0x58,      readHighFuseByteArg2 = 0x08,  
    readLockByte = 0x58,          readLockByteArg2 = 0x00,  
    
    readProgramMemory = 0x20,  
    loadExtendedAddressByte = 0x4D,
    
};  // end of enum

typedef struct {
   byte sig [3];
   char * desc;
   unsigned long flashSize;
   unsigned int baseBootSize;
} signatureType;

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
  { { 0x1E, 0x95, 0x0F }, "ATmega328P",  32 * kb,       512 },

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
  
  // Atmega32U2 family
  { { 0x1E, 0x93, 0x89 }, "ATmega8U2",    8 * kb,   512 },
  { { 0x1E, 0x94, 0x89 }, "ATmega16U2",  16 * kb,   512 },
  { { 0x1E, 0x95, 0x8A }, "ATmega32U2",  32 * kb,   512 },

  // Atmega32U4 family
  { { 0x1E, 0x94, 0x88 }, "ATmega16U4",  16 * kb,   512 },
  { { 0x1E, 0x95, 0x87 }, "ATmega32U4",  32 * kb,   512 },
  
  // ATmega1284P family
  { { 0x1E, 0x97, 0x05 }, "ATmega1284P", 128 * kb,   1 * kb },
  
  // ATtiny4313 family
  { { 0x1E, 0x91, 0x0A }, "ATtiny2313A", 2 * kb,   0 },
  { { 0x1E, 0x92, 0x0D }, "ATtiny4313",  4 * kb,   0 },
  
  // ATtiny13 family
  { { 0x1E, 0x90, 0x07 }, "ATtiny13A",   1 * kb,   0 },
  
  // Atmega8A family
  { { 0x1E, 0x93, 0x07 }, "ATmega8A",    8 * kb, 256 },
  
  };  // end of signatures

// if signature found in above table, this is its index
int foundSig = -1;
byte lastAddressMSB = 0;

// execute one programming instruction ... b1 is command, b2, b3, b4 are arguments
//  processor may return a result on the 4th transfer, this is returned.
byte program (const byte b1, const byte b2 = 0, const byte b3 = 0, const byte b4 = 0)
  {
  SPI.transfer (b1);  
  SPI.transfer (b2);  
  SPI.transfer (b3);  
  return SPI.transfer (b4);  
  } // end of program
  
byte readFlash (unsigned long addr)
  {
  
  // set the extended (most significant) address byte if necessary
  byte MSB = (addr >> 16) & 0xFF;
  if (MSB != lastAddressMSB)
    {
    program (loadExtendedAddressByte, 0, MSB); 
    lastAddressMSB = MSB;
    }  // end if different MSB
     
  byte high = (addr & 1) ? 0x08 : 0;  // set if high byte wanted
  addr >>= 1;  // turn into word address
  return program (readProgramMemory | high, highByte (addr), lowByte (addr));
  } // end of readFlash
  
void showHex (const byte b, const boolean newline = false)
  {
  // try to avoid using sprintf
  char buf [4] = { ((b >> 4) & 0x0F) | '0', (b & 0x0F) | '0', ' ' , 0 };
  if (buf [0] > '9')
    buf [0] += 7;
  if (buf [1] > '9')
    buf [1] += 7;
  Serial.print (buf);
  if (newline)
    Serial.println ();
  }  // end of showHex 
  
void showYesNo (const boolean b, const boolean newline = false)
  {
  if (b)
    Serial.print ("Yes");
  else
    Serial.print ("No");
  if (newline)
    Serial.println ();
  }  // end of showYesNo 
  
void readBootloader ()
  {
  unsigned long addr;
  unsigned int  len;
  byte hFuse = program (readHighFuseByte, readHighFuseByteArg2);
  
  if (signatures [foundSig].baseBootSize == 0)
    {
    Serial.println ("No bootloader support.");
    return;  
    }
    
  addr = signatures [foundSig].flashSize;
  len = signatures [foundSig].baseBootSize;
  
  Serial.print ("Bootloader in use: ");
  showYesNo ((hFuse & _BV (0)) == 0, true);
  Serial.print ("EEPROM preserved through erase: ");
  showYesNo ((hFuse & _BV (3)) == 0, true);
  Serial.print ("Watchdog timer always on: ");
  showYesNo ((hFuse & _BV (4)) == 0, true);
  
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
  
  Serial.print ("Bootloader is ");
  Serial.print (len);
  Serial.print (" bytes starting at ");
  Serial.println (addr, HEX);
  Serial.println ();
  Serial.println ("Bootloader:");
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
  Serial.print ("MD5 sum of bootloader = ");
  
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
  
void readProgram ()
  {
  unsigned long addr = 0;
  unsigned int  len = 256;
  Serial.println ();
  Serial.println ("First 256 bytes of program memory:");
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

  } // end of readProgram
  
void startProgramming ()
  {
  byte confirm;
  pinMode (RESET, OUTPUT);
  pinMode (SCK, OUTPUT);
  
  // we are in sync if we get back programAcknowledge on the third byte
  do 
    {
    delay (100);
    // ensure SCK low
    digitalWrite (SCK, LOW);
    // then pulse reset, see page 309 of datasheet
    digitalWrite (RESET, HIGH);
    delay (1);  // pulse for at least 2 clock cycles
    digitalWrite (RESET, LOW);
    delay (25);  // wait at least 20 mS
    SPI.transfer (progamEnable);  
    SPI.transfer (programAcknowledge);  
    confirm = SPI.transfer (0);  
    SPI.transfer (0);  
    } while (confirm != programAcknowledge);
    
  Serial.println ("Entered programming mode OK.");
  }  // end of startProgramming

void getSignature ()
  {
  byte sig [3];
  Serial.print ("Signature = ");
  for (byte i = 0; i < 3; i++)
    {
    sig [i] = program (readSignatureByte, 0, i); 
    showHex (sig [i]);
    }
  Serial.println ();
  
  for (int j = 0; j < NUMITEMS (signatures); j++)
    {
    if (memcmp (sig, signatures [j].sig, sizeof sig) == 0)
      {
      foundSig = j;
      Serial.print ("Processor = ");
      Serial.println (signatures [j].desc);
      Serial.print ("Flash memory size = ");
      Serial.println (signatures [j].flashSize, DEC);
      return;
      }
    }

  Serial.println ("Unrecogized signature.");  
  }  // end of getSignature

void getFuseBytes ()
  {
  Serial.print ("LFuse = ");
  showHex (program (readLowFuseByte, readLowFuseByteArg2), true);
  Serial.print ("HFuse = ");
  showHex (program (readHighFuseByte, readHighFuseByteArg2), true);
  Serial.print ("EFuse = ");
  showHex (program (readExtendedFuseByte, readExtendedFuseByteArg2), true);
  Serial.print ("Lock byte = ");
  showHex (program (readLockByte, readLockByteArg2), true);
  Serial.print ("Clock calibration = ");
  showHex (program (readCalibrationByte), true);
  }  // end of getFuseBytes

void setup ()
  {
  Serial.begin (115200);
  while (!Serial) ;  // for Leonardo, Micro etc.
  Serial.println ();
  Serial.println ("Atmega chip detector.");
 
  digitalWrite(RESET, HIGH);  // ensure SS stays high for now
  SPI.begin ();
  
  // slow down SPI for benefit of slower processors like the Attiny
  SPI.setClockDivider(SPI_CLOCK_DIV64);

  pinMode (CLOCKOUT, OUTPUT);
  
  // set up Timer 1
  TCCR1A = _BV (COM1A0);  // toggle OC1A on Compare Match
  TCCR1B = _BV(WGM12) | _BV(CS10);   // CTC, no prescaling
  OCR1A =  0;       // output every cycle
  
  startProgramming ();
  getSignature ();
  getFuseBytes ();
  
  if (foundSig != -1)
    {
    readBootloader ();
    }
    
  readProgram ();
 }  // end of setup

void loop () {}

