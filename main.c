#include <stdio.h>
#include <msp430.h> 
#include <string.h>
#include "main.h"
#include "src/DynamentComms.h"
#include "src/ModbusComms.h"
#include "src/comms.h"
#include "src/uart.h"
#include "src/studiolib.h"
#include "stdbool.h"

/**
 * main.c
 * Considering 1MHz
 */
#define POLL_COUNT              10
volatile int pollCounter = POLL_COUNT;  // Define pollCounter

#define WATCHDOG_INTERVAL_MS    1000    // Define the watchdog timer interval in milliseconds (e.g., 1000 ms)

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

void Timer1_A_Setup(void)
{
    TA1CCTL0 = CCIE;         // Enable Timer1_A interrupt
    TA1CCR0 = 1000;          // 1 ms interval (1 MHz / 1000 = 1ms)
    TA1CTL = TASSEL_2 | MC_1 | ID_0 | TACLR;  // SMCLK (1 MHz), Up mode, No divider, Clear timer
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


// These communications functions are assumed to be defined elsewhere:

extern void InitialiseDynamentComms(void);
extern void DynamentCommsHandler(void);
extern void ModbusCommsHandler(void);
extern void RequestLiveData2(void (*cb)(int, float, float));



void active_all()
{
    clockConfigure();
    uart_init();
    initGLED();
    initREDLED();


    Timer_Init();
    Timer1_A_Setup();
    UART_sendString("Main Code Running!r\n");

}


//--------------------------------------------------------------------------
// Watchdog Function: Pump/update the watchdog timer
void Watchdog_Init(void)
{
    // Stop the watchdog timer initially to configure it
    WDTCTL = WDTPW + WDTHOLD; // Disable Watchdog Timer

    // Configure the watchdog timer: watchdog reset after a timeout
    WDTCTL = WDTPW + WDTSSEL_2 + WDTTMSEL + WDTIS_4; // ACLK, interval = 1000 ms (1 second)

    // Enable Watchdog interrupt (optional)
    // WDTCTL |= WDTIE;  // Uncomment if you want an interrupt instead of a reset
}

// Function to refresh (clear) the watchdog timer to prevent reset
void Watchdog_Refresh(void)
{
    // Clear the Watchdog Timer to prevent a reset
    WDTCTL = WDTPW + WDTCNTCL; // Clear the Watchdog Timer
}

// Watchdog interrupt service routine (optional, if interrupt mode is enabled)
// This function will be triggered when the watchdog timer expires
#pragma vector=WDT_VECTOR
__interrupt void WDT_ISR(void)
{
    // Handle the Watchdog timeout event here if using interrupt mode
    // For example, you could reset some peripherals or log an error
    __no_operation(); // Placeholder for actual ISR logic
}
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




/* *****TEST***** */
/*
#pragma vector=USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void)
{
    if (UCA1IFG & UCRXIFG)
    {
        uint8_t receivedByte = UCA1RXBUF;

        // Check for Buffer Overflow Before Storing Data
        uint16_t nextPut = (tail + 1) % P2P_BUFFER_SIZE;
        if (nextPut != head)  // Ensure buffer is not full
        {
            RX_Buffer[tail] = receivedByte;
            tail = nextPut;
        }
        else
        {
            // Buffer Overflow Detected - Handle It (Maybe log an error or clear the buffer)
            UART_sendString("Buffer Overflow!\n");
        }

        // Debug Output: Accumulate received bytes into a full packet
        // Print packet when it's fully received or when a delimiter is found.
        if (receivedByte == EOF || receivedByte == DLE)  // Assuming EOF or DLE indicates end of packet
        {
            // Print the entire packet
            UART_sendString("Full Packet Received: ");

            // Assuming the buffer is full, print the entire packet
            uint16_t idx = head;
            while (idx != tail)  // While there are unread bytes
            {
                UART_sendHex(RX_Buffer[idx]);
                idx = (idx + 1) % P2P_BUFFER_SIZE;
            }

            UART_sendString(" <Rx End>\n");

            // Optional: Reset buffer indices if necessary
            head = tail;  // Mark buffer as processed
        }

        // Clear RX flag to complete the interrupt
        UCA1IFG &= ~UCRXIFG;
    }
}
*/
/*
#pragma vector=USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void)
{
    if (UCA1IFG & UCRXIFG)
    {
        uint8_t receivedByte = UCA1RXBUF;

        // Check for Buffer Overflow Before Storing Data
        uint16_t nextPut = (tail + 1) % P2P_BUFFER_SIZE;
        if (nextPut != head)  // Ensure buffer is not full
        {
            RX_Buffer[tail] = receivedByte;
            tail = nextPut;
        }
        else
        {
            // Buffer Overflow Detected - Handle It (Maybe log an error or clear the buffer)
            UART_sendString("Buffer Overflow!\n");
        }

        // Debug Output
        UART_sendString("Byte Received in ISR: ");
        UART_sendHex(receivedByte);
        UART_sendString("\n");

        UCA1IFG &= ~UCRXIFG;  // Clear RX flag
    }

}
*/

/*
// ISRs -------------------------------

#pragma vector=USCI_A1_VECTOR
__interrupt void USCI_A1_ISR(void)
{


    if (UCA1IFG & UCRXIFG)  // Check if RX flag is set
    {
        uint8_t receivedByte = UCA1RXBUF;  // Read received byte
        UART_sendString("Byte Received in ISR: ");
        UART_sendHex(receivedByte);
        UART_sendString("\n");

        // Store in buffer if needed
        RX_Buffer[tail] = receivedByte;
        tail = (tail + 1) % P2P_BUFFER_SIZE;

        // **Clear RX flag to prevent repeated interrupts**
        UCA1IFG &= ~UCRXIFG;
    }
    delay_ms(10000);
}
*/
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
    Watchdog_Init();

    // Initialize communications

    if (COMMS_PROTOCOL == DYNAMENT_PROTOCOL)
    {
        InitialiseDynamentComms();
    }

    // Main loop
    for( ; ;)
    {
        Watchdog_Refresh();

        //UART_sendString("System Running!\r\n");

        if (COMMS_PROTOCOL == DYNAMENT_PROTOCOL)
        {
            DynamentCommsHandler();
        }
        else
        {
            ModbusCommsHandler();
        }
        delay_ms(500);
    }

}
