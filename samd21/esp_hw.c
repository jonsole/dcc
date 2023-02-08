/*
 * esp_hw.c
 *
 * Created: 21/05/2016 21:39:52
 *  Author: Jon
 */ 

#include "dmac.h"
#include "esp.h"
#include "pio.h"
#include "sercom.h"

static void ESP_InterruptHandler(void *EspVoid, uint8_t DmaChannel, uint16_t IntPending)
{
	ESP_t *Esp = (ESP_t *)EspVoid;
	const uint8_t IntStatus = DMAC->CHINTFLAG.reg;
	if (IntStatus & DMAC_CHINTFLAG_TCMPL)
	{
		/* Synchronise buffer outdex with PDC transmit address */
		ESP_HwTxDmaSync(Esp);

		/* Re-start DMA if there's any more data in buffer */
		ESP_HwTxKick(Esp);
	}

	/* Clear all channel interrupts */
	DMAC->CHINTFLAG.reg = IntStatus;
}


void ESP_HwInit(ESP_t *Esp, uint8_t DmaChannel, uint8_t Timer)
{
	/* Initialise Rx and Tx buffers */
	BufferInit(Esp->Hw.RxUsartBuffer);
	BufferInit(Esp->Hw.TxUsartBuffer);

	/* Allocate DMA channels for USART transmit and receive */
	Esp->Hw.RxUsartDmaChannel = DMAC_ChannelAllocate(NULL, NULL, DmaChannel);
	Esp->Hw.TxUsartDmaChannel = DMAC_ChannelAllocate(ESP_InterruptHandler, Esp, DMAC_NO_CHANNEL);
	Esp->Hw.TxUsartDmaTrigger = SERCOM_DmaTxTrigger(Esp->Instance);
	Esp->Hw.RxUsartDmaTrigger = SERCOM_DmaRxTrigger(Esp->Instance); 
	
	/* Initialise USART */
	SERCOM_UsartInit(Esp->Instance, 115200, 3, 1);
	SERCOM_UsartTxEnable(Esp->Instance);
	SERCOM_UsartRxEnable(Esp->Instance);

	/* Start transmit and receive DMA */
	ESP_HwRxDmaStart(Esp);
	ESP_HwTxKick(Esp);
}

void ESP_HwTask(ESP_t *Esp)
{
	ESP_HwRxDmaSync(Esp);
}
