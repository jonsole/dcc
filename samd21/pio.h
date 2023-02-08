/*
 * pio.h
 *
 * Created: 11/07/2016 21:05:39
 *  Author: Jon
 */ 
#ifndef PIO_H_
#define PIO_H_

#include <sam.h>
#include <stdint.h>

enum
{
	PIO_PERIPHERAL_A = 0,
	PIO_PERIPHERAL_B,
	PIO_PERIPHERAL_C,
	PIO_PERIPHERAL_D,
	PIO_PERIPHERAL_E,
	PIO_PERIPHERAL_F,
	PIO_PERIPHERAL_G,
	PIO_PERIPHERAL_H,
	PIO_PERIPHERAL_I,
};

static __inline uint8_t PIO_GetPioGroup(uint8_t Pio)
{
	return Pio >> 5;
}

static __inline void PIO_EnablePeripheral(uint8_t Pio)
{
	const uint8_t Group = PIO_GetPioGroup(Pio);
	PORT->Group[Group].PINCFG[Pio % 32].bit.PMUXEN = 1;
}

static __inline void PIO_DisablePeripheral(uint8_t Pio)
{
	const uint8_t Group = PIO_GetPioGroup(Pio);
	PORT->Group[Group].PINCFG[Pio % 32].bit.PMUXEN = 0;
}

static __inline void PIO_SetPeripheral(uint8_t Pio, uint8_t Peripheral)
{
	const uint8_t Group = PIO_GetPioGroup(Pio);
	const uint8_t Pmux = (Pio % 32) / 2;
	
	/* Check if odd or even PIO */
	if (Pio % 2)
		PORT->Group[Group].PMUX[Pmux].bit.PMUXO = Peripheral;
	else
		PORT->Group[Group].PMUX[Pmux].bit.PMUXE = Peripheral;
}

static __inline void PIO_EnableInput(uint8_t Pio)
{
	const uint8_t Group = PIO_GetPioGroup(Pio);
	PORT->Group[Group].DIRCLR.reg = 1 << Pio % 32;
	PORT->Group[Group].PINCFG[Pio % 32].bit.INEN = 1;
	PORT->Group[Group].CTRL.bit.SAMPLING = 1;
}

static __inline void PIO_EnablePullUp(uint8_t Pio)
{
	const uint8_t Group = PIO_GetPioGroup(Pio);
	PORT->Group[Group].OUTSET.reg = 1UL << (Pio % 32);
	PORT->Group[Group].PINCFG[Pio % 32].bit.PULLEN = 1;
}

static __inline void PIO_EnablePullDown(uint8_t Pio)
{
	const uint8_t Group = PIO_GetPioGroup(Pio);
	PORT->Group[Group].OUTCLR.reg = 1UL << (Pio % 32);
	PORT->Group[Group].PINCFG[Pio % 32].bit.PULLEN = 1;
}

static __inline void PIO_DisablePull(uint8_t Pio)
{
	const uint8_t Group = PIO_GetPioGroup(Pio);
	PORT->Group[Group].PINCFG[Pio % 32].bit.PULLEN = 0;
}

static __inline void PIO_SetStrongDrive(uint8_t Pio)
{
	const uint8_t Group = PIO_GetPioGroup(Pio);
	PORT->Group[Group].PINCFG[Pio % 32].bit.DRVSTR = 1;
}

static __inline void PIO_EnableOutput(uint8_t Pio)
{
	const uint8_t Group = PIO_GetPioGroup(Pio);
	PORT->Group[Group].DIRSET.reg = 1UL << (Pio % 32);
}

static __inline uint8_t PIO_Read(uint8_t Pio)
{
	const uint8_t Group = PIO_GetPioGroup(Pio);
	return (PORT->Group[Group].IN.reg >> (Pio % 32)) & 0x01;
}

static __inline void PIO_Set(uint8_t Pio)
{
	const uint8_t Group = PIO_GetPioGroup(Pio);
	PORT->Group[Group].OUTSET.reg = 1UL << (Pio % 32);
}

static __inline void PIO_Clear(uint8_t Pio)
{
	const uint8_t Group = PIO_GetPioGroup(Pio);
	PORT->Group[Group].OUTCLR.reg = 1UL << (Pio % 32);
}


static __inline void PIO_SetGroup(uint8_t Group, uint32_t Data, uint32_t Mask)
{
	uint32_t PioData = PORT->Group[Group].OUT.reg;
	PioData &= ~Mask;
	PioData |= Data;
	PORT->Group[Group].OUT.reg = PioData;
}

#endif /* PIO_H_ */