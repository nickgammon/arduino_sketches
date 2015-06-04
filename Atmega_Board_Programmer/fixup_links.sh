# /bin/bash

# get rid of old links
rm -v General_Stuff.h
rm -v HV_Parallel_Utils.ino
rm -v HV_Pins.h
rm -v HV_Serial_Utils.ino
rm -v ICSP_Utils.ino
rm -v Programming_Utils.ino
rm -v Signatures.h

# make new ones
ln -v ../Atmega_Hex_Uploader/General_Stuff.h
ln -v ../Atmega_Hex_Uploader/HV_Parallel_Utils.ino
ln -v ../Atmega_Hex_Uploader/HV_Pins.h
ln -v ../Atmega_Hex_Uploader/HV_Serial_Utils.ino
ln -v ../Atmega_Hex_Uploader/ICSP_Utils.ino
ln -v ../Atmega_Hex_Uploader/Programming_Utils.ino
ln -v ../Atmega_Hex_Uploader/Signatures.h

