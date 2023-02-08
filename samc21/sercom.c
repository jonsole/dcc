/*
 * sercom.c
 *
 * Created: 17/10/2016 20:28:38
 *  Author: Jon
 */ 
#include "sercom.h"
#include "pio.h"
#include "clk.h"

static void (*SERCOM_IntHandler[6])(uint8_t, void *);
static void *SERCOM_IntHandlerData[6];

#define SERCOM_REF_CLK_HZ	(F_GCLK0)

void SERCOM0_Handler(void)  __attribute__((__interrupt__));
void SERCOM0_Handler(void)
{
	const uint8_t IntPending = SERCOM0->USART.INTFLAG.reg;
	SERCOM_IntHandler[0](IntPending, SERCOM_IntHandlerData[0]);
}

void SERCOM1_Handler(void)  __attribute__((__interrupt__));
void SERCOM1_Handler(void)
{
	const uint8_t IntPending = SERCOM1->USART.INTFLAG.reg;
	SERCOM_IntHandler[1](IntPending, SERCOM_IntHandlerData[1]);
}

void SERCOM2_Handler(void)  __attribute__((__interrupt__));
void SERCOM2_Handler(void)
{
	const uint8_t IntPending = SERCOM2->USART.INTFLAG.reg;
	SERCOM_IntHandler[2](IntPending, SERCOM_IntHandlerData[2]);
}

void SERCOM3_Handler(void)  __attribute__((__interrupt__));
void SERCOM3_Handler(void)
{
	const uint8_t IntPending = SERCOM3->USART.INTFLAG.reg;
	SERCOM_IntHandler[3](IntPending, SERCOM_IntHandlerData[3]);
}

void SERCOM4_Handler(void)  __attribute__((__interrupt__));
void SERCOM4_Handler(void)
{
	const uint8_t IntPending = SERCOM4->USART.INTFLAG.reg;
	SERCOM_IntHandler[4](IntPending, SERCOM_IntHandlerData[4]);
}

void SERCOM5_Handler(void)  __attribute__((__interrupt__));
void SERCOM5_Handler(void)
{
	const uint8_t IntPending = SERCOM4->USART.INTFLAG.reg;
	SERCOM_IntHandler[5](IntPending, SERCOM_IntHandlerData[5]);
}


void SERCOM_UsartInit(uint8_t Instance, uint32_t BaudHz, uint8_t RxPad, uint8_t TxPad)
{
	SercomUsart *Usart = &SERCOM_GetSercom(Instance)->USART;

	static const uint8_t GclkPch[6] =     {SERCOM0_GCLK_ID_CORE, SERCOM1_GCLK_ID_CORE, SERCOM2_GCLK_ID_CORE, SERCOM3_GCLK_ID_CORE, SERCOM4_GCLK_ID_CORE, SERCOM5_GCLK_ID_CORE};
	static const uint8_t GclkPchSlow[6] = {SERCOM0_GCLK_ID_SLOW, SERCOM1_GCLK_ID_SLOW, SERCOM2_GCLK_ID_SLOW, SERCOM3_GCLK_ID_SLOW, SERCOM4_GCLK_ID_SLOW, SERCOM5_GCLK_ID_SLOW};

	MCLK->APBCMASK.reg |= (MCLK_APBCMASK_SERCOM0 << Instance);

	/* Enable peripheral clocks */
	CLK_EnablePeripheral(0, GclkPchSlow[Instance]);
	CLK_EnablePeripheral(0, GclkPch[Instance]);

	/* Reset USART */
	Usart->CTRLA.reg = SERCOM_USART_CTRLA_SWRST;
	SERCOM_UsartSyncResetWait(Usart);

	/* Configure SERCOM in USART mode */
	Usart->CTRLA.reg = SERCOM_USART_CTRLA_DORD | SERCOM_USART_CTRLA_RXPO(RxPad) | SERCOM_USART_CTRLA_TXPO(TxPad) | SERCOM_USART_CTRLA_MODE(1);
	Usart->CTRLB.reg = SERCOM_USART_CTRLB_CHSIZE(0 /* 8 Bits */);
	uint64_t BaudRate = (uint64_t)65536 * (SERCOM_REF_CLK_HZ - 16 * BaudHz) / SERCOM_REF_CLK_HZ;
	Usart->BAUD.reg = (uint16_t)BaudRate;
	Usart->CTRLA.bit.ENABLE = 1;
}

