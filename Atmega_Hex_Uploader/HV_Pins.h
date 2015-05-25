
#if HIGH_VOLTAGE_PARALLEL
  // pin assignments for parallel high-voltage programming
  
  // ---------------------------------------------
  //                 Pin on target Atmega328 chip
  // ---------------------------------------------
  
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

  // high-voltage programming commands we can send (when action is ACTION_LOAD_COMMAND)
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
        }; // end of commands
       
#endif //  HIGH_VOLTAGE_PARALLEL
