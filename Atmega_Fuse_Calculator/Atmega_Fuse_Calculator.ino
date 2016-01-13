// Atmega chip fuse caculator
// Author: Nick Gammon
// Date: 22nd May 2012
// Version: 1.11

// Version 1.1: Output an 8 MHz clock on pin 9
// Version 1.2: Corrected flash size for Atmega1284P.
// Version 1.3: Added signatures for ATtiny2313A, ATtiny4313, ATtiny13
// Version 1.4: Added signature for Atmega8A
//              Fixed bug in displaying bootloader size
// Version 1.5: Added signature for Atmega32U4
// Version 1.6: Allowed for running on the Leonardo, Micro, etc.
// Version 1.7: Fixed compiling problems under IDE 1.5.8
// Version 1.8: Cleaned up _BV () macro to use bit () macro instead for readability
// Version 1.9: Display message if cannot enter programming mode.
// Version 1.10: Added support for At90USB82, At90USB162
// Version 1.11: Got rid of compiler warnings in IDE 1.6.7

#define VERSION "1.11"

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

const byte CLOCKOUT = 9;
const byte RESET = 10;  // --> goes to reset on the target board

const int ENTER_PROGRAMMING_ATTEMPTS = 50;

#if ARDUINO < 100
  const byte SCK = 13;    // SPI clock
#endif

// stringification for Arduino IDE version
#define xstr(s) str(s)
#define str(s) #s

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

// handler for special things like bootloader size
typedef void (*specialHandlerFunction) (const byte val, const unsigned int bootLoaderSize);

// item for one piece of fuse information
typedef struct {
   byte whichFuse;
   byte mask;
   const char * meaningIfProgrammed;
   specialHandlerFunction specialHandler;
  } fuseMeaning;
   

// Messages stored in PROGMEM to save RAM

const char descExternalResetDisable    [] PROGMEM = "External Reset Disable";
const char descDebugWireEnable         [] PROGMEM = "Debug Wire Enable";
const char descSerialProgrammingEnable [] PROGMEM = "Enable Serial (ICSP) Programming";
const char descWatchdogTimerAlwaysOn   [] PROGMEM = "Watchdog Timer Always On";
const char descEEPROMsave              [] PROGMEM = "Preserve EEPROM through chip erase";
const char descBootIntoBootloader      [] PROGMEM = "Boot into bootloader";
const char descDivideClockBy8          [] PROGMEM = "Divide clock by 8";
const char descClockOutput             [] PROGMEM = "Clock output";
const char descSelfProgrammingEnable   [] PROGMEM = "Self Programming Enable";
const char descHardwareBootEnable      [] PROGMEM = "Hardare Boot Enable";
const char descOCDEnable               [] PROGMEM = "OCD Enable";
const char descJtagEnable              [] PROGMEM = "JTAG Enable";
const char descOscillatorOptions       [] PROGMEM = "Oscillator Options";
const char descBrownOutDetectorEnable  [] PROGMEM = "Brown out detector enable";
const char descBrownOutDetectorLevel   [] PROGMEM = "Brown out detector level";

// calculate size of bootloader
void fBootloaderSize (const byte val, const unsigned int bootLoaderSize)
  {
  Serial.print (F("Bootloader size: "));  
  unsigned int len = bootLoaderSize;
  switch (val & 3)
    {
    case 0: len *= 8; break;  
    case 1: len *= 4; break;  
    case 2: len *= 2; break;  
    case 3: len *= 1; break;  
    }  // end of switch
  
  Serial.print (len);
  Serial.println (F(" bytes."));
  } // end of fBootloaderSize

// show brownout level
void fBrownoutDetectorLevel (const byte val, const unsigned int bootLoaderSize)
  {
  Serial.print (F("Brownout detection at: "));  
  switch (val & 3)
    {
    case 0b11: Serial.println (F("disabled."));   break;
    case 0b10: Serial.println (F("1.8V."));       break;
    case 0b01: Serial.println (F("2.7V."));       break;
    case 0b00: Serial.println (F("4.3V."));       break;
    default:    Serial.println (F("reserved."));   break;
    }  // end of switch
  
  } // end of fBrownoutDetectorLevel

