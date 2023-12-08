/*
 * dcc_bits.cpp
 *
 * Created: 07/11/2023 16:19:57
 *  Author: jonso
 */ 

#include <sam.h>
#include "os.h"
#include "pio.h"
#include "mem.h"
#include "dcc.h"
#include "rtime.h"
#include "debug.h"
#include "ac.h"
#include "dcc_packet.h"


typedef enum
{
	DCC_STATE_IDLE_1,
	DCC_STATE_IDLE_2,

	DCC_STATE_PREAMBLE_1,
	DCC_STATE_PREAMBLE_2,
	
	DCC_STATE_PAYLOAD_START_1,
	DCC_STATE_PAYLOAD_START_2,
	DCC_STATE_PAYLOAD_START_3,
	DCC_STATE_PAYLOAD_START_4,

	DCC_STATE_PAYLOAD_BIT0_1,
	DCC_STATE_PAYLOAD_BIT0_2,
	DCC_STATE_PAYLOAD_BIT0_3,
	DCC_STATE_PAYLOAD_BIT0_4,

	DCC_STATE_PAYLOAD_BIT1_1,
	DCC_STATE_PAYLOAD_BIT1_2,
	
	DCC_STATE_PAYLOAD_END_1,
	DCC_STATE_PAYLOAD_END_2,

	DCC_STATE_GAP,

	DCC_STATE_COMPLETE_1,
	DCC_STATE_COMPLETE_2,
} DCC_State_t;


volatile static DCC_State_t DCC_State;

bool DCC_TxIsComplete(void)
{
	return DCC_State >= DCC_STATE_COMPLETE_1;
}

bool DCC_TxIsIdle(void)
{
	return DCC_State <= DCC_STATE_IDLE_2;
}

void DCC_TxIdle(void)
{
	OS_InterruptDisable();
	PanicFalse(DCC_TxIsComplete());
	DCC_State = (DCC_State == DCC_STATE_COMPLETE_1) ? DCC_STATE_IDLE_1 : DCC_STATE_IDLE_2;
	OS_InterruptEnable();
}

static uint8_t  DCC_PreambleCount;
static uint8_t  DCC_EndCount;

static uint8_t DCC_PayloadIndex;
static uint8_t DCC_PayloadByte;
static uint8_t DCC_PayloadBitsLeft;

extern DCC_Packet_t *DCC_TxPacket;
extern DCC_Packet_t *DCC_ScheduledPacket;
extern volatile uint32_t DCC_ScheduledPacketTime;
extern volatile uint32_t DCC_TimerTime;

static const uint16_t StatePattern[] =
{
	TCC_PATT_PGV0, //	DCC_STATE_IDLE_1
	TCC_PATT_PGV1, //	DCC_STATE_IDLE_2

	TCC_PATT_PGV0, //	DCC_STATE_PREAMBLE_1
	TCC_PATT_PGV1, //	DCC_STATE_PREAMBLE_2
	
	TCC_PATT_PGV0, //	DCC_STATE_PAYLOAD_START_1
	TCC_PATT_PGV0, //	DCC_STATE_PAYLOAD_START_2
	TCC_PATT_PGV1, //	DCC_STATE_PAYLOAD_START_3
	TCC_PATT_PGV1, //	DCC_STATE_PAYLOAD_START_4

	TCC_PATT_PGV0, //	DCC_STATE_PAYLOAD_BIT0_1
	TCC_PATT_PGV0, //	DCC_STATE_PAYLOAD_BIT0_2
	TCC_PATT_PGV1, //	DCC_STATE_PAYLOAD_BIT0_3
	TCC_PATT_PGV1, //	DCC_STATE_PAYLOAD_BIT0_4

	TCC_PATT_PGV0, //	DCC_STATE_PAYLOAD_BIT1_1
	TCC_PATT_PGV1, //	DCC_STATE_PAYLOAD_BIT1_2

	TCC_PATT_PGV0, //	DCC_STATE_PAYLOAD_END_1
	TCC_PATT_PGV1, //	DCC_STATE_PAYLOAD_END_2

	TCC_PATT_PGV0, //	DCC_STATE_GAP

	TCC_PATT_PGV0, //	DCC_STATE_COMPLETE_1
	TCC_PATT_PGV1, //	DCC_STATE_COMPLETE_2
};





static void DCC_GetNextByte(void)
{
	PanicNull(DCC_TxPacket);
	if (DCC_PayloadIndex < DCC_TxPacket->Size)
	{
		DCC_PayloadByte = DCC_TxPacket->Data[DCC_PayloadIndex];
		DCC_PayloadIndex += 1;
		DCC_PayloadBitsLeft = 8;
		DCC_State = DCC_STATE_PAYLOAD_START_1;
	}
	else
	{
		DCC_State = DCC_STATE_PAYLOAD_END_1;
	}
}

static void DCC_GetNextBit(void)
{
	if (DCC_PayloadBitsLeft)
	{
		DCC_PayloadBitsLeft -= 1;
		if (DCC_PayloadByte & 0x80)
			DCC_State = DCC_STATE_PAYLOAD_BIT1_1;
		else
			DCC_State = DCC_STATE_PAYLOAD_BIT0_1;
		DCC_PayloadByte = DCC_PayloadByte << 1;
	}
	else
		DCC_GetNextByte();
}

