/*
 * ac.c
 *
 * Created: 24/02/2023 16:02:31
 *  Author: jonso
 */ 

#include <sam.h>
#include <stdint.h>

#include "pio.h"
#include "debug.h"
#include "rtime.h"

uint32_t AC_TriggeredCount = 0;
uint32_t AC_TriggerStart;
uint32_t AC_TriggerEnd;

extern volatile Time_t DCC_TimerTime;


void AC_Handler(void)
{
	uint32_t status = AC->INTFLAG.reg;
	AC->INTFLAG.reg = status;
	if (AC->STATUSA.bit.STATE3)
	{
		AC_TriggerStart = DCC_TimerTime;
		PIO_Set(PIN_PB09);
	}
	else
	{
		AC_TriggerEnd = DCC_TimerTime;
		PIO_Clear(PIN_PB09);
	}
		
	AC_TriggeredCount += 1;
}


void AC_Init(void)
{
	/* Enable AC clock */
	MCLK->APBCMASK.reg |= MCLK_APBCMASK_AC;
	
	/* Drive GCLK_AC from 1MHz GCLK1 */
	CLK_EnablePeripheral(GCLK_PCHCTRL_GEN_GCLK1_Val, ADC1_GCLK_ID);
	
	PIO_EnableInput(PIN_PB05B_AC_AIN6);
	PIO_DisablePull(PIN_PB05B_AC_AIN6);
	PIO_SetPeripheral(PIN_PB05B_AC_AIN6, PIO_PERIPHERAL_B);
	PIO_EnablePeripheral(PIN_PB05B_AC_AIN6);

	AC->CTRLA.reg = AC_CTRLA_SWRST;			// Reset AC register
	while (AC->SYNCBUSY.bit.SWRST);
	
	// COMPx - Positive Pin, and Negative VDDscale source, Hysteresis Control, High Speed, Interruption Rising
	AC->COMPCTRL[3].reg = AC_COMPCTRL_MUXPOS_PIN2 | AC_COMPCTRL_MUXNEG_VSCALE |
						  AC_COMPCTRL_HYSTEN | AC_COMPCTRL_INTSEL_TOGGLE |
						  AC_COMPCTRL_SPEED_LOW | AC_COMPCTRL_OUT_OFF | AC_COMPCTRL_FLEN_MAJ5;
	AC->SCALER[3].reg = AC_SCALER_VALUE(7);
	AC->CTRLA.bit.ENABLE = 1;
	
	NVIC_SetPriority(AC_IRQn, 3);
	NVIC_EnableIRQ(AC_IRQn);	
}


	