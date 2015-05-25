
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
    
#if HIGH_VOLTAGE_PARALLEL
  // if even length force out last page latch
  if ((length & 1) == 0)
    writeFlash (addr + length, 0xFF);
#endif // HIGH_VOLTAGE_PARALLEL    

  }  // end of writeData
