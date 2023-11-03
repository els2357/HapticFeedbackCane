// Hardware initialization functions
// Ethan Sprinkle

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

#ifndef INIT_H_
#define INIT_H_

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void initHw();
void EnableWideTimer();
void disableWideTimers();
void EnableTrigTimer();
void disableTrigTimer();

#endif
