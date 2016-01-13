// Programming_Utils.ino
//
// General useful functions needed for all sketches
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
  for (unsigned int i = 0; i < len; i++)
    writeFlash (i, 0xFF);
}  // end of clearPage
  

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
    
#if HIGH_VOLTAGE_PARALLEL || HIGH_VOLTAGE_SERIAL
  // if we finished on odd byte force out last page latch
  if (((addr + length) & 1) == 1)
    writeFlash (addr + length, 0xFF);
#endif // HIGH_VOLTAGE_PARALLEL    

  }  // end of writeData
  
 
// show a byte in hex with leading zero and optional newline
void showHex (const byte b, const boolean newline, const boolean show0x)
  {
  if (show0x)
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


// convert a boolean to Yes/No
void showYesNo (const boolean b, const boolean newline)
  {
  if (b)
    Serial.print (F("Yes"));
  else
    Serial.print (F("No"));
  if (newline)
    Serial.println ();
  }  // end of showYesNo
