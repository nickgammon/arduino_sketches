// HV_Parallel_Utils.ino
//
// Functions needed for high-voltage PARALLEL progamming
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


#if HIGH_VOLTAGE_PARALLEL

// Latch in an action. The data argument is loaded into the appropriate latch. It might be
// a command, an address, or data, depending on the action type.
// BS1 and BS2 modify the actions.

void HVprogram (const byte action, const byte data, const byte bs1 = 0, const byte bs2 = 0)
  {
  // XA1 and XA0 determine the action
  switch (action)
    {
    case ACTION_LOAD_ADDRESS:
        digitalWrite (XA1, LOW);
        digitalWrite (XA0, LOW);
        break;
    
    case ACTION_LOAD_DATA:
        digitalWrite (XA1, LOW);
        digitalWrite (XA0, HIGH);
        break;
        
    case ACTION_LOAD_COMMAND:
        digitalWrite (XA1, HIGH);
        digitalWrite (XA0, LOW);
        break;
        
    case ACTION_IDLE:
        digitalWrite (XA1, HIGH);
        digitalWrite (XA0, HIGH);
        break;
      
    }  // end of switch on action
    
  digitalWrite (BS1, bs1);
  digitalWrite (BS2, bs2);
  
  // set up the data byte
  for (byte i = 0; i < 8; i++)
    digitalWrite (dataPins [i], (data & bit (i)) ? HIGH : LOW);
    
  digitalWrite (XTAL1, HIGH);  // pulse XTAL to send command to target
  digitalWrite (XTAL1, LOW);
  }  // end of HVprogram	

// Read a byte of data by setting the data pins to input, enabling output 
// (output from the target, input to the programmer), reading the 8 bits
// disabling output, and then putting the pins back as outputs.
// BS1 and BS2 modify what data is read (eg. high byte or low byte)

byte HVreadData (const byte bs1 = LOW, const byte bs2 = LOW)
  {
  // set up requested bytes
  digitalWrite (BS1, bs1);
  digitalWrite (BS2, bs2);
    
  // make the data pins input, ready for reading from
  for (byte i = 0; i < 8; i++)
    pinMode (dataPins [i], INPUT);
  // enable output, data should now be on the 8 pins
  digitalWrite (OE, LOW);    // Enable output
  delayMicroseconds (1);
  // copy data in
  byte result = 0;
  for (byte i = 0; i < 8; i++)
    if (digitalRead (dataPins [i]) == HIGH)
      result |= bit (i);
  // we are done reading, disable the chips output
  digitalWrite (OE, HIGH);    // Disable output
  delayMicroseconds (1);
  // now our control lines can be output again
  for (byte i = 0; i < 8; i++)
    pinMode (dataPins [i], OUTPUT);
  return result;
  }  // end of HVreadData
 
byte readFlash (unsigned long addr)
  {
  byte high = addr & 1;  // set if high byte wanted
  addr >>= 1;  // turn into word address

  HVprogram (ACTION_LOAD_COMMAND, CMD_READ_FLASH);
  HVprogram (ACTION_LOAD_ADDRESS, addr >> 8, HIGH);
  HVprogram (ACTION_LOAD_ADDRESS, addr, LOW);
    
  return high ? HVreadData (HIGH) : HVreadData (LOW);
  } // end of readFlash
  
// write a byte to the flash memory buffer (ready for committing)
void writeFlash (unsigned long addr, const byte data)
  {

  byte high = addr & 1;      // set if high byte wanted
  addr >>= 1;  // turn into word address

  HVprogram (ACTION_LOAD_COMMAND, CMD_WRITE_FLASH);
  HVprogram (ACTION_LOAD_ADDRESS, addr >> 8, HIGH);
  HVprogram (ACTION_LOAD_ADDRESS, addr, LOW);
  if (high)
    {
    HVprogram (ACTION_LOAD_DATA, data, HIGH);
    digitalWrite (PAGEL, HIGH);  // pulse PAGEL to load latch
    digitalWrite (PAGEL, LOW);
    }
  else
    HVprogram (ACTION_LOAD_DATA, data, LOW);
  
  } // end of writeFlash  

  
