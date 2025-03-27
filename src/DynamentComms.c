
#include <msp430.h>
#include <stdio.h>   // For debug printing (adjust as needed for your system)
#include "src/DynamentComms.h"
#include"src/ModbusComms.h"
#include "stdbool.h"
#include "src/comms.h"
#include "stdint.h"
#include "uart.h"
#include "studiolib.h"

//------------------------------------------------------------------------------
// External low-level UART functions; these must be implemented elsewhere.
// p2pRxByte returns p2pRxOk when a byte is successfully received.


// Global variables for packet reception and state
bool receivingPacket = false;
uint8_t volatile rxBuffer[DYNAMENT_MAX_PACKET_SIZE];
unsigned int rxCount = 0;
bool DLEReceived = false;
bool waitingForCommand = false;
uint8_t command = 0;
bool EOFReceived = false;
uint16_t rcvCsum = 0;
uint16_t calcCsum = 0;
bool packetComplete = false;
bool csumError = false;
int csumByteReceived = 0;
bool packetNAKed = false;
uint8_t errorCode = 0;
bool packetACKed = false;

// State machine variables
enum
{
    CommsModeIdle,
    CommsModeRequestingLiveDataSimple,
    CommsModeRequestingLiveData2,
};

int currentMode = CommsModeIdle;
GetFloat_CallBack_t readGasCallBack = 0;                // callback pointer to return values to if set to point to a function
GetDualFloat_CallBack_t readDualGasCallBack = 0;        // callback pointer to return dual values to if set to point to a function


//------------------------------------------------------------------------------
// Private function prototypes
uint16_t UpdateChecksum(uint16_t currentCRC, uint8_t newByte);
uint16_t UpdateCRCTab(uint16_t index);
void Reset(void);
void CharReceived(uint8_t chr);
bool SendDynamentPacket(uint8_t cmd, uint8_t variableID, uint8_t dlen, uint8_t *dataPtr);
void ProcessReceivedPacket(void);
void ReadLiveDataResponse(uint8_t *dataPtr, int len);
void ReadLiveData2Response(uint8_t *dataPtr, int len);
void ACKReceived(void);
void NAKReceived(void);
void PacketChecksumError(void);
void PacketTimedOut(void);
void PacketSent(void);
//------------------------------------------------------------------------------

// Public functions
//------------------------------------------------------------------------------
void InitialiseDynamentComms(void)
{
    Reset();                // set all the packet reception variables to their default to await an incoming packet
}



void DynamentCommsHandler(void)
{
    uint8_t ch;
    uint8_t received = 0;

    while(p2pRxByte(&ch) == p2pRxOk)
    {
        received = 1;
        UART_sendString("CharReceived Called with: ");
        UART_sendHex(ch);
        UART_sendString("\n");
        CharReceived(ch);
    }
    if (!received) {
        UART_sendString("DynamentCommsHandler: No Data Processed\n");
    }

    // If a frame timeout occurs (incomplete frame), handle it
    if(frameComplete)
    {
        UART_sendString("Frame Complete\n");
        frameComplete = false;
        PacketTimedOut();
    }

    // Check to see if no response has been received to a previously transmitted packet
    if(messageTimeOut)
    {
        UART_sendString("Message Timeout\n");
        messageTimeOut = false;
    }
}


//------------------------------------------------------------------------------

// Private functions
//------------------------------------------------------------------------------
uint16_t UpdateChecksum(uint16_t currentCRC, uint8_t newByte)
{
    uint16_t retVal;
    if(CSUM_TYPE == CSUM_CRC)
    {
        retVal = (uint16_t)((currentCRC << 8) ^ UpdateCRCTab((uint16_t)((currentCRC >> 8) ^ newByte)));
    }
    else
    {
        retVal = (uint16_t)(currentCRC + newByte);
    }
    return retVal;
}

uint16_t UpdateCRCTab(uint16_t index)
{
    uint16_t uiIndex;
    uint16_t uiCrcValue = 0, uiTempCrcValue;
    uiTempCrcValue = (uint16_t)(((unsigned int)index) << 8);
    for(uiIndex = 0; uiIndex < 8; uiIndex++)
    {
        if (((uiCrcValue ^ uiTempCrcValue) & CRC_VALUE_MASK) > 0)
        {
            uiCrcValue = (uint16_t)((uiCrcValue << 1) ^ CRC_POLYNOMIAL);
        }
        else
        {
            uiCrcValue = (uint16_t)(uiCrcValue << 1);
        }
        uiTempCrcValue = (uint16_t)(uiTempCrcValue << 1);
    }
    return uiCrcValue;
}

/* Function: Reset all teh variables associated with a packet being received - typically called
           when a packet has timed out or been aborted */

