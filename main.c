#include <stdio.h>
#include <msp430.h> 
#include <string.h>
#include "main.h"
#include "src/DynamentComms.h"
#include "src/ModbusComms.h"
#include "src/comms.h"
#include "stdbool.h"

/**
 * main.c
 * Considering 1MHz
 */
#define POLL_COUNT              8
volatile int pollCounter = POLL_COUNT;  // Define pollCounter

void clockConfigure(void)
{
    CSCTL0_H = CSKEY_H;               // Unlock CS registers
    CSCTL1 = DCOFSEL_0;               // DCO at 1 MHz
    CSCTL2 = SELA__VLOCLK | SELS__DCOCLK | SELM__DCOCLK;
    CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;  // No dividers
    CSCTL0_H = 0;                      // Lock CS registers
}

void Timer_Init(void)
{
    TA0CCR0 = 62500;             // 1 second (assuming 1MHz / 16)
    TA0CCTL0 = CCIE;             // Enable CCR0 interrupt
    TA0CTL = TASSEL_2 | MC_1 | TACLR | ID_3; // SMCLK, Up mode, /8 divider
}


// Timer flag (set in timer ISR)
volatile bool timerFlag = false;

//--------------------------------------------------------------------------
// Function Prototypes
//--------------------------------------------------------------------------
void RequestGasReading(void);
void ReadingReceived(int status, float value);
void DualReadingReceived(int status, float reading1, float reading2);
void Watchdog(void);
void delay_ms(unsigned int ms);

// These communications functions are assumed to be defined elsewhere:

extern void InitialiseDynamentComms(void);
extern void DynamentCommsHandler(void);
extern void ModbusCommsHandler(void);
extern void RequestLiveData2(void (*cb)(int, float, float));

//--------------------------------------------------------------------------
// Main Function
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

    // Release from reset
    UCA0CTLW0 &= ~UCSWRST;

    // ------Setup IRQ A1 RXIFG------
    UCA1CTLW0 &= ~UCSWRST; // Correctly release from reset

    UCA1IE |= UCRXIE;       // Enable RX interrupt (Do NOT disable it after!)

    // Debugging output
    UART_sendString("UCA1IE: ");
    UART_sendHex(UCA1IE);
    UART_sendString("\n");

    // Manually trigger the interrupt for testing (optional)
    UCA1IFG |= UCRXIFG;  // Force RX ISR to execute
}

//Delay
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
initREDLED(void)
{
    P4DIR |= BIT6;  // Set P1.0 as output
    P4OUT &= ~BIT6; // Turn off LED initially
}
void toggleGLED(void)
{
    P1OUT ^= BIT0;  // Toggle the state of P1.0


}
toggleRLED(void)
{
    P4OUT ^= BIT6;
    delay_ms(1000);
}
void active_all()
{
    clockConfigure();
    uart_init();
    initGLED();
    initREDLED();
    UART_Read();

    //toggleLED();

    UART_sendString("Main Code Running!r\n");
    Timer_Init();

}


//--------------------------------------------------------------------------
// Watchdog Function: Pump/update the watchdog timer
void Watchdog(void)
{

}
//--------------------------------------------------------------------------
// For MSP430, if you are using the watchdog, you can reset it here.

//--------------------------------------------------------------------------
// RequestGasReading: Determines which protocol to use to poll the sensor
//--------------------------------------------------------------------------
void RequestGasReading(void)
{
    // For debugging: output via UART (if available)
    UART_sendString("RequestGasReading called\n");

    if (COMMS_PROTOCOL == DYNAMENT_PROTOCOL)
    {
        UART_sendString("Using Dynament_Protocol\n");
        // Request dual reading; callback function DualReadingReceived will be called
        RequestLiveData2(DualReadingReceived);
        UART_sendString("RequstedLiveData2 passed\n");
    }
    else
    {
        UART_sendString("Using Modbus Protocol\n");
        // Request a measurand reading; callback ReadingReceived will be called
        ReadMeasurand(GAS_READING_MEASURAND, ReadingReceived);
    }
}