// show brownout level (alternative)
void fBrownoutDetectorLevelAtmega8U2 (const byte val, const unsigned int bootLoaderSize)
  {
  Serial.print (F("Brownout detection at: "));  
  switch (val)
    {
    case 0b111: Serial.println (F("disabled."));   break;
    case 0b110: Serial.println (F("2.7V."));       break;
    case 0b100: Serial.println (F("3.0V."));       break;
    case 0b011: Serial.println (F("3.5V."));       break;
    case 0b001: Serial.println (F("4.0V."));       break;
    case 0b000: Serial.println (F("4.3V."));       break;
    default:    Serial.println (F("reserved."));   break;
    }  // end of switch
  
  } // end of fBrownoutDetectorLevelAtmega8U2

// show brownout level (alternative)
void fBrownoutDetectorLevelAtmega32U4 (const byte val, const unsigned int bootLoaderSize)
  {
  Serial.print (F("Brownout detection at: "));  
  switch (val)
    {
    case 0b111: Serial.println (F("disabled."));   break;
    case 0b110: Serial.println (F("2.0V."));       break;
    case 0b101: Serial.println (F("2.2V."));       break;
    case 0b100: Serial.println (F("2.4V."));       break;
    case 0b011: Serial.println (F("2.6V."));       break;
    case 0b010: Serial.println (F("3.4V."));       break;
    case 0b001: Serial.println (F("3.5V."));       break;
    case 0b000: Serial.println (F("4.3V."));       break;
    default:    Serial.println (F("reserved."));   break;
    }  // end of switch
  
  } // end of fBrownoutDetectorLevelAtmega32U4


// show clock start-up times
void fStartUpTime (const byte val, const unsigned int bootLoaderSize)
  {
  Serial.print (F("Start-up time: SUT0:"));  
  if ((val & 1) == 0)  // if zero, the fuse is "programmed"
    Serial.print (F(" [X]"));
  else
    Serial.print (F(" [ ]"));
  Serial.print (F("  SUT1:"));  
  if ((val & 2) == 0)  // if zero, the fuse is "programmed"
    Serial.print (F(" [X]"));
  else
    Serial.print (F(" [ ]"));
  Serial.println (" (see datasheet)");
  } // end of fStartUpTime
  
// work out clock source
void fClockSource (const byte val, const unsigned int bootLoaderSize)
  {
  Serial.print (F("Clock source: "));  
  switch (val)
    {
    case 0b1000 ... 0b1111: Serial.println (F("low-power crystal."));   break;
    case 0b0110 ... 0b0111: Serial.println (F("full-swing crystal."));       break;
    case 0b0100 ... 0b0101: Serial.println (F("low-frequency crystal."));       break;
    case 0b0011:            Serial.println (F("internal 128 KHz oscillator."));       break;
    case 0b0010:            Serial.println (F("calibrated internal oscillator."));       break;
    case 0b0000:            Serial.println (F("external clock."));       break;
    default:                Serial.println (F("reserved."));   break;
    }  // end of switch
  
  } // end of fClockSource

// work out clock source (Atmega8A)
void fClockSource2 (const byte val, const unsigned int bootLoaderSize)
  {
  Serial.print (F("Clock source: "));  
  switch (val)
    {
    case 0b1010 ... 0b1111: Serial.println (F("low-power crystal."));   break;
    case 0b1001           : Serial.println (F("low-frequency crystal."));       break;
    case 0b0101 ... 0b1000: Serial.println (F("external RC oscillator."));       break;
    case 0b0001 ... 0b0100: Serial.println (F("calibrated internal oscillator."));       break;
    case 0b0000:            Serial.println (F("external clock."));       break;
    default:                Serial.println (F("reserved."));   break;
    }  // end of switch
  
  } // end of fClockSource2

  
// fuses for various processors

