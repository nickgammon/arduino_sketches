// Atmega hex file uploader (from SD card)
// Author: Nick Gammon
// Date: 22nd May 2012
// Version: 1.23     // NB update 'Version' variable below!

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

const bool allowTargetToRun = true;  // if true, programming lines are freed when not programming

#define ALLOW_MODIFY_FUSES true   // make false if this sketch doesn't fit into memory
#define ALLOW_FILE_SAVING true    // make false if this sketch doesn't fit into memory
#define SAFETY_CHECKS true        // check for disabling SPIEN, or enabling RSTDISBL

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

// for SDFat library see: http://code.google.com/p/beta-lib/downloads/list
#include <SdFat.h>

#include <avr/eeprom.h>

// #include <memdebug.h>

const char Version [] = "1.23";

// bit banged SPI pins
#ifdef __AVR_ATmega2560__
  // Atmega2560
  const byte MSPIM_SCK = 4;  // port G bit 5
  const byte MSPIM_SS  = 5;  // port E bit 3
  const byte BB_MISO   = 6;  // port H bit 3
  const byte BB_MOSI   = 7;  // port H bit 4
#else
  // Atmega328
  const byte MSPIM_SCK = 4;  // port D bit 4
  const byte MSPIM_SS  = 5;  // port D bit 5
  const byte BB_MISO   = 6;  // port D bit 6
  const byte BB_MOSI   = 7;  // port D bit 7
#endif

// 8 MHz clock on this pin
const byte CLOCKOUT = 9;

