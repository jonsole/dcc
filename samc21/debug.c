#include "debug.h"
#include "dmac.h"
#include "pio.h"
#include "buffer.h"
#include "sercom.h"
#include "os.h"

Debug_t DebugState;

static void Debug_TxDmaSync(void)
{
	DMAC_Descriptor_t *Desc = DMAC_ChannelGetBaseDescriptor(DebugState.TxUsartDmaChannel);
	uint16_t ByteCount = DMAC_ChannelGetByteCount(DebugState.TxUsartDmaChannel);
	uint8_t *TxAddress = (uint8_t *)Desc->SRCADDR.reg - ByteCount;
	BufferSetOutdexFromAddress(DebugState.TxBuffer, TxAddress);
}

static void Debug_TxKick(void)
{
	if (DebugState.Usart)
	{
		OS_InterruptDisable();

		/* Can only start DMA if transmit isn't in progress */
		if (!DebugState.TxInProgress)
		{
			/* Check if there is actually anything to transfer in buffer */
			const uint16_t Amount = BufferAmount(DebugState.TxBuffer);
			if (Amount)
			{
				const uint16_t AmountToWrap = BufferAmountToWrap(DebugState.TxBuffer);
			
				/* Update descriptor */
				DMAC_Descriptor_t *DmaDesc = DMAC_ChannelGetBaseDescriptor(DebugState.TxUsartDmaChannel);
				DmaDesc->BTCNT.reg = (Amount > AmountToWrap) ? AmountToWrap : Amount;
				DmaDesc->SRCADDR.reg = (uint32_t)(DebugState.TxBuffer.Buffer + BufferOutdex(DebugState.TxBuffer) + DmaDesc->BTCNT.reg);
	
				/* Start transfer */
				uint8_t ChannelId = DMAC->CHID.reg;
				DMAC->CHID.reg = DebugState.TxUsartDmaChannel;
				DMAC->CHCTRLA.reg = DMAC_CHCTRLA_ENABLE;
				DMAC->CHID.reg = ChannelId;
				
				/* Set in progress flag */
				PIO_Set(PIN_PA20);
				DebugState.TxInProgress = 1;
			}
		}
	
		OS_InterruptEnable();
	}
}

static void Debug_RxInterruptHandler(uint8_t IntPending, void *DebugVoid)
{
	const uint16_t Data = SERCOM_UsartGetData(DebugState.Usart);
	BufferWrite(DebugState.RxBuffer, (uint8_t)Data);
	OS_SignalSend(DebugState.ClientTask, DebugState.ClientSignal);
}

static void Debug_TxInterruptHandler(void *DebugVoid, uint8_t DmaChannel, uint16_t IntPending)
{
	uint8_t ChannelId = DMAC->CHID.reg;
	DMAC->CHID.reg = DebugState.TxUsartDmaChannel;	
	const uint8_t IntStatus = DMAC->CHINTFLAG.reg;
	DMAC->CHINTFLAG.reg = IntStatus;
	DMAC->CHID.reg = ChannelId;

	PIO_Clear(PIN_PA20);
	if (IntStatus & DMAC_CHINTFLAG_TCMPL)
	{
		/* Synchronise buffer outdex with PDC transmit address */
		Debug_TxDmaSync();

		/* Clear in progress flag */
		DebugState.TxInProgress = 0;

		/* Re-start DMA if there's any more data in buffer */
		Debug_TxKick();
	}
	
	if (IntStatus & DMAC_CHINTFLAG_TERR)
		Panic();
			
	
}

void Debug_FormatPutChar(char Char, void *Context)
{
	if (BufferSpace(DebugState.TxBuffer) < 2)
		return;
		
	BufferWrite(DebugState.TxBuffer, Char);
	if (Char == '\n')
		Debug_TxKick();
	else if (BufferAmount(DebugState.TxBuffer) > 80)
		Debug_TxKick();
}

uint8_t Debug_GetChar(void)
{
	if (BufferIsEmpty(DebugState.RxBuffer))
		return 0;
		
	return BufferRead(DebugState.RxBuffer);
}

void Debug_PutChar(char Char)
{
	if (BufferSpace(DebugState.TxBuffer) < 2)
		Panic();

	BufferWrite(DebugState.TxBuffer, Char);
	Debug_TxKick();
}

