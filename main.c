#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <ftdi.h>
#include <string.h>
#include <sys/stat.h>
#include "satlink.h"

#define B375000 375000

void usage();
int openFtdiDevice(struct ftdi_context* ftdic, int interface);
int dumpBiosToFile(struct ftdi_context* ftdic, char* filename);
int dumpMemoryToFile(struct ftdi_context* ftdic, char* filename, DWORD address, DWORD count);
int writeFileToMemory(struct ftdi_context* ftdic, char* filename, DWORD address, BYTE execute);
int closeFtdiDevice(struct ftdi_context* ftdic);

void usage()
{  
    printf("Satlink Usage:\n");
    printf("satlink -b output.bin\n \t(dumps bios to output.bin)\n");
    printf("satlink -r hex_address count output.bin\n \t(reads count bytes from hex_address to output.bin)\n");
    printf("satlink -w hex_address input.bin\n \t(writes input.bin to hex_address)\n");
    printf("satlink -e hex_address sl.bin\n \t(writes sl.bin to hex_address and then executes it)\n");
    
    printf("\nExamples:\n");
    printf("\tsatlink -b bios.bin\n");
    printf("\tsatlink -e 0x06004000 sl.bin\n");
    
    exit(-1);
    
    return;  
}

int main(int argc, char **argv)
{
    struct ftdi_context ftdic;    
    int interface = 0;       
    char* filename;
    char command;
    int count;
    DWORD address;
    int result;
    int execute;
    
    printf("Satlink %s\n", VER);    
    
    // validate cla
    if(argc < 2 || argv[1][0] != '-')
    {
        usage();
        return -1;
    }
    command = argv[1][1];
    execute = 0;
    
    // open the FTDI device
    result = openFtdiDevice(&ftdic, interface);
    if(result != 0)
    {
        printf("Failed to open FTDI device.\n");
        return -1;
    }     
    
    switch(command)
    {
        case 'b':
        {
            // satlink -b bios.bin
            if(argc < 3)
            {
                printf("Invalid syntax\n");
                usage();        
            }
            filename = argv[2];
            
            printf("Dumping bios to %s\n", filename);
            result = dumpBiosToFile(&ftdic, filename);
            if(result != 0)
            {
                printf("Failed to dump bios!!\n");
                break;
            }        
            
            printf("Successfully dumped bios to %s\n", filename);        
            break;
        }    
        case 'r':
        {
            // satlink -r 0x06004000 1024 bios.bin
            if(argc < 5)
            {
                printf("Invalid syntax\n");
                usage();        
            }
            
            // read the hex address
            result = sscanf(argv[2], "0x%x", &address);
            if(result != 1)
            {
                printf("Failed to convert %s into a valid address!!\n", argv[2]);
                usage();        
            }
            
            // read the count
            count = atoi(argv[3]);
            
            // filename 
            filename = argv[4];        
            printf("Reading %d bytes from address 0x%x to %s \n", count, address, filename);      
            
            result = dumpMemoryToFile(&ftdic, filename, address, count);
            if(result != 0)
            {
                printf("Failed to dump bios!!\n");
                break;
            }        
            
            printf("Successfully dumped memory to %s\n", filename);        
            break;
            
            break;
        }
        case 'e':
        {
            execute = 1;
            // intentional fall through to write 
        }
        case 'w':
        {
            // "satlink -w 0x06004000 input.bin        
            if(argc < 4)
            {
                printf("Invalid syntax\n");
                usage();        
            }
            
            // read the hex address
            result = sscanf(argv[2], "0x%x", &address);
            if(result != 1)
            {
                printf("Failed to convert %s into a valid address!!\n", argv[2]);
                usage();        
            }
            
            // filename 
            filename = argv[3];        
            printf("Writing %s to 0x%x\n", filename, address);
            
            result = writeFileToMemory(&ftdic, filename, address, execute);
            if(result != 0)
            {
                printf("Failed to write file to memory!!\n");
                break;          
            }        
            break;
        }
        
        default: 
        {
            printf("Invalid syntax\n");
            usage();        
        }      
    };
    
    // close the FTDI device
    closeFtdiDevice(&ftdic);
    return 0;
}