/*

Connect target processor like this:

  D4: (SCK)   --> SCK as per datasheet
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
  #define BB_SCK_PORT PORTG
  const byte BB_SCK_BIT = 5;
  const byte BB_MISO_BIT = 3;
  const byte BB_MOSI_BIT = 4;
#else
  // Atmega328
  #define BB_MISO_PORT PIND
  #define BB_MOSI_PORT PORTD
  #define BB_SCK_PORT PORTD
  const byte BB_SCK_BIT = 4;
  const byte BB_MISO_BIT = 6;
  const byte BB_MOSI_BIT = 7;
#endif

// control speed of programming
const byte BB_DELAY_MICROSECONDS = 4;


// target board reset goes to here
const byte RESET = MSPIM_SS;

// SD chip select pin
const uint8_t chipSelect = SS;

const unsigned long NO_PAGE = 0xFFFFFFFF;
const int MAX_FILENAME = 13;
const int LAST_FILENAME_LOCATION_IN_EEPROM = 0;

// actions to take
enum {
    checkFile,
    verifyFlash,
    writeToFlash,
};

// file system object
SdFat sd;

// copy of fuses/lock bytes found for this processor
byte fuses [5];

// meaning of bytes in above array
enum {
      lowFuse,
      highFuse,
      extFuse,
      lockByte,
      calibrationByte
};

// structure to hold signature and other relevant data about each chip
typedef struct {
   byte sig [3];
   const char * desc;
   unsigned long flashSize;
   unsigned int baseBootSize;
   unsigned long pageSize;     // bytes
   byte fuseWithBootloaderSize;  // ie. one of: lowFuse, highFuse, extFuse
   byte timedWrites;    // if pollUntilReady won't work by polling the chip
} signatureType;

const unsigned long kb = 1024;
const byte NO_FUSE = 0xFF;


// see Atmega datasheets
const signatureType signatures [] PROGMEM = 
  {
//     signature        description   flash size   bootloader  flash  fuse
//                                                     size    page    to
//                                                             size   change

  // Attiny84 family
  { { 0x1E, 0x91, 0x0B }, "ATtiny24",   2 * kb,           0,   32,   NO_FUSE },
  { { 0x1E, 0x92, 0x07 }, "ATtiny44",   4 * kb,           0,   64,   NO_FUSE },
  { { 0x1E, 0x93, 0x0C }, "ATtiny84",   8 * kb,           0,   64,   NO_FUSE },

  // Attiny85 family
  { { 0x1E, 0x91, 0x08 }, "ATtiny25",   2 * kb,           0,   32,   NO_FUSE },
  { { 0x1E, 0x92, 0x06 }, "ATtiny45",   4 * kb,           0,   64,   NO_FUSE },
  { { 0x1E, 0x93, 0x0B }, "ATtiny85",   8 * kb,           0,   64,   NO_FUSE },

  // Atmega328 family
  { { 0x1E, 0x92, 0x0A }, "ATmega48PA",   4 * kb,         0,    64,  NO_FUSE },
  { { 0x1E, 0x93, 0x0F }, "ATmega88PA",   8 * kb,       256,   128,  extFuse },
  { { 0x1E, 0x94, 0x0B }, "ATmega168PA", 16 * kb,       256,   128,  extFuse },
  { { 0x1E, 0x95, 0x0F }, "ATmega328P",  32 * kb,       512,   128,  highFuse },

  // Atmega644 family
  { { 0x1E, 0x94, 0x0A }, "ATmega164P",   16 * kb,      256,   128,  highFuse },
  { { 0x1E, 0x95, 0x08 }, "ATmega324P",   32 * kb,      512,   128,  highFuse },
  { { 0x1E, 0x96, 0x0A }, "ATmega644P",   64 * kb,   1 * kb,   256,  highFuse },

  // Atmega2560 family
  { { 0x1E, 0x96, 0x08 }, "ATmega640",    64 * kb,   1 * kb,   256,  highFuse },
  { { 0x1E, 0x97, 0x03 }, "ATmega1280",  128 * kb,   1 * kb,   256,  highFuse },
  { { 0x1E, 0x97, 0x04 }, "ATmega1281",  128 * kb,   1 * kb,   256,  highFuse },
  { { 0x1E, 0x98, 0x01 }, "ATmega2560",  256 * kb,   1 * kb,   256,  highFuse },
      
  { { 0x1E, 0x98, 0x02 }, "ATmega2561",  256 * kb,   1 * kb,   256,  highFuse },
  
  // Atmega32U2 family
  { { 0x1E, 0x93, 0x89 }, "ATmega8U2",    8 * kb,       512,   128,  highFuse  },
  { { 0x1E, 0x94, 0x89 }, "ATmega16U2",  16 * kb,       512,   128,  highFuse  },
  { { 0x1E, 0x95, 0x8A }, "ATmega32U2",  32 * kb,       512,   128,  highFuse  },

  // Atmega32U4 family -  (datasheet is wrong about flash page size being 128 words)
  { { 0x1E, 0x94, 0x88 }, "ATmega16U4",  16 * kb,       512,   128,  highFuse },
  { { 0x1E, 0x95, 0x87 }, "ATmega32U4",  32 * kb,       512,   128,  highFuse },

  // ATmega1284P family
  { { 0x1E, 0x97, 0x05 }, "ATmega1284P", 128 * kb,   1 * kb,   256,  highFuse  },
  
  // ATtiny4313 family
  { { 0x1E, 0x91, 0x0A }, "ATtiny2313A",   2 * kb,        0,    32,  NO_FUSE  },
  { { 0x1E, 0x92, 0x0D }, "ATtiny4313",    4 * kb,        0,    64,  NO_FUSE  },

  // ATtiny13 family
  { { 0x1E, 0x90, 0x07 }, "ATtiny13A",     1 * kb,        0,    32,  NO_FUSE },
 
   // Atmega8A family
  { { 0x1E, 0x93, 0x07 }, "ATmega8A",      8 * kb,      256,    64,  highFuse, true },

  };  // end of signatures

char name[MAX_FILENAME] = { 0 };  // current file name

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
       
// number of items in an array
#define NUMITEMS(arg) ((unsigned int) (sizeof (arg) / sizeof (arg [0])))

// programming commands to send via SPI to the chip
enum {
    progamEnable = 0xAC,
    
      // writes are preceded by progamEnable
      chipErase = 0x80,
      writeLockByte = 0xE0,
      writeLowFuseByte = 0xA0,
      writeHighFuseByte = 0xA8,
      writeExtendedFuseByte = 0xA4,
    
    pollReady = 0xF0,
    
    programAcknowledge = 0x53,
    
    readSignatureByte = 0x30,
    readCalibrationByte = 0x38,
    
    readLowFuseByte = 0x50,       readLowFuseByteArg2 = 0x00,
    readExtendedFuseByte = 0x50,  readExtendedFuseByteArg2 = 0x08,
    readHighFuseByte = 0x58,      readHighFuseByteArg2 = 0x08,  
    readLockByte = 0x58,          readLockByteArg2 = 0x00,  
    
    readProgramMemory = 0x20,  
    writeProgramMemory = 0x4C,
    loadExtendedAddressByte = 0x4D,
    loadProgramMemory = 0x40,
    
};  // end of enum

// which program instruction writes which fuse
const byte fuseCommands [4] = { writeLowFuseByte, writeHighFuseByte, writeExtendedFuseByte, writeLockByte };

// types of record in .hex file
enum {
    hexDataRecord,  // 00
    hexEndOfFile,   // 01
    hexExtendedSegmentAddressRecord, // 02
    hexStartSegmentAddressRecord,  // 03
    hexExtendedLinearAddressRecord, // 04
    hexStartLinearAddressRecord // 05
};

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


// if signature found in above table, this is its index
int foundSig = -1;
byte lastAddressMSB = 0;
// copy of current signature entry for matching processor
signatureType currentSignature;
boolean haveSDcard;

// execute one programming instruction ... b1 is command, b2, b3, b4 are arguments
//  processor may return a result on the 4th transfer, this is returned.
byte program (const byte b1, const byte b2 = 0, const byte b3 = 0, const byte b4 = 0)
  {
  noInterrupts ();
  BB_SPITransfer (b1);  
  BB_SPITransfer (b2);  
  BB_SPITransfer (b3);  
  byte b = BB_SPITransfer (b4);  
  interrupts ();
  return b;
  } // end of program
  
// read a byte from flash memory
byte readFlash (unsigned long addr)
  {
  byte high = (addr & 1) ? 0x08 : 0;  // set if high byte wanted
  addr >>= 1;  // turn into word address

  // set the extended (most significant) address byte if necessary
  byte MSB = (addr >> 16) & 0xFF;
  if (MSB != lastAddressMSB)
    {
    program (loadExtendedAddressByte, 0, MSB); 
    lastAddressMSB = MSB;
    }  // end if different MSB

  return program (readProgramMemory | high, highByte (addr), lowByte (addr));
  } // end of readFlash
  
// write a byte to the flash memory buffer (ready for committing)
byte writeFlash (unsigned long addr, const byte data)
  {
  byte high = (addr & 1) ? 0x08 : 0;  // set if high byte wanted
  addr >>= 1;  // turn into word address
  program (loadProgramMemory | high, 0, lowByte (addr), data);
  } // end of writeFlash  
      
// show a byte in hex with leading zero and optional newline
void showHex (const byte b, const boolean newline = false, const boolean show0x = true)
  {
  if (show0x)
    Serial.print (F("0x"));
  char buf [4];
  sprintf (buf, "%02X ", b);
  Serial.print (buf);
  if (newline)
    Serial.println ();
  }  // end of showHex 
    
// convert two hex characters into a byte
//    returns true if error, false if OK
boolean hexConv (const char * (& pStr), byte & b)
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

// poll the target device until it is ready to be programmed
void pollUntilReady ()
  {
  if (currentSignature.timedWrites)
    delay (10);  // at least 2 x WD_FLASH which is 4.5 mS
  else
    {  
    while ((program (pollReady) & 1) == 1)
      {}  // wait till ready  
    }  // end of if
  }  // end of pollUntilReady

unsigned long pagesize;
unsigned long pagemask;
unsigned long oldPage;
unsigned int progressBarCount;

// show one progress symbol, wrap at 64 characters
void showProgress ()
  {
  if (progressBarCount++ % 64 == 0)
    Serial.println (); 
  Serial.print (F("#"));  // progress bar
  }  // end of showProgress
  
// clear entire temporary page to 0xFF in case we don't write to all of it 
void clearPage ()
{
  unsigned int len = currentSignature.pageSize;
  for (int i = 0; i < len; i++)
    writeFlash (i, 0xFF);
}  // end of clearPage
  
// commit page to flash memory
void commitPage (unsigned long addr)
  {
  addr >>= 1;  // turn into word address
  
  // set the extended (most significant) address byte if necessary
  byte MSB = (addr >> 16) & 0xFF;
  if (MSB != lastAddressMSB)
    {
    program (loadExtendedAddressByte, 0, MSB); 
    lastAddressMSB = MSB;
    }  // end if different MSB
    
  showProgress ();
  
  program (writeProgramMemory, highByte (addr), lowByte (addr));
  pollUntilReady (); 
  
  clearPage();  // clear ready for next page full
  }  // end of commitPage
 
// write data to temporary buffer, ready for committing  
void writeData (const unsigned long addr, const byte * pData, const int length)
  {
  // write each byte
  for (int i = 0; i < length; i++)
    {
    unsigned long thisPage = (addr + i) & pagemask;
    // page changed? commit old one
    if (thisPage != oldPage && oldPage != NO_PAGE)
      commitPage (oldPage);
    // now this is the current page
    oldPage = thisPage;
    // put byte into work buffer
    writeFlash (addr + i, pData [i]);
    }  // end of for
    
  }  // end of writeData
  
// count errors
unsigned int errors;
  
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
  
boolean gotEndOfFile;
unsigned long extendedAddress;

unsigned long lowestAddress;
unsigned long highestAddress;
unsigned long bytesWritten;
unsigned int lineCount;

/*
Line format:

  :nnaaaatt(data)ss
  
  Where:
  :      = a colon
  
  (All of below in hex format)
  
  nn     = length of data part
  aaaa   = address (eg. where to write data)
  tt     = transaction type
           00 = data
           01 = end of file
           02 = extended segment address (changes high-order byte of the address)
           03 = start segment address *
           04 = linear address *
           05 = start linear address *
  (data) = variable length data
  ss     = sumcheck

            * We don't use these
   
*/