void Debug_Init(OS_TaskId_t TaskId, OS_SignalSet_t DebugSignal)
{
	DebugState.Usart = &SERCOM4->USART;
	DebugState.Instance = 4;
	DebugState.ClientTask = TaskId;
	DebugState.ClientSignal = DebugSignal;

	/* Initialise USART */
	SERCOM_UsartInit(DebugState.Instance, 115200, 3, 1);
	SERCOM_UsartTxEnable(DebugState.Instance);
	SERCOM_UsartRxEnable(DebugState.Instance);

	/* Configure PIOs for this USART */
	PanicFalse(DebugState.Instance == 4);
	PIO_SetPeripheral(PIN_PB10, PIO_PERIPHERAL_D);
	PIO_EnablePeripheral(PIN_PB10);
	PIO_SetPeripheral(PIN_PB11, PIO_PERIPHERAL_D);
	PIO_EnablePeripheral(PIN_PB11);

	/* Allocate DMA channels for USART transmit */
	DebugState.TxUsartDmaChannel = DMAC_ChannelAllocate(Debug_TxInterruptHandler, NULL, DMAC_NO_CHANNEL);
	DebugState.TxUsartDmaTrigger = SERCOM_DmaTxTrigger(DebugState.Instance);

	DMAC_Descriptor_t *DmaDesc = DMAC_ChannelGetBaseDescriptor(DebugState.TxUsartDmaChannel);
	DmaDesc->BTCTRL.reg = DMAC_BTCTRL_BEATSIZE_BYTE | DMAC_BTCTRL_SRCINC | DMAC_BTCTRL_BLOCKACT_INT | DMAC_BTCTRL_VALID;
	DmaDesc->DSTADDR.reg = (uint32_t)&DebugState.Usart->DATA;
	DmaDesc->DESCADDR.reg = 0;

	/* Reset channel */
	DMAC->CHID.reg = DebugState.TxUsartDmaChannel;
	DMAC->CHCTRLA.reg &= ~DMAC_CHCTRLA_ENABLE;
	DMAC->CHCTRLA.reg = DMAC_CHCTRLA_SWRST;

	/* Configure and enable DMA interrupt */
	DMAC->CHCTRLB.reg = DMAC_CHCTRLB_TRIGACT_BEAT | DMAC_CHCTRLB_TRIGSRC(DebugState.TxUsartDmaTrigger);
	DMAC->CHINTENSET.reg = DMAC_CHINTENSET_MASK;
	
	BufferInit(DebugState.TxBuffer);
	BufferInit(DebugState.RxBuffer);
	DebugState.Level = 0;

	/* Enable Rx interrupt */
	SERCOM_UsartEnableInterrupt(DebugState.Instance, SERCOM_USART_INTENSET_RXC, Debug_RxInterruptHandler, &DebugState);
}

void Debug_SetLevel(uint8_t Level)
{
	DebugState.Level = Level;
}

uint8_t Debug_GetLevel(void)
{
	return DebugState.Level;
}

void Debug_Panic(const char *Msg, const char *File, int Line)
{
	OS_InterruptDisable();

	/* Turn of power to track if we crash to stop runaway trains */
	PIO_DisablePeripheral(PIN_PA08);
	PIO_DisablePeripheral(PIN_PA09);
	PIO_EnableOutput(PIN_PA08);
	PIO_Clear(PIN_PA08);
	PIO_EnableOutput(PIN_PA09);
	PIO_Clear(PIN_PA09);

	while (BufferAmount(DebugState.TxBuffer))
		Debug_TxInterruptHandler(0, DebugState.TxUsartDmaChannel, 0);		
	
	/* Reset DMA */
	DMAC->CHID.reg = DebugState.TxUsartDmaChannel;
	DMAC->CHCTRLA.reg &= ~DMAC_CHCTRLA_ENABLE;
	DMAC->CHCTRLA.reg = DMAC_CHCTRLA_SWRST;

	/* Output debug string */	
	Debug_PanicPrintF("\n%s, at %s:%u\n", Msg, File, Line);	
	
	for (;;)
		__WFI();
}

void Debug_PanicFormatPutChar(char Char, void *Context)
{
	while (!SERCOM_UsartTxReady(DebugState.Instance));
	DebugState.Usart->DATA.reg = Char;
}
