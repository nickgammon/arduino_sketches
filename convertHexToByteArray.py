#!/usr/bin/env python2

# Import initelhex: https://pythonhosted.org/IntelHex/index.html
import sys
import os
import hashlib
from intelhex import IntelHex

def main():
    # Check if one input argument is provided
    if len(sys.argv) != 2:
        print "Usage:", sys.argv[0], "bootloader.hex"
        sys.exit(1)

    # Input arguments
    hexfile = sys.argv[1]

    # Read bootloader data
    bootloader = IntelHex(hexfile)
    bootloader_bin = bootloader.tobinarray()
    #bootloader.dump()

    # Calculate md5sum
    md5 = hashlib.md5()
    fd = open(hexfile, 'rb')
    md5.update(fd.read())
    fd.close()

    # Print header
    print '// File =', hexfile
    print '// Loader start:', hex(bootloader.minaddr()), 'length', len(bootloader_bin)
    print '// MD5 sum =', md5.hexdigest()
    print
    print 'const uint8_t', os.path.splitext(hexfile)[0] + '_hex [] PROGMEM = {'

    # Print data
    line = ""
    for i in range(len(bootloader_bin)):
        line += hex(bootloader_bin[i]) + ', '
        if i % 16 == 15:
            print line
            line = ''

    # Print footer
    print '}; // end of', os.path.splitext(hexfile)[0] + '_hex'

if __name__ == "__main__":
    main()
