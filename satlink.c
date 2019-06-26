#include "satlink.h"

void dumpPacket(BYTE* packet)
{
    BYTE packetLength = packet[1];
    BYTE i;
    
    printf("Packet: ");
    
    // include the dir and checksum field
    for(i = 0; i < packetLength + 2; i++)
    {
        if(i % 10 == 0)
        {
            printf("\n");
        }
        printf("0x%02x ", packet[i]);    
    }
    
    printf("\n");  
}

// calculates and return an additive checksum of all bytes in the packet excluding the dir and checksum byte
BYTE calculateChecksum(BYTE* packet)
{
    BYTE checkSum = 0;
    BYTE packetLength = 0;
    BYTE i;
    
    // the 2nd byte is always the packetLength
    packetLength = packet[1];
    
    // packet length does not include the dir field or the ending checksum byte
    for(i = 0; i < packetLength; i++)
    {
        checkSum += packet[i+1];
    }   
    
    return checkSum;  
}

// calculates and validates a checksum on the packetLength
// the checksum is stored in the last byte in the packetLength
// Returns 0 for success, <0 for error
int validateChecksum(BYTE* packet)
{
    BYTE checkSum = 0;
    BYTE packetLength = 0;
    BYTE i;
    
    // the 2nd byte is always the packetLength
    packetLength = packet[1];
    
    // packet length does not include the dir field or the ending checksum byte
    for(i = 0; i < packetLength; i++)
    {
        checkSum += packet[i+1];
    }   
    
    // verify the checksum
    if(checkSum != packet[packetLength+1])
    {
        return -1;
    }
    
    return 0;
}

// reads numBytes at address from saturn into outbuffer
// outBuffer must be atleast numBytes length
// Maximum bytes to request in a single packet is MAX_DATALEN.
// All read requests must have atleast two packets (a READ_START and a READ_END)
// Returns 0 for success, <0 for error
int readSatMemory(struct ftdi_context* ftdic, BYTE* outBuffer, DWORD address, DWORD numBytes)
{
    SAT_READ_REQ readReq;
    PSAT_READ_RESP readResp;
    BYTE buffer[255];    
    int bytesSent;
    int bytesRecv;
    int bytesRead;
    int result;
    
    // validate numBytes
    if(numBytes < 4)
    {
        printf("readSatMemory: numBytes must be atleast 4.\n");
        return -1;
    }
    
    // this is how many bytes we have read from the memory so far
    bytesRead = 0;    
    
    // initialize the read request
    memset(&readReq, 0, sizeof(readReq));
    readReq.dir = TO_SAT;
    readReq.packetLength = sizeof(readReq) - 2; // subtract dir and checksum
    
    while(numBytes)
    {    
        // first iteration through loop
        if(bytesRead == 0)
        {  
            // read always start with a READ_START
            readReq.opcode = READ_START;        
            if(numBytes <= MAX_DATALEN)
            {
                // we need to split this up into two requests even though it could fit in one
                readReq.dataLength = numBytes/2;          
            }
            else
            {
                // we can only read MAX_DATALEN bytes at a time
                readReq.dataLength = MAX_DATALEN;
            }        
        }
        else
        {    // since we are not the 1st packet, determine if we are either the last or a middle packet
            if(numBytes <= MAX_DATALEN)
            {
                //this is the last packet
                readReq.dataLength = numBytes;          
                readReq.opcode = READ_END;
            }
            else
            {
                // this is not the last packet
                readReq.dataLength = MAX_DATALEN;
                readReq.opcode = READ_CONT;          
            }
        }
        
        // add the address to read and calculate the checksum of the packet
        readReq.address = ntohl(address+bytesRead);        
        readReq.checksum = calculateChecksum((BYTE*)&readReq);
        
        //printf("The checksum is %d\n", readReq.checksum);
        dumpPacket((BYTE*)&readReq);
        
        // send packet to the device
        bytesSent = ftdi_write_data (ftdic, (BYTE*)&readReq, readReq.packetLength + 2);
        if(bytesSent < 0)
        {
            printf("readSatMemory: Failed to write to the device (%s)\n", ftdi_get_error_string(ftdic));
            return -2;    
        }
        printf("Wrote %d bytes!!\n", bytesSent);      
        
        // Read data forever
        printf("waiting for read!!\n");
        do
        {
            bytesRecv = ftdi_read_data(ftdic, buffer, sizeof(buffer));
            if(bytesRecv == 0)
            {
                //printf("recved 0\n");
                usleep(1);
            }
        }
        while(bytesRecv == 0);
        
        if(bytesRecv <= 0)
        {
            printf("readSatMemory: Failed to read data!!\n");
            return -3;      
        }
        else if(bytesRecv < sizeof(SAT_READ_REQ))
        {
            printf("readSatMemory: Didn't recieve enough bytes in response!!\n");
            return -4;
        }
        
        // packet looks sane, let's validate the checksum
        result = validateChecksum((BYTE*)buffer);
        if(result != 0)
        {
            printf("readSatMemory: failed to validate checksum for packet!!\n");
            return -5;
        }
        
        // we have enough bytes to validate the buffer    
        dumpPacket((BYTE*)buffer);
        
        readResp = (PSAT_READ_RESP)buffer;
        
        // validates the length that came back
        if(bytesRecv < readResp->packetLength + 2 || readResp->packetLength > MAX_PACKETLEN)
        {
            printf("readSatMemory: BytesRecv is less than packetLength!!\n");
            return -6;
        }
        
        if(bytesRecv < readResp->dataLength + 9 || readResp->dataLength > MAX_DATALEN)
        {
            printf("readSatMemory: BytesRecv is less than dataLength!!\n");
            return -7;
        }
        
        if(numBytes < readResp->dataLength)
        {
            printf("readSatMemory: Not enough bytes left in recv buffer!!\n");
            return -8;
        }
        
        // verify that the packet was a success packet
        if(readResp->dir != TO_PC || readResp->opcode != RESP_SUCCESS)
        {
            printf("readSatMemory: Packet was error response!!\n");
            return -9;
        }
        
        // copy the requested bytes into the buffer
        memcpy(outBuffer + bytesRead, readResp->data, readResp->dataLength);
        
        // increment counters
        numBytes -= readResp->dataLength;
        bytesRead += readResp->dataLength;
        printf("%d bytes remaining\n", numBytes);
        
        
    } // while()        
    
    return 0;
}