static void DCC_PacketStart(DCC_Packet_t *Packet)
{
	PanicNull(Packet);

	//if (Packet->Address != 0xFF)
	//	Debug("PacketStart, %p, state %u\n", Packet, Packet->State);
	
	PanicFalse(Packet->State == DCC_Packet_t::SCHEDULED);
	Packet->State = DCC_Packet_t::ACTIVE;
	
	DCC_TxPacket = Packet;
	DCC_TxPacket->PacketStart();

	DCC_State = DCC_STATE_PREAMBLE_1;	
	DCC_PreambleCount = DCC_TxPacket->PreambleBits;
	DCC_EndCount = 1;
	DCC_PayloadIndex = 0;	
}



static void DCC_PacketEnd(void)
{
	PanicNull(DCC_TxPacket);

	//if (DCC_TxPacket->Address != 0xFF)
	//	Debug("PacketEnd, %p, state %u\n", DCC_TxPacket, DCC_TxPacket->State);

	PanicFalse(DCC_TxPacket->State == DCC_Packet_t::ACTIVE);
	DCC_TxPacket->State = DCC_Packet_t::COMPLETE;

	DCC_TxPacket->PacketEnd();
	
	DCC_State = DCC_STATE_COMPLETE_1;
	OS_SignalSend(DCC_TASK_ID, OS_SIGNAL_USER);
}






void TCC0_Handler(void)  __attribute__((__interrupt__));
void TCC0_Handler(void)
{
	uint16_t TccPattern = StatePattern[DCC_State] | TCC_PATT_PGE0 | TCC_PATT_PGE1;
	switch (DCC_State)
	{
		case DCC_STATE_IDLE_1:
		{
			/* No packet ready yet, move to other idle state */
			DCC_State = DCC_STATE_IDLE_2;
		}
		break;

		case DCC_STATE_IDLE_2:
		{			
			if (DCC_ScheduledPacket && Time_Le(DCC_ScheduledPacketTime, DCC_TimerTime))
			{
				DCC_PacketStart(DCC_ScheduledPacket);
				break;
			}

			/* Move to other idle state */
			DCC_State = DCC_STATE_IDLE_1;
		}
		break;
		
		case DCC_STATE_PREAMBLE_1:
			DCC_State = DCC_STATE_PREAMBLE_2;
			break;
		
		case DCC_STATE_PREAMBLE_2:
			DCC_PreambleCount -= 1;
			if (DCC_PreambleCount == 0)
				DCC_GetNextByte();
			else
				DCC_State = DCC_STATE_PREAMBLE_1;
			break;
		
		case DCC_STATE_PAYLOAD_START_1:
			DCC_State = DCC_STATE_PAYLOAD_START_2;
			break;
		case DCC_STATE_PAYLOAD_START_2:
			DCC_State = DCC_STATE_PAYLOAD_START_3;
			break;
		case DCC_STATE_PAYLOAD_START_3:
			DCC_State = DCC_STATE_PAYLOAD_START_4;
			break;
		case DCC_STATE_PAYLOAD_START_4:
			DCC_GetNextBit();
			break;
		
		case DCC_STATE_PAYLOAD_BIT0_1:
			DCC_State = DCC_STATE_PAYLOAD_BIT0_2;
			break;
		case DCC_STATE_PAYLOAD_BIT0_2:
			DCC_State = DCC_STATE_PAYLOAD_BIT0_3;
			break;
		case DCC_STATE_PAYLOAD_BIT0_3:
			DCC_State = DCC_STATE_PAYLOAD_BIT0_4;
			break;
		case DCC_STATE_PAYLOAD_BIT0_4:
			DCC_GetNextBit();
			break;
		
		case DCC_STATE_PAYLOAD_BIT1_1:
			DCC_State = DCC_STATE_PAYLOAD_BIT1_2;
			break;
		case DCC_STATE_PAYLOAD_BIT1_2:
			DCC_GetNextBit();
			break;

		case DCC_STATE_PAYLOAD_END_1:
			DCC_State = DCC_STATE_PAYLOAD_END_2;
			break;
		
		case DCC_STATE_PAYLOAD_END_2:
		{
			DCC_EndCount -= 1;
			if (DCC_EndCount == 0)
			{
				/* Store packet completion time for this address */
				PanicNull(DCC_TxPacket->AddressInfo);
				DCC_TxPacket->AddressInfo->HoldOffTime = Time_Add(DCC_TimerTime, 5000);

				DCC_EndCount = DCC_TxPacket->DataEnd();
				if (DCC_EndCount)
					DCC_State = DCC_STATE_GAP;
				else
					DCC_PacketEnd();
			}
			else
			{
				DCC_State = DCC_STATE_PAYLOAD_END_1;
			}
		}
		break;
		
		case DCC_STATE_GAP:
		{
			DCC_EndCount -= 1;
			if (DCC_EndCount == 0)
				DCC_PacketEnd();
		}
		break;
		
		case DCC_STATE_COMPLETE_1:
		{
			DCC_State = DCC_STATE_COMPLETE_2;
		}
		break;

		case DCC_STATE_COMPLETE_2:
		{
			DCC_State = DCC_STATE_COMPLETE_1;
		}
		break;
		
	}
	
	while (TCC0->SYNCBUSY.reg & TCC_SYNCBUSY_PATT) {}
	TCC0->PATTBUF.reg = TccPattern;

	/* Clear OVF interrupt */
	TCC0->INTFLAG.reg = TCC_INTFLAG_OVF;

	/* Advance clock */
	DCC_TimerTime += 58;	
}