void Reset(void)
{
    receivingPacket = false;
    messageTimeOut = 0;
    frameTimeout = 0;
    rxCount = 0;
    DLEReceived = false;
    waitingForCommand = false;
    command = 0;
    EOFReceived = false;
    rcvCsum = 0;
    calcCsum = 0;
    packetComplete = false;
    csumError = false;
    csumByteReceived = 0;
    packetNAKed = false;
    packetACKed = false;
    errorCode = 0;
}

void PacketSent(void)
{
    Reset();
    messageTimeOut = DYNAMENT_RESPONSE_TIMEOUT;
}

// Function Called when a single uint8_t of data is received in order to process it as part of an ongoing packet frame being received
void CharReceived(uint8_t chr)
{

    UART_sendString("CharReceived called with: ");
    UART_sendHex(chr);
    UART_sendString("\n");

    if(rxCount >= DYNAMENT_MAX_PACKET_SIZE)
    {
        Reset();
        return;
    }



    if(chr == DLE && !EOFReceived)
    {
        if(!receivingPacket)
        {
            receivingPacket = true;
            rxCount = 0;
            rxBuffer[rxCount++] = chr;
            calcCsum = UpdateChecksum(calcCsum, chr);
            DLEReceived = true;
        }
        else if(DLEReceived)
        {
            // This is a stuffed DLE character: update checksum but do not add twice.
            calcCsum = UpdateChecksum(calcCsum, chr);
        }
        else
        {
            DLEReceived = true;
            rxBuffer[rxCount++] = chr;
            calcCsum = UpdateChecksum(calcCsum, chr);
        }
    }
    else if(chr == EOF && DLEReceived && !EOFReceived)
    {
        UART_sendString("EOF Received!\n");
        rxBuffer[rxCount++] = chr;
        calcCsum = UpdateChecksum(calcCsum, chr);
        EOFReceived = true;
        csumByteReceived = 0;
        rcvCsum = 0;
    }
    else if(EOFReceived)
    {
        rxBuffer[rxCount++] = chr;
        csumByteReceived++;

        UART_sendString("Checksum byte received: ");
        UART_sendHex(chr);
        UART_sendString("\n");

        if(csumByteReceived >= 2)
        {
            UART_sendString("Received Checksum: ");
            UART_sendHex(rcvCsum);
            UART_sendString("\n");
            rcvCsum = (uint16_t)((rxBuffer[rxCount - 2] * 0x100) + rxBuffer[rxCount - 1]);
            packetComplete = true;
            if(rcvCsum != calcCsum)
            {
                csumError = true;
                UART_sendString("Checksum Error Detected!\n");
            }
            else
            {
                UART_sendString("Checksum Matched!\n");
            }
        }
    }
    else if(packetNAKed)
    {
        errorCode = chr;
        packetComplete = true;
        rxBuffer[rxCount++] = chr;
    }
    else if(receivingPacket)
    {
        if(DLEReceived && rxCount == 1)
        {
            command = chr;
            if(command == NAK)
                packetNAKed = true;
            if(command == ACK)
            {
                packetACKed = true;
                packetComplete = true;
            }
        }
        rxBuffer[rxCount++] = chr;
        calcCsum = UpdateChecksum(calcCsum, chr);
    }

    if(chr != DLE)
    {
        DLEReceived = false;
    }

    frameTimeout = DYNAMENT_FRAME_TIMEOUT;

    if(packetComplete)
    {
        if(packetACKed)
            ACKReceived();
        else if(packetNAKed)
            NAKReceived();
        else if(csumError)
            PacketChecksumError();
        else
            ProcessReceivedPacket();

        Reset();
    }
}

/*
 * Function: Create a packet of data in the Dynament protocol
   Inputs : cmd - the type of packet (read or write variable typically)
            variableID - the id of the variable to be read or written
            dlen - the number of bytes of data within the packet
            dataPtr - pointer to array of bytes of the packet data
   Outputs: Returns true if packet sent else returns false
   */



