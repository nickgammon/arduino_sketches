// File_Utils.ino
//
// Functions related to the SD card (if present) including listing the directory, 
// and reading an interpreting a .HEX file.
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

#if SD_CARD_ACTIVE

// types of record in .hex file
enum {
    hexDataRecord,  // 00
    hexEndOfFile,   // 01
    hexExtendedSegmentAddressRecord, // 02
    hexStartSegmentAddressRecord,  // 03
    hexExtendedLinearAddressRecord, // 04
    hexStartLinearAddressRecord // 05
};

bool gotEndOfFile;
unsigned long extendedAddress;
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

char name[MAX_FILENAME] = { 0 };  // current file name

bool processLine (const char * pLine, const byte action)
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
bool readHexFile (const char * fName, const byte action)
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
      eraseMemory ();
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


bool chooseInputFile ()
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
    eeprom_write_block ((const void *) &lastFileName, LAST_FILENAME_LOCATION_IN_EEPROM, MAX_FILENAME);
  
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
  if (!startProgramming ())
    return;  
  
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
  bool allFF;
  unsigned int i;
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
    sprintf (linebuf, ":10%04X00", (unsigned int) address & 0xFFFF);
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
  if (!startProgramming ())
    return;

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
  if (!startProgramming ())
    return;
    
  // verify it
  readHexFile(name, verifyFlash);
  }  // end of verifyFlashContents
  
void initFile ()
  {
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
  }  // end of initFile
  
#endif // SD_CARD_ACTIVE