const fuseMeaning ATmega48PA_fuses [] PROGMEM = 
  {
    { extFuse,  0x01, descSelfProgrammingEnable },
  
    { highFuse, 0x80, descExternalResetDisable }, 
    { highFuse, 0x40, descDebugWireEnable },
    { highFuse, 0x20, descSerialProgrammingEnable },
    { highFuse, 0x10, descWatchdogTimerAlwaysOn },
    { highFuse, 0x08, descEEPROMsave },
    
    { lowFuse,  0x80, descDivideClockBy8 },
    { lowFuse,  0x40, descClockOutput },
  
    // special (combined) bits
    { lowFuse,  0x30, NULL, fStartUpTime },
    { lowFuse,  0x0F, NULL, fClockSource },
  
    { highFuse, 0x07, NULL, fBrownoutDetectorLevel },
    
  };  // end of ATmega48PA_fuses
  
const fuseMeaning ATmega88PA_fuses [] PROGMEM = 
  {
  
    { highFuse, 0x80, descExternalResetDisable }, 
    { highFuse, 0x40, descDebugWireEnable },
    { highFuse, 0x20, descSerialProgrammingEnable },
    { highFuse, 0x10, descWatchdogTimerAlwaysOn },
    { highFuse, 0x08, descEEPROMsave },
    
    { lowFuse,  0x80, descDivideClockBy8 },
    { lowFuse,  0x40, descClockOutput },
  
    { extFuse, 0x01, descBootIntoBootloader },
  
    // special (combined) bits
    { extFuse,  0x06, NULL, fBootloaderSize },
    { lowFuse,  0x30, NULL, fStartUpTime },
    { lowFuse,  0x0F, NULL, fClockSource },
  
    { highFuse, 0x07, NULL, fBrownoutDetectorLevel },
    
  };  // end of ATmega88PA_fuses 
  
    
const fuseMeaning ATmega328P_fuses [] PROGMEM = 
  {
    { highFuse, 0x80, descExternalResetDisable }, 
    { highFuse, 0x40, descDebugWireEnable },
    { highFuse, 0x20, descSerialProgrammingEnable },
    { highFuse, 0x10, descWatchdogTimerAlwaysOn },
    { highFuse, 0x08, descEEPROMsave },
    { highFuse, 0x01, descBootIntoBootloader },
    
    { lowFuse,  0x80, descDivideClockBy8 },
    { lowFuse,  0x40, descClockOutput },
  
    // special (combined) bits
    { highFuse, 0x06, NULL, fBootloaderSize },
    { lowFuse,  0x30, NULL, fStartUpTime },
    { lowFuse,  0x0F, NULL, fClockSource },
    
    { extFuse,  0x07, NULL, fBrownoutDetectorLevel },
    
  };  // end of ATmega328P_fuses
  
const fuseMeaning ATmega8U2_fuses [] PROGMEM = 
  {
    
    { extFuse,  0x08, descHardwareBootEnable }, 
      
    { highFuse, 0x80, descDebugWireEnable },
    { highFuse, 0x40, descExternalResetDisable }, 
    { highFuse, 0x20, descSerialProgrammingEnable },
    { highFuse, 0x10, descWatchdogTimerAlwaysOn },
    { highFuse, 0x08, descEEPROMsave },
    { highFuse, 0x01, descBootIntoBootloader },
    
    { lowFuse,  0x80, descDivideClockBy8 },
    { lowFuse,  0x40, descClockOutput },
  
    // special (combined) bits
    { highFuse, 0x06, NULL, fBootloaderSize },
    { lowFuse,  0x30, NULL, fStartUpTime },
    { lowFuse,  0x0F, NULL, fClockSource },
    { extFuse,  0x07, NULL, fBrownoutDetectorLevelAtmega8U2 },
  
    
  };  // end of ATmega8U2_fuses  
  
