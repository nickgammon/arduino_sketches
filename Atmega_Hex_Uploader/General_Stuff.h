
unsigned long pagesize;
unsigned long pagemask;
unsigned long oldPage;

// count errors
unsigned int errors;

const unsigned long NO_PAGE = 0xFFFFFFFF;

unsigned int progressBarCount;

      
// number of items in an array
#define NUMITEMS(arg) ((unsigned int) (sizeof (arg) / sizeof (arg [0])))

// stringification for Arduino IDE version
#define xstr(s) str(s)
#define str(s) #s