bool SendDynamentPacket(uint8_t cmd, uint8_t variableID, uint8_t dlen, uint8_t *dataPtr)
{
    unsigned int x;

    uint8_t txBuf[DYNAMENT_MAX_PACKET_SIZE];
    unsigned short txBufPtr = 0;
    uint16_t csum = 0;

    txBuf[txBufPtr++] = DLE;
    csum = UpdateChecksum(csum, DLE);
    txBuf[txBufPtr++] = cmd;
    csum = UpdateChecksum(csum, cmd);
    if(cmd == READ_VAR)
    {
        txBuf[txBufPtr++] = variableID;
        csum = UpdateChecksum(csum, variableID);
    }
    else if(cmd == WRITE_REQUEST)
    {
        txBuf[txBufPtr++] = WRITE_PASSWORD_1;
        csum = UpdateChecksum(csum, WRITE_PASSWORD_1);

        txBuf[txBufPtr++] = WRITE_PASSWORD_2;
        csum = UpdateChecksum(csum, WRITE_PASSWORD_2);

        txBuf[txBufPtr++] = variableID;
        csum = UpdateChecksum(csum, variableID);
    }

    if(dlen > 0)
    {
        if(dlen == DLE)
        {
            txBuf[txBufPtr++] = DLE;
            csum = UpdateChecksum(csum, DLE);
        }
        txBuf[txBufPtr++] = dlen;
        csum = UpdateChecksum(csum, dlen);
        for(x = 0; x < dlen; x++)
        {
            if(dataPtr[x] == DLE)
            {
                txBuf[txBufPtr++] = DLE;
                csum = UpdateChecksum(csum, DLE);
            }
            txBuf[txBufPtr++] = dataPtr[x];
            csum = UpdateChecksum(csum, dataPtr[x]);
        }
    }
    txBuf[txBufPtr++] = DLE;
    csum = UpdateChecksum(csum, DLE);

    txBuf[txBufPtr++] = EOF;
    csum = UpdateChecksum(csum, EOF);

    // Append checksum (two bytes)
    txBuf[txBufPtr++] = (uint8_t)((csum >> 8) & 0x00ff);
    txBuf[txBufPtr++] = (uint8_t)(csum & 0x00ff);

    if(dlen > DYNAMENT_MAX_PACKET_SIZE)
        return false;

    // Transmit packet to serial port
    p2pTxData(txBuf, txBufPtr);
    PacketSent();

    unsigned int idx;
    UART_sendString ("TX: ");
    for (idx=0; idx < txBufPtr; idx++)
        UART_sendHex(txBuf[idx]);
    UART_sendString(" <Tx End>\n");
    //*** END TEST */



    return true;

}

void ProcessReceivedPacket(void)
{
    uint8_t cmd = rxBuffer[1];
    uint8_t len = rxBuffer[2];
    unsigned int x;
    int i;
    //***TEST*** - print out received character for debug purposes
    UART_sendString("Raw Packet :");
    for(i = 0; i<sizeof(rxCount); i++){
        UART_sendHex(rxBuffer[i]);
    }
    UART_sendString("\n");
    //***END TEST*** */
    if(cmd == DAT)      // data packet response to a read variable received
    {
        uint8_t rcvData[200];
        for(x = 0; x < len && (x + 3) < rxCount; x++)
        {
            rcvData[x] = rxBuffer[x + 3];
        }

        switch(currentMode)
        {
            case CommsModeRequestingLiveDataSimple: // waiting for a response to a read live data simple packet
                ReadLiveDataResponse(rcvData, len);
                break;
            case CommsModeRequestingLiveData2:      // waiting for a response to a read live data 2 packet
                ReadLiveData2Response(rcvData, len);
                break;

        }
    }
    else
    {
        // do nothing..
    }
}

// Function: Called when a data response to a read simple live data packet has been received
// Inputs : dataPtr - pointer to the data contents of the packet
//          len - the number of bytes of data within the packet

void ReadLiveDataResponse(uint8_t *dataPtr, int len)
{
    float gasValue = 0;

    // get the status values from the message
    uint16_t statusVal1 = (uint16_t)((dataPtr[3] * 0x100) + dataPtr[2]);

    // get the float value from the two registers that cover the actual reading
    uint32_t intVal = (uint32_t)((dataPtr[7] * 0x1000000) +
                                  (dataPtr[6] * 0x10000) +
                                  (dataPtr[5] * 0x100) +
                                  dataPtr[4]);
    float *fPtr = (float *)&intVal;
    gasValue = *fPtr;

    // call the callback routine to rerpot the received gas reading to the calling layer
    if(currentMode == CommsModeRequestingLiveDataSimple && readGasCallBack != 0)
    {
        if(gasValue < -1 || statusVal1 > 0)
            // any gas reading less than -1 is likely to be an error signal (e.g. sensor warming up or in fault) or any
            // status flags set is likely to be an error too
            readGasCallBack(READ_RESPONSE_VALUE_INVALID, gasValue);
        else
            readGasCallBack(READ_RESPONSE_VALUE_VALID, gasValue);
    }
    currentMode = CommsModeIdle;
    readGasCallBack = 0;
}


// Function: Called when a data response to a read live data 2 packet has been received
// Inputs : dataPtr - pointer to the data contents of the packet
//          len - the number of bytes of data within the packet