boolean processLine (const char * pLine, const byte action)
  {
  if (*pLine++ != ':')
     {
     Serial.println (F("Line does not start with ':' character.")); 
     return true;  // error
     } 
  
  const int maxHexData = 40;
  byte hexBuffer [maxHexData];
  int bytesInLine = 0;
  
  if (action == checkFile)
    if (lineCount++ % 40 == 0)
      showProgress ();
    
  // convert entire line from ASCII into binary
  while (isxdigit (*pLine))
    {
    // can't fit?
    if (bytesInLine >= maxHexData)
      {
      Serial.println (F("Line too long to process."));  
      return true;
      } // end if too long
      
    if (hexConv (pLine, hexBuffer [bytesInLine++]))
      return true;
    }  // end of while
    
  if (bytesInLine < 5)
    {
    Serial.println (F("Line too short."));
    return true;  
    } 

  // sumcheck it
  
  byte sumCheck = 0;
  for (int i = 0; i < (bytesInLine - 1); i++)
    sumCheck += hexBuffer [i];
    
  // 2's complement
  sumCheck = ~sumCheck + 1;
  
  // check sumcheck
  if (sumCheck != hexBuffer [bytesInLine - 1])
    {
    Serial.print (F("Sumcheck error. Expected: "));  
    showHex (sumCheck);
    Serial.print (F(", got: "));
    showHex (hexBuffer [bytesInLine - 1], true);
    return true;
    }
  
  // length of data (eg. how much to write to memory)
  byte len = hexBuffer [0];
  
  // the data length should be the number of bytes, less
  //   length / address (2) / transaction type / sumcheck
  if (len != (bytesInLine - 5))
    {
    Serial.print (F("Line not expected length. Expected "));
    Serial.print (len, DEC);
    Serial.print (F(" bytes, got "));
    Serial.print (bytesInLine - 5, DEC);
    Serial.println (F(" bytes."));
    return true;
    }
    
  // two bytes of address
  unsigned long addrH = hexBuffer [1];
  unsigned long addrL = hexBuffer [2];
  
  unsigned long addr = addrL | (addrH << 8);
  
  byte recType = hexBuffer [3];

  switch (recType)
    {
    // stuff to be written to memory
    case hexDataRecord:
      lowestAddress  = min (lowestAddress, addr + extendedAddress);
      highestAddress = max (lowestAddress, addr + extendedAddress + len - 1);
      bytesWritten += len;
    
      switch (action)
        {
        case checkFile:  // nothing much to do, we do the checks anyway
          break;
          
        case verifyFlash:
          verifyData (addr + extendedAddress, &hexBuffer [4], len);
          break;
        
        case writeToFlash:
          writeData (addr + extendedAddress, &hexBuffer [4], len);
          break;      
        } // end of switch on action
      break;
  
    // end of data
    case hexEndOfFile:
      gotEndOfFile = true;
      break;
  
    // we are setting the high-order byte of the address
    case hexExtendedSegmentAddressRecord: 
      extendedAddress = ((unsigned long) hexBuffer [4]) << 12;
      break;
      
    // ignore these, who cares?
    case hexStartSegmentAddressRecord:
    case hexExtendedLinearAddressRecord:
    case hexStartLinearAddressRecord:
      break;
        
    default:  
      Serial.print (F("Cannot handle record type: "));
      Serial.println (recType, DEC);
      return true;  
    }  // end of switch on recType
    
  return false;
  } // end of processLine
  
