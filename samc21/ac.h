/*
 * ac.h
 *
 * Created: 24/02/2023 17:23:27
 *  Author: jonso
 */ 


#ifndef AC_H_
#define AC_H_

#include "pio.h"
#include "rtime.h"
#include "debug.h"

extern uint32_t AC_TriggeredCount;
extern uint32_t AC_TriggerStart;
extern uint32_t AC_TriggerEnd;
extern volatile Time_t DCC_TimerTime;

void AC_Init(void);

static inline void AC_EnableTrigger(void)
{
	AC_TriggerStart = AC_TriggerEnd = DCC_TimerTime;
	AC->COMPCTRL[3].bit.ENABLE = 1;
	AC->INTENSET.reg |= AC_INTENSET_COMP3;
}

static inline Time_t AC_DisableTrigger(void)
{
	if (AC->STATUSA.bit.STATE3)
		AC_TriggerEnd = DCC_TimerTime;

	AC->COMPCTRL[3].bit.ENABLE = 0;
	AC->INTENCLR.reg |= AC_INTENCLR_COMP3;
	PIO_Clear(PIN_PB09);
	return Time_Sub(AC_TriggerEnd, AC_TriggerStart);
}

static inline void AC_ResetTriggerCount(void)
{
	AC_TriggeredCount = 0;
}

static inline uint32_t AC_TriggerCount(void)
{
	return AC_TriggeredCount;
}


#endif /* AC_H_ */