void ReadLiveData2Response(uint8_t *dataPtr, int len)
{
    float gasValue1 = 0;
    float gasValue2 = 0;

    // *** TEST ***** -print out received character for debug purpose
    UART_sendString("ReadLiveData2Response: ");
    UART_sendHex(*dataPtr);
    UART_sendString("RLD -- Stage 1\n");

    // Get the status values from the message
    uint16_t statusVal1 = (uint16_t)((dataPtr[3] * 0x100) + dataPtr[2]);
    uint16_t statusVal2 = (uint16_t)((dataPtr[41] * 0x100) + dataPtr[40]);

    // Get the float value for gas reading 1
    uint32_t intVal = (uint32_t)((dataPtr[7] * 0x1000000) +
                                  (dataPtr[6] * 0x10000) +
                                  (dataPtr[5] * 0x100) +
                                  dataPtr[4]);
    float *fPtr = (float *)&intVal;
    gasValue1 = *fPtr;

    // Get the float value for gas reading 2
    intVal = (uint32_t)((dataPtr[15] * 0x1000000) +
                        (dataPtr[14] * 0x10000) +
                        (dataPtr[13] * 0x100) +
                        dataPtr[12]);
    fPtr = (float *)&intVal;
    gasValue2 = *fPtr;

    /* Debuggin: Print currentMode and readGasCallBack */
//    printf("RLD2 -- currentMode = %d, readDualGasCallBack = %X\n", currentMode, readDualGasCallBack); //***TEST***
//    printf("RLD2 -- Status1 = %d, Status2 = %d, Gas1 = %f, Gas2 = %f\n", statusVal1, statusVal2, gasValue1, gasValue2);

    // Call the callback routine to report the received gas reading
    if(currentMode == CommsModeRequestingLiveData2 && readDualGasCallBack != 0)
    {
        UART_sendString("RLD2 -- Stage 2\n");
        if(statusVal1 > 0 || statusVal2 > 0)    // Any status flags set indicate an invalid value
        {
            readDualGasCallBack(READ_RESPONSE_VALUE_INVALID, gasValue1, gasValue2);
            // Reset the callback only after it has been executed
            //printf("RLD2 -- Stage 2.1\n");
        }
        else
        {
            readDualGasCallBack(READ_RESPONSE_VALUE_VALID, gasValue1, gasValue2);
        }
    }
    else
    {
        //printf("RLD2 -- Stage 2.3: currentMode = %d, callback = %p\n", currentMode, (void*)readDualGasCallBack);
    }

    currentMode = CommsModeIdle;
    readDualGasCallBack = 0;
    UART_sendString("RLD2 -- Stage 3\n");
}

/* Function: Called when an ACK paclet is received to a previously transmitted messages e.g. a Write Variable message
              - the data to be written can now be sent */
void ACKReceived(void)
{
    // ACK handling can be implemented here.
}

// Function: Called when a NAK packet to a previously transmitted message is received
void NAKReceived(void)
{
    // call the callback routone with a status PACKET_IINVALID
    if(currentMode == CommsModeRequestingLiveDataSimple && readGasCallBack != 0)
    {
        //printf("PacketTimedOut called, resetting callback.\n");
        readGasCallBack(READ_RESPONSE_INVALID_REGISTER, 0);
    }
    currentMode = CommsModeIdle;
    readGasCallBack = 0;
}

// Function: Called when a received packet has detecged a checksum error
void PacketChecksumError(void)
{
    PacketTimedOut(); // treat the packet as having received no response
}

//Function: Called when no response has been received to a previously transmitted packet
void PacketTimedOut(void)
{
    if(currentMode == CommsModeRequestingLiveDataSimple && readGasCallBack != 0)
    {
        readGasCallBack(READ_RESPONSE_TIMED_OUT, 0);
    }
    currentMode = CommsModeIdle;
    readGasCallBack = 0;
    Reset();
}


// Function: Send the command to read a live data packet to obtain the current gas reading
//Inputs: cb - pointer to callback routine to be called when a result has been determined
void RequestLiveDataSimple(GetFloat_CallBack_t cb)
{

    SendDynamentPacket(READ_VAR, LIVE_DATA_SIMPLE, 0, 0);
    readGasCallBack = cb;
    currentMode = CommsModeRequestingLiveDataSimple;

}

// Function: Send the command to read a live data 2 packet to obtain the current gas reading and status
//Inputs: cb - pointer to callback routine to be called when a result has been determined
void RequestLiveData2(GetDualFloat_CallBack_t cb)
{
    UART_sendString("Setting readDualGasCallBack to: ");
    UART_sendPointer((void *)cb);  // Send the callback pointer value
    //printf("Setting readDualGasCallBack to: %p\n", (void*)cb); //***TEST***
    SendDynamentPacket(READ_VAR, LIVE_DATA_2, 0,0);
    readDualGasCallBack = cb;
    currentMode = CommsModeRequestingLiveData2;

}

/*** end of file ***/

