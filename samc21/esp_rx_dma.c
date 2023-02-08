#include "esp.h"
#include "clk.h"
#include "dmac.h"
#include "pio.h"
#include "evsys.h"
#include "os.h"
#include "os_task_id.h"


void ESP_HwRxDmaSync(ESP_t *Esp)
{
	DMAC_Descriptor_t *Desc = DMAC_ChannelGetBaseDescriptor(Esp->Hw.RxUsartDmaChannel);
	uint16_t ByteCount = DMAC_ChannelGetByteCount(Esp->Hw.RxUsartDmaChannel);
	uint8_t *RxAddress = (uint8_t *)Desc->DSTADDR.reg - ByteCount;
	BufferSetIndexFromAddress(Esp->Hw.RxUsartBuffer, RxAddress);
}


void TC2_Handler(void) __attribute__ ((__interrupt__));
void TC2_Handler(void)
{
	uint32_t Status = TC2->COUNT16.INTFLAG.reg;
	//if (Status & TC_INTFLAG_OVF)
	//	OS_SignalSend(ESP_TASK_ID, ESP_SIGNAL_RX_IDLE);
	TC2->COUNT16.INTFLAG.reg = Status;
}


void ESP_HwRxDmaStart(ESP_t *Esp, uint8_t Timer)
{
	/* Select channel and reset it */
	DMAC->CHID.reg = Esp->Hw.RxUsartDmaChannel;
	DMAC->CHCTRLA.reg &= ~DMAC_CHCTRLA_ENABLE;
	DMAC->CHCTRLA.reg = DMAC_CHCTRLA_SWRST;

	/* Initialise receive descriptor */
	DMAC_Descriptor_t *DmaDesc = DMAC_ChannelGetBaseDescriptor(Esp->Hw.RxUsartDmaChannel);
	DmaDesc->BTCTRL.reg = DMAC_BTCTRL_BEATSIZE_BYTE | DMAC_BTCTRL_DSTINC | DMAC_BTCTRL_BLOCKACT_NOACT | DMAC_BTCTRL_VALID | DMAC_BTCTRL_EVOSEL_BEAT; 
	DmaDesc->BTCNT.reg = BufferSize(Esp->Hw.RxUsartBuffer);
	DmaDesc->SRCADDR.reg = (uint32_t)&Esp->Hw.Usart->DATA;
	DmaDesc->DSTADDR.reg = (uint32_t)Esp->Hw.RxUsartBuffer.Buffer + DmaDesc->BTCNT.reg;
	DmaDesc->DESCADDR.reg = (uint32_t)DmaDesc;

	/* Configure and start transfer */
	DMAC->CHCTRLB.reg = DMAC_CHCTRLB_TRIGACT_BEAT | DMAC_CHCTRLB_LVL(3) | DMAC_CHCTRLB_TRIGSRC(Esp->Hw.RxUsartDmaTrigger) | DMAC_CHCTRLB_EVOE;
	DMAC->CHINTENCLR.reg = DMAC_CHINTENCLR_MASK;
	DMAC->CHCTRLA.reg = DMAC_CHCTRLA_ENABLE;
	
	Tc *Tcx = NULL;
	switch (Timer)
	{
		case 2:
		{
			/* Enable TC2 Bus clock */
			MCLK->APBCMASK.reg |= MCLK_APBCMASK_TC2;
			Tcx = TC2;
		}
		break;
		
		case 3:
		{
			/* Enable TC3 Bus clock */
			MCLK->APBCMASK.reg |= MCLK_APBCMASK_TC3;
			Tcx = TC3;
		}
		break;
		
		default:
			Panic();		
	}

	/* Enable 1MHz GCLK1 for TC2/TC3 */
	CLK_EnablePeripheral(1, TC2_GCLK_ID);

	/* Configure TC2 to count down and generate interrupt when timer underflows.
	   Timer is re-triggered USART Rx DMAC beat event so that interrupt is only generated when USART
	   has been idle for 200uS */ 
	Tcx->COUNT8.CTRLA.reg = TC_CTRLA_MODE_COUNT8 | TC_CTRLA_PRESCALER_DIV1; 
	Tcx->COUNT8.CTRLBSET.reg = TC_CTRLBSET_ONESHOT | TC_CTRLBCLR_DIR;
	Tcx->COUNT8.WAVE.reg = TC_WAVE_WAVEGEN_NFRQ;
	Tcx->COUNT8.PER.reg = Tcx->COUNT8.COUNT.reg = 200;
	Tcx->COUNT8.EVCTRL.reg = TC_EVCTRL_TCEI | TC_EVCTRL_EVACT_RETRIGGER;

	/* Enable update interrupt */
	Tcx->COUNT8.INTENSET.reg = TC_INTENSET_OVF;
	
	NVIC_SetPriority(TC0_IRQn + Timer, 1);
	NVIC_EnableIRQ(TC0_IRQn + Timer);

	/* Enable TC2 */
	Tcx->COUNT8.CTRLA.reg |= TC_CTRLA_ENABLE;

	/* Re-trigger timer on DMAC channel event */
	EVSYS_AsyncChannel(3, EVSYS_ID_GEN_DMAC_CH_0 + Esp->Hw.RxUsartDmaChannel, EVSYS_ID_USER_TC0_EVU + Timer);
}

