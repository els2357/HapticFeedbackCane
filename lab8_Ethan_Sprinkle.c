// Lab 8 - Final Project
// Ethan Sprinkle
// Base Code Provided by Jason Losh

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz

// Hardware configuration:
// UART Interface:
//   U0TX (PA1) and U0RX (PA0) are connected to the 2nd controller
//   The USB on the 2nd controller enumerates to an ICDI interface and a virtual COM port
//   Configured to 115,200 baud, 8N1
// Frequency counter and timer input:
//   SIGNAL_IN on PC6 (WT1CCP0)

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
//#include "motor.h"
#include "init.h"
#include "clock.h"
#include "uart0.h"
#include "wait.h"
#include "eeprom.h"
#include "tm4c123gh6pm.h"

#define TRIG_0   (*((volatile uint32_t *)(0x42000000 + (0x400243FC-0x40000000)*32 + 1*4))) //PE1
#define TRIG_1   (*((volatile uint32_t *)(0x42000000 + (0x400243FC-0x40000000)*32 + 2*4))) //PE2
#define TRIG_2   (*((volatile uint32_t *)(0x42000000 + (0x400243FC-0x40000000)*32 + 3*4))) //PE3

// Global variables
uint32_t distance[3];
uint8_t  channel = 0;
uint8_t  phase = 0;
uint8_t  eventStatus[20];

//-----------------------------------------------------------------------------
// Wide Timer Interrupts
//-----------------------------------------------------------------------------

void isr_0()
{
    if (phase == 0)
    {
        WTIMER1_TAV_R = 0;
        phase = 1;
    }

    else
    {
        distance[0] = WTIMER1_TAV_R * 0.0043125;
        phase = 2;
    }

    WTIMER1_ICR_R = TIMER_ICR_CAECINT;
}

void isr_1()
{
    if (phase == 0)
    {
        WTIMER2_TAV_R = 0;
        phase = 1;
    }

    else
    {
        distance[1] = WTIMER2_TAV_R * 0.0043125;
        phase = 2;
    }

    WTIMER2_ICR_R = TIMER_ICR_CAECINT;
}

void isr_2()
{
    if (phase == 0)
    {
        WTIMER3_TAV_R = 0;
        phase = 1;
    }

    else
    {
        distance[2] = WTIMER3_TAV_R * 0.0043125;
        phase = 2;
    }

    WTIMER3_ICR_R = TIMER_ICR_CAECINT;
}

void timer_isr()
{
    if (phase != 2)
    {
        distance[channel] = 0;
    }
    channel++;
    if (channel > 2) channel = 0;
    phase = 0;
    switch (channel)
    {
    case 0 :
        TRIG_0 = 1;
        waitMicrosecond(10);
        TRIG_0 = 0;
        break;

    case 1 :
        TRIG_1 = 1;
        waitMicrosecond(10);
        TRIG_1 = 0;
        break;

    case 2 :
        TRIG_2 = 1;
        waitMicrosecond(10);
        TRIG_2 = 0;
        break;
    }

    TIMER4_ICR_R = TIMER_ICR_TATOCINT;           // clear interrupt flag
}

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

char* getFieldString(USER_DATA * data, uint8_t fieldNumber)
{
    char fieldStringbuffer[MAX_CHARS+1] = NULL;
    int index = data->fieldPosition[fieldNumber];

    //If fieldNumber is > field Count OR field is a number, return NULL
    if ( (fieldNumber > (data->fieldCount)) || ((data->fieldType[fieldNumber]) == 'n') )
    {
        return NULL;
    }

    //Else, return the field equivalent String
    else
    {
        //Loop through buffer string until NULL terminator is found
        while( data->buffer[index] != NULL)
        {
            char c = data->buffer[index];
            strncat(fieldStringbuffer, &c, 1);
            index++;
        }

        char* fieldString = (fieldStringbuffer);
        return fieldString;
    }
}

int32_t getFieldInteger(USER_DATA *data, uint8_t fieldNumber)
{
    int32_t fieldInt = 0;
    char fieldStringbuffer[MAX_CHARS+1] = NULL;

    //ASSUMES FIELD NUMBER STARTS AT 0 NOT 1
    //If fieldNumber is > fieldCount OR is an alpha value, return NULL
    if ( (fieldNumber > (data->fieldCount)) || ((data->fieldType[(fieldNumber)]) == 'a') )
    {
        return fieldInt;
    }

    //Else, return the field equivalent Int using atoi()
    else
    {
        int index = data->fieldPosition[fieldNumber];
        while((data->buffer[index]) != NULL)
        {
            char c = data->buffer[index];
            strncat(fieldStringbuffer, &c, 1);
            index++;
        }
        fieldInt = atoi(fieldStringbuffer);
        return fieldInt;
    }
}