// reads the saturn's bios into outBuffer. outBuffer must be BIOS_SIZE
int readSatBios(struct ftdi_context* ftdic, BYTE* outBuffer)
{
    int result;
    
    // this is simply a wrapper for readsatmory. 
    result = readSatMemory(ftdic, outBuffer, BIOS_ADDR, BIOS_SIZE);
    if(result != 0)
    {
        printf("Failed to read BIOS!!\n");
        return -1;      
    }   
    
    return result;    
}

// write numBytes at address from inBuffer
int writeSatMemory(struct ftdi_context* ftdic, DWORD address, BYTE* inBuffer, DWORD numBytes)
{
    PSAT_WRITE_REQ writeReq;
    SAT_WRITE_RESP writeResp;
    BYTE buffer[255];    
    int bytesSent;
    int bytesRecv;
    int bytesWritten;
    int result;    
    
    // validate numBytes
    if(numBytes == 0)
    {
        printf("writeSatMemory: numBytes must be greater than zero.\n");
        return -1;
    }
    
    // this is how many bytes we have written to memory so far
    bytesWritten = 0;
    writeReq = (PSAT_WRITE_REQ)buffer;
    
    while(numBytes)
    {    
        // initialize the read request
        memset(buffer, 0, sizeof(buffer));
        writeReq->dir = TO_SAT;    
        writeReq->opcode = WRITE;
        
        if(numBytes <= MAX_DATALEN)
        {
            //this is the last packet
            writeReq->dataLength = numBytes;                  
        }
        else
        {
            // this is not the last packet
            writeReq->dataLength = MAX_DATALEN;        
        }
        
        // packetlength includes the header but does not include the dir or checksum bytes
        writeReq->packetLength = writeReq->dataLength + sizeof(SAT_WRITE_RESP) - 2;
        
        // add the address to write to
        writeReq->address = ntohl(address+bytesWritten);      
        
        // copy the bytes to write in
        memcpy(writeReq->data, inBuffer+bytesWritten, writeReq->dataLength);
        
        // the checksum is the last byte in the packet
        writeReq->data[writeReq->dataLength] = calculateChecksum((BYTE*)writeReq);
        
        //printf("The checksum is %d\n", readReq.checksum);
        dumpPacket((BYTE*)writeReq);
        
        // send packet to the device
        bytesSent = ftdi_write_data (ftdic, (BYTE*)writeReq, writeReq->packetLength + 2);
        if(bytesSent < 0)
        {
            printf("writeSatMemory: Failed to write to the device (%s)\n", ftdi_get_error_string(ftdic));
            return -2;    
        }
        printf("Wrote %d bytes!!\n", bytesSent);      
        
        // Read data forever
        printf("waiting for read!!\n");
        do
        {
            bytesRecv = ftdi_read_data(ftdic, (BYTE*)&writeResp, sizeof(writeResp));
            if(bytesRecv == 0)
            {
                printf("recved 0\n");
                usleep(1);
            }
        }
        while(bytesRecv == 0);
        
        
        if(bytesRecv <= 0)
        {
            printf("writeSatMemory: Failed to read data!!\n");
            return -3;      
        }
        else if(bytesRecv < sizeof(SAT_WRITE_RESP))
        {
            printf("writeSatMemory: Didn't recieve enough bytes in response!!\n");
            return -4;
        }        
        
        // validates the length that came back
        if(bytesRecv != writeResp.packetLength + 2)
        {
            printf("writeSatMemory: BytesRecv is less than packetLength!!\n");
            return -6;
        }
        
        // we have enough bytes to validate the buffer    
        dumpPacket((BYTE*)&writeResp);
        
        // packet looks sane, let's validate the checksum
        result = validateChecksum((BYTE*)&writeResp);
        if(result != 0)
        {
            printf("writeSatMemory: failed to validate checksum for packet!!\n");
            return -5;
        }
        
        if(writeResp.dataLength != 0)
        {
            printf("writeSatMemory: BytesRecv is less than dataLength!!\n");
            return -7;
        }
        
        // verify that the packet was a success packet
        if(writeResp.dir != TO_PC || writeResp.opcode != RESP_SUCCESS)
        {
            printf("writeSatMemory: Packet was error response!!\n");
            return -9;
        }
        
        // increment counters
        numBytes -= writeReq->dataLength;
        bytesWritten += writeReq->dataLength;
        printf("%d bytes remaining\n", numBytes);    
        
    } // while()        
    
    return 0;
}