void SERCOM_UsartEnableInterrupt(uint8_t Instance, uint8_t InterruptMask, void (*IntHandler)(uint8_t, void *), void *IntData)
{
	SercomUsart *Usart = &SERCOM_GetSercom(Instance)->USART;
	
	/* Store handler function and data */
	SERCOM_IntHandlerData[Instance] = IntData;
	SERCOM_IntHandler[Instance] = IntHandler;
	
	/* Enable interrupt(s) */
	Usart->INTENSET.reg = InterruptMask;	
	
	/* Enable SERCOM interrupts, priority 0 */
	NVIC_SetPriority(SERCOM_GetIrqNumber(Instance), 0);
	NVIC_EnableIRQ(SERCOM_GetIrqNumber(Instance));
}



void SERCOM_I2cMasterInit(uint8_t Instance, uint16_t SclSpeedHz)
{
	SercomI2cm *I2c = &SERCOM_GetSercom(Instance)->I2CM;
	
	static const uint8_t GclkPch[6] =     {SERCOM0_GCLK_ID_CORE, SERCOM1_GCLK_ID_CORE, SERCOM2_GCLK_ID_CORE, SERCOM3_GCLK_ID_CORE, SERCOM4_GCLK_ID_CORE, SERCOM5_GCLK_ID_CORE};
	static const uint8_t GclkPchSlow[6] = {SERCOM0_GCLK_ID_SLOW, SERCOM1_GCLK_ID_SLOW, SERCOM2_GCLK_ID_SLOW, SERCOM3_GCLK_ID_SLOW, SERCOM4_GCLK_ID_SLOW, SERCOM5_GCLK_ID_SLOW};

	MCLK->APBCMASK.reg |= (MCLK_APBCMASK_SERCOM0 << Instance);

	/* Enable peripheral clocks */
	CLK_EnablePeripheral(0, GclkPchSlow[Instance]);
	CLK_EnablePeripheral(0, GclkPch[Instance]);
	
	/* Reset I2C */
	I2c->CTRLA.reg = SERCOM_I2CM_CTRLA_SWRST;
	SERCOM_I2cmSyncWait(I2c);
	
	/* Select I2C Master mode */
	I2c->CTRLB.reg = SERCOM_I2CM_CTRLB_SMEN; 
	SERCOM_I2cmSyncWait(I2c);
 	
	/* Set I2C master SCL baud rate */
	I2c->BAUD.reg = SERCOM_I2CM_BAUD_BAUD(SERCOM_REF_CLK_HZ / (2 * SclSpeedHz) - 1);
	SERCOM_I2cmSyncWait(I2c);
	
	/* Enable SERCOM in I2C master mode */
	I2c->CTRLA.reg = SERCOM_I2CM_CTRLA_MODE(0x05) | SERCOM_I2CM_CTRLA_SDAHOLD(3) | SERCOM_I2CM_CTRLA_ENABLE;
	SERCOM_I2cmSyncWait(I2c);
	
    /* Set the I2C bus to IDLE state */
	I2c->STATUS.reg = SERCOM_I2CM_STATUS_BUSSTATE(0x01);
	SERCOM_I2cmSyncWait(I2c);
	
	/* Enable SERCOM ERROR interrupts */
	//I2c->INTENSET.bit.ERROR = 1;			 	
	SERCOM_I2cmSyncWait(I2c);
}


bool SERCOM_I2cMasterWrite(uint8_t Instance, uint8_t Address, const uint8_t *Data, uint16_t DataSize)
{
	SercomI2cm *I2c = &SERCOM_GetSercom(Instance)->I2CM;

	I2c->ADDR.reg = (Address << 1) | 0x00;
	while (0 == (I2c->INTFLAG.reg & SERCOM_I2CM_INTFLAG_MB));

	if (I2c->STATUS.reg & SERCOM_I2CM_STATUS_RXNACK)
	{
		I2c->CTRLB.reg |= SERCOM_I2CM_CTRLB_CMD(3);
		return false;
	}

	for (uint16_t Index = 0; Index < DataSize; Index++)
	{
		I2c->DATA.reg = Data[Index];
		while (0 == (I2c->INTFLAG.reg & SERCOM_I2CM_INTFLAG_MB));

		if (I2c->STATUS.reg & SERCOM_I2CM_STATUS_RXNACK)
		{
			I2c->CTRLB.reg |= SERCOM_I2CM_CTRLB_CMD(3);
			return false;
		}
	}

	I2c->CTRLB.reg |= SERCOM_I2CM_CTRLB_CMD(3);
	return true;
}




