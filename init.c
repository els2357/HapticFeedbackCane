// Hardware initialization functions
// Ethan Sprinkle

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include "tm4c123gh6pm.h"
#include "init.h"

// Bitband aliases
#define ECHO_0   (*((volatile uint32_t *)(0x42000000 + (0x400063FC-0x40000000)*32 + 6*4))) //PC6 WT1A
#define ECHO_1   (*((volatile uint32_t *)(0x42000000 + (0x400073FC-0x40000000)*32 + 0*4))) //PD0 WT2A
#define ECHO_2   (*((volatile uint32_t *)(0x42000000 + (0x400073FC-0x40000000)*32 + 2*4))) //PD2 WT3A
#define TRIGGER  (*((volatile uint32_t *)(0x42000000 + (0x400073FC-0x40000000)*32 + 4*4))) //PD4 WT4A

// PortC, PortD, & PortE masks
#define TRIG_0_MASK 2      //2^1
#define TRIG_1_MASK 4      //2^2
#define TRIG_2_MASK 8      //2^3
#define ECHO_0_MASK 64     //2^6
#define ECHO_1_MASK 1      //2^0
#define ECHO_2_MASK 4      //2^2
#define TRIGGER_MASK 16    //2^4

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------
// Initialize Hardware
void initHw()
{
    // Initialize system clock to 40 MHz
    initSystemClockTo40Mhz();

    // Enable clocks
    SYSCTL_RCGCTIMER_R |= SYSCTL_RCGCTIMER_R1 | SYSCTL_RCGCTIMER_R4;
    SYSCTL_RCGCWTIMER_R |= SYSCTL_RCGCWTIMER_R1 | SYSCTL_RCGCWTIMER_R2 | SYSCTL_RCGCWTIMER_R3 | SYSCTL_RCGCWTIMER_R4;
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R0 | SYSCTL_RCGCGPIO_R4 | SYSCTL_RCGCGPIO_R2 | SYSCTL_RCGCGPIO_R3;
    _delay_cycles(3);

    // Configure ECHO and TRIGGER GPIOs
    GPIO_PORTE_DIR_R |= TRIG_1_MASK | TRIG_2_MASK | TRIG_0_MASK;    // TRIGs are outputs
    GPIO_PORTE_DEN_R |= TRIG_1_MASK | TRIG_2_MASK | TRIG_0_MASK;    // enable TRIGs

    // // Configure SIGNAL_IN for frequency and time measurements
    // Wide Timer 1A
    GPIO_PORTC_AFSEL_R |= ECHO_0_MASK;              // select alternative functions for SIGNAL_IN pin
    GPIO_PORTC_PCTL_R &= ~GPIO_PCTL_PC6_M;           // map alt fns to SIGNAL_IN
    GPIO_PORTC_PCTL_R |= GPIO_PCTL_PC6_WT1CCP0;
    GPIO_PORTC_DEN_R |= ECHO_0_MASK;                // enable bit 6 for digital input

    // Wide Timer 2A
    GPIO_PORTD_AFSEL_R |= ECHO_1_MASK;              // select alternative functions for SIGNAL_IN pin
    GPIO_PORTD_PCTL_R &= ~GPIO_PCTL_PD0_M;           // map alt fns to SIGNAL_IN
    GPIO_PORTD_PCTL_R |= GPIO_PCTL_PD0_WT2CCP0;
    GPIO_PORTD_DEN_R |= ECHO_1_MASK;                // enable bit 6 for digital input

    // Wide Timer 3A
    GPIO_PORTD_AFSEL_R |= ECHO_2_MASK;              // select alternative functions for SIGNAL_IN pin
    GPIO_PORTD_PCTL_R &= ~GPIO_PCTL_PD2_M;           // map alt fns to SIGNAL_IN
    GPIO_PORTD_PCTL_R |= GPIO_PCTL_PD2_WT3CCP0;
    GPIO_PORTD_DEN_R |= ECHO_2_MASK;                // enable bit 6 for digital input

    // Wide Timer 4A
    GPIO_PORTD_AFSEL_R |= TRIGGER_MASK;              // select alternative functions for SIGNAL_IN pin
    GPIO_PORTD_PCTL_R &= ~GPIO_PCTL_PD4_M;           // map alt fns to SIGNAL_IN
    GPIO_PORTD_PCTL_R |= GPIO_PCTL_PD4_WT4CCP0;
    GPIO_PORTD_DEN_R |= TRIGGER_MASK;                // enable bit 6 for digital input
}

