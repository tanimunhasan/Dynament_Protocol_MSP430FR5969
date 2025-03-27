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
#include "studiolib.h"
#include <stdarg.h>

volatile uint8_t rxIndex = 0; // Index for buffer

/* Global variables ------------------------------*/
volatile int frameTimeout = 0;  // used to determine when a frame/packet has been received (no data received within a period of time after data has been received)
volatile int messageTimeout = 0; // used to determine when a message has been sent but no response received
volatile bool frameComplete = false;
volatile bool messageTimeOut = false;

volatile uint16_t g_uiCommsTimeout  = 0;
uint8_t RX_Buffer[P2P_BUFFER_SIZE] __attribute__((aligned(16)));
volatile uint16_t head = 0;  // Define it ONCE here
volatile uint16_t tail = 0;  // Define it ONCE here




#pragma vector=TIMER1_A0_VECTOR
__interrupt void Timer_A1_ISR(void)
{
    if (g_uiCommsTimeout > 0)
    {
        g_uiCommsTimeout--;
    }

    if (frameTimeout > 0)
    {
        --frameTimeout;
        if (frameTimeout <= 0)
        {
            frameComplete = true;
        }
    }

    if (messageTimeout > 0)
    {
        --messageTimeout;
        if (messageTimeout <= 0)
        {
            messageTimeOut = true;
        }
    }
}


#pragma vector=USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void)
{
    if (UCA1IFG & UCRXIFG)
    {
        uint8_t receivedByte = UCA1RXBUF;

        // Debugging Output
        UART_sendString("ISR Received: ");
        UART_sendHex(receivedByte);
        UART_sendString("\n");

        // Calculate next buffer position
        uint16_t nextPut = (tail + 1) % P2P_BUFFER_SIZE;

        if (nextPut != head)  // Ensure buffer is not full
        {
            RX_Buffer[tail] = receivedByte;
            tail = nextPut;
        }
        else
        {
            // Buffer Overflow
            UART_sendString("Buffer Overflow!\n");
        }

        UCA1IFG &= ~UCRXIFG;  // Clear RX flag
    }
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

    if (head != tail) {
        *pucData = RX_Buffer[head++];

        if (P2P_BUFFER_SIZE == head  ) {
            head = 0;
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

/* TEST  */

uint8_t p2pRxByte(uint8_t* pucData)
{
    uint8_t ucStatus = 0;

    if (head != tail)  // Check if buffer has data
    {
        *pucData = RX_Buffer[head++];

        if (head >= P2P_BUFFER_SIZE) {
            head = 0;  // Wrap around
        }

        ucStatus = p2pRxOk;
        UART_sendString("p2pRxByte Retrieved: ");
        UART_sendHex(*pucData);
        UART_sendString("\n");
    }
    else
    {
        ucStatus = p2pRxNone;
        UART_sendString("p2pRxByte: No Data\n");
    }

    return ucStatus;
}
/* TEST  */


/*
uint8_t UART_Read(void)
{
    if (head == tail)
    {
        // Buffer is empty
        UART_sendString("UART_Read: Buffer Empty!\n");
        return 0xFF; // Return a special value indicating "no data"
    }
    uint8_t data = RX_Buffer[head];
    head = (head + 1) % P2P_BUFFER_SIZE;
    return data;
}
*/