//------------------------------------------------------------------------------
boolean readHexFile (const char * fName, const byte action)
  {
  const int maxLine = 80;
  char buffer[maxLine];
  ifstream sdin (fName);
  int lineNumber = 0;
  gotEndOfFile = false;
  extendedAddress = 0;
  errors = 0;
  lowestAddress = 0xFFFFFFFF;
  highestAddress = 0;
  bytesWritten = 0;
  progressBarCount = 0;

  pagesize = currentSignature.pageSize;
  pagemask = ~(pagesize - 1);
  oldPage = NO_PAGE;

  Serial.print (F("Processing file: "));
  Serial.println (fName);

  // check for open error
  if (!sdin.is_open()) 
    {
    Serial.println (F("Could not open file."));
    return true;
    }

  switch (action)
    {
    case checkFile:
      Serial.println (F("Checking file ..."));
      break;
      
    case verifyFlash:
      Serial.println (F("Verifying flash ..."));
      break;
    
    case writeToFlash:
      Serial.println (F("Erasing chip ..."));
      program (progamEnable, chipErase);   // erase it
      delay (20);  // for Atmega8
      pollUntilReady (); 
      clearPage();  // clear temporary page
      Serial.println (F("Writing flash ..."));
      break;      
    } // end of switch
 
  while (sdin.getline (buffer, maxLine))
    {
    lineNumber++;
    int count = sdin.gcount();
    if (sdin.fail()) 
      {
      Serial.print (F("Line "));
      Serial.println (lineNumber);
      Serial.print (F(" too long."));
      return true;
      }  // end of fail (line too long?)
      
    // ignore empty lines
    if (count > 1)
      {
      if (processLine (buffer, action))
        {
        Serial.print (F("Error in line "));
        Serial.println (lineNumber);
        return true;  // error
        }
      }
    }    // end of while each line
    
  if (!gotEndOfFile)
    {
    Serial.println (F("Did not get 'end of file' record."));
    return true;
    }

  switch (action)
    {
    case writeToFlash:
      // commit final page
      if (oldPage != NO_PAGE)
        commitPage (oldPage);
      Serial.println ();   // finish line of dots
      Serial.println (F("Written."));
      break;
      
    case verifyFlash:
       Serial.println ();   // finish line of dots
       if (errors == 0)
          Serial.println (F("No errors found."));
        else
          {
          Serial.print (errors, DEC);
          Serial.println (F(" verification error(s)."));
          if (errors > 100)
            Serial.println (F("First 100 shown."));
          }  // end if
       break;
        
    case checkFile:
      Serial.println ();   // finish line of dots
      Serial.print (F("Lowest address  = 0x"));
      Serial.println (lowestAddress, HEX);
      Serial.print (F("Highest address = 0x"));
      Serial.println (highestAddress, HEX);
      Serial.print (F("Bytes to write  = "));
      Serial.println (bytesWritten, DEC);
      break;
        
    }  // end of switch
  
  return false;
}  // end of readHexFile