void checkEventTrue(uint8_t event_n)
{
    int32_t sensor_n = readEeprom(event_n*8);

    //Marks event false if sensor != 0, 1, or 2
    if ( (sensor_n != 0) && (sensor_n != 1) && (sensor_n != 2) )
    {
        eventStatus[event_n] = 0;
    }

    //Marks events 0-15 true if distances are within range
    else
    {
        uint32_t event_min = readEeprom(event_n*8 + 1);
        uint32_t event_max = readEeprom(event_n*8 + 2);
        uint32_t dist = distance[sensor_n];

        if (dist >= event_min && dist <= event_max)
        {
            eventStatus[event_n] = 1;
        }

        else
        {
            eventStatus[event_n] = 0;
        }
    }
}

void checkCompoundEventTrue(uint8_t event_n)
{
    uint32_t active = readEeprom(event_n*8 + 0);
    uint32_t event1 = readEeprom(event_n*8 + 1);
    uint32_t event2 = readEeprom(event_n*8 + 2);

    if (active != 1)
    {
        eventStatus[event_n] = 0;
        return;
    }

    if (eventStatus[event1] == 1 && eventStatus[event2] == 1)
    {
        eventStatus[event_n] = 1;
    }

    else
    {
        eventStatus[event_n] = 0;
    }
}

playEvent(uint16_t event_n)
{
    if (readEeprom(8*event_n + 3) == 0)
    {
        return;
    }

    else
    {
        uint8_t b;
        for(b=1; b <= readEeprom(8*event_n + 4); b++)
        {
            setMotorSpeed(readEeprom(8*event_n + 7));
            waitMicrosecond(1000*readEeprom(8*event_n + 5));
            setMotorSpeed(0);
            waitMicrosecond(1000*readEeprom(8*event_n + 6));
        }
    }
}