void EnableWideTimer()
{
    //Enable WideTimer 1
    WTIMER1_CTL_R &= ~TIMER_CTL_TAEN;                // turn-off counter before reconfiguring
    WTIMER1_CFG_R = 4;                               // configure as 32-bit counter (A only)
    WTIMER1_TAMR_R = TIMER_TAMR_TACMR | TIMER_TAMR_TAMR_CAP | TIMER_TAMR_TACDIR;
                                                     // configure for edge time mode, count up
    WTIMER1_CTL_R = TIMER_CTL_TAEVENT_BOTH;           // measure time from positive edge to negative edge
    WTIMER1_IMR_R = TIMER_IMR_CAEIM;                 // turn-on interrupts
    WTIMER1_TAV_R = 0;                               // zero counter for first period
    WTIMER1_CTL_R |= TIMER_CTL_TAEN;                 // turn-on counter
    NVIC_EN3_R = 1 << (INT_WTIMER1A-16-96);         // turn-on interrupt 112 (WTIMER1A)

    //Enable WideTimer 2
    WTIMER2_CTL_R &= ~TIMER_CTL_TAEN;                // turn-off counter before reconfiguring
    WTIMER2_CFG_R = 4;                               // configure as 32-bit counter (A only)
    WTIMER2_TAMR_R = TIMER_TAMR_TACMR | TIMER_TAMR_TAMR_CAP | TIMER_TAMR_TACDIR;
                                                     // configure for edge time mode, count up
    WTIMER2_CTL_R = TIMER_CTL_TAEVENT_BOTH;           // measure time from positive edge to negative edge
    WTIMER2_IMR_R = TIMER_IMR_CAEIM;                 // turn-on interrupts
    WTIMER2_TAV_R = 0;                               // zero counter for first period
    WTIMER2_CTL_R |= TIMER_CTL_TAEN;                 // turn-on counter
    NVIC_EN3_R = 1 << (INT_WTIMER2A-16-96);         // turn-on interrupt 114 (WTIMER2A)

    //Enable WideTimer 3
    WTIMER3_CTL_R &= ~TIMER_CTL_TAEN;                // turn-off counter before reconfiguring
    WTIMER3_CFG_R = 4;                               // configure as 32-bit counter (A only)
    WTIMER3_TAMR_R = TIMER_TAMR_TACMR | TIMER_TAMR_TAMR_CAP | TIMER_TAMR_TACDIR;
                                                     // configure for edge time mode, count up
    WTIMER3_CTL_R = TIMER_CTL_TAEVENT_BOTH;           // measure time from positive edge to negative edge
    WTIMER3_IMR_R = TIMER_IMR_CAEIM;                 // turn-on interrupts
    WTIMER3_TAV_R = 0;                               // zero counter for first period
    WTIMER3_CTL_R |= TIMER_CTL_TAEN;                 // turn-on counter
    NVIC_EN3_R = 1 << (INT_WTIMER3A-16-96);         // turn-on interrupt 116 (WTIMER3A)
}

void disableWideTimers()
{
    //Disable WideTimer 1
    WTIMER1_CTL_R &= ~TIMER_CTL_TAEN;                // turn-off counter
    NVIC_DIS3_R = 1 << (INT_WTIMER1A-16-96);        // turn-off interrupt 112 (WTIMER1A)

    //Disable WideTimer 2
    WTIMER2_CTL_R &= ~TIMER_CTL_TAEN;                // turn-off counter
    NVIC_DIS3_R = 1 << (INT_WTIMER2A-16-96);        // turn-off interrupt 114 (WTIMER1A)

    //Disable WideTimer 3
    WTIMER3_CTL_R &= ~TIMER_CTL_TAEN;                // turn-off counter
    NVIC_DIS3_R = 1 << (INT_WTIMER3A-16-96);        // turn-off interrupt 116 (WTIMER1A)
}

void EnableTrigTimer()
{
// Configure Timer 4 as the time base
    TIMER4_CTL_R &= ~TIMER_CTL_TAEN;                 // turn-off timer before reconfiguring
    TIMER4_CFG_R = TIMER_CFG_32_BIT_TIMER;           // configure as 32-bit timer (A+B)
    TIMER4_TAMR_R = TIMER_TAMR_TAMR_PERIOD;          // configure for periodic mode (count down)
    TIMER4_TAILR_R = 3000000;                       // set load value to 4e6 for 20 Hz interrupt rate
    TIMER4_IMR_R = TIMER_IMR_TATOIM;                 // turn-on interrupts
    TIMER4_CTL_R |= TIMER_CTL_TAEN;                  // turn-on timer
    NVIC_EN2_R = 1 << (INT_TIMER4A-16-64);             // turn-on interrupt 86 (TIMER4A)

    WTIMER4_CTL_R &= ~TIMER_CTL_TAEN;                // turn-off counter before reconfiguring
    WTIMER4_CFG_R = 4;                               // configure as 32-bit counter (A only)
    WTIMER4_TAMR_R = TIMER_TAMR_TAMR_CAP | TIMER_TAMR_TACDIR; // configure for edge count mode, count up
    WTIMER4_CTL_R = TIMER_CTL_TAEVENT_POS;           // count positive edges
    WTIMER4_IMR_R = 0;                               // turn-off interrupts
    WTIMER4_TAV_R = 0;                               // zero counter for first period
    WTIMER4_CTL_R |= TIMER_CTL_TAEN;                 // turn-on counter
}

void disableTrigTimer()
{
    //Disable Timer s
    TIMER4_CTL_R &= ~TIMER_CTL_TAEN;                 // turn-off time base timer
    WTIMER4_CTL_R &= ~TIMER_CTL_TAEN;                // turn-off event counter
    NVIC_DIS2_R = 1 << (INT_TIMER4A-16-64);            // turn-off interrupt 37 (TIMER1A)
}