void startProgramming ()
  {
  Serial.println (F("Attempting to enter programming mode ..."));
    
  byte confirm;
  pinMode (RESET, OUTPUT);
  digitalWrite (MSPIM_SCK, LOW);
  pinMode (MSPIM_SCK, OUTPUT);
  pinMode (BB_MOSI, OUTPUT);
  
  // we are in sync if we get back programAcknowledge on the third byte
  do 
    {
    // regrouping pause
    delay (100);

    // ensure SCK low
    noInterrupts ();
    digitalWrite (MSPIM_SCK, LOW);
    // then pulse reset, see page 309 of datasheet
    digitalWrite (RESET, HIGH);
    delayMicroseconds (10);  // pulse for at least 2 clock cycles
    digitalWrite (RESET, LOW);
    interrupts ();

    delay (25);  // wait at least 20 mS
    noInterrupts ();
    BB_SPITransfer (progamEnable);  
    BB_SPITransfer (programAcknowledge);  
    confirm = BB_SPITransfer (0);  
    BB_SPITransfer (0);  
    interrupts ();
    } while (confirm != programAcknowledge);
    
  Serial.println (F("Entered programming mode OK."));
  }  // end of startProgramming

void stopProgramming ()
  {
  // turn off pull-ups
  digitalWrite (RESET, LOW);  
  digitalWrite (MSPIM_SCK, LOW);
  digitalWrite (BB_MOSI, LOW);
  digitalWrite (BB_MISO, LOW);
  
  // set everything back to inputs
  pinMode (RESET, INPUT);
  pinMode (MSPIM_SCK, INPUT);
  pinMode (BB_MOSI, INPUT);
  pinMode (BB_MISO, INPUT);
  
  Serial.println (F("Programming mode off."));
    
  } // end of startProgramming
  
