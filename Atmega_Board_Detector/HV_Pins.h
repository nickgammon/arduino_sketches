// HV_Pins.h
//
// Pin numbers and commands for high-voltage serial or parallel programming
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


#if HIGH_VOLTAGE_PARALLEL || HIGH_VOLTAGE_SERIAL

  // high-voltage programming commands we can send (when action is LOAD_COMMAND)
  enum {
        CMD_CHIP_ERASE       = 0b10000000,
        CMD_WRITE_FUSE_BITS  = 0b01000000,
        CMD_WRITE_LOCK_BITS  = 0b00100000,
        CMD_WRITE_FLASH      = 0b00010000,
        CMD_WRITE_EEPROM     = 0b00010001,
        CMD_READ_SIGNATURE   = 0b00001000,
        CMD_READ_FUSE_BITS   = 0b00000100,
        CMD_READ_FLASH       = 0b00000010,
        CMD_READ_EEPROM      = 0b00000011,
        CMD_NO_OPERATION     = 0b00000000,
        }; // end of commands

#endif // HIGH_VOLTAGE_PARALLEL || HIGH_VOLTAGE_SERIAL

#if HIGH_VOLTAGE_PARALLEL
  // pin assignments for parallel high-voltage programming
  
// Arduino pins                 Target pins
// ------------                 -----------------------
  const byte dataPins [8] = { 
    6,                        // 14 (PB0) (data bit 0)
    7,                        // 15 (PB1) (data bit 1)
    8,                        // 16 (PB2) (data bit 2)
    9,                        // 17 (PB3) (data bit 3)  
    10,                       // 18 (PB4) (data bit 4)
    11,                       // 19 (PB5) (data bit 5)  
    12,                       // 23 (PC0) (data bit 6)
    13                        // 24 (PC1) (data bit 7)
    };    // end of dataPins

  enum {                                        
      RDY     = A0,           //  3 (PD1) (low means busy)   
      OE      = A1,           //  4 (PD2) (low means output enabled)    
      WR      = A2,           //  5 (PD3) (low means write)    
      BS1     = A3,           //  6 (PD4)     
      XTAL1   = A4,           //  9 (XTAL1)     
      XA0     = A5,           // 11 (PD5)     
      XA1     =  2,           // 12 (PD6)     
      PAGEL   =  3,           // 13 (PD7)     
      BS2     =  4,           // 25 (PC2)     
      VCC     =  5,           // 7 and 20 (VCC and AVCC)
  };  // end of other pins                  
                                            
  
  /* Note: /RESET (Pin 1) is brought to 12V by connecting a transistor and MOSFET (high-side driver)
     via a RC network to (the target) VCC. R = 22k, C = 10 nF. This gives a delay of around 40 uS between
     VCC and /RESET. The transistor turns on the MOSFET, which switches +12V to /RESET.
     For transistor/MOSFET schematic see: http://www.gammon.com.au/images/Arduino/MOSFET_high_side_driver.png
     
          |------------------------------------------------------->  VCC and AVCC
          |
   D5 >---|--/\/\/\/\---|-----------> Transistor ---> MOSFET -----> /RESET
               22k      |
                       ===  10 nF
                        |
                        |
                        V
                       Gnd
       
     Also connect the grounds. Gnd to pins 8 and 22.
     Decoupling capacitors: 0.1 uF between VCC/AVCC (pins 7 and 20) and Gnd.
     Not connected on target: pins 2, 10, 21, 26, 27, 28.
  */
  
  // when XTAL1 is pulsed the settings in XA1 and XA0 control the action
  enum {
       ACTION_LOAD_ADDRESS,
       ACTION_LOAD_DATA,
       ACTION_LOAD_COMMAND,
       ACTION_IDLE
       };    // end of actions

       
#endif //  HIGH_VOLTAGE_PARALLEL

#if HIGH_VOLTAGE_SERIAL

// ATMEL ATTINY 25/45/85 / ARDUINO
//
//             +-\/-+
//    /RESET  1|    |8  VCC  
// (SCI) PB3  2|    |7  PB2 (SDO)
// (N/C) PB4  3|    |6  PB1 (SII) 
//       GND  4|    |5  PB0 (SDI)
//             +----+


// Arduino pins                                  Target pins
// ------------                                  -------------------
enum {
    VCC   = 3,  // VCC and /RESET (see above for comments)  (pin 8)
    SDI   = 4,  // Serial Data Input                --> PB0 (pin 5)
    SII   = 5,  // Serial Instruction Input         --> PB1 (pin 6)
    SDO   = 6,  // Serial Data Output               --> PB2 (pin 7)
    SCI   = 7,  // Serial Clock Input (min. 220nS)  --> PB3 (pin 2)
};

enum {
    SII_LOAD_COMMAND        = 0b01001100,
    SII_LOAD_ADDRESS_LOW    = 0b00001100,
    SII_LOAD_ADDRESS_HIGH   = 0b00011100,
    SII_READ_LOW_BYTE       = 0b01101000,
    SII_READ_HIGH_BYTE      = 0b01111000,
    SII_WRITE_LOW_BYTE      = 0b01100100,
    SII_WRITE_HIGH_BYTE     = 0b01110100,
    SII_LOAD_LOW_BYTE       = 0b00101100,
    SII_LOAD_HIGH_BYTE      = 0b00111100,
    SII_WRITE_EXTENDED_FUSE = 0b01100110,
    SII_PROGRAM_LOW_BYTE    = 0b01101101, 
    SII_PROGRAM_HIGH_BYTE   = 0b01111101,
    SII_READ_EEPROM         = 0b01101000,
    
    // various actions are latched in by ORing in this value
    SII_OR_MASK             = 0b00001100,
   
    };  // end of chip commands
        
#endif // HIGH_VOLTAGE_SERIAL
