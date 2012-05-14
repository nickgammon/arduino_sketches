// Atmega hex file uploader (from SD card)
// Author: Nick Gammon
// Date: 11th May 2012
// Version: 1.4     // NB update 'Version' variable below!

// Version 1.1: Some code cleanups as suggested on the Arduino forum.
// Version 1.2: Cleared temporary flash area to 0xFF before doing each page
// Version 1.3: Added ability to read from flash and write to disk, also to erase flash
// Version 1.4: Slowed down bit-bang SPI to make it more reliable on slower processors

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

const char Version [] = "1.4";

// bit banged SPI pins
const byte MSPIM_SCK = 4;  // port D bit 4
const byte MSPIM_SS  = 5;  // port D bit 5
const byte BB_MISO   = 6;  // port D bit 6
const byte BB_MOSI   = 7;  // port D bit 7

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

// for fast port access (Atmega328)
#define BB_MISO_PORT PIND
#define BB_MOSI_PORT PORTD
#define BB_SCK_PORT PORTD
const byte BB_SCK_BIT = 4;
const byte BB_MISO_BIT = 6;
const byte BB_MOSI_BIT = 7;

// control speed of programming
const byte BB_DELAY_MICROSECONDS = 4;


// target board reset goes to here
const byte RESET = MSPIM_SS;

// SD chip select pin
const uint8_t chipSelect = SS;

const unsigned long NO_PAGE = 0xFFFFFFFF;
const int MAX_FILENAME = 13;

// actions to take
enum {
    checkFile,
    verifyFlash,
    writeToFlash,
};

// file system object
SdFat sd;

// copy of fuses/lock bytes found for this processor
byte fuses [4];

// meaning of bytes in above array
enum {
      lowFuse,
      highFuse,
      extFuse,
      lockByte
};

// structure to hold signature and other relevant data about each chip
typedef struct {
   byte sig [3];
   char * desc;
   unsigned long flashSize;
   unsigned int baseBootSize;
   unsigned long pageSize;     // bytes
   byte fuseWithBootloaderSize;  // ie. one of: lowFuse, highFuse, extFuse
} signatureType;

const unsigned long kb = 1024;
const byte NO_FUSE = 0xFF;


// see Atmega datasheets
const signatureType signatures [] = 
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

  // ATmega1284P family
  { { 0x1E, 0x97, 0x05 }, "ATmega1284P", 256 * kb,   1 * kb,   256,  highFuse  },
  
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
    hexStartSegmentAddressRecord  // 03
};

// Bit Banged SPI transfer
byte BB_SPITransfer (byte c)
{       
  byte bit;
   
  for (bit = 0; bit < 8; bit++) 
    {
    // write MOSI on falling edge of previous clock
    if (c & 0x80)
        BB_MOSI_PORT |= _BV (BB_MOSI_BIT);
    else
        BB_MOSI_PORT &= ~_BV (BB_MOSI_BIT);
    c <<= 1;
 
    // read MISO
    c |= (BB_MISO_PORT & _BV (BB_MISO_BIT)) != 0;
 
   // clock high
    BB_SCK_PORT |= _BV (BB_SCK_BIT);
 
    // delay between rise and fall of clock
    delayMicroseconds (BB_DELAY_MICROSECONDS);
 
    // clock low
    BB_SCK_PORT &= ~_BV (BB_SCK_BIT);
    }
   
  return c;
  }  // end of BB_SPITransfer 


// if signature found in above table, this is its index
int foundSig = -1;
byte lastAddressMSB = 0;


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
  while ((program (pollReady) & 1) == 1)
    {}  // wait till ready
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
  unsigned int len = signatures [foundSig].pageSize;
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
      lowestAddress = min (lowestAddress, addr + extendedAddress);
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
      
    // ignore this, who cares?
    case hexStartSegmentAddressRecord:
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

  pagesize = signatures [foundSig].pageSize;
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
    if (memcmp (sig, signatures [j].sig, sizeof sig) == 0)
      {
      foundSig = j;
      Serial.print (F("Processor = "));
      Serial.println (signatures [j].desc);
      Serial.print (F("Flash memory size = "));
      Serial.print (signatures [j].flashSize, DEC);
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
  
  Serial.print (F("LFuse = "));
  showHex (fuses [lowFuse], true);
  Serial.print (F("HFuse = "));
  showHex (fuses [highFuse], true);
  Serial.print (F("EFuse = "));
  showHex (fuses [extFuse], true);
  Serial.print (F("Lock byte = "));
  showHex (fuses [lockByte], true);
  }  // end of getFuseBytes