void getSignature ()
  {
  foundSig = -1;
  lastAddressMSB = 0;
    
  byte sig [3];
  Serial.print (F("Signature = "));
  for (byte i = 0; i < 3; i++)
    {
    sig [i] = program (readSignatureByte, 0, i); 
    showHex (sig [i]);
    }  // end for each signature byte
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
      
      // make sure extended address is zero to match lastAddressMSB variable
      program (loadExtendedAddressByte, 0, 0); 
      return;
      }  // end of signature found
    }  // end of for each signature

  Serial.println (F("Unrecogized signature."));  
  }  // end of getSignature
  
void getFuseBytes ()
  {
  fuses [lowFuse]   = program (readLowFuseByte, readLowFuseByteArg2);
  fuses [highFuse]  = program (readHighFuseByte, readHighFuseByteArg2);
  fuses [extFuse]   = program (readExtendedFuseByte, readExtendedFuseByteArg2);
  fuses [lockByte]  = program (readLockByte, readLockByteArg2);
  fuses [calibrationByte]  = program (readCalibrationByte);
  
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
  
// write specified value to specified fuse/lock byte
void writeFuse (const byte newValue, const byte instruction)
  {
  if (newValue == 0)
    return;  // ignore
  
  program (progamEnable, instruction, 0, newValue);
  pollUntilReady (); 
  }  // end of writeFuse
  
boolean updateFuses (const boolean writeIt)
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
    writeFuse (fuses [fusenumber], fuseCommands [fusenumber]);      
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

void showDirectory ()
  {
  if (!haveSDcard)
    {
    Serial.println (F("*** No SD card detected."));
    return;
    }
    
  // list files in root directory
  
  SdFile file;
  char name[MAX_FILENAME];
  
  Serial.println ();  
  Serial.println (F("HEX files in root directory:"));  
  Serial.println ();  
  
  // back to start of directory
  sd.vwd()->rewind ();
  
  // open next file in root.  The volume working directory, vwd, is root
  while (file.openNext(sd.vwd(), O_READ)) {
    file.getFilename(name);
    byte len = strlen (name);
    if (len > 4 && strcmp (&name [len - 4], ".HEX") == 0)
      {
      Serial.print (name);
      for (byte i = strlen (name); i < 13; i++)
        Serial.write (' ');  // space out so dates line up
      Serial.print (F(" : "));
      char buf [12];
      sprintf (buf, "%10lu", file.fileSize ()); 
     
      Serial.print (buf);
      Serial.print (F(" bytes."));
      
      dir_t d;
      if (!file.dirEntry(&d)) 
        Serial.println(F("Failed to find file date/time."));
      else if (d.creationDate != FAT_DEFAULT_DATE)
        {
        Serial.print(F("  Created: "));
        file.printFatDate(&Serial, d.creationDate);
        Serial.print(F(" "));
        file.printFatTime(&Serial, d.creationTime);
        Serial.print(F(".  Modified: "));
        file.printFatDate(&Serial, d.lastWriteDate);
        Serial.print(F(" "));
        file.printFatTime(&Serial, d.lastWriteTime);
        }  // end of got date/time from directory
      Serial.println ();
      }
    file.close();
    }  // end of listing files
    
  }  // end of showDirectory
  
char lastFileName [MAX_FILENAME] = { 0 };
  
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
  
  // set up 8 MHz timer on pin 9
  pinMode (CLOCKOUT, OUTPUT); 
  // set up Timer 1
  TCCR1A = bit (COM1A0);  // toggle OC1A on Compare Match
  TCCR1B = bit (WGM12) | bit (CS10);   // CTC, no prescaling
  OCR1A =  0;       // output every cycle
  
  Serial.println (F("Reading SD card ..."));

  // initialize the SD card at SPI_HALF_SPEED to avoid bus errors with
  // breadboards.  use SPI_FULL_SPEED for better performance.
  if (!sd.begin (chipSelect, SPI_HALF_SPEED)) 
    {
    sd.initErrorPrint();
    haveSDcard = false;
    }
  else
    {
    haveSDcard = true;
    showDirectory ();
    }
  
//  Serial.print (F("Free memory = "));
//  Serial.println (getFreeMemory (), DEC);
  
  // find what filename they used last
  eeprom_read_block (&lastFileName, LAST_FILENAME_LOCATION_IN_EEPROM, MAX_FILENAME);
  lastFileName [MAX_FILENAME - 1] = 0;  // ensure terminating null
  
  // ensure file name valid
  for (byte i = 0; i < strlen (lastFileName); i++)
    {
    if (!isprint (lastFileName [i]))
      {
      lastFileName [0] = 0;
      break; 
      }  
    }
  
}  // end of setup