//--------------------------------------------------------------------------
// Callback: ReadingReceived - called when a single gas reading is received
//--------------------------------------------------------------------------
void ReadingReceived(int status, float value)
{
    if (status == READ_RESPONSE_VALUE_VALID)
    {
        // Process the valid gas reading (value) here
        UART_sendString("ReadingReceived: Valid reading: ");
        UART_sendFloat(value);
        UART_sendString("r\n");
    }
    else
    {
        UART_sendString("ReadingReceived: Invalid reading, status=%d\n");
        UART_sendFloat(status);
        UART_sendString("\n");
    }
}

//--------------------------------------------------------------------------
// Callback: DualReadingReceived - called when two gas readings are received
//--------------------------------------------------------------------------
void DualReadingReceived(int status, float reading1, float reading2)
{
    UART_sendString("DualReadingReceived DRR\n");
    if (status == READ_RESPONSE_VALUE_VALID)
    {
        //printf("DualReadingReceived: Status=%d, Gas1=%f, Gas2=%f\n", status, reading1, reading2);
        // For example, send one reading to another device over UART (ESP)
        //uart_write_blocking(ESP_UART_ID, (const uint8_t *)&reading1, sizeof(reading1));
        //printf("Sent Successfully to ESP32\n");

        UART_sendString("Callback Executed:  Status =");
        printInt(status);

        UART_sendString("Gas1 = ");
        UART_sendFloat(reading1);
        UART_sendString("Gas2 =");
        UART_sendFloat(reading2);
        UART_sendString("\n");
    }
    else
    {
        //printf("DualReadingReceived: Invalid reading, status=%d\n", status);
        UART_sendString("Invalid reading, status =");
        char buffer[10];
        snprintf(buffer,sizeof(buffer),"%d", status);
        UART_sendString(buffer);
        UART_sendString("\n");
    }
}








// ISRs -------------------------------
#pragma vector=USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void)
{
    toggleGLED();
    UART_sendString("ISR Triggered\n");
    UART_sendString("UCA1IFG: ");
    UART_sendHex(UCA1IFG); // Print the interrupt flag status
    UART_sendString("\n");

    if (UCA1IFG & UCRXIFG)  // Check if RX flag is set
    {
        uint8_t receivedByte = UCA1RXBUF;  // Read received byte
        UART_sendString("Byte Received in ISR: ");
        UART_sendHex(receivedByte);
        UART_sendString("\n");

        // Store in buffer if needed
        g_aucRxBuffer[g_uiRxBufferPut] = receivedByte;
        g_uiRxBufferPut = (g_uiRxBufferPut + 1) % P2P_BUFFER_SIZE;

        // **Clear RX flag to prevent repeated interrupts**
        UCA1IFG &= ~UCRXIFG;
    }
}

// Timer ISR - Handles LED toggle & sensor polling
#pragma vector=TIMER0_A0_VECTOR
__interrupt void Timer_A0_ISR(void)
{
    P4OUT ^= BIT6;  // Toggle LED to indicate ISR is running

    if (pollCounter > 0)
    {
        --pollCounter;
        if (pollCounter <= 0)
        {
            RequestGasReading();  // Send request to sensor (UART)

            pollCounter = POLL_COUNT;
        }
    }
}

void main(void)
{

    WDTCTL = WDTPW | WDTHOLD;         // Stop watchdog timer
    PM5CTL0 &= ~LOCKLPM5;
    active_all();
    __bis_SR_register(GIE); // Enable global interrupts
    __enable_interrupt();


    // Initialize communications

    if (COMMS_PROTOCOL == DYNAMENT_PROTOCOL)
    {
        InitialiseDynamentComms();
    }

    // Main loop
    for( ; ;)
    {
        //toggleGLED();
        //RequestGasReading();
        //toggleRLED();

        UART_sendString("System Running!\r\n");

        if (COMMS_PROTOCOL == DYNAMENT_PROTOCOL)
        {
            DynamentCommsHandler();
        }
        else
        {
            ModbusCommsHandler();
        }
        delay_ms(2000);
    }

}