void showFuseName (const byte which)
  {
  switch (which)
    {
    case lowFuse:   Serial.print (F("low"));      break;
    case highFuse:  Serial.print (F("high"));     break;
    case extFuse:   Serial.print (F("extended")); break;
    case lockByte:  Serial.print (F("lock"));     break;
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
  
  byte fusenumber = signatures [foundSig].fuseWithBootloaderSize;
  
  // if no fuse, can't change it
  if (fusenumber == NO_FUSE)
    {
    Serial.println (F("No bootloader fuse."));  
    return false;  // ok return
    }
    
  addr = signatures [foundSig].flashSize;
  len = signatures [foundSig].baseBootSize;
    
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

//------------------------------------------------------------------------------
//      SETUP
//------------------------------------------------------------------------------
void setup () 
  {
  Serial.begin(115200);

  Serial.println ();
  Serial.println ();
  Serial.println (F("Atmega hex file uploader."));
  Serial.println (F("Written by Nick Gammon."));
  Serial.print   (F("Version "));
  Serial.println (Version);
  
  // set up 8 MHz timer on pin 9
  pinMode (CLOCKOUT, OUTPUT); 
  // set up Timer 1
  TCCR1A = _BV (COM1A0);  // toggle OC1A on Compare Match
  TCCR1B = _BV(WGM12) | _BV(CS10);   // CTC, no prescaling
  OCR1A =  0;       // output every cycle
  
  Serial.println (F("Reading SD card ..."));

  // initialize the SD card at SPI_HALF_SPEED to avoid bus errors with
  // breadboards.  use SPI_FULL_SPEED for better performance.
  if (!sd.begin (chipSelect, SPI_HALF_SPEED)) 
    sd.initErrorHalt();

  // list files in root directory
  
  SdFile file;
  char name[MAX_FILENAME];
  
  Serial.println ();  
  Serial.println (F("HEX files in root directory:"));  
  Serial.println ();  
  
  // open next file in root.  The volume working directory, vwd, is root
  while (file.openNext(sd.vwd(), O_READ)) {
    file.getFilename(name);
    byte len = strlen (name);
    if (len > 4 && strcmp (&name [len - 4], ".HEX") == 0)
      {
      Serial.print (name);
      Serial.print (F(" : "));
      Serial.print (file.fileSize (), DEC);
      Serial.print (F(" bytes."));
      
      dir_t d;
      if (!file.dirEntry(&d)) 
        Serial.println(F("Failed to find file date/time."));
      else
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
  
}  // end of setup

char lastFileName [MAX_FILENAME] = { 0 };


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
  
  // check file would fit into device memory
  if (highestAddress > signatures [foundSig].flashSize)
    {
    Serial.print (F("Highest address of 0x"));      
    Serial.print (highestAddress, HEX); 
    Serial.print (F(" exceeds available flash memory top 0x"));      
    Serial.println (signatures [foundSig].flashSize, HEX); 
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
  
void readFlashContents ()
  {
  progressBarCount = 0;
  pagesize = signatures [foundSig].pageSize;
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
  
    getline (name, sizeof name);
      
    if (strcmp (name, "YES") == 0)
      break;
      
    }  // end of checking if file exists
    
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
    
  for (unsigned long address = 0; address < signatures [foundSig].flashSize; address += sizeof memBuf)
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
  
void writeFlashContents ()
  {
  if (chooseInputFile ())
    return;  

  // now commit to flash
  readHexFile(name, writeToFlash);

  // verify anyway
  readHexFile(name, verifyFlash);

  // now fix up fuses so we can boot    
  updateFuses (true);
    
  }  // end of writeFlashContents
  
void verifyFlashContents ()
  {
  if (chooseInputFile ())
    return;  
    
  // verify it
  readHexFile(name, verifyFlash);
  }  // end of verifyFlashContents
  
void eraseFlashContents ()
  {
  Serial.println (F("Erase all flash memory. ARE YOU SURE? Type 'YES' to confirm ..."));  

  getline (name, sizeof name);
    
  if (strcmp (name, "YES") != 0)
    {
    Serial.println (F("Flash not erased."));  
    return;
    }
    
  Serial.println (F("Erasing chip ..."));
  program (progamEnable, chipErase);   // erase it
  pollUntilReady (); 

  Serial.println (F("Flash memory erased."));
    
  }  // end of eraseFlashContents

  
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
    while  (true)
      {}
    }  // end of no signature

 // ask for verify or write
  Serial.println (F("Actions:"));
  Serial.println (F(" [E] erase flash"));
  Serial.println (F(" [R] read from flash (save to disk)"));
  Serial.println (F(" [V] verify flash (compare to disk)"));
  Serial.println (F(" [W] write to flash (read from disk)"));
  Serial.println (F("Enter action:"));
  
  // discard any old junk
  while (Serial.available ())
    Serial.read ();
  
  char command;
  do
    {
    command = toupper (Serial.read ());
    } while (!isalpha (command));

  switch (command)
    {
    case 'R': 
      readFlashContents (); 
      break; 
      
    case 'W': 
      writeFlashContents (); 
      break; 
      
    case 'V': 
      verifyFlashContents (); 
      break; 
      
    case 'E': 
      eraseFlashContents (); 
      break; 
    
    default: 
      Serial.println (F("Unknown command.")); 
      break;
    }  // end of switch on command

}  // end of loop

