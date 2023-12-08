/*
 * dcc.c
 *
 * Created: 18/03/2021 21:27:59
 *  Author: jonso
 */ 
#include <sam.h>
#include <memory.h>

#include "os.h"
#include "pio.h"
#include "mem.h"
#include "dcc.h"
#include "clk.h"
#include "rtime.h"
#include "debug.h"
#include "ac.h"
#include "dcc_packet.h"

volatile Time_t DCC_TimerTime;

static uint8_t	DCC_Mode;


DCC_Packet_t *DCC_PacketSendList;

DCC_Packet_t *volatile DCC_TxPacket;
DCC_Packet_t *volatile DCC_ScheduledPacket;
Time_t DCC_ScheduledPacketTime;


DCC_AddressInfo_t *DCC_AddressInfoList;

static const uint8_t ResetPacket[] = { 0x00, 0x00, 0x00 };




DCC_AddressInfo_t *DCC_AllocAddressInfo(uint8_t Address)
{
	/* Find existing Address structure */
	DCC_AddressInfo_t *AddressInfo = DCC_AddressInfoList;
	while (AddressInfo)
	{
		if (AddressInfo->Address == Address)
			return AddressInfo;
		AddressInfo = AddressInfo->Next;
	}
	
	/* No matching structure found, allocate one */
	AddressInfo = MEM_Create(DCC_AddressInfo_t);
	PanicNull(AddressInfo);
	AddressInfo->Next = DCC_AddressInfoList;
	AddressInfo->Address = Address;
	AddressInfo->HoldOffTime = DCC_TimerTime;
	AddressInfo->List = NULL;
	DCC_AddressInfoList = AddressInfo;
	//Debug("Alloc AI %p for %u", AddressInfo, Address);
	return AddressInfo;
}


/* Get next packet to be transmitted */
DCC_Packet_t *DCC_NextPacket(Time_t &TxTime)
{
	DCC_Packet_t *TxPacket = NULL;
		
	DCC_AddressInfo_t *AddressInfo = DCC_AddressInfoList;
	//Debug("NP time %u\n", TxTime);
	while (AddressInfo)
	{
		//Debug("  %u, time %u, hold off %u\n", AddressInfo->Address, AddressInfo->List ? AddressInfo->List->Time : 0, AddressInfo->List ? AddressInfo->HoldOffTime : 0);
		
		/* If we have already found a packet, see if this packet can be Tx'ed earlier */
		if (TxPacket)
		{
			if (Time_Lt(AddressInfo->HoldOffTime, TxTime))
			{
				if (AddressInfo->List && Time_Lt(AddressInfo->List->Time, TxTime))
				{				
					TxPacket = AddressInfo->List;
					TxTime =  Time_Lt(AddressInfo->HoldOffTime, AddressInfo->List->Time) ? AddressInfo->List->Time : AddressInfo->HoldOffTime;
				}
			}
		}
		else
		{
			if (AddressInfo->List)
			{
				TxPacket = AddressInfo->List;
				TxTime = Time_Lt(AddressInfo->HoldOffTime, AddressInfo->List->Time) ? AddressInfo->List->Time : AddressInfo->HoldOffTime;
			}						
		}
		
		AddressInfo = AddressInfo->Next;
	}
	
	//Debug("  time %u, addr %u\n", TxTime, TxPacket ? TxPacket->Address : 0);

	return TxPacket;
}


static void DCC_InsertPacket(DCC_Packet_t *Packet)
{
	DCC_AddressInfo_t *AddressInfo = DCC_AllocAddressInfo(Packet->Address);
	Packet->AddressInfo = AddressInfo;
	
	/* Remove any packets that are the same */
	DCC_Packet_t **ListPacketRef, *ListPacket;
	for (ListPacketRef = &AddressInfo->List; (ListPacket = *ListPacketRef) != NULL;)
	{
		if (Packet->IsSame(ListPacket))
		{
			//Debug("Cancel %p\n", ListPacket);
			*ListPacketRef = ListPacket->Next;
			delete ListPacket;
		}
		else
			ListPacketRef = &ListPacket->Next;
	}
	
	/* Insert packet into time ordered list */
	for (ListPacketRef = &AddressInfo->List; (ListPacket = *ListPacketRef) != NULL; ListPacketRef = &ListPacket->Next)
	{
		/* Exit loop if packet is after new packet */
		if (Time_Gt(ListPacket->Time, Packet->Time))
			break;
	}
	
	/* Insert packet into the list */
	Packet->Next = *ListPacketRef;
	*ListPacketRef = Packet;
}