int openFtdiDevice(struct ftdi_context* ftdic, int interface)
{
    int vid = 0x0403; // vendor id
    int pid = 0x6001; // device idnt f;
    int f;
    
    // Init  
    if (ftdi_init(ftdic) < 0)  
    {
        printf("ftdi_init failed\n");
        return EXIT_FAILURE;
    }
    
    // Select first interface
    ftdi_set_interface(ftdic, interface);
    
    // Open device
    f = ftdi_usb_open(ftdic, vid, pid);
    if (f < 0)
    {
        printf("unable to open ftdi device: %d (%s)\n", f, ftdi_get_error_string(ftdic));
        return -1;
    }
    
    // Set baudrate
    f = ftdi_set_baudrate(ftdic, 375000);
    if (f < 0)
    {
        printf("unable to set baudrate: %d (%s)\n", f, ftdi_get_error_string(ftdic));
        return -1;
    }
    
    // set the line property
    // Data Bits: 8, Stop Bits: 2, Parity Bits: None    
    f = ftdi_set_line_property(ftdic, BITS_8, STOP_BIT_2, NONE);
    if(f < 0)
    {
        printf("unable to set line property: %d (%s)\n", f, ftdi_get_error_string(ftdic));
        return -1;
    }
    
    return 0;
}

int closeFtdiDevice(struct ftdi_context* ftdic)
{     
    ftdi_usb_close(ftdic);
    ftdi_deinit(ftdic);
    
    return 0;
}

// writes the count bytes of address to filename
// return 0 for success;
int dumpMemoryToFile(struct ftdi_context* ftdic, char* filename, DWORD address, DWORD count)
{
    FILE* outFile;
    BYTE* fileBuf;
    int result;    
    
    fileBuf = malloc(count);
    if(fileBuf == 0)
    {
        printf("Failed to allocate filebuffer!!\n");
        return -1;      
    } 
    memset(fileBuf, 0, count);
    
    outFile = fopen(filename, "w");
    if(outFile == NULL)
    {
        printf("Failed to open %s for writing!!\n", filename);
        return -2;
    }
    
    result = readSatMemory(ftdic, fileBuf, address, count);
    if(result != 0)
    {
        printf("readSatMemory failed!!\n");
        return -3;
    }
    
    result = fwrite(fileBuf, 1, count, outFile);
    if(result != count)
    {
        printf("Didn't write enough bytes!!\n");
        return -4;
    } 
    
    fclose(outFile);    
    return 0;  
}

int dumpBiosToFile(struct ftdi_context* ftdic, char* filename)
{
    // this is just a wrapper for dump memory
    return dumpMemoryToFile(ftdic, filename, BIOS_ADDR, BIOS_SIZE);
}

int writeFileToMemory(struct ftdi_context* ftdic, char* filename, DWORD address, BYTE execute)
{
    FILE* inFile;
    BYTE* fileBuf;
    struct stat status;
    int result;    
    DWORD count = 0;
    
    // get the filesize
    result = stat(filename, &status);
    if(result != 0)
    {
        printf("Failed to stat %s\n", filename);
        return -1;
    }
    count = status.st_size;
    
    // open the file for reading
    inFile = fopen(filename, "r");
    if(inFile == NULL)
    {
        printf("Failed to open %s for writing!!\n", filename);
        return -2;
    }      
    
    fileBuf = malloc(count);
    if(fileBuf == 0)
    {
        printf("Failed to allocate filebuffer!!\n");
        return -3;      
    } 
    memset(fileBuf, 0, count);
    
    printf("The file is %d bytes\n", count);
    
    // read the file to the buffer
    result = fread(fileBuf, 1, count, inFile);
    if(result != count)
    {
        printf("Didn't read enough bytes!!\n");
        return -4;
    }     
    
    if(execute)
    {      
        result = writeSatMemoryAndExecute(ftdic, address, fileBuf, count);
    }
    else
    {
        result = writeSatMemory(ftdic, address, fileBuf, count);
    }
    if(result != 0)
    {
        printf("readSatMemory failed!!\n");
        return -3;
    }
    
    free(fileBuf);
    fclose(inFile);    
    return 0;  
    
    
}
