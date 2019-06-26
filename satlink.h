//
// This header was created using DataLink_Protocol_V11.pdf
// http://www.gamingenterprisesinc.com/DataLink/DataLink_Protocol_V11.pdf
//

#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <ftdi.h>

#define DEBUG

#ifdef DEBUG
#define debugprintf    printf
#else
#define debugprintf    
#endif

#define VER         "v0.10"

#define BIOS_SIZE   524288
#define BIOS_ADDR   0

// direction flags
#define TO_SAT      0x5A // message is being sent to the saturn from the pc
#define TO_PC       0xA5 // message is being sent to the pc from the saturn

// message opcodes
#define READ_START      0x01 // 1st packet in a read request to the saturn
#define READ_CONT       0x11 // middle packet(s) in a read request
#define READ_END        0x21 // last packet in a read request

#define WRITE            0x09 // write to memory request
#define WRITE_EXECUTE    0x19 // write to memory and execute at address

#define RESP_SUCCESS     0xFF // successful response message from the saturn
#define RESP_ERROR       0x00 // erroor response message from the saturn

#define MAX_DATALEN      191
#define MAX_PACKETLEN    MAX_DATALEN + 7

typedef unsigned char BYTE;
typedef unsigned int DWORD;

// these structures must be byte aligned
#pragma pack(push,1)

// SAT_READ_REQ and SAT_WRITE_RESP are identical structures
typedef struct _SAT_READ_REQ
{
    BYTE dir;           // Either TO_SAT or TO_PC
    BYTE packetLength;  // length of packet and data. not counting dir byte or checksum byte
    BYTE opcode;
    DWORD address;      // address to read or write from, big-endian
    BYTE dataLength;    // number of bytes to read or write
    BYTE checksum;      // additive checksum  
} SAT_READ_REQ, *PSAT_READ_REQ, SAT_WRITE_RESP, *PSAT_WRITE_RESP;

// SAT_READ_RESP and SAT_WRITE_REQ are identical structures
typedef struct _SAT_READ_RESP
{
    BYTE dir;           // Either TO_SAT or TO_PC
    BYTE packetLength;  // length of packet and data. not counting dir byte or checksum byte
    BYTE opcode;
    DWORD address;      // address to read or write from, big-endian
    BYTE dataLength;    // number of bytes to read or write
    BYTE data[];        // requested data from read request. last byte is checksum of all bytes except dir and checksum
}SAT_READ_RESP, *PSAT_READ_RESP, SAT_WRITE_REQ, *PSAT_WRITE_REQ;

#pragma pack(pop)

// helper functions
void dumpPacket(BYTE* packet);
BYTE calculateChecksum(BYTE* packet);
int validateChecksum(BYTE* packet);
int readSatMemory(struct ftdi_context* ftdic, BYTE* outBuffer, DWORD address, DWORD numBytes); // reads numBytes at address into outBuffer
int writeSatMemory(struct ftdi_context* ftdic, DWORD address, BYTE* inBuffer, DWORD numBytes); // write numBytes at address from inBuffer
int writeSatMemoryAndExecute(struct ftdi_context* ftdic, DWORD address, BYTE* inBuffer, DWORD numBytes); // write numBytes at address from inBuffer then jumps to address
int readSatBios(struct ftdi_context* ftdic, BYTE* outBuffer); // reads the saturn's bios into outBuffer. outBuffer must be BIOS_SIZE