static bool DCC_SchedulePacket(DCC_Packet_t *Packet)
{
	PanicNull(Packet);
	PanicFalse(Packet->State == DCC_Packet_t::CREATED);
	
	/* Ask packet when it want to be sent */
	Time_t Time = DCC_TimerTime;
	if (Packet->Schedule(Time))
	{
		/* Packet want to be sent, store time and update state */
		Packet->Time = Time;
		Packet->State = DCC_Packet_t::SCHEDULED;		
		
		/* Insert packet into queue in chronological order */
		DCC_InsertPacket(Packet);
		return true;
	}
	else
	{
		/* Packet no longer wants to be sent */
		//if (Packet->Address != 0xFF)
		//	Debug("Free %p\n", Packet);	
		
		/* Inform client that packet has finished */
		if (Packet->Signal)
			OS_SignalSend(Packet->TaskId, Packet->Signal);
					
		/* Free packet memory */
		delete(Packet);
		return false;
	}	
}


static bool DCC_ReSchedulePacket(DCC_Packet_t *Packet)
{
	PanicFalse(Packet->State == DCC_Packet_t::COMPLETE);
	Packet->State = DCC_Packet_t::CREATED;
	
	DCC_AddressInfo_t *AddressInfo = Packet->AddressInfo;
	PanicNull(AddressInfo);
	
	/* Find packet in the list and remove it */
	DCC_Packet_t **ListPacketRef, *ListPacket;
	for (ListPacketRef = &AddressInfo->List; (ListPacket = *ListPacketRef) != NULL; ListPacketRef = &ListPacket->Next)
	{
		if (ListPacket == Packet)
		{
			*ListPacketRef = ListPacket->Next;
			break;
		}
	}
	
	/* We must have found the packet, if not something has gone seriously wrong */
	PanicNull(ListPacket);
	//if (ListPacket->Address != 0xFF)
	//	Debug("Resched %p\n", ListPacket);
	
	/* Re-schedule the packet */
	return DCC_SchedulePacket(ListPacket);
}









void DCC_Task(void *Instance)
{	
	for (;;)
	{
		/* Check if active packet is now complete */
		if (DCC_TxIsComplete())
		{
			/* Attempt to schedule the packet again */
			DCC_ReSchedulePacket(DCC_TxPacket);
			
			/* Clear scheduled packet to prevent DCC Tx state machine from re-starting immediately */
			DCC_ScheduledPacket = NULL;
			
			/* Move DCC Tx state machine back to idle */
			DCC_TxIdle();
		}
		
		/* If no packet active, get next packet to be scheduled */
		OS_InterruptDisable();
		if (DCC_TxIsIdle())
		{
			DCC_ScheduledPacket = NULL;
			OS_InterruptEnable();
			
			/* Move packets from send list to scheduled list */
			while (DCC_PacketSendList)
			{
				/* Remove from send list */
				DCC_Packet_t *Packet = DCC_PacketSendList;
				DCC_PacketSendList = Packet->Next;
			
				/* Schedule packet */
				DCC_SchedulePacket(Packet);			
			}

			/* Update schedule packet */
			DCC_ScheduledPacket = DCC_NextPacket(DCC_ScheduledPacketTime);

			/* In Normal mode send idle packets */
			if (DCC_Mode == DCC_MODE_NORMAL)
			{
				/* If no packet scheduled or packet is more than 20ms away, send an idle packet */
				if ((DCC_ScheduledPacket == NULL) ||
				    (Time_Gt(DCC_ScheduledPacketTime, Time_Add(DCC_TimerTime, 20000))))
				{	
					DCC_Packet_t *Packet = new DCC_IdlePacket_t();
					DCC_SchedulePacket(Packet);
					
					/* Update scheduled packet */
					DCC_ScheduledPacket = DCC_NextPacket(DCC_ScheduledPacketTime);
				}
			}
		}			
		else
			OS_InterruptEnable();
			
		OS_SignalWait(OS_SIGNAL_USER);		
	}
}


void DCC_Disable(void)
{
	PIO_DisablePeripheral(PIN_PA08);
	PIO_DisablePeripheral(PIN_PA09);
}

void DCC_Enable(void)
{
	PIO_EnablePeripheral(PIN_PA08);
	PIO_EnablePeripheral(PIN_PA09);
}


