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
	uint8_t State;
	uint8_t Pin;
} BUT_Button_t;

#define NUM_BUTTONS (16)
BUT_Button_t Button[16];

void BUT_InitButton(uint8_t Instance, uint8_t Pin)
{
	PIO_EnableInput(Pin);
	PIO_EnablePullUp(Pin);
	
	BUT_Button_t *b = &Button[Instance];
	b->Pin = Pin;
	b->State = PIO_Read(b->Pin) ? 0xFF : 0x00;
}


void BUT_ScanButtons(void)
{
	for (int Index = 0; Index < NUM_BUTTONS; Index++)
	{
		BUT_Button_t *b = &Button[Index];
		b->State = (b->State << 1) | PIO_Read(b->Pin);
	}
}

bool BUT_IsButtonPressed(uint8_t Instance)
{
	BUT_Button_t *b = &Button[Instance];
	return ((b->State & 0xF) == 0x00);
}
