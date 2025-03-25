/*
 * comms.h
 *
 *  Created on: 10 Mar 2025
 *      Author: tanim
 */

#ifndef SRC_COMMS_H_
#define SRC_COMMS_H_

#include <stdint.h>
#include "main.h"

#define P2P_BUFFER_SIZE  256
extern volatile int frameTimeout;
extern volatile int messageTimeout;
extern volatile bool frameComplete;
extern volatile bool messageTimeOut;


// Circular buffer for UART reception
extern uint8_t g_aucRxBuffer[P2P_BUFFER_SIZE]  __attribute__((aligned(16)));
extern volatile uint16_t g_uiRxBufferGet;
extern volatile uint16_t g_uiRxBufferPut;

enum { p2pRxNone, p2pRxOk, p2pRxError };

//UART initialization function
void p2pTxData(uint8_t* pucData, int len);
void p2pTxByte(uint8_t ucData);
uint8_t p2pRxByte(uint8_t* pucData);

void UART_sendHex(uint8_t byte);
void UART_sendChar(char c);

void UART_sendString(const char *str);

void UART_sendFloat(float value);
// Function to read a bybte from the circular buffer
void UART_sendPointer(void *ptr);

uint8_t UART_Read(void);
void printInt(int num);

//UART interrupt service routine


#endif /* SRC_COMMS_H_ */
