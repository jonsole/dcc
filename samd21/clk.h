/*
 * clk.h
 *
 * Created: 07/09/2016 20:03:29
 *  Author: Jon
 */ 

#ifndef CLK_H_
#define CLK_H_

#include <samd21.h>

extern void CLK_Init(void);
extern void CLK_EnablePeripheral(uint8_t ClkGen, uint8_t PerCh);

#define F_MCLK	(48000000UL)
#define F_GCLK0	(48000000UL)

#endif