const fuseMeaning ATmega32U4_fuses [] PROGMEM = 
  {
    
    { extFuse,  0x08, descHardwareBootEnable }, 
      
    { highFuse, 0x80, descOCDEnable },
    { highFuse, 0x40, descJtagEnable }, 
    { highFuse, 0x20, descSerialProgrammingEnable },
    { highFuse, 0x10, descWatchdogTimerAlwaysOn },
    { highFuse, 0x08, descEEPROMsave },
    { highFuse, 0x01, descBootIntoBootloader },
    
    { lowFuse,  0x80, descDivideClockBy8 },
    { lowFuse,  0x40, descClockOutput },
  
    // special (combined) bits
    { highFuse, 0x06, NULL, fBootloaderSize },
    { lowFuse,  0x30, NULL, fStartUpTime },
    { lowFuse,  0x0F, NULL, fClockSource },
    { extFuse,  0x07, NULL, fBrownoutDetectorLevelAtmega32U4 },
  
    
  };  // end of ATmega32U4_fuses  
  
const fuseMeaning ATmega164P_fuses [] PROGMEM = 
  {
   
    { highFuse, 0x80, descOCDEnable },
    { highFuse, 0x40, descJtagEnable }, 
    { highFuse, 0x20, descSerialProgrammingEnable },
    { highFuse, 0x10, descWatchdogTimerAlwaysOn },
    { highFuse, 0x08, descEEPROMsave },
    { highFuse, 0x01, descBootIntoBootloader },
    
    { lowFuse,  0x80, descDivideClockBy8 },
    { lowFuse,  0x40, descClockOutput },
  
    // special (combined) bits
    { highFuse, 0x06, NULL, fBootloaderSize },
    { lowFuse,  0x30, NULL, fStartUpTime },
    { lowFuse,  0x0F, NULL, fClockSource },
    { extFuse,  0x07, NULL, fBrownoutDetectorLevel },
    
  };  // end of ATmega164P_fuses  
  
const fuseMeaning ATtiny4313_fuses [] PROGMEM = 
  {
    { extFuse,  0x01, descSelfProgrammingEnable },
  
    { highFuse, 0x80, descDebugWireEnable },
    { highFuse, 0x40, descEEPROMsave },
    { highFuse, 0x20, descSerialProgrammingEnable },
    { highFuse, 0x10, descWatchdogTimerAlwaysOn },
    { highFuse, 0x01, descExternalResetDisable }, 
    
    { lowFuse,  0x80, descDivideClockBy8 },
    { lowFuse,  0x40, descClockOutput },
  
    // special (combined) bits
    { lowFuse,  0x30, NULL, fStartUpTime },
    { lowFuse,  0x0F, NULL, fClockSource },
  
    { highFuse, 0x0E, NULL, fBrownoutDetectorLevel },
    
  };  // end of ATtiny4313_fuses
 
const fuseMeaning ATtiny13_fuses [] PROGMEM = 
  {
    { highFuse, 0x10, descSelfProgrammingEnable },
    { highFuse, 0x08, descDebugWireEnable },
    { highFuse, 0x01, descExternalResetDisable }, 
    
    { lowFuse,  0x80, descSerialProgrammingEnable },
    { lowFuse,  0x40, descEEPROMsave },
    { lowFuse,  0x20, descWatchdogTimerAlwaysOn },
    { lowFuse,  0x10, descDivideClockBy8 },
  
    // special (combined) bits
    { lowFuse,  0x0C, NULL, fStartUpTime },
    { lowFuse,  0x03, NULL, fClockSource },
  
    { highFuse, 0x06, NULL, fBrownoutDetectorLevel },
    
  };  // end of ATtiny13_fuses

const fuseMeaning ATmega8_fuses [] PROGMEM = 
  {
    { highFuse, 0x80, descExternalResetDisable }, 
    { highFuse, 0x40, descWatchdogTimerAlwaysOn },
    { highFuse, 0x20, descSelfProgrammingEnable },
    { highFuse, 0x10, descOscillatorOptions },
    { highFuse, 0x80, descEEPROMsave },
    { highFuse, 0x01, descBootIntoBootloader },

    { lowFuse,  0x80, descBrownOutDetectorLevel },
    { lowFuse,  0x40, descBrownOutDetectorEnable },
  
    // special (combined) bits
    { highFuse, 0x06, NULL, fBootloaderSize },

    { lowFuse,  0x30, NULL, fStartUpTime },
    { lowFuse,  0x0F, NULL, fClockSource2 },
  
  };  // end of ATmega8_fuses
   
