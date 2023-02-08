/*
 * ltp305.h
 *
 * Created: 02/07/2021 17:11:33
 *  Author: jonso
 */ 
#include <ctype.h>
#include <string.h>

#include "pio.h"
#include "sercom.h"
#include "ltp305.h"

#define NUM_DRIVERS (1)
static uint8_t LTP305_I2cAddr[2] = { 0x61, 0x62 };

extern const uint8_t LTP305_Font7x5[];

#define LTP305_SERCOM	(3)
#define LTP305_SDA		(PIN_PA22)
#define LTP305_SCL		(PIN_PA23)

void LTP305_Init(void)
{
	/* Initialise I2C Master on SERCOM0, enable strong pull-ups on PIOs */
	SERCOM_I2cMasterInit(LTP305_SERCOM, 400);
	PIO_EnablePullUp(LTP305_SDA);	PIO_SetStrongDrive(LTP305_SDA); PIO_EnableInput(LTP305_SDA);
	PIO_EnablePullUp(LTP305_SCL);	PIO_SetStrongDrive(LTP305_SCL); PIO_EnableInput(LTP305_SCL);
	PIO_SetPeripheral(LTP305_SDA, PIO_PERIPHERAL_C); PIO_EnablePeripheral(LTP305_SDA);
	PIO_SetPeripheral(LTP305_SCL, PIO_PERIPHERAL_C); PIO_EnablePeripheral(LTP305_SCL);

	for (uint8_t Index = 0; Index < NUM_DRIVERS; Index++)
	{
		/* Initialise display driver registers for dual 7x5 LED matrices */	
		static const uint8_t InitTable1[] = { 0x00, 0b00011000 };
		SERCOM_I2cMasterWrite(LTP305_SERCOM, LTP305_I2cAddr[Index], InitTable1, sizeof(InitTable1));
		static const uint8_t InitTable2[] = { 0x0D, 0b00000111 };
		SERCOM_I2cMasterWrite(LTP305_SERCOM, LTP305_I2cAddr[Index], InitTable2, sizeof(InitTable2));
	}
	
	LTP305_DrawChar(0,'A');
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

	SERCOM_I2cMasterWrite(LTP305_SERCOM, LTP305_I2cAddr[Display / 2], Buffer, sizeof(Buffer));	
}

void LTP305_DrawChar(uint8_t Display, char Char)
{
	if (isprint(Char))
	{
		const uint8_t *Font = &LTP305_Font7x5[(Char - 32) * 7];
		LTP305_DrawImage(Display, Font);		
	}	
}

void LPT305_SetBrightness(uint8_t Brightness)
{
	for (uint8_t Index = 0; Index < NUM_DRIVERS; Index++)
	{
		uint8_t InitTable1[] = { 0x19, Brightness };
		SERCOM_I2cMasterWrite(LTP305_SERCOM, LTP305_I2cAddr[Index], InitTable1, sizeof(InitTable1));
	}
}

void LTP305_Update(void)
{
	for (uint8_t Index = 0; Index < NUM_DRIVERS; Index++)
	{
		static const uint8_t Update[] = { 0x0C, 0x00 };
		SERCOM_I2cMasterWrite(LTP305_SERCOM, LTP305_I2cAddr[Index], Update, sizeof(Update));
	}
}