byte readFuse (const byte which)
  {
  if (which == calibrationByte)
    {
    HVprogram (ACTION_LOAD_COMMAND, CMD_READ_SIGNATURE);
    HVprogram (ACTION_LOAD_ADDRESS, 0);  // as instructed
    }
  else
    HVprogram (ACTION_LOAD_COMMAND, CMD_READ_FUSE_BITS);
    
  switch (which)
    {
    case lowFuse:         return HVreadData (LOW, LOW);
    case highFuse:        return HVreadData (HIGH, HIGH);
    case extFuse:         return HVreadData (LOW, HIGH);
    case lockByte:        return HVreadData (HIGH, LOW);
    case calibrationByte: return HVreadData (HIGH, LOW);
    }  // end of switch 
  return 0;
  }  // end of readFuse
  
void readSignature (byte sig [3])
  {
  HVprogram (ACTION_LOAD_COMMAND, CMD_READ_SIGNATURE);
  for (byte i = 0; i < 3; i++)
    {
    HVprogram (ACTION_LOAD_ADDRESS, i);  // which byte
    sig [i]  = HVreadData ();
    }
  lastAddressMSB = 0;
  }  // end of readSignature

// poll the target device until it is ready to be programmed
void pollUntilReady ()
  {
  // wait until not busy
  while (digitalRead (RDY) == LOW)
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
  HVprogram (ACTION_LOAD_ADDRESS, addr >> 8, HIGH);
  HVprogram (ACTION_LOAD_ADDRESS, addr, LOW);
  digitalWrite (WR, LOW);
  digitalWrite (WR, HIGH);
  pollUntilReady (); 
  clearPage();  // clear ready for next page full
  HVprogram (ACTION_LOAD_COMMAND, CMD_NO_OPERATION);
  }  // end of commitPage

void eraseMemory ()
  {
  HVprogram (ACTION_LOAD_COMMAND, CMD_CHIP_ERASE);

  digitalWrite (WR, LOW);  // pulse WR to erase chip
  digitalWrite (WR, HIGH);
    
  pollUntilReady (); 
  clearPage();  // clear temporary page
  }  // end of eraseMemory

// write specified value to specified fuse/lock byte
void writeFuse (const byte newValue, const byte whichFuse)
  {
  if (newValue == 0)
    return;  // ignore

  // write fuse/lock bits command
  if (whichFuse == lockByte)
    HVprogram (ACTION_LOAD_COMMAND, CMD_WRITE_LOCK_BITS);
  else
    HVprogram (ACTION_LOAD_COMMAND, CMD_WRITE_FUSE_BITS);
  
  // latch in the new value
  switch (whichFuse)
    {
    case lowFuse:    
      HVprogram (ACTION_LOAD_DATA, newValue);  
      break; 
    case highFuse:   
      HVprogram (ACTION_LOAD_DATA, newValue);  
      digitalWrite (BS1, HIGH);    // do this AFTER programming the data
      break; 
    case extFuse:    
      HVprogram (ACTION_LOAD_DATA, newValue); 
      digitalWrite (BS2, HIGH);    // do this AFTER programming the data
      break; 
    case lockByte:   
      HVprogram (ACTION_LOAD_DATA, newValue, LOW,  LOW);  
      break; 
    default: return;
    }  // end of switch
    
  digitalWrite (WR, LOW);  // pulse WR to program fuse
  digitalWrite (WR, HIGH);
  
  pollUntilReady (); 
  }  // end of writeFuse

// put chip into programming mode    
bool startProgramming ()
  {
  Serial.println (F("Activating high-voltage PARALLEL programming mode."));

  digitalWrite (PAGEL, LOW);
  digitalWrite (XA1, LOW);
  digitalWrite (XA0, LOW);
  digitalWrite (BS1, LOW);
  digitalWrite (BS2, LOW);
  // Enter programming mode
  digitalWrite (VCC, HIGH);   // This brings /RESET to 12V after 40 uS by a transistor
  delayMicroseconds (10);
  digitalWrite (WR, HIGH);    // Read mode
  digitalWrite (OE, HIGH);    // Not output-enable
  delay(5);
  return true;
 }  // end of  startProgramming
  
void stopProgramming ()
  {
  // all pins to LOW
  for (byte i = 0; i < 20; i++)
    {
    if (i != RDY)
      digitalWrite (i, LOW);
    }
  Serial.println (F("Programming mode off."));
  } // end of stopProgramming  

// called from setup()  
void initPins ()
  {
    // all pins to output and LOW
  for (byte i = 0; i < 20; i++)
    {
    if (i != RDY)
      {
      digitalWrite (i, LOW);
      pinMode (i, OUTPUT);
      }
    }
  // however the RDY pin is an input
  pinMode (RDY, INPUT); 
  }  // end of initPins
  
#endif  // HIGH_VOLTAGE_PARALLEL
