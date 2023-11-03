// UART0 Library
// Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    -

// Hardware configuration:
// UART Interface:
//   U0TX (PA1) and U0RX (PA0) are connected to the 2nd controller
//   The USB on the 2nd controller enumerates to an ICDI interface and a virtual COM port
//   Configured to 115,200 baud, 8N1

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include "tm4c123gh6pm.h"
#include "uart0.h"

// PortA masks
#define UART_TX_MASK 2
#define UART_RX_MASK 1


//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize UART0
void initUart0()
{
    // Enable clocks
    SYSCTL_RCGCUART_R |= SYSCTL_RCGCUART_R0;
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R0;
    _delay_cycles(3);

    // Configure UART0 pins
    GPIO_PORTA_DR2R_R |= UART_TX_MASK;                  // set drive strength to 2mA (not needed since default configuration -- for clarity)
    GPIO_PORTA_DEN_R |= UART_TX_MASK | UART_RX_MASK;    // enable digital on UART0 pins
    GPIO_PORTA_AFSEL_R |= UART_TX_MASK | UART_RX_MASK;  // use peripheral to drive PA0, PA1
    GPIO_PORTA_PCTL_R &= ~(GPIO_PCTL_PA1_M | GPIO_PCTL_PA0_M); // clear bits 0-7
    GPIO_PORTA_PCTL_R |= GPIO_PCTL_PA1_U0TX | GPIO_PCTL_PA0_U0RX;
                                                        // select UART0 to drive pins PA0 and PA1: default, added for clarity

    // Configure UART0 to 115200 baud, 8N1 format
    UART0_CTL_R = 0;                                    // turn-off UART0 to allow safe programming
    UART0_CC_R = UART_CC_CS_SYSCLK;                     // use system clock (40 MHz)
    UART0_IBRD_R = 21;                                  // r = 40 MHz / (Nx115.2kHz), set floor(r)=21, where N=16
    UART0_FBRD_R = 45;                                  // round(fract(r)*64)=45
    UART0_LCRH_R = UART_LCRH_WLEN_8 | UART_LCRH_FEN;    // configure for 8N1 w/ 16-level FIFO
    UART0_CTL_R = UART_CTL_TXE | UART_CTL_RXE | UART_CTL_UARTEN;
                                                        // enable TX, RX, and module
}

// Set baud rate as function of instruction cycle frequency
void setUart0BaudRate(uint32_t baudRate, uint32_t fcyc)
{
    uint32_t divisorTimes128 = (fcyc * 8) / baudRate;   // calculate divisor (r) in units of 1/128,
                                                        // where r = fcyc / 16 * baudRate
    divisorTimes128 += 1;                               // add 1/128 to allow rounding
    UART0_CTL_R = 0;                                    // turn-off UART0 to allow safe programming
    UART0_IBRD_R = divisorTimes128 >> 7;                // set integer value to floor(r)
    UART0_FBRD_R = ((divisorTimes128) >> 1) & 63;       // set fractional value to round(fract(r)*64)
    UART0_LCRH_R = UART_LCRH_WLEN_8 | UART_LCRH_FEN;    // configure for 8N1 w/ 16-level FIFO
    UART0_CTL_R = UART_CTL_TXE | UART_CTL_RXE | UART_CTL_UARTEN;
                                                        // turn-on UART0
}

// Blocking function that writes a serial character when the UART buffer is not full
void putcUart0(char c)
{
    while (UART0_FR_R & UART_FR_TXFF);               // wait if uart0 tx fifo full
    UART0_DR_R = c;                                  // write character to fifo
}

// Blocking function that writes a string when the UART buffer is not full
void putsUart0(char* str)
{
    uint8_t i = 0;
    while (str[i] != '\0')
        putcUart0(str[i++]);
}

// Blocking function that returns with serial data once the buffer is not empty
char getcUart0()
{
    while (UART0_FR_R & UART_FR_RXFE);               // wait if uart0 rx fifo empty
    return UART0_DR_R & 0xFF;                        // get character from fifo
}

// Returns the status of the receive buffer
bool kbhitUart0()
{
    return !(UART0_FR_R & UART_FR_RXFE);
}

//-----------------------------------------------------------------------------
// Parse User Data
//-----------------------------------------------------------------------------

void getsUart0(USER_DATA * data)
{
    int count = 0;
    unsigned char c = 0;

    while (count < MAX_CHARS)
    {
        c = getcUart0();
        if (c == 127 || c == 8) // if input is backspace, decrement count if count is > 0
        {
            if (count > 0)
            {
                count--;
            }
        }

        else if (c == 10 || c == 13) //if input is CR or LF
        {
            data->buffer[count] = '\0';
            return;
        }

        else
        {
            if (c >= 32)
            {
                data->buffer[count++] = c;
                if (count >= MAX_CHARS)
                {
                    data->buffer[count] = '\0';
                    return;
                }
            }
        }
    }
    return;
}

void parseFields(USER_DATA * data)
{
    int index = 0;
    int dataIndex = 0;

    data->fieldCount = 0;

    char previous = 'd';
    char current = 'x';

    //Loops through buffer until NULL terminator is reached
    while(data->buffer[index] != NULL)
    {

        char c = data->buffer[index];

        //If char is alpha, ('a'-'z') || ('A'-'Z') ==> fieldType = 'a'
        if( ((c >= 97) && (c <= 122)) || ((c >= 65) &&  (c <= 90)) )
        {
            current = 'a';
            index++;
        }

        //If char is numeric, ('0'-'9') || '-' || '.' ==> fieldType = 'n'
        else if ( ((c >= 48) && (c <= 57)) || (c == 45) ||  (c == 46) )
        {
            current = 'n';
            index++;
        }

        //Else char is a delimiter ==> fieldType = 'd'
        //Set previous to d in order to continue through string
        //replace delimiters with NULL character
        else
        {
            current = 'd';
            previous = 'd';
            data->buffer[index] = NULL;
            index++;
        }

        //Occurs when transition from 'd' to 'a' OR 'd' to 'n'
        if((current != previous) && (previous == 'd'))
        {
            //Increment Field Count
            (data->fieldCount)++;

            //Stores the fieldType of Transition
            data->fieldType[dataIndex] = current;

            //Stores fieldPosition of Transition and increments dataIndex
            data->fieldPosition[dataIndex] = (index-1);

            dataIndex++;

            //Updates previous c
            previous = current;

            //returns if fieldCount Reaches 5
            if ((data->fieldCount) == MAX_FIELDS) return;
        }
    }
}

bool isCommand(USER_DATA* data, const char strCommand[], uint8_t minArguments)
{
    char* field0 = getFieldString(data, 0);

    //Returns True if first field is equal to command AND number of arguments >= # of minimum arguments
    if ( (!strcmp(field0, strCommand)) && ((data->fieldCount) > minArguments) )
    {
        return 1;
    }

    else return 0;
}