int main(void)
{
    waitMicrosecond(500000);
    // Initialize hardware
	initHw();
	initUart0();
	initPMW();
	initEeprom();

    // Setup UART0 baud rate
    setUart0BaudRate(115200, 40e6);

    //Enable Timers
    EnableTrigTimer();
    EnableWideTimer();

    setMotorSpeed(0);
    if ( kbhitUart0() )
    {
        //getcUart0();
        toggleGreenLight();
    }
    waitMicrosecond(500000);

    while(1)
    {
        toggleBlueLight();
        //Loop through eeprom to see if events 0-15 are true
        int8_t r;
        for (r = 0; r < 16; r++)
        {
            checkEventTrue(r);
        }

        for (r = 16; r < 20; r++)
        {
            checkCompoundEventTrue(r);
        }

        //check for active event
        for (r = 19; r >= 0; r--)
        {
            if (eventStatus[r] == 1)
            {
                playEvent(r);
                waitMicrosecond(1000000);
                break;
            }
        }

        if ( kbhitUart0() )
        {
            USER_DATA data;
            char str[50] = NULL;

            getsUart0(&data);
            parseFields(&data);

            //help command
            if (isCommand(&data, "help", 0))
            {
                putsUart0("reboot        (no params)\n");
                putsUart0("event         EVENT SENSOR MIN_DIST_MM MAX_DIST_MM\n");
                putsUart0("and           EVENT EVENT1 EVENT2\n");
                putsUart0("erase         EVENT\n");
                putsUart0("show events   (no params)\n");
                putsUart0("show patterns (no params)\n");
                putsUart0("haptic        EVENT on/off\n");
                putsUart0("pattern       EVENT PWM BEATS ON_TIME OFF_TIME\n");
                putsUart0("display       (no params)\n");
            }

            //reboot command
            if (isCommand(&data, "reboot", 0))
            {
                NVIC_APINT_R = NVIC_APINT_VECTKEY | NVIC_APINT_SYSRESETREQ;
            }

            //update event MIN_MM and MAX_MM
            if (isCommand(&data, "event", 4))
            {
                int32_t event_num = getFieldInteger(&data, 1);
                int32_t sensor =    getFieldInteger(&data, 2);
                int32_t min_mm =    getFieldInteger(&data, 3);
                int32_t max_mm =    getFieldInteger(&data, 4);

                if (event_num >=0 && event_num < 16)
                {
                    writeEeprom( (0 + 8*event_num), (uint32_t) sensor );
                    writeEeprom( (1 + 8*event_num), (uint32_t) min_mm );
                    writeEeprom( (2 + 8*event_num), (uint32_t) max_mm );
                    snprintf(str, sizeof(str), "Distances for EVENT %2"PRIu32" entered.\n\n", event_num);
                    putsUart0(str);
                }

                else if (event_num > 16 && event_num < 20)
                {
                    putsUart0("EVENTS 16-19 Reserved for compound Events (""and"" command)\n\n");
                }

                else
                {
                    putsUart0("Invalid Event number. Valid Events: 0-15\n\n");
                }
            }

            //compound event
            if (isCommand(&data, "and", 3))
            {
                int8_t event_num = getFieldInteger(&data, 1);
                int8_t event1 =    getFieldInteger(&data, 2);
                int8_t event2 =    getFieldInteger(&data, 3);

                if (event_num < 16)
                {
                    putsUart0("EVENTS 0-15 Reserved for single Events (""event"" command)\n\n");
                }

                else if (event_num > 19)
                {
                    putsUart0("Invalid Event number. Valid Compound Events: 16-19\n\n");
                }

                else
                {
                    writeEeprom( (0 + 8*event_num), (uint32_t) 1 );
                    writeEeprom( (1 + 8*event_num), (uint32_t) event1 );
                    writeEeprom( (2 + 8*event_num), (uint32_t) event2 );
                    snprintf(str, sizeof(str), "Compound EVENT %2"PRIu32" entered.\n\n", event_num);
                    putsUart0(str);
                }

            }

            //erase event
            if (isCommand(&data, "erase", 1))
            {
                int8_t event_num = getFieldInteger(&data, 1);
                if (event_num >=0 && event_num < 20)
                {
                    writeEeprom( (0 + 8*event_num), (uint32_t) -1 );
                    snprintf(str, sizeof(str), "EVENT %2"PRIu32" erased.\n\n", event_num);
                    putsUart0(str);
                }

                else
                {
                    putsUart0("Invalid Event number. Valid Events: 0-19\n\n");
                }
            }

            //show events or patterns
            if (isCommand(&data, "show", 1))
            {
                char * str_show = getFieldString(&data, 1);

                if (!strcmp(str_show,"events"))
                {
                    putsUart0("\nEVENT LIST\n");
                    int i;
                    for(i = 0; i < 16; i++)
                    {
                        char str0[40], str1[40], str2[40], str3[40] = NULL;
                        uint16_t sensor_index = (uint16_t)(0+8*i);
                        uint16_t min_index = (uint16_t)(1+8*i);
                        uint16_t max_index = (uint16_t)(2+8*i);

                        snprintf(str0, sizeof(str0), "EVENT %2"PRIu32"  ", i);
                        putsUart0(str0);
                        snprintf(str1, sizeof(str1), "SENSOR %2"PRId32"  ", (int32_t)readEeprom( sensor_index ));
                        putsUart0(str1);
                        snprintf(str2, sizeof(str2), "Min Distance: %4"PRIu32" mm  ", readEeprom( min_index ));
                        putsUart0(str2);
                        snprintf(str3, sizeof(str3), "Max Distance: %4"PRIu32" mm\n", readEeprom( max_index ));
                        putsUart0(str3);
                    }
                    putsUart0("\n");

                    putsUart0("COMPOUND EVENTS\n");
                    for(i = 16; i < 20; i++)
                    {
                        char str0[40], str1[40], str2[40], str3[40] = NULL;
                        uint16_t active = (uint16_t)(0+8*i);
                        uint16_t event1 = (uint16_t)(1+8*i);
                        uint16_t event2 = (uint16_t)(2+8*i);


                        snprintf(str0, sizeof(str0), "EVENT %2"PRIu32"  ", i);
                        putsUart0(str0);
                        snprintf(str1, sizeof(str1), "ACTIVE %2"PRId32"  ", (int32_t)readEeprom( active ));
                        putsUart0(str1);
                        snprintf(str2, sizeof(str2), "EVENT %2"PRIu32" AND ", readEeprom( event1 ));
                        putsUart0(str2);
                        snprintf(str3, sizeof(str3), "EVENT %2"PRIu32"\n", readEeprom( event2 ));
                        putsUart0(str3);
                    }
                    putsUart0("\n");

                }

                else if (!strcmp(str_show,"patterns"))
                {
                    putsUart0("\nPATTERN LIST\n");
                    int i;
                    for(i = 0; i < 20; i++)
                    {
                        char str0[40], str1[40], str2[40], str3[40], str4[40], str5[40] = NULL;
                        uint16_t haptic_index =  (uint16_t)(3+8*i);
                        uint16_t beat_index =    (uint16_t)(4+8*i);
                        uint16_t ontime_index =  (uint16_t)(5+8*i);
                        uint16_t offtime_index = (uint16_t)(6+8*i);
                        uint16_t pwm_index =     (uint16_t)(7+8*i);

                        snprintf(str0, sizeof(str0), "EVENT %2"PRIu32"  ", i);
                        putsUart0(str0);
                        snprintf(str1, sizeof(str1), "Haptics: %1"PRIu32" (on = 1/off = 0)  ", readEeprom( haptic_index ));
                        putsUart0(str1);
                        snprintf(str5, sizeof(str5), "PWM: %3"PRIu32"%%  ", readEeprom( pwm_index ));
                        putsUart0(str5);
                        snprintf(str2, sizeof(str2), "Beat Count: %2"PRIu32"  ", readEeprom( beat_index ));
                        putsUart0(str2);
                        snprintf(str3, sizeof(str3), "Time on: %4"PRIu32" ms  ", readEeprom( ontime_index ));
                        putsUart0(str3);
                        snprintf(str4, sizeof(str4), "Time off: %4"PRIu32" ms  \n", readEeprom( offtime_index ));
                        putsUart0(str4);
                    }
                    putsUart0("\n");
                }
            }

            //update haptic
            if (isCommand(&data, "haptic", 2))
            {
                int8_t event_num = getFieldInteger(&data, 1);
                char*  str =       getFieldString(&data, 2);

                //if haptic set to "on"
                if (!strcmp(str,"on"))
                {
                    writeEeprom( (3 + 8*event_num), (uint32_t) 1 );
                    putsUart0("Haptic is on.\n\n");
                }

                else if (!strcmp(str,"off"))
                {
                    writeEeprom( (3 + 8*event_num), (uint32_t) 0 );
                    putsUart0("Haptic is off.\n\n");
                }
            }

            //update pattern
            if (isCommand(&data, "pattern", 5))
            {
                int8_t   event_num =   getFieldInteger(&data, 1);
                int8_t   pwm =         getFieldInteger(&data, 2);
                int8_t   beats =       getFieldInteger(&data, 3);
                int32_t  ms_on_time =  getFieldInteger(&data, 4);
                int32_t  ms_off_time = getFieldInteger(&data, 5);

                uint16_t beat_index =    (uint16_t)(4+8*event_num);
                uint16_t ontime_index =  (uint16_t)(5+8*event_num);
                uint16_t offtime_index = (uint16_t)(6+8*event_num);
                uint16_t pwm_index =     (int16_t)(7+8*event_num);

                if (event_num >=0 && event_num < 20)
                {
                    char str10[50] = NULL;
                    writeEeprom( pwm_index, (uint32_t) pwm );
                    writeEeprom( beat_index, (uint32_t) beats );
                    writeEeprom( ontime_index, (uint32_t) ms_on_time );
                    writeEeprom( offtime_index, (uint32_t) ms_off_time );
                    snprintf(str10, sizeof(str10), "Patterns for EVENT %2"PRId32" entered.\n", event_num);
                    putsUart0(str10);
                }

                else
                {
                    putsUart0("Invalid Event number. Valid Events: 0-19\n");
                }

                putsUart0("\n");
            }

            if (isCommand(&data, "display", 0))
            {
                while( !kbhitUart0() )
                {
                    snprintf(str, sizeof(str), "Sensor 0:    %5"PRIu32" (mm)\n", distance[0]);
                    putsUart0(str);
                    snprintf(str, sizeof(str), "Sensor 1:    %5"PRIu32" (mm)\n", distance[1]);
                    putsUart0(str);
                    snprintf(str, sizeof(str), "Sensor 2:    %5"PRIu32" (mm)\n\n\n", distance[2]);
                    putsUart0(str);
                    waitMicrosecond(100000);
                }
            }

        }
    }

    return 0;
}