void DCC_Init(DCC_Mode_t Mode)
{
	static uint32_t DCC_TaskStack[512];
	
	DCC_Mode = Mode;
		
	/* Initialize GPIO (PORT) */
	PIO_EnableOutput(PIN_PA08);
	PIO_Clear(PIN_PA08);
	PIO_SetPeripheral(PIN_PA08, PIO_PERIPHERAL_E);	// TCC0/WO[0]
	PIO_EnableOutput(PIN_PA09);
	PIO_Clear(PIN_PA09);
	PIO_SetPeripheral(PIN_PA09, PIO_PERIPHERAL_E);	// TCC0/WO[1]
	DCC_Enable();

	/* Enable TCC0 Bus clock */
	MCLK->APBCMASK.reg |= MCLK_APBCMASK_TCC0;

	/* Enable 1MHz GCLK1 for TCC0 */
	CLK_EnablePeripheral(1, TCC0_GCLK_ID);

	/* Initialize TCC0 */
	TCC0->CTRLA.reg &=~(TCC_CTRLA_ENABLE);
	TCC0->WAVE.reg |= TCC_WAVE_WAVEGEN_NFRQ;

	/* Enable update interrupt */
	TCC0->INTENSET.reg |= TCC_INTENSET_OVF;
	NVIC_SetPriority(TCC0_IRQn, 0);
	NVIC_EnableIRQ(TCC0_IRQn);

	/* Set TCC0 timer interrupt for every 58uS */
	TCC0->PERBUF.reg = TCC0->PER.reg = 58;

	/* Enable TCC0 */
	TCC0->CTRLA.reg |= TCC_CTRLA_ENABLE;
	
	OS_TaskInit(DCC_TASK_ID, DCC_Task, NULL, DCC_TaskStack, sizeof(DCC_TaskStack));
}


void DCC_SendPacket(DCC_Packet_t *Packet)
{
	PanicNull(Packet);
	PanicFalse(Packet->State == DCC_Packet_t::CREATED);

	/* Add to send list */
	Packet->Next = DCC_PacketSendList;
	DCC_PacketSendList = Packet;
	OS_SignalSend(DCC_TASK_ID, OS_SIGNAL_USER);
}


void DCC_SetLocomotiveSpeed(uint8_t Loco, uint8_t Speed, uint8_t Forward)
{
	/* Skip E-Stop value of 0x01 */
	if (Speed > 0)
	Speed += 1;

	DCC_SendPacket(new DCC_SpeedPacket_t(Loco, Speed, Forward));
}


void DCC_StopLocomotive(uint8_t Loco, uint8_t Forward)
{
	DCC_SendPacket(new DCC_SpeedPacket_t(Loco, 0x01, Forward));
}


void DCC_SetLocomotiveFunctions(uint8_t Loco, uint8_t Functions, uint8_t Group)
{
	if (Group < 13)
	DCC_SendPacket(new DCC_FunctionPacket_t(Loco, Functions, Group));
	else
	DCC_SendPacket(new DCC_FunctionExpansionPacket_t(Loco, Functions, Group));
}


bool DCC_CvWrite(uint16_t CvId, uint8_t Value)
{
	OS_SignalSet_t Signal = 1 << 15;
	bool Acked;
	
	DCC_Mode = DCC_MODE_SERVICE;
	
	/* Send 3 or more reset packets */
	DCC_SendPacket(new DCC_ServicePacket_t(ResetPacket, sizeof(ResetPacket), Signal));
	OS_SignalWait(Signal);
	
	DCC_SendPacket(new DCC_CvWritePacket_t(CvId, Value, &Acked, Signal));
	OS_SignalWait(Signal);
	
	//DCC_SendPacket(new DCC_ServicePacket_t(ResetPacket, sizeof(ResetPacket), Signal));
	//OS_SignalWait(Signal);
	
	DCC_Mode = DCC_MODE_NORMAL;
	return Acked;
}


uint8_t DCC_CvRead(uint16_t CvId)
{
	OS_SignalSet_t Signal = (1 << 15);
	uint8_t CvValue;
	DCC_Mode = DCC_MODE_SERVICE;

	/* Send 3 or more reset packets */
	DCC_SendPacket(new DCC_ServicePacket_t(ResetPacket, sizeof(ResetPacket), Signal));
	OS_SignalWait(Signal);

	DCC_SendPacket(new DCC_CvReadPacket_t(CvId, &CvValue, Signal));
	OS_SignalWait(Signal);
	
	//DCC_SendPacket(new DCC_ServicePacket_t(ResetPacket, sizeof(ResetPacket), Signal));
	//OS_SignalWait(Signal);

	DCC_Mode = DCC_MODE_NORMAL;
	return CvValue;
}

