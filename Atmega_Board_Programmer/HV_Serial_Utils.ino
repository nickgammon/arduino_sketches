// HV_Serial_Utils.ino
//
// Functions needed for high-voltage SERIAL progamming
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


#if HIGH_VOLTAGE_SERIAL

// send one bit (see datasheet)
byte HVbit (const byte sii, const byte sdi)
{
  digitalWrite (SII, sii ? HIGH : LOW);
  digitalWrite (SDI, sdi ? HIGH : LOW);
  digitalWrite (SCI, HIGH);  // pulse clock
  digitalWrite (SCI, LOW);
  return digitalRead (SDO);  // data bit is available on trailing edge of clock
}  // end of HVbit

// transfer one byte (see datasheet)
// 11 bits per transfer, MSB first
byte HVtransfer (byte instruction, byte data)
  {

  byte result = HVbit (0, 0);  // start bit, also first bit of result
  
  // seven more bits of data
  for (byte i = 0; i < 7; i++)
    {
    result <<= 1;
    result |= HVbit (instruction & 0x80, data & 0x80);
    instruction <<= 1;
    data <<= 1;
    }  // end of for each bit
 
  // last bit to be sent
  HVbit (instruction & 0x80, data & 0x80);
  
  // two stop bits
  HVbit (0, 0);  
  HVbit (0, 0);  

  return result;
  }  // end of HVtransfer

// Read a byte from flash by setting up the low and high byte of the address, then
// reading the entire word. We return which byte was wanted (low or high).
byte readFlash (unsigned long addr)
  {
  byte high = addr & 1;  // set if high byte wanted
  addr >>= 1;  // turn into word address
    
  HVtransfer (SII_LOAD_COMMAND, CMD_READ_FLASH);
  HVtransfer (SII_LOAD_ADDRESS_LOW, addr & 0xFF);
  HVtransfer (SII_LOAD_ADDRESS_HIGH, (addr >> 8) & 0xFF);
  HVtransfer (SII_READ_LOW_BYTE, 0);
  byte lowResult = HVtransfer (SII_READ_LOW_BYTE | SII_OR_MASK, 0);
  HVtransfer (SII_READ_HIGH_BYTE, 0);
  byte highResult = HVtransfer (SII_READ_HIGH_BYTE | SII_OR_MASK, 0);
  
  return high ? highResult : lowResult;
  } // end of readFlash
  
// write a byte to the flash memory buffer (ready for committing)
void writeFlash (unsigned long addr, const byte data)
  {
  byte high = addr & 1;      // set if high byte wanted
  addr >>= 1;  // turn into word address
  static byte lowData = 0xFF;
  
  // save until we have both bytes in the word
  if (!high)
    {
    lowData = data;
    return;
    }
  
  // write to flash command
  HVtransfer (SII_LOAD_COMMAND, CMD_WRITE_FLASH);
  
  // address 
  HVtransfer (SII_LOAD_ADDRESS_LOW, addr & 0xFF);
  
  // latch in low byte
  HVtransfer (SII_LOAD_LOW_BYTE, lowData);
  HVtransfer (SII_PROGRAM_LOW_BYTE, 0);
  HVtransfer (SII_PROGRAM_LOW_BYTE | SII_OR_MASK, 0);

  // latch in high byte  
  HVtransfer (SII_LOAD_HIGH_BYTE, data);
  HVtransfer (SII_PROGRAM_HIGH_BYTE, 0);
  HVtransfer (SII_PROGRAM_HIGH_BYTE | SII_OR_MASK, 0);

  lowData = 0xFF;
  
  } // end of writeFlash  

// read a fuse byte
byte readFuse (const byte which)
  {
  
  if (which == calibrationByte)
    {
    HVtransfer (SII_LOAD_COMMAND, CMD_READ_SIGNATURE);
    HVtransfer (SII_LOAD_ADDRESS_LOW, 0);
    }
  else
    HVtransfer (SII_LOAD_COMMAND, CMD_READ_FUSE_BITS);
    
  byte operation;
  
  switch (which)
    {
    case lowFuse:          operation = 0b01101000; break;
    case highFuse:         operation = 0b01111010; break;
    case extFuse:          operation = 0b01101010; break;  
    case lockByte:         operation = 0b01111000; break; 
    case calibrationByte:  operation = 0b01111000; break;
        
    default:   return 0;
    
    }  // end of switch 

  HVtransfer (operation, 0);
  return HVtransfer (operation | SII_OR_MASK, 0);

  }  // end of readFuse

