/*
 * evsys.c
 *
 * Created: 13/02/2020 20:16:52
 *  Author: WinUser
 */ 

#include "evsys.h"

void EVSYS_Init(void)
{
 	/* Enable Event System Bus clock */
 	PM->APBCMASK.reg |= PM_APBCMASK_EVSYS;
}


void EVSYS_AsyncChannel(uint8_t Channel, uint8_t Generator, uint8_t User)
{
    //GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_EVSYS_0 + Channel;
	//EVSYS->USER[User].reg = EVSYS_USER_CHANNEL(Channel + 1);
 	//EVSYS->CHANNEL[Channel].reg = EVSYS_CHANNEL_EDGSEL_NO_EVT_OUTPUT | EVSYS_CHANNEL_PATH_ASYNCHRONOUS | EVSYS_CHANNEL_EVGEN(Generator);
}


void EVSYS_SyncChannel(uint8_t Channel, uint8_t Edge, uint8_t Generator, uint8_t User)
{
    //GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_EVSYS_0 + Channel;
	//EVSYS->USER[User].reg = EVSYS_USER_CHANNEL(Channel + 1);
	//EVSYS->CHANNEL[Channel].reg = Edge | EVSYS_CHANNEL_PATH_SYNCHRONOUS | EVSYS_CHANNEL_EVGEN(Generator);
}