// structure for information about a single processor
typedef struct {
   byte sig [3];
   const char * desc;
   unsigned long flashSize;
   unsigned int baseBootSize;
   const fuseMeaning * fusesInfo;
   int numberOfFuseInfo;
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
  { { 0x1E, 0x91, 0x08 }, "ATtiny25",   2 * kb,         0, ATmega48PA_fuses, NUMITEMS (ATmega48PA_fuses) },  // same as ATmega48PA
  { { 0x1E, 0x92, 0x06 }, "ATtiny45",   4 * kb,         0, ATmega48PA_fuses, NUMITEMS (ATmega48PA_fuses) },
  { { 0x1E, 0x93, 0x0B }, "ATtiny85",   8 * kb,         0, ATmega48PA_fuses, NUMITEMS (ATmega48PA_fuses) },

  // Atmega328 family
  { { 0x1E, 0x92, 0x0A }, "ATmega48PA",   4 * kb,         0, ATmega48PA_fuses, NUMITEMS (ATmega48PA_fuses) },
  { { 0x1E, 0x93, 0x0F }, "ATmega88PA",   8 * kb,       256, ATmega88PA_fuses, NUMITEMS (ATmega88PA_fuses) },
  { { 0x1E, 0x94, 0x0B }, "ATmega168PA", 16 * kb,       256, ATmega88PA_fuses, NUMITEMS (ATmega88PA_fuses) },  // same as ATmega88PA
  { { 0x1E, 0x95, 0x0F }, "ATmega328P",  32 * kb,       512, ATmega328P_fuses, NUMITEMS (ATmega328P_fuses) },

  // Atmega644 family
  { { 0x1E, 0x94, 0x0A }, "ATmega164P",   16 * kb,      256, ATmega164P_fuses, NUMITEMS (ATmega164P_fuses) },
  { { 0x1E, 0x95, 0x08 }, "ATmega324P",   32 * kb,      512, ATmega164P_fuses, NUMITEMS (ATmega164P_fuses) },
  { { 0x1E, 0x96, 0x0A }, "ATmega644P",   64 * kb,   1 * kb, ATmega164P_fuses, NUMITEMS (ATmega164P_fuses) },

  // Atmega2560 family
  { { 0x1E, 0x96, 0x08 }, "ATmega640",    64 * kb,   1 * kb, ATmega164P_fuses, NUMITEMS (ATmega164P_fuses) },  // same as ATmega164P
  { { 0x1E, 0x97, 0x03 }, "ATmega1280",  128 * kb,   1 * kb, ATmega164P_fuses, NUMITEMS (ATmega164P_fuses) },  // same as ATmega164P
  { { 0x1E, 0x97, 0x04 }, "ATmega1281",  128 * kb,   1 * kb, ATmega164P_fuses, NUMITEMS (ATmega164P_fuses) },  // same as ATmega164P
  { { 0x1E, 0x98, 0x01 }, "ATmega2560",  256 * kb,   1 * kb, ATmega164P_fuses, NUMITEMS (ATmega164P_fuses) },  // same as ATmega164P
  { { 0x1E, 0x98, 0x02 }, "ATmega2561",  256 * kb,   1 * kb, ATmega164P_fuses, NUMITEMS (ATmega164P_fuses) },  // same as ATmega164P
  
  // AT90USB family
  { { 0x1E, 0x93, 0x82 }, "At90USB82",    8 * kb,   512, ATmega8U2_fuses, NUMITEMS (ATmega8U2_fuses) },  // same as ATmega8U2
  { { 0x1E, 0x94, 0x82 }, "At90USB162",  16 * kb,   512, ATmega8U2_fuses, NUMITEMS (ATmega8U2_fuses) },  // same as ATmega8U2

  // Atmega32U2 family
  { { 0x1E, 0x93, 0x89 }, "ATmega8U2",    8 * kb,   512, ATmega8U2_fuses, NUMITEMS (ATmega8U2_fuses) },
  { { 0x1E, 0x94, 0x89 }, "ATmega16U2",  16 * kb,   512, ATmega8U2_fuses, NUMITEMS (ATmega8U2_fuses) },  // same as ATmega8U2
  { { 0x1E, 0x95, 0x8A }, "ATmega32U2",  32 * kb,   512, ATmega8U2_fuses, NUMITEMS (ATmega8U2_fuses) },  // same as ATmega8U2
  
  
  // Atmega32U4 family
  { { 0x1E, 0x94, 0x88 }, "ATmega16U4",  16 * kb,   512, ATmega32U4_fuses, NUMITEMS (ATmega32U4_fuses) },
  { { 0x1E, 0x95, 0x87 }, "ATmega32U4",  32 * kb,   512, ATmega32U4_fuses, NUMITEMS (ATmega32U4_fuses) },
  
  // ATmega1284P family
  { { 0x1E, 0x97, 0x05 }, "ATmega1284P", 128 * kb,   1 * kb, ATmega164P_fuses, NUMITEMS (ATmega164P_fuses) },  // same as ATmega164P
  
  // ATtiny4313 family
  { { 0x1E, 0x91, 0x0A }, "ATtiny2313A", 2 * kb,   0, ATtiny4313_fuses, NUMITEMS (ATtiny4313_fuses) },
  { { 0x1E, 0x92, 0x0D }, "ATtiny4313",  4 * kb,   0, ATtiny4313_fuses, NUMITEMS (ATtiny4313_fuses) },
  
  // ATtiny13 family
  { { 0x1E, 0x90, 0x07 }, "ATtiny13A",   1 * kb,   0, ATtiny13_fuses, NUMITEMS (ATtiny13_fuses) },
  
  // Atmega8A family
  { { 0x1E, 0x93, 0x07 }, "ATmega8A",    8 * kb, 256, ATmega8_fuses, NUMITEMS (ATmega8_fuses) },
  
  };  // end of signatures