// read all 3 signature bytes  
void readSignature (byte sig [3])
  {
  HVtransfer (SII_LOAD_COMMAND, CMD_READ_SIGNATURE);
 
  
  for (byte i = 0; i < 3; i++)
    {
    HVtransfer (SII_LOAD_ADDRESS_LOW, i);
    HVtransfer (SII_READ_LOW_BYTE, 0);
    sig [i] = HVtransfer (SII_READ_LOW_BYTE  | SII_OR_MASK, 0);
    }
    
  lastAddressMSB = 0;
  }  // end of readSignature

// poll the target device until it is ready to be programmed
void pollUntilReady ()
  {
  // wait until not busy
  while (digitalRead (SDO) == LOW)
    { }
    
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
  
  HVtransfer (SII_LOAD_ADDRESS_HIGH, addr >> 8);
  HVtransfer (SII_WRITE_LOW_BYTE, 0);
  HVtransfer (SII_WRITE_LOW_BYTE | SII_OR_MASK, 0);
  
  pollUntilReady (); 
  clearPage();  // clear ready for next page full
  HVtransfer (SII_LOAD_COMMAND, CMD_NO_OPERATION);
  
  }  // end of commitPage

// erase all memory (also resets the lock bits)
void eraseMemory ()
  {
  HVtransfer (SII_LOAD_COMMAND, CMD_CHIP_ERASE);
  HVtransfer (SII_WRITE_LOW_BYTE, 0);
  HVtransfer (SII_WRITE_LOW_BYTE | SII_OR_MASK, 0);
  
  pollUntilReady (); 
  clearPage();  // clear temporary page
  }  // end of eraseMemory

// write specified value to specified fuse/lock byte
void writeFuse (const byte newValue, const byte whichFuse)
  {
  if (newValue == 0)
    return;  // ignore

  // write fuse command
  if (whichFuse == lockByte)
    HVtransfer (SII_LOAD_COMMAND, CMD_WRITE_LOCK_BITS);
  else
    HVtransfer (SII_LOAD_COMMAND, CMD_WRITE_FUSE_BITS);

  HVtransfer (SII_LOAD_LOW_BYTE, newValue);
  
  // latch in the new value
  switch (whichFuse)
    {
    case lowFuse:    
      HVtransfer (SII_WRITE_LOW_BYTE, 0);
      HVtransfer (SII_WRITE_LOW_BYTE | SII_OR_MASK, 0);
      break; 
      
    case highFuse:   
      HVtransfer (SII_WRITE_HIGH_BYTE, 0);
      HVtransfer (SII_WRITE_HIGH_BYTE | SII_OR_MASK, 0);
      break; 
    
    case extFuse:
      HVtransfer (SII_WRITE_EXTENDED_FUSE, 0);
      HVtransfer (SII_WRITE_EXTENDED_FUSE | SII_OR_MASK, 0);
      break; 

    case lockByte:
      HVtransfer (SII_WRITE_LOW_BYTE, 0);
      HVtransfer (SII_WRITE_LOW_BYTE | SII_OR_MASK, 0);
      break; 
    
    default: return;
    }  // end of switch
    
  
  pollUntilReady (); 
  }  // end of writeFuse

// put chip into programming mode    
bool startProgramming ()
  {
  Serial.println (F("Activating high-voltage SERIAL programming mode."));
  pinMode (SDI, OUTPUT);
  pinMode (SII, OUTPUT);
  pinMode (SDO, OUTPUT);
  pinMode (SCI, OUTPUT);

  digitalWrite (SDI, LOW);
  digitalWrite (SII, LOW);
  digitalWrite (SDO, LOW);
  // Enter programming mode
  digitalWrite (VCC, HIGH);   // This brings /RESET to 12V after 40 uS by a transistor
  delayMicroseconds (60);
  pinMode (SDO, INPUT);       // This should be an input for reading from
  delayMicroseconds (300);
  return true;
 }  // end of  startProgramming
  
// turn off programming mode
void stopProgramming ()
  {
  digitalWrite (VCC, LOW);
  pinMode (SDI, INPUT);
  pinMode (SII, INPUT);
  pinMode (SDO, INPUT);
  pinMode (SCI, INPUT);
  Serial.println (F("Programming mode off."));
  } // end of stopProgramming  

// called from setup()  
void initPins ()
  {
  digitalWrite (VCC, LOW);
  pinMode (VCC, OUTPUT);
  }  // end of initPins
  
#endif  // HIGH_VOLTAGE_SERIAL
