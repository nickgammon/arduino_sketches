// General_Stuff.h
//
// Variables and defines required by all sketches
//
// Author: Nick Gammon

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
