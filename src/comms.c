/*
 * comms.c
 *
 *  Created on: 7 Mar 2025
 *      Author: Sayed
 */
#include <msp430.h>
#include "stdint.h"
#include "stdbool.h"
#include "comms.h"
#include "DynamentComms.h"
#include "ModbusComms.h"
#include "main.h"
#include <stdio.h>

#include <stdarg.h>



/* Global variables ------------------------------*/
volatile int frameTimeout = 0;  // used to determine when a frame/packet has been received (no data received within a period of time after data has been received)
volatile int messageTimeout = 0; // used to determine when a message has been sent but no response received
volatile bool frameComplete = false;
volatile bool messageTimeOut = false;


uint8_t g_aucRxBuffer[P2P_BUFFER_SIZE] __attribute__((aligned(16)));
volatile uint16_t g_uiRxBufferGet = 0;  // Define it ONCE here
volatile uint16_t g_uiRxBufferPut = 0;  // Define it ONCE here

void UART_sendChar(char c)
{
    // Wait for the transmit buffer to be ready
    while (!(UCA0IFG & UCTXIFG));
    // Transmit the character
    UCA0TXBUF = c;
}

void UART_sendString(const char *str)
{
    while (*str)
    {
        UART_sendChar(*str++);
    }
}

void UART_sendFloat(float value)
{
    char buffer[20];
    snprintf(buffer, sizeof(buffer), "%.2f", value);
    UART_sendString(buffer);
}

void UART_sendHex(uint8_t byte)
{
    // Declare a buffer for the hex string (2 characters + null terminator)
    char hexBuffer[3];

    // Convert the high nibble (upper 4 bits) to a hex character
    hexBuffer[0] = (byte >> 4) > 9 ? (byte >> 4) + 'A' - 10 : (byte >> 4) + '0';

    // Convert the low nibble (lower 4 bits) to a hex character
    hexBuffer[1] = (byte & 0x0F) > 9 ? (byte & 0x0F) + 'A' - 10 : (byte & 0x0F) + '0';

    // Null terminate the string
    hexBuffer[2] = '\0';

    // Send the two characters over UART (one at a time)
    UART_sendString(hexBuffer);
}

// Function : transmit a given number of bytes of data
// Inputs : pucData - the array of data to send
//          len - the number of bytes to send
void p2pTxData(uint8_t* pucData, int len) {
    unsigned int x;
    for (x = 0; x < len; x++) {
        p2pTxByte(pucData[x]);
    }
}

// Function : Transmit a single byte of data
// Inputs : ucData - the data byte to send
void p2pTxByte(uint8_t ucData) {
    while (!(UCA1IFG & UCTXIFG)); // Wait until the TX buffer is ready
    UCA1TXBUF = ucData;           // Send the byte
}


// Function : Reads the next byte of received data if any are waiting to be read
// Inputs : pucData - pointer to a byte to transfer the next received byte to
// Output : Returns p2pRxOk if a byte has been received else returns p2pRxNone if
//          there is no data waiting to be read
/*
uint8_t p2pRxByte(uint8_t* pucData)
{
    uint8_t ucStatus = 0;

    if (g_uiRxBufferGet != g_uiRxBufferPut) {
        *pucData = g_aucRxBuffer[g_uiRxBufferGet++];

        if (P2P_BUFFER_SIZE == g_uiRxBufferGet  ) {
            g_uiRxBufferGet = 0;
        }
        else
        {
            // Nothing to do
        }

        ucStatus = p2pRxOk;
    }
    else
    {
        ucStatus = p2pRxNone;
    }

    return ucStatus;
}
*/

uint8_t p2pRxByte(uint8_t* pucData)
{
    if (g_uiRxBufferGet == g_uiRxBufferPut) {
        UART_sendString("p2pRxByte: No data available!\n");
        return p2pRxNone;
    }

    *pucData = g_aucRxBuffer[g_uiRxBufferGet];
    g_uiRxBufferGet = (g_uiRxBufferGet + 1) % P2P_BUFFER_SIZE;

    UART_sendString("p2pRxByte: Read byte ");
    UART_sendHex(*pucData);
    UART_sendString("\n");

    return p2pRxOk;
}


void UART_sendPointer(void *ptr)
{
    // Cast the pointer to an unsigned long to ensure it's correctly formatted for transmission
    unsigned long ptrValue = (unsigned long)ptr;
    int i;
    // Send the pointer value byte by byte
    for (i = sizeof(ptrValue) - 1; i >= 0; i--) {
        UART_sendHex((uint8_t)(ptrValue >> (i * 8)));  // Shift the pointer to get each byte
    }

    // Optionally, send a newline or other separator to make it easier to read
    UART_sendString("\n");
}

uint8_t UART_Read(void)
{
    if (g_uiRxBufferGet == g_uiRxBufferPut)
    {
        // Buffer is empty
        return 0; // Or implement a blocking mechanism
    }
    uint8_t data = g_aucRxBuffer[g_uiRxBufferGet];
    g_uiRxBufferGet = (g_uiRxBufferGet + 1) % P2P_BUFFER_SIZE;
    return data;
}

void printInt(int num) {
    char buffer[20];  // Assuming the integer won't be larger than 20 digits (which is a huge number)
    int index = 0;

    if (num == 0) {
        UART_sendChar('0');
        return;
    }

    if (num < 0) {
        UART_sendChar('-');
        num = -num;  // Make the number positive for further processing
    }

    // Convert the integer to a string (in reverse order)
    while (num > 0) {
        buffer[index++] = (num % 10) + '0'; // Get the last digit as a character
        num = num / 10; // Remove the last digit
    }
    int i;
    // Print the number in the correct order
    for (i = index - 1; i >= 0; i--) {
        UART_sendChar(buffer[i]);
    }
}

