/*
 * stdiolib.h
 *
 *  Created on: 27 Mar 2025
 *      Author: tanim
 */

#ifndef SRC_STUDIOLIB_H_
#define SRC_STUDIOLIB_H_

#include <stdint.h>
#include <stdio.h>

void UART_sendHex(uint8_t byte);
void UART_sendChar(char c);

void UART_sendString(const char *str);

void UART_sendFloat(float value);
// Function to read a bybte from the circular buffer
void UART_sendPointer(void *ptr);
void printInt(int num);

#endif /* SRC_STUDIOLIB_H_ */
