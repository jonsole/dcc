/*
 * sercom.h
 *
 * Created: 17/10/2016 21:24:31
 *  Author: Jon
 */ 


#ifndef SERCOM_H_
#define SERCOM_H_

#include <stdint.h>
#include <stdbool.h>
#include <sam.h>


static __inline uint8_t SERCOM_DmaTxTrigger(uint8_t Instance)
{
	return SERCOM0_DMAC_ID_TX + (Instance * (SERCOM1_DMAC_ID_TX - SERCOM0_DMAC_ID_TX));
}

static __inline uint8_t SERCOM_DmaRxTrigger(uint8_t Instance)
{
	return SERCOM0_DMAC_ID_RX + (Instance * (SERCOM1_DMAC_ID_RX - SERCOM0_DMAC_ID_RX));
}

static __inline uint8_t SERCOM_GetIrqNumber(uint8_t Instance)
{
	return SERCOM0_IRQn + Instance;
}

static __inline Sercom *SERCOM_GetSercom(uint8_t Instance)
{
	Sercom* InstanceTable[] =
	{
		SERCOM0, SERCOM1, SERCOM2, SERCOM3, SERCOM4, SERCOM5
	};
	return InstanceTable[Instance];
}

static __inline void SERCOM_UsartSyncResetWait(SercomUsart *Usart)
{
	while (Usart->SYNCBUSY.bit.SWRST);
}

static __inline void SERCOM_UsartSyncEnableWait(SercomUsart *Usart)
{
	while (Usart->SYNCBUSY.bit.ENABLE);
}

static __inline void SERCOM_UsartEnable(uint8_t Instance)
{
	SercomUsart *Usart = &SERCOM_GetSercom(Instance)->USART;
	Usart->CTRLA.reg |= SERCOM_USART_CTRLA_ENABLE;
	SERCOM_UsartSyncEnableWait(Usart);
}

static __inline void SERCOM_UsartDisable(uint8_t Instance)
{
	SercomUsart *Usart = &SERCOM_GetSercom(Instance)->USART;
	Usart->CTRLA.reg &= ~SERCOM_USART_CTRLA_ENABLE;
	SERCOM_UsartSyncEnableWait(Usart);
}

static __inline void SERCOM_UsartTxEnable(uint8_t Instance)
{
	SercomUsart *Usart = &SERCOM_GetSercom(Instance)->USART;
	Usart->CTRLB.bit.TXEN = 1;
}

static __inline void SERCOM_UsartTxDisable(uint8_t Instance)
{
	SercomUsart *Usart = &SERCOM_GetSercom(Instance)->USART;
	Usart->CTRLB.bit.TXEN = 0;
}

static __inline void SERCOM_UsartRxEnable(uint8_t Instance)
{
	SercomUsart *Usart = &SERCOM_GetSercom(Instance)->USART;
	Usart->CTRLB.bit.RXEN = 1;
}

static __inline void SERCOM_UsartRxDisable(uint8_t Instance)
{
	SercomUsart *Usart = &SERCOM_GetSercom(Instance)->USART;
	Usart->CTRLB.bit.RXEN = 0;
}

static __inline bool SERCOM_UsartTxReady(uint8_t Instance)
{
	SercomUsart *Usart = &SERCOM_GetSercom(Instance)->USART;
	return Usart->INTFLAG.bit.DRE;
}

static __inline void SERCOM_UsartSetRxPad(uint8_t Instance, uint8_t RxPad)
{
	SERCOM_UsartDisable(Instance);
	SercomUsart *Usart = &SERCOM_GetSercom(Instance)->USART;
	Usart->CTRLA.bit.RXPO = RxPad;
	SERCOM_UsartEnable(Instance);
}

static __inline uint16_t SERCOM_UsartGetData(SercomUsart *Usart)
{
	return (uint16_t)Usart->DATA.reg;	
}

static __inline void SERCOM_I2cmSyncWait(SercomI2cm *I2c)
{
	while (I2c->SYNCBUSY.bit.SYSOP);
}

static __inline void SERCOM_I2cmSyncEnableWait(SercomI2cm *I2c)
{
	while (I2c->SYNCBUSY.bit.ENABLE);
}

void SERCOM_UsartInit(uint8_t Instance, uint32_t BaudHz, uint8_t RxPad, uint8_t TxPad);
void SERCOM_UsartWrite(uint8_t Instance, const uint8_t *Data, uint16_t DataSize);

void SERCOM_I2cMasterInit(uint8_t Instance, uint16_t SclSpeedHz);
bool SERCOM_I2cMasterWrite(uint8_t Instance, uint8_t Address, const uint8_t *Data, uint16_t DataSize);

#endif /* SERCOM_H_ */