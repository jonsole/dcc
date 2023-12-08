/*
 * button.c
 *
 * Created: 22/10/2021 21:44:50
 *  Author: jonso
 */ 

#include <stdbool.h>

#include "pio.h"
#include "button.h"

typedef struct
{
	uint16_t State;
	uint8_t Pin;
	uint16_t Counter;
	bool PullDown;
	BUT_Event_t Event;
} BUT_Button_t;

#define NUM_BUTTONS (9)
BUT_Button_t Button[9];

void BUT_InitButton(uint8_t Instance, uint8_t Pin, bool PullDown)
{
	PIO_EnableInput(Pin);
	if (PullDown)
		PIO_EnablePullDown(Pin);
	else
		PIO_EnablePullUp(Pin);
	
	BUT_Button_t *b = &Button[Instance];
	b->Pin = Pin;
	b->State = 0x00;
	b->Counter = 0;
	b->PullDown = PullDown;
	b->Event = BUT_EVENT_NONE;
}


void BUT_ScanButtons(void)
{
	for (int Index = 0; Index < NUM_BUTTONS; Index++)
	{
		BUT_Button_t *b = &Button[Index];
		const uint16_t State = b->State;
		
		uint16_t	PioState = PIO_Read(b->Pin) ? 0x00 : 0x01;
		if (b->PullDown)
			PioState ^= 0x01;
			
		b->State = (State << 1) | (PioState);
		b->Event = BUT_EVENT_NONE;

		/* If button active for 7 scans then button has been pressed */
		if ((State == 0b0111111111111111) && (b->State == 0b1111111111111111))
		{
			b->Counter = 0;
			b->Event = BUT_EVENT_PRESSED;
		}
		/* If button inactive for 7 scans then button has been released or clicked */
		else if ((State == 0b1000000000000000) && (b->State == 0b0000000000000000))
		{
			if (b->Counter < 512)
				b->Event = BUT_EVENT_RELEASED;
			else
				b->Event = BUT_EVENT_HELD_RELEASED;
	
			b->Counter = 0;
		}
		else if (b->State == 0b1111111111111111)
		{
			if (b->Counter < 512)
			{
				b->Counter += 1;
				if (b->Counter == 512)
					b->Event = BUT_EVENT_HELD;
			}
		}		
	}
}

BUT_Event_t BUT_GetEvent(uint8_t Instance)
{
	BUT_Button_t *b = &Button[Instance];
	return b->Event;
}

bool BUT_IsPressed(uint8_t Instance)
{
	BUT_Button_t *b = &Button[Instance];
	return (b->State == 0b1111111111111111);
}