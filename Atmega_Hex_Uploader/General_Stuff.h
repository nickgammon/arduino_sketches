// General_Stuff.h
//
// Variables and defines required by all sketches
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


unsigned long pagesize;
unsigned long pagemask;
unsigned long oldPage;

// count errors
unsigned int errors;

const unsigned long NO_PAGE = 0xFFFFFFFF;

unsigned int progressBarCount;

// if signature found in signature table, this is its index
int foundSig = -1;
byte lastAddressMSB = 0;
// copy of current signature entry for matching processor
signatureType currentSignature;

      
// number of items in an array
#define NUMITEMS(arg) ((unsigned int) (sizeof (arg) / sizeof (arg [0])))

// stringification for Arduino IDE version
#define xstr(s) str(s)
#define str(s) #s

void showHex (const byte b, const boolean newline = false, const boolean show0x = true);
void showYesNo (const boolean b, const boolean newline = false);
void commitPage (unsigned long addr, bool showMessage = false);
