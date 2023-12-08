/*
 * eic.c
 *
 * Created: 02/07/2021 11:34:43
 *  Author: jonso
 */ 

#include <sam.h>
#include "eic.h"
#include "pio.h"


void (*EIC_InterruptHandler[8])(void *, const uint32_t);
void *EIC_InterruptData[8];


void EIC_Init(void)
{
	/* For generic clock generator 2, select the 1Mhz GCLK1 Clock as input to generate 15.25 Hz clock */
	GCLK->GENCTRL[2].reg = GCLK_GENCTRL_DIVSEL | GCLK_GENCTRL_DIV(15) | GCLK_GENCTRL_GENEN | GCLK_GENCTRL_SRC_GCLKGEN1 | GCLK_GENCTRL_OE;

	/* Enable EIC clock */
	MCLK->APBAMASK.reg |= MCLK_APBAMASK_EIC;
	
	/* Drive GCLK_EIC from 2 KHz GCLK2 */
	GCLK->PCHCTRL[EIC_GCLK_ID].reg &= ~GCLK_PCHCTRL_CHEN;
	while (GCLK->PCHCTRL[EIC_GCLK_ID].reg & GCLK_PCHCTRL_CHEN);
	GCLK->PCHCTRL[EIC_GCLK_ID].reg = GCLK_PCHCTRL_GEN_GCLK2;
	GCLK->PCHCTRL[EIC_GCLK_ID].reg |= GCLK_PCHCTRL_CHEN;
	while (!(GCLK->PCHCTRL[EIC_GCLK_ID].reg & GCLK_PCHCTRL_CHEN));
	
	NVIC_SetPriority(EIC_IRQn, 3);
	NVIC_EnableIRQ(EIC_IRQn);

	EIC->CTRLA.bit.ENABLE = 1;
	while (EIC->SYNCBUSY.bit.ENABLE);
}


void EIC_ConfigureEdgeInterrupt(uint8_t Pio, uint8_t ExtInt, bool Filter, void (*Handler)(void *, const uint32_t), void *HandlerData)
{
	PIO_SetPeripheral(Pio, PIO_PERIPHERAL_A);
	PIO_EnablePeripheral(Pio);

	EIC_InterruptHandler[ExtInt] = Handler;
	EIC_InterruptData[ExtInt] = HandlerData;
	
	EIC->CTRLA.bit.ENABLE = 0;
	while (EIC->SYNCBUSY.bit.ENABLE);
	
	const uint8_t Group = ExtInt / 8;
	const uint8_t Shift = (ExtInt % 8) * 4;
	EIC->CONFIG[Group].reg &= ~(0xF << Shift);
	EIC->CONFIG[Group].reg |= (EIC_CONFIG_SENSE0_BOTH | (Filter ? EIC_CONFIG_FILTEN0 : 0)) << Shift;
	EIC->INTENSET.reg |= (1UL << ExtInt);

	EIC->CTRLA.bit.ENABLE = 1;
	while (EIC->SYNCBUSY.bit.ENABLE);
}

void EIC_Handler(void)  __attribute__((__interrupt__));
void EIC_Handler(void)
{
	uint32_t Status = EIC->INTFLAG.reg;
	EIC->INTFLAG.reg = Status;

	uint8_t Int = 0;
	uint32_t IntStatus = Status;
	while (IntStatus)
	{
		if ((IntStatus & 0x01) && EIC_InterruptHandler[Int])
		EIC_InterruptHandler[Int](EIC_InterruptData[Int], Status);
		
		IntStatus = IntStatus >> 1;
		Int += 1;
	}
}


