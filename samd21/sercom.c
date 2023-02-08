/*
 * sercom.c
 *
 * Created: 17/10/2016 20:28:38
 *  Author: Jon
 */ 
#include "sercom.h"
#include "pio.h"
#include "clk.h"

#define SERCOM_REF_CLK_HZ	(F_GCLK0)


void SERCOM_UsartInit(uint8_t Instance, uint32_t BaudHz, uint8_t RxPad, uint8_t TxPad)
{
	SercomUsart *Usart = &SERCOM_GetSercom(Instance)->USART;

	PM->APBCMASK.reg |= 1UL << (PM_APBCMASK_SERCOM0_Pos + Instance);
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_SERCOMX_SLOW;
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID(GCLK_CLKCTRL_ID_SERCOM0_CORE_Val + Instance);

	/* Reset USART */
	Usart->CTRLA.reg = SERCOM_USART_CTRLA_SWRST;
	SERCOM_UsartSyncResetWait(Usart);

	/* Configure SERCOM in USART mode */	
	Usart->CTRLA.reg = SERCOM_USART_CTRLA_DORD | SERCOM_USART_CTRLA_MODE_USART_INT_CLK  |
					   SERCOM_USART_CTRLA_RXPO(RxPad) | SERCOM_USART_CTRLA_TXPO(TxPad);
	Usart->CTRLB.reg = SERCOM_USART_CTRLB_CHSIZE(0 /* 8 Bits */);
	uint64_t BaudRate = (uint64_t)65536 * (SERCOM_REF_CLK_HZ - 16 * BaudHz) / SERCOM_REF_CLK_HZ;
	Usart->BAUD.reg = (uint16_t)BaudRate;
	Usart->CTRLA.bit.ENABLE = 1;
}

void SERCOM_UsartWrite(uint8_t Instance, const uint8_t *Data, uint16_t DataSize)
{
	SercomUsart *Usart = &SERCOM_GetSercom(Instance)->USART;
	
	for (uint16_t Index = 0; Index < DataSize; Index++)
	{
		while (!Usart->INTFLAG.bit.DRE);
		Usart->DATA.reg = Data[Index];
	}	
}

void SERCOM_I2cMasterInit(uint8_t Instance, uint16_t SclSpeedHz)
{
	SercomI2cm *I2c = &SERCOM_GetSercom(Instance)->I2CM;
	
	PM->APBCMASK.reg |= 1UL << (PM_APBCMASK_SERCOM0_Pos + Instance);
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID_SERCOMX_SLOW;
	GCLK->CLKCTRL.reg = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_ID(GCLK_CLKCTRL_ID_SERCOM0_CORE_Val + Instance);

	/* Reset I2C */
	I2c->CTRLA.reg = SERCOM_I2CM_CTRLA_SWRST;
	SERCOM_I2cmSyncWait(I2c);
	
	/* Configure SERCOM in I2C master mode */
	I2c->CTRLA.reg = SERCOM_I2CM_CTRLA_MODE(0x05) | SERCOM_I2CM_CTRLA_SDAHOLD(3);
	I2c->CTRLB.reg = SERCOM_I2CM_CTRLB_SMEN; 
	SERCOM_I2cmSyncWait(I2c);
	I2c->BAUD.reg = SERCOM_I2CM_BAUD_BAUD(SERCOM_REF_CLK_HZ / (2 * SclSpeedHz) - 1);
	SERCOM_I2cmSyncWait(I2c);
	I2c->CTRLA.bit.ENABLE = 1;
	SERCOM_I2cmSyncEnableWait(I2c);
	
    /* Set the I2C bus to IDLE state */
	I2c->STATUS.reg = SERCOM_I2CM_STATUS_BUSSTATE(0x01);
	SERCOM_I2cmSyncWait(I2c);	
}


bool SERCOM_I2cMasterWrite(uint8_t Instance, uint8_t Address, const uint8_t *Data, uint16_t DataSize)
{
	SercomI2cm *I2c = &SERCOM_GetSercom(Instance)->I2CM;

	/* Acknowledge section is set as ACK signal by writing 0 in ACKACT bit */	I2c->CTRLB.reg &= ~SERCOM_I2CM_CTRLB_ACKACT;
	SERCOM_I2cmSyncWait(I2c);

	/* Address slave */
	I2c->ADDR.reg = (Address << 1) | 0x00;
	while (!I2c->INTFLAG.bit.MB) {};

	if (I2c->STATUS.reg & SERCOM_I2CM_STATUS_RXNACK)
	{
		I2c->CTRLB.reg |= SERCOM_I2CM_CTRLB_CMD(3);
		return false;
	}

	for (uint16_t Index = 0; Index < DataSize; Index++)
	{
		I2c->DATA.reg = Data[Index];
		while (!I2c->INTFLAG.bit.MB) {};

		if (I2c->STATUS.reg & SERCOM_I2CM_STATUS_RXNACK)
		{
			I2c->CTRLB.reg |= SERCOM_I2CM_CTRLB_CMD(3);
			return false;
		}
	}

	I2c->CTRLB.reg |= SERCOM_I2CM_CTRLB_CMD(3);
	return true;
}




