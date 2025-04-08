#include <msp430.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "comms.h"
#include "DynamentComms.h"
#include "ModbusComms.h"
#include "uart.h"
#include "studiolib.h"
#include "main.h"



void uart_init(void)
{
    // Configure UART pins

    //-----eUSCI_A0 for USB Terminal (P2.0 TX, P2.1 RX) ---------------
    P2SEL1 |= BIT0 | BIT1;      // TX: P2.0, RX: P2.1
    P2SEL0 &= ~(BIT0 | BIT1);

    // Put eUSCI_A0 into reset mode before configuring
    UCA0CTLW0 = UCSWRST;
    UCA0CTLW0 |= UCSSEL__SMCLK;  // Select SMCLK as clock source

    // ---------- eUSCI_A1 for N2O Sensor (P2.5 TX, P2.6 RX) ----------
    P2SEL1 |= BIT5 | BIT6;             // Set P2.5 -> TXD, P2.6 -> RXD (Dynament Sensor)
    P2SEL0 &= ~(BIT5 | BIT6);          // Secondary function

    UCA1CTLW0 = UCSWRST;               // Put eUSCI_A1 in reset mode
    UCA1CTLW0 |= UCSSEL__SMCLK;        // Use SMCLK

    // Baud Rate = 38400 bps @ 1 MHz
    UCA0BRW = 1;
    UCA0MCTLW = UCOS16 | (10 << 4) | (0x00 << 8);  // UCBRFx = 10, UCBRSx = 0x00

    UCA1BRW = 1; // 🔹 Fix: Set Baud Rate for UCA1
    UCA1MCTLW = UCOS16 | (10 << 4) | (0x00 << 8);  // Modulation settings

    // Release from reset
    UCA0CTLW0 &= ~UCSWRST;
    UCA1CTLW0 &= ~UCSWRST; // Correctly release from reset

    // Enable RX interrupt for UCA1
    UCA1IE |= UCRXIE;

    // Debugging output
    DEBUG_STRING("UCA1CTLW0 After Release: ");
    UART_sendHex(UCA1CTLW0);
    DEBUG_STRING("\n");

    DEBUG_STRING("CSCTL1: ");
    UART_sendHex(CSCTL1);
    DEBUG_STRING("\n");

    // Manually trigger the interrupt for testing (optional)
   // UCA1IFG |= UCRXIFG;  // Force RX ISR to execute
}

void delay_ms(unsigned int ms)
{
    while (ms--)
    {
        __delay_cycles(1000);          // 1 ms delay at 1 MHz
    }
}


void initGLED(void)
{
    P1DIR |= BIT0;  // Set P1.0 as output
    P1OUT &= ~BIT0; // Turn off LED initially
}

void initREDLED(void)
{
    P4DIR |= BIT6;  // Set P1.0 as output
    P4OUT &= ~BIT6; // Turn off LED initially
}
void toggleGLED(void)
{
    P1OUT ^= BIT0;  // Toggle the state of P1.0


}
void toggleRLED(void)
{
    P4OUT ^= BIT6;
    delay_ms(1000);
}