// if signature found in above table, this is its index
int foundSig = -1;
byte lastAddressMSB = 0;
 
// execute one programming instruction ... b1 is command, b2, b3, b4 are arguments
//  processor may return a result on the 4th transfer, this is returned.
byte program (const byte b1, const byte b2 = 0, const byte b3 = 0, const byte b4 = 0);
byte program (const byte b1, const byte b2, const byte b3, const byte b4)
  {
  SPI.transfer (b1);  
  SPI.transfer (b2);  
  SPI.transfer (b3);  
  return SPI.transfer (b4);  
  } // end of program
 
void showHex (const byte b, const boolean newline = false);
void showHex (const byte b, const boolean newline)
  {
  Serial.print (F("0x"));
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
  
bool startProgramming ()
  {
  Serial.print (F("Attempting to enter programming mode ..."));
  digitalWrite (RESET, HIGH);  // ensure SS stays high for now
  SPI.begin ();
  SPI.setClockDivider (SPI_CLOCK_DIV64);

  byte confirm;
  pinMode (RESET, OUTPUT);
  pinMode (SCK, OUTPUT);
  unsigned int timeout = 0;

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
    
    if (confirm != programAcknowledge)
      {
      Serial.print (".");
      if (timeout++ >= ENTER_PROGRAMMING_ATTEMPTS)
        {
        Serial.println ();
        Serial.println (F("Failed to enter programming mode. Double-check wiring!"));
        return false;
        }  // end of too many attempts
      }  // end of not entered programming mode
    } while (confirm != programAcknowledge);

  Serial.println ();
  Serial.println (F("Entered programming mode OK."));
  return true;
  }  // end of startProgramming