boolean chooseInputFile ()
  {
  Serial.println ();  
  Serial.print (F("Choose disk file [ "));
  Serial.print (lastFileName);
  Serial.println (F(" ] ..."));
  
  getline (name, sizeof name);

  // no name? use last one  
  if (name [0] == 0)
    memcpy (name, lastFileName, sizeof name);
  
  if (readHexFile(name, checkFile))
    {
    Serial.println (F("***********************************"));
    return true;  // error, don't attempt to write
    }
  
  // remember name for next time
  memcpy (lastFileName, name, sizeof lastFileName);
  
  char fileNameInEEPROM [MAX_FILENAME];
  eeprom_read_block (&fileNameInEEPROM, LAST_FILENAME_LOCATION_IN_EEPROM, MAX_FILENAME);
  fileNameInEEPROM [MAX_FILENAME - 1] = 0;  // ensure terminating null
  
  // save new file name if it changed from what we have saved
  if (strcmp (fileNameInEEPROM, lastFileName) != 0)
    eeprom_write_block (&lastFileName, LAST_FILENAME_LOCATION_IN_EEPROM, MAX_FILENAME);
  
  // check file would fit into device memory
  if (highestAddress > currentSignature.flashSize)
    {
    Serial.print (F("Highest address of 0x"));      
    Serial.print (highestAddress, HEX); 
    Serial.print (F(" exceeds available flash memory top 0x"));      
    Serial.println (currentSignature.flashSize, HEX); 
    Serial.println (F("***********************************"));
    return true; 
    }
  
  // check start address makes sense
  if (updateFuses (false))
    {
    Serial.println (F("***********************************"));
    return true;
    }
  
   return false;   
  }  // end of chooseInputFile
  
boolean getYesNo ()
  {
  char response [5];
  getline (response, sizeof response);
    
  return strcmp (response, "YES") == 0;
  }  // end of getYesNo
  
#if ALLOW_FILE_SAVING  
void readFlashContents ()
  {
  if (!haveSDcard)
    {
    Serial.println (F("*** No SD card detected."));
    return;
    }
    
  progressBarCount = 0;
  pagesize = currentSignature.pageSize;
  pagemask = ~(pagesize - 1);
  oldPage = NO_PAGE;
  byte lastMSBwritten = 0;
    
  while (true)
    {
    
    Serial.println ();  
    Serial.println (F("Choose file to save as: "));
    
    getline (name, sizeof name);
    int len = strlen (name);
      
    if (len < 5 || strcmp (&name [len - 4], ".HEX") != 0)
      {
      Serial.println (F("File name must end in .HEX"));  
      return;
      }
      
    // if file doesn't exist, proceed
    if (!sd.vwd()->exists (name))
      break;
      
    Serial.print (F("File "));
    Serial.print (name);    
    Serial.println (F(" exists. Overwrite? Type 'YES' to confirm ..."));  
  
    if (getYesNo ())
      break;
      
    }  // end of checking if file exists
  
  // ensure back in programming mode  
  startProgramming ();
  
  SdFile myFile;

  // open the file for writing
  if (!myFile.open(name, O_WRITE | O_CREAT | O_TRUNC)) 
    {
    Serial.print (F("Could not open file "));
    Serial.print (name);
    Serial.println (F(" for writing."));
    return;    
    }

  byte memBuf [16];
  boolean allFF;
  int i;
  char linebuf [50];
  byte sumCheck;
  
  Serial.println (F("Copying flash memory to SD card (disk) ..."));  
    
  for (unsigned long address = 0; address < currentSignature.flashSize; address += sizeof memBuf)
    {
      
    unsigned long thisPage = address & pagemask;
    // page changed? show progress
    if (thisPage != oldPage && oldPage != NO_PAGE)
      showProgress ();
    // now this is the current page
    oldPage = thisPage;
      
    // don't write lines that are all 0xFF
    allFF = true;
    
    for (i = 0; i < sizeof memBuf; i++)
      {
      memBuf [i] = readFlash (address + i); 
      if (memBuf [i] != 0xFF)
        allFF = false;
      }  // end of reading 16 bytes
    if (allFF)
      continue;

    byte MSB = address >> 16;
    if (MSB != lastMSBwritten)
      {
      sumCheck = 2 + 2 + (MSB << 4);
      sumCheck = ~sumCheck + 1;      
      // hexExtendedSegmentAddressRecord (02)
      sprintf (linebuf, ":02000002%02X00%02X\r\n", MSB << 4, sumCheck);    
      myFile.print (linebuf);   
      lastMSBwritten = MSB;
      }  // end if different MSB
      
    sumCheck = 16 + lowByte (address) + highByte (address);
    sprintf (linebuf, ":10%04X00", address & 0xFFFF);
    for (i = 0; i < sizeof memBuf; i++)
      {
      sprintf (&linebuf [(i * 2) + 9] , "%02X",  memBuf [i]);
      sumCheck += memBuf [i];
      }  // end of reading 16 bytes
  
    // 2's complement
    sumCheck = ~sumCheck + 1;
    // append sumcheck
    sprintf (&linebuf [(sizeof memBuf * 2) + 9] , "%02X\r\n",  sumCheck);
         
    myFile.clearWriteError ();
    myFile.print (linebuf);   
    if (myFile.getWriteError ())
       {
       Serial.println ();  // finish off progress bar
       Serial.println (F("Error writing file."));
       myFile.close ();
       return; 
       }   // end of an error
       
    }  // end of reading flash 
  
  Serial.println ();  // finish off progress bar
  myFile.print (":00000001FF\r\n");    // end of file record
  myFile.close ();  
  // ensure written to disk
  sd.vwd()->sync ();
  Serial.print (F("File "));
  Serial.print (name);
  Serial.println (F(" saved."));
  }  // end of readFlashContents
