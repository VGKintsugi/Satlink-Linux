## Sega Saturn USB DataLink (Linux)

Satlink is a Linux utility that communicates with the [Saturn USB DataLink](http://www.gamingenterprisesinc.com/DataLink/) to read memory, write memory, and launch Saturn homebrew. Satlink was developed because the original DataLink software was Windows only. 

## Examples
```Bash
./satlink
Satlink v0.10
Satlink Usage:
satlink -b output.bin
    (dumps bios to output.bin)
satlink -r hex_address count output.bin
    (reads count bytes from hex_address to output.bin)
satlink -w hex_address input.bin
    (writes input.bin to hex_address)
satlink -e hex_address sl.bin
    (writes sl.bin to hex_address and then executes it)

Examples:
    satlink -b bios.bin
    satlink -e 0x06004000 sl.bin
```

### Compiling
run 'make'

### Compiling Issues
Make sure you have the "libftdi1" and "libftdi1-dev" packages installed.  
Edit the Makefile to make sure the include path to libftdh.h is correct

### Credits
- [Sega Saturn DataLink Protocol Specification V1.1](http://www.gamingenterprisesinc.com/DataLink/DataLink_Protocol_V11.pdf)