void getSignature ()
  {
  byte sig [3];
  Serial.print (F("Signature = "));
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
      Serial.print (F("Processor = "));
      Serial.println (signatures [j].desc);
      Serial.print (F("Flash memory size = "));
      Serial.println (signatures [j].flashSize, DEC);
      return;
      }
    }

  Serial.println (F("Unrecogized signature."));  
  }  // end of getSignature

// Print a string from Program Memory directly to save RAM 
byte printProgStr (const char * str)
{
  char c;
  if (str == NULL)
    return 0;
  byte count = 0;
  while ((c = pgm_read_byte (str++)))
    {
    Serial.print(c);
    count++;
    }
  return count;
} // end of printProgStr

void showFuseMeanings ()
  {
  if (signatures [foundSig].fusesInfo == NULL)
     {
     Serial.println (F("No fuse information for this processor."));
     return; 
     } // end if no information
   
  for (int i = 0; i < signatures [foundSig].numberOfFuseInfo; i++)
    {
    fuseMeaning thisFuse;

    // make a copy of this table entry to save a lot of effort
    memcpy_P (&thisFuse, &signatures [foundSig].fusesInfo [i], sizeof thisFuse);
    
    // find the fuse value
    byte val = thisFuse.whichFuse;
    // and which mask this entry is for
    byte mask = thisFuse.mask;
    
    // if we have a description, show it
    if (thisFuse.meaningIfProgrammed)
      {
      byte count = printProgStr (thisFuse.meaningIfProgrammed);
      while (count++ < 40)
        Serial.print (".");
      if ((fuses [val] & mask) == 0)  // if zero, the fuse is "programmed"
        Serial.println (F(" [X]"));
      else
        Serial.println (F(" [ ]"));
      }  // end of description available
      
    // some fuses use multiple bits so we'll call a special handling function
    if (thisFuse.specialHandler)
      {
       // get value into low-order bits
        byte adjustedVal = fuses [val] & mask;
        while ((mask & 1) == 0)
          {
          adjustedVal >>= 1;
          mask >>= 1;
          }
        
        thisFuse.specialHandler (adjustedVal, signatures [foundSig].baseBootSize);
      }  // end if special handler
     
    } // end of for each fuse meaning
    
  }  // end of showFuseMeanings
  
void getFuseBytes ()
  {
  fuses [lowFuse] = program (readLowFuseByte, readLowFuseByteArg2);
  fuses [highFuse] = program (readHighFuseByte, readHighFuseByteArg2);
  fuses [extFuse] = program (readExtendedFuseByte, readExtendedFuseByteArg2);
  fuses [lockByte] = program (readLockByte, readLockByteArg2);
  fuses [calibrationByte]  = program (readCalibrationByte);  
  
  Serial.print ("LFuse = ");
  showHex (fuses [lowFuse], true);
  Serial.print ("HFuse = ");
  showHex (fuses [highFuse], true);
  Serial.print ("EFuse = ");
  showHex (fuses [extFuse], true);
  Serial.print ("Lock byte = ");
  showHex (fuses [lockByte], true);
  Serial.print (F("Clock calibration = "));
  showHex (fuses [calibrationByte], true);
  
  }  // end of getFuseBytes

void setup ()
  {
  Serial.begin (115200);
  while (!Serial) ;  // for Leonardo, Micro etc.
  Serial.println ();
  Serial.println (F("Atmega fuse calculator."));
  Serial.println (F("Written by Nick Gammon."));
  Serial.println (F("Version " VERSION));
  Serial.println (F("Compiled on " __DATE__ " at " __TIME__ " with Arduino IDE " xstr(ARDUINO) "."));

  pinMode (CLOCKOUT, OUTPUT);
  
  // set up Timer 1
  TCCR1A = bit (COM1A0);  // toggle OC1A on Compare Match
  TCCR1B = bit (WGM12) | bit (CS10);   // CTC, no prescaling
  OCR1A =  0;       // output every cycle
  
  if (startProgramming ())
    {
    getSignature ();
    getFuseBytes ();
    
    if (foundSig != -1)
      showFuseMeanings ();
    }  // end of if entered programming mode OK
    
 }  // end of setup

void loop () {}