#endif

void writeFlashContents ()
  {
  if (!haveSDcard)
    {
    Serial.println (F("*** No SD card detected."));
    return;
    }
    
  if (chooseInputFile ())
    return;  

  // ensure back in programming mode  
  startProgramming ();

  // now commit to flash
  readHexFile(name, writeToFlash);

  // verify
  readHexFile(name, verifyFlash);

  // now fix up fuses so we can boot    
  updateFuses (true);
    
  }  // end of writeFlashContents
  
void verifyFlashContents ()
  {
  if (!haveSDcard)
    {
    Serial.println (F("*** No SD card detected."));
    return;
    }
    
  if (chooseInputFile ())
    return;  

  // ensure back in programming mode  
  startProgramming ();
    
  // verify it
  readHexFile(name, verifyFlash);
  }  // end of verifyFlashContents
  
void eraseFlashContents ()
  {
  Serial.println (F("Erase all flash memory. ARE YOU SURE? Type 'YES' to confirm ..."));  
    
  if (!getYesNo ())
    {
    Serial.println (F("Flash not erased."));  
    return;
    }
    
  // ensure back in programming mode  
  startProgramming ();
    
  Serial.println (F("Erasing chip ..."));
  program (progamEnable, chipErase);   // erase it
  delay (20);  // for Atmega8
  pollUntilReady (); 

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
  startProgramming ();
    
  // tell them what we are doing
  Serial.print (F("Changing "));
  showFuseName (fusenumber);
  Serial.println (F(" fuse ..."));
    
  // change it
  writeFuse (newValue, fuseCommands [fusenumber]);      
  
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

  startProgramming ();
  getSignature ();
  getFuseBytes ();
  
  // don't have signature? don't proceed
  if (foundSig == -1)
    {
    Serial.println (F("Halted."));
    while  (true)
      {}
    }  // end of no signature

 // ask for verify or write
  Serial.println (F("Actions:"));
  Serial.println (F(" [E] erase flash"));
#if ALLOW_MODIFY_FUSES    
  Serial.println (F(" [F] modify fuses"));
#endif
  if (haveSDcard)
    {
    Serial.println (F(" [L] list directory"));
#if ALLOW_FILE_SAVING    
    Serial.println (F(" [R] read from flash (save to disk)"));
#endif
    Serial.println (F(" [V] verify flash (compare to disk)"));
    Serial.println (F(" [W] write to flash (read from disk)"));
    }  // end of if SD card detected
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
    startProgramming ();
  
  switch (command)
    {
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
      
    case 'E': 
      eraseFlashContents (); 
      break; 
#if ALLOW_MODIFY_FUSES    
    case 'F': 
      modifyFuses (); 
      break; 
#endif

   case 'L':
      showDirectory ();
      break;
     
    default: 
      Serial.println (F("Unknown command.")); 
      break;
    }  // end of switch on command

}  // end of loop

