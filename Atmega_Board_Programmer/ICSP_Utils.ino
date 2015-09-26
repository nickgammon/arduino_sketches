// ICSP_Utils.ino
//
// Functions needed for SPI (ICSP) progamming
//
// Author: Nick Gammon

/* ----------------------------------------------------------------------------
NOTE: This file should only be modified in the Atmega_Hex_Uploader directory.
Copies in other directories are hard-linked to this one.
After modifying it run the shell script:
  fixup_links.sh
This script needs to be run in the directories:
  Atmega_Board_Programmer and Atmega_Board_Detector
That will ensure that those directories now are using the same file.
------------------------------------------------------------------------------ */


#if ICSP_PROGRAMMING

#if !USE_BIT_BANGED_SPI
  #include <SPI.h>
#endif // !USE_BIT_BANGED_SPI

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

// execute one programming instruction ... b1 is command, b2, b3, b4 are arguments
//  processor may return a result on the 4th transfer, this is returned.
byte program (const byte b1, const byte b2 = 0, const byte b3 = 0, const byte b4 = 0);
byte program (const byte b1, const byte b2, const byte b3, const byte b4)
  {
  noInterrupts ();
#if USE_BIT_BANGED_SPI

  BB_SPITransfer (b1);
  BB_SPITransfer (b2);
  BB_SPITransfer (b3);
  byte b = BB_SPITransfer (b4);
#else
  SPI.transfer (b1);
  SPI.transfer (b2);
  SPI.transfer (b3);
  byte b = SPI.transfer (b4);
#endif // (not) USE_BIT_BANGED_SPI

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
void writeFlash (unsigned long addr, const byte data)
  {
  byte high = (addr & 1) ? 0x08 : 0;  // set if high byte wanted
  addr >>= 1;  // turn into word address
  program (loadProgramMemory | high, 0, lowByte (addr), data);
  } // end of writeFlash

byte readFuse (const byte which)
  {
  switch (which)
    {
    case lowFuse:         return program (readLowFuseByte, readLowFuseByteArg2);
    case highFuse:        return program (readHighFuseByte, readHighFuseByteArg2);
    case extFuse:         return program (readExtendedFuseByte, readExtendedFuseByteArg2);
    case lockByte:        return program (readLockByte, readLockByteArg2);
    case calibrationByte: return program (readCalibrationByte);
    }  // end of switch

   return 0;
  }  // end of readFuse

void readSignature (byte sig [3])
  {
  for (byte i = 0; i < 3; i++)
    sig [i] = program (readSignatureByte, 0, i);

  // make sure extended address is zero to match lastAddressMSB variable
  program (loadExtendedAddressByte, 0, 0);
  lastAddressMSB = 0;

  }  // end of readSignature

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

// commit page to flash memory
void commitPage (unsigned long addr, bool showMessage)
  {

  if (showMessage)
    {
    Serial.print (F("Committing page starting at 0x"));
    Serial.println (addr, HEX);
    }
  else
    showProgress ();

  addr >>= 1;  // turn into word address

  // set the extended (most significant) address byte if necessary
  byte MSB = (addr >> 16) & 0xFF;
  if (MSB != lastAddressMSB)
    {
    program (loadExtendedAddressByte, 0, MSB);
    lastAddressMSB = MSB;
    }  // end if different MSB

  program (writeProgramMemory, highByte (addr), lowByte (addr));
  pollUntilReady ();

  clearPage();  // clear ready for next page full
  }  // end of commitPage

void eraseMemory ()
  {
  program (progamEnable, chipErase);   // erase it
  delay (20);  // for Atmega8
  pollUntilReady ();
  clearPage();  // clear temporary page
  }  // end of eraseMemory

// write specified value to specified fuse/lock byte
void writeFuse (const byte newValue, const byte whichFuse)
  {
  if (newValue == 0)
    return;  // ignore

  program (progamEnable, fuseCommands [whichFuse], 0, newValue);
  pollUntilReady ();
  }  // end of writeFuse

// put chip into programming mode
bool startProgramming ()
  {

  Serial.print (F("Attempting to enter ICSP programming mode ..."));

  byte confirm;
  pinMode (RESET, OUTPUT);

#if USE_BIT_BANGED_SPI
  digitalWrite (MSPIM_SCK, LOW);
  pinMode (MSPIM_SCK, OUTPUT);
  pinMode (BB_MOSI, OUTPUT);
#else
  digitalWrite (RESET, HIGH);  // ensure SS stays high for now
  SPI.begin ();
  SPI.setClockDivider (SPI_CLOCK_DIV64);
  pinMode (SCK, OUTPUT);
#endif  // (not) USE_BIT_BANGED_SPI

  unsigned int timeout = 0;

  // we are in sync if we get back programAcknowledge on the third byte
  do
    {
    // regrouping pause
    delay (100);

    // ensure SCK low
    noInterrupts ();

#if USE_BIT_BANGED_SPI
    digitalWrite (MSPIM_SCK, LOW);
#else
    digitalWrite (SCK, LOW);
#endif  // (not) USE_BIT_BANGED_SPI

    // then pulse reset, see page 309 of datasheet
    digitalWrite (RESET, HIGH);
    delayMicroseconds (10);  // pulse for at least 2 clock cycles
    digitalWrite (RESET, LOW);
    interrupts ();

    delay (25);  // wait at least 20 mS
    noInterrupts ();
#if USE_BIT_BANGED_SPI
    BB_SPITransfer (progamEnable);
    BB_SPITransfer (programAcknowledge);
    confirm = BB_SPITransfer (0);
    BB_SPITransfer (0);
#else
    SPI.transfer (progamEnable);
    SPI.transfer (programAcknowledge);
    confirm = SPI.transfer (0);
    SPI.transfer (0);
#endif  // (not) USE_BIT_BANGED_SPI

    interrupts ();

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

void stopProgramming ()
  {
  digitalWrite (RESET, LOW);
  pinMode (RESET, INPUT);

#if USE_BIT_BANGED_SPI
  // turn off pull-ups
  digitalWrite (MSPIM_SCK, LOW);
  digitalWrite (BB_MOSI, LOW);
  digitalWrite (BB_MISO, LOW);

  // set everything back to inputs
  pinMode (MSPIM_SCK, INPUT);
  pinMode (BB_MOSI, INPUT);
  pinMode (BB_MISO, INPUT);
#else
  SPI.end ();

  // turn off pull-ups, if any
  digitalWrite (SCK,   LOW);
  digitalWrite (MOSI,  LOW);
  digitalWrite (MISO,  LOW);

  // set everything back to inputs
  pinMode (SCK,   INPUT);
  pinMode (MOSI,  INPUT);
  pinMode (MISO,  INPUT);
#endif  // (not) USE_BIT_BANGED_SPI

  Serial.println (F("Programming mode off."));

  } // end of stopProgramming

// called from setup()
void initPins ()
  {

  // set up 8 MHz timer on pin 9
  pinMode (CLOCKOUT, OUTPUT);
  // set up Timer 1
  TCCR1A = bit (COM1A0);  // toggle OC1A on Compare Match
  TCCR1B = bit (WGM12) | bit (CS10);   // CTC, no prescaling
  OCR1A =  0;       // output every cycle



  }  // end of initPins

#endif // ICSP_PROGRAMMING
