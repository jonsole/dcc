/*
 * eic.c
 *
 * Created: 02/07/2021 11:34:43
 *  Author: jonso
 */ 

#include <samd21.h>
#include "eic.h"
#include "pio.h"
#include "clk.h"


void (*EIC_InterruptHandler[16])(void *, const uint32_t);
void *EIC_InterruptData[16];


void EIC_Init(void)
{
	/* Enable EIC clock */
	PM->APBAMASK.reg |= PM_APBAMASK_EIC;
	
	/* Drive GCLK_EIC from 16Hz GCLK3 */
	CLK_EnablePeripheral(3, GCLK_CLKCTRL_ID_EIC);
	
	NVIC_EnableIRQ(EIC_IRQn);

	EIC->CTRL.bit.ENABLE = 1;
	while (EIC->STATUS.bit.SYNCBUSY);
}


void EIC_ConfigureEdgeInterrupt(uint8_t Pio, uint8_t ExtInt, bool Filter, void (*Handler)(void *, const uint32_t), void *HandlerData)
{
	PIO_SetPeripheral(Pio, PIO_PERIPHERAL_A);
	PIO_EnablePeripheral(Pio);

	EIC_InterruptHandler[ExtInt] = Handler;
	EIC_InterruptData[ExtInt] = HandlerData;
	
	EIC->CTRL.bit.ENABLE = 0;
	while (EIC->STATUS.bit.SYNCBUSY);
	
	const uint8_t Group = ExtInt / 8;
	const uint8_t Shift = (ExtInt % 8) * 4;
	EIC->CONFIG[Group].reg &= ~(0xF << Shift);
	EIC->CONFIG[Group].reg |= (EIC_CONFIG_SENSE0_BOTH | (Filter ? EIC_CONFIG_FILTEN0 : 0)) << Shift;
	EIC->INTENSET.reg |= (1UL << ExtInt);

	EIC->CTRL.bit.ENABLE = 1;
	while (EIC->STATUS.bit.SYNCBUSY);
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