// write numBytes at address from inBuffer then jumps to address
int writeSatMemoryAndExecute(struct ftdi_context* ftdic, DWORD address, BYTE* inBuffer, DWORD numBytes)
{
    PSAT_WRITE_REQ writeReq;
    SAT_WRITE_RESP writeResp;
    BYTE buffer[255];    
    int bytesSent;
    int bytesRecv;
    int result;    
    
    
    
    // validate numBytes
    if(numBytes > MAX_DATALEN)
    {
        // since the amount to write and execute is greater than the MAX_DATALEN we start writing the initial payload
        // with the regular write function
        result = writeSatMemory(ftdic, address + MAX_DATALEN, inBuffer + MAX_DATALEN, numBytes - MAX_DATALEN);
        if(result != 0)
        {
            printf("writeSatMemoryAndExecute: failed to write initial payload!!\n");
            return -1;
        }
        
        // we should have exactly one full packet left now
        numBytes = MAX_DATALEN;    
    }
    
    printf("Returned from writeSatmemoryAndExecute\n");
    
    writeReq = (PSAT_WRITE_REQ)buffer;
    
    // initialize the read request
    memset(writeReq, 0, sizeof(buffer));
    writeReq->dir = TO_SAT;    
    writeReq->opcode = WRITE_EXECUTE;
    
    //this is the last packet
    writeReq->dataLength = numBytes;                  
    
    // packetlength includes the header but does not include the dir or checksum bytes
    writeReq->packetLength = writeReq->dataLength + sizeof(SAT_WRITE_RESP) - 2;
    
    // add the address to write to
    writeReq->address = ntohl(address);      
    
    memcpy(writeReq->data, inBuffer, writeReq->dataLength);
    
    // the checksum is the last byte in the packet
    writeReq->data[writeReq->dataLength] = calculateChecksum((BYTE*)writeReq);
    
    //printf("The checksum is %d\n", readReq.checksum);
    dumpPacket((BYTE*)writeReq);
    
    // send packet to the device
    bytesSent = ftdi_write_data (ftdic, (BYTE*)writeReq, writeReq->packetLength + 2);
    if(bytesSent < 0)
    {
        printf("writeSatMemory: Failed to write to the device (%s)\n", ftdi_get_error_string(ftdic));
        return -2;    
    }
    printf("Wrote %d bytes!!\n", bytesSent);      
    
    // Read data forever
    printf("waiting for read!!\n");
    do
    {
        bytesRecv = ftdi_read_data(ftdic, (BYTE*)&writeResp, sizeof(writeResp));
        if(bytesRecv == 0)
        {
            printf("recved 0\n");
            usleep(1);
        }
    }
    while(bytesRecv == 0);
    
    if(bytesRecv <= 0)
    {
        printf("writeSatMemory: Failed to read data!!\n");
        return -3;      
    }
    else if(bytesRecv < sizeof(SAT_WRITE_RESP))
    {
        printf("writeSatMemory: Didn't recieve enough bytes in response!!\n");
        return -4;
    }        
    
    // validates the length that came back
    if(bytesRecv != writeResp.packetLength + 2)
    {
        printf("writeSatMemory: BytesRecv is less than packetLength!!\n");
        return -6;
    }
    
    // we have enough bytes to validate the buffer    
    dumpPacket((BYTE*)&writeResp);
    
    // packet looks sane, let's validate the checksum
    result = validateChecksum((BYTE*)&writeResp);
    if(result != 0)
    {
        printf("writeSatMemory: failed to validate checksum for packet!!\n");
        return -5;
    }
    
    if(writeResp.dataLength != 0)
    {
        printf("writeSatMemory: BytesRecv is less than dataLength!!\n");
        return -7;
    }
    
    // verify that the packet was a success packet
    if(writeResp.dir != TO_PC || writeResp.opcode != RESP_SUCCESS)
    {
        printf("writeSatMemory: Packet was error response!!\n");
        return -9;
    }    
    
    return 0;
}

