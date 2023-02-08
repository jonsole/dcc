/*
 * ltp305.h
 *
 * Created: 02/07/2021 17:11:33
 *  Author: jonso
 */ 


#ifndef LTP305_H_
#define LTP305_H_

#include <string.h>

#include "pio.h"
#include "sercom.h"

static uint8_t ImageLeft[11];
static uint8_t ImageRight[11];

#define NUM_DRIVERS (1)
static uint8_t LTP305_I2cAddr[2] = { 0x61, 0x62 };

extern const char LTP305_Font7x5[];



void LTP305_Init(void)
{
	/* Initialise I2C Master on SERCOM2, enable strong pull-ups on PIOs */
	SERCOM_EnableClock(2);
	SERCOM_I2cMasterInit(2, 400);
	PIO_EnablePullUp(PIN_PA12);	PIO_SetStrongDrive(PIN_PA12); PIO_EnableInput(PIN_PA12);
	PIO_EnablePullUp(PIN_PA13);	PIO_SetStrongDrive(PIN_PA13); PIO_EnableInput(PIN_PA13);
	PIO_SetPeripheral(PIN_PA12C_SERCOM2_PAD0, PIO_PERIPHERAL_C); PIO_EnablePeripheral(PIN_PA12C_SERCOM2_PAD0);
	PIO_SetPeripheral(PIN_PA13C_SERCOM2_PAD1, PIO_PERIPHERAL_C); PIO_EnablePeripheral(PIN_PA13C_SERCOM2_PAD1);

	for (uint8_t Index = 0; Index < NUM_DRIVERS; Index++)
	{
		/* Initialise display driver registers for dual 7x5 LED matrices */	
		static const uint8_t InitTable1[] = { 0x00, 0b00011000 };
		SERCOM_I2cMasterWrite(2, LTP305_I2cAddr[Index], InitTable1, sizeof(InitTable1));
		static const uint8_t InitTable2[] = { 0x0D, 0b00001110 };
		SERCOM_I2cMasterWrite(2, LTP305_I2cAddr[Index], InitTable2, sizeof(InitTable2));
	}
}

void LTP305_DrawImage(uint8_t Display, const uint8_t *Image)
{
	uint8_t Buffer[8];
	
	if (Display % 2)
	{
		Buffer[0] = 0x0E;
		memset(Buffer + 1, 0, 7);
		
		for (uint8_t i = 0; i < 8; i++)
		{
			for (uint8_t y = 0; y < 7; y++)
			Buffer[1 + i] |= Image[y] & (1 << i) ? 1 << y : 0;
		}
	}
	else
	{
		Buffer[0] = 0x01;
		memcpy(Buffer + 1, Image, 7);
	}

	SERCOM_I2cMasterWrite(2, LTP305_I2cAddr[Display / 2], Buffer, sizeof(Buffer));	
}

void LTP305_DrawChar(uint8_t Display, char Char)
{
	if (isprint(Char))
	{
		const uint8_t *Font = &LTP305_Font7x5[(Char - 32) * 7];
		LTP305_DrawImage(Display, Font);		
	}	
}

void LTP305_Update(void)
{
	for (uint8_t Index = 0; Index < NUM_DRIVERS; Index++)
	{
		static const uint8_t Update[] = { 0x0C, 0x00 };
		SERCOM_I2cMasterWrite(2, LTP305_I2cAddr[Index], Update, sizeof(Update));
	}
}

#endif /* LTP305_H_ */