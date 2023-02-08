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

typedef struct DCC_Packet
{
	OS_ListNode_t Node;
	uint8_t  InitialBurstCount;	// Number of times to send initially
	uint16_t ResendPeriodMs;	// Time between sends after initial burst 
	uint8_t Priority:2;
	uint8_t PreambleBits:6;
	uint8_t Size;
	uint8_t Data[6];
	OS_TaskId_t TaskId;
	OS_SignalSet_t Signal;
} DCC_Packet_t;

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
	
	DCC_STATE_PACKET_DELAY,
} DCC_State_t;


static DCC_State_t DCC_State;
static DCC_Mode_t DCC_Mode;

static uint8_t DCC_PreambleCount;
static uint8_t DCC_EndCount;
static uint16_t DCC_EndDelay;

static OS_List_t DCC_PacketList;
static DCC_Packet_t *DCC_Packet;

static uint8_t DCC_PayloadIndex;
static uint8_t DCC_PayloadByte;
static uint8_t DCC_PayloadBitCount;

static const uint8_t ResetPacket[] = { 0x00, 0x00, 0x00 };

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
	
	TCC_PATT_PGV0, //	DCC_STATE_PACKET_DELAY
};

#define DCC_SIGNAL_PKT_COMPLETE (1 << (OS_SIGNAL_USER + 1))
#define DCC_SIGNAL_IDLE			(1 << (OS_SIGNAL_USER + 2))


static void DCC_GetNextByte(void)
{	
	if (DCC_PayloadIndex < DCC_Packet->Size)
	{
		DCC_PayloadByte = DCC_Packet->Data[DCC_PayloadIndex];
		DCC_PayloadIndex += 1;
		DCC_PayloadBitCount = 8;
		DCC_State = DCC_STATE_PAYLOAD_START_1;
	}
	else
	{
		DCC_State = DCC_STATE_PAYLOAD_END_1;	
	}
}

static void DCC_GetNextBit(void)
{
	if (DCC_PayloadBitCount)
	{
		DCC_PayloadBitCount -= 1;
		if (DCC_PayloadByte & 0x80)
			DCC_State = DCC_STATE_PAYLOAD_BIT1_1;
		else
			DCC_State = DCC_STATE_PAYLOAD_BIT0_1;			
		DCC_PayloadByte = DCC_PayloadByte << 1;
	}
	else
		DCC_GetNextByte();
}

void TCC0_Handler(void)  __attribute__((__interrupt__));
void TCC0_Handler(void)
{
	uint16_t TccPattern = StatePattern[DCC_State] | TCC_PATT_PGE0 | TCC_PATT_PGE1;
	switch (DCC_State)
	{				
		case DCC_STATE_IDLE_1:
			OS_SignalSend(DCC_TASK_ID, DCC_SIGNAL_IDLE);
			DCC_State = DCC_STATE_IDLE_2;
			break;
			
		case DCC_STATE_IDLE_2:
			if (DCC_Packet)
			{
				DCC_PreambleCount = DCC_Packet->PreambleBits;
				DCC_EndCount = 1;
				DCC_PayloadIndex = 0;				
				DCC_State = DCC_STATE_PREAMBLE_1;
			}
			else
				DCC_State = DCC_STATE_IDLE_1;
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
		case DCC_STATE_PAYLOAD_START_2:
		case DCC_STATE_PAYLOAD_START_3:
			DCC_State += 1;			
			break;
		case DCC_STATE_PAYLOAD_START_4:
			DCC_GetNextBit();
			break;
							
		case DCC_STATE_PAYLOAD_BIT0_1:
		case DCC_STATE_PAYLOAD_BIT0_2:
		case DCC_STATE_PAYLOAD_BIT0_3:
			DCC_State += 1;			
			break;
		case DCC_STATE_PAYLOAD_BIT0_4:
			DCC_GetNextBit();
			break;
			
		case DCC_STATE_PAYLOAD_BIT1_1:
			DCC_State += 1;			
			break;
		case DCC_STATE_PAYLOAD_BIT1_2:
			DCC_GetNextBit();
			break;

		case DCC_STATE_PAYLOAD_END_1:
			DCC_State = DCC_STATE_PAYLOAD_END_2;
			break;
					
		case DCC_STATE_PAYLOAD_END_2:
			DCC_EndCount -= 1;
			if (DCC_EndCount == 0)
			{
				DCC_State = DCC_STATE_PACKET_DELAY;
				DCC_EndDelay = 5000 / 58;
				OS_SignalSend(DCC_TASK_ID, DCC_SIGNAL_PKT_COMPLETE);
			}
			else
				DCC_State = DCC_STATE_PAYLOAD_END_1;
			break;
			
		case DCC_STATE_PACKET_DELAY:
			DCC_EndDelay -= 1;
			if (DCC_EndDelay == 0)
				DCC_State = DCC_STATE_IDLE_1;
			break;
	}
	
	while(TCC0->SYNCBUSY.reg  & TCC_SYNCBUSY_PATT);
	TCC0->PATTBUF.reg = TccPattern;

	/* Clear OVF interrupt */
	TCC0->INTFLAG.reg = TCC_INTFLAG_OVF;
}



void DCC_Disable(void)
{
	PIO_DisablePeripheral(PIN_PA08);
	PIO_DisablePeripheral(PIN_PA09);
}

static void DCC_InitPacket(DCC_Packet_t *Packet, const uint8_t *Data, uint8_t Size, uint8_t PreambleBits, uint8_t InitialBurstCount, uint16_t ResendPeriodMs, uint8_t Priority, OS_SignalSet_t Signal)
{
	memcpy(Packet->Data, Data, Size);
	Packet->Size = Size;
	Packet->PreambleBits = PreambleBits;
	Packet->InitialBurstCount = InitialBurstCount;
	Packet->ResendPeriodMs = ResendPeriodMs;
	Packet->Priority = Priority;
	Packet->TaskId = OS_TaskId();
	Packet->Signal = Signal;
}

void DCC_SendPacket(const uint8_t *Data, uint8_t Size, uint8_t InitialBurstCount, uint16_t ResendPeriodMs, uint8_t Priority, OS_SignalSet_t Signal)
{
	DCC_Packet_t *Packet = MEM_Create(DCC_Packet_t);
	if (Packet)
	{
		DCC_InitPacket(Packet, Data, Size, 14, InitialBurstCount, ResendPeriodMs, Priority, Signal);

		if (Packet->Priority)
			OS_ListAddHead(&DCC_PacketList, &Packet->Node);
		else
			OS_ListAddTail(&DCC_PacketList, &Packet->Node);
	}
}

void DCC_SendServicePacket(const uint8_t *Data, uint8_t Size, uint8_t InitialBurstCount, uint8_t Priority, OS_SignalSet_t Signal)
{
	DCC_Packet_t *Packet = MEM_Create(DCC_Packet_t);
	if (Packet)
	{
		DCC_InitPacket(Packet, Data, Size, 50, InitialBurstCount, 0, Priority, Signal);

		if (Packet->Priority)
			OS_ListAddHead(&DCC_PacketList, &Packet->Node);
		else
			OS_ListAddTail(&DCC_PacketList, &Packet->Node);
	}
}

void DCC_UpdatePacket(const uint8_t *Data, uint8_t Size, uint8_t InitialBurstCount, uint16_t ResendPeriodMs, uint8_t Priority)
{
	/* Scan through list of repeatable packets looking for match */
	DCC_Packet_t *Packet = (DCC_Packet_t *)(DCC_PacketList.Head);
	while (Packet->Node.Succ)
	{
		if (Packet->Data[0] == Data[0])
			break;
			
		Packet = (DCC_Packet_t *)Packet->Node.Succ;
	}

	/* If no match found, check current packet or create a new packet */
	if (Packet->Node.Succ == NULL)
	{
		Packet = MEM_Create(DCC_Packet_t);
		if (Packet)
		{
			OS_InterruptDisable();
		
			/* Check if current packet matches */
			if (DCC_Packet && DCC_Packet->Data[0] == Data[0])
			{
				/* Clear retry count on current packet so it will be freed */
				DCC_Packet->InitialBurstCount = 0;
			}

			OS_InterruptEnable();			

			/* Create new packet */		
			DCC_InitPacket(Packet, Data, Size, 14, InitialBurstCount, ResendPeriodMs, Priority, 0);

			if (Packet->Priority)
				OS_ListAddHead(&DCC_PacketList, &Packet->Node);
			else
				OS_ListAddTail(&DCC_PacketList, &Packet->Node);
		}
	}
	else
	{
		/* Update payload in packet */
		DCC_InitPacket(Packet, Data, Size, 14, InitialBurstCount, ResendPeriodMs, Priority, 0);
	}				
}
	
void DCC_Start(void)
{
	static uint8_t IdlePacket[] = { 0xFF, 0x00, 0xFF };
	DCC_UpdatePacket(IdlePacket, sizeof(IdlePacket), 0xFF, 0, 0);	
}




void DCC_DirectWriteByte(uint16_t CvId, uint8_t Value)
{
	OS_SignalSet_t Signal = 1 << 15;

	/* Send 3 or more reset packets */
	DCC_SendServicePacket(ResetPacket, sizeof(ResetPacket), 3, 2, Signal);
	OS_SignalWait(Signal);
	
	/* FFS - Hidden in the documentation "The Configuration variable being addressed is
	   the provided 10 bit address plus 1.  CV #1 is defined by the address 00 00000000" */
	uint16_t Address = CvId - 1;
	
	/* Instructions packets using Direct CV Addressing are 4 byte packets of the format: 
	   Long-preamble   0  0111CCAA  0  AAAAAAAA  0  DDDDDDDD  0  EEEEEEEE  1 */
	uint8_t CvWrite[4];
	CvWrite[0] = 0x7C | (Address >> 8);
	CvWrite[1] = Address & 0xFF;
	CvWrite[2] = Value;
	CvWrite[3] = CvWrite[0] ^ CvWrite[1] ^ CvWrite[2];	
	DCC_SendServicePacket(CvWrite, sizeof(CvWrite), 5, 2, Signal);
	OS_SignalWait(Signal);
	
	DCC_SendServicePacket(ResetPacket, sizeof(ResetPacket), 6, 2, Signal);		
	OS_SignalWait(Signal);
}

void DCC_DirectVerifyByte(uint16_t CvId, uint8_t Value)
{
	OS_SignalSet_t Signal = (1 << 15);	

	/* Send 3 or more reset packets */
	DCC_SendServicePacket(ResetPacket, sizeof(ResetPacket), 3, 2, Signal);
	OS_SignalWait(Signal);

	/* TODO: Clear current sense trigger */
		
	/* FFS - Hidden in the documentation "The Configuration variable being addressed is
	   the provided 10 bit address plus 1.  CV #1 is defined by the address 00 00000000" */
	uint16_t Address = CvId - 1;

	/* Instructions packets using Direct CV Addressing are 4 byte packets of the format: 
	   Long-preamble   0  0111CCAA  0  AAAAAAAA  0  DDDDDDDD  0  EEEEEEEE  1 */
	uint8_t CvWrite[4];
	CvWrite[0] = 0x74 | (Address >> 8);
	CvWrite[1] = Address & 0xFF;
	CvWrite[2] = Value;
	CvWrite[3] = CvWrite[0] ^ CvWrite[1] ^ CvWrite[2];	
	DCC_SendServicePacket(CvWrite, sizeof(CvWrite), 5, 2, Signal);
	OS_SignalWait(Signal);
	
	/* TODO: Check if current sense triggered */
	
	DCC_SendServicePacket(ResetPacket, sizeof(ResetPacket), 6, 2, Signal);		
	OS_SignalWait(Signal);
}

void DCC_Task(void *Instance)
{	
	for (;;)
	{
		OS_SignalSet_t Sig = OS_SignalWait(DCC_SIGNAL_PKT_COMPLETE | DCC_SIGNAL_IDLE);
		
		if (Sig & DCC_SIGNAL_PKT_COMPLETE)
		{
			/* Check packet hasn't yet been freed/put back on list */
			if (DCC_Packet != NULL)
			{
				if (DCC_Packet->InitialBurstCount != 0xFF)
					DCC_Packet->InitialBurstCount -= 1;
				
				/* If packet has more retries put it back on the list, otherwise free it */
				if (DCC_Packet->InitialBurstCount)
				{
					if (DCC_Packet->Priority)
						OS_ListAddHead(&DCC_PacketList, &DCC_Packet->Node);
					else
						OS_ListAddTail(&DCC_PacketList, &DCC_Packet->Node);
				}
				else
				{
					if (DCC_Packet->Signal)
						OS_SignalSend(DCC_Packet->TaskId, DCC_Packet->Signal);
					MEM_Free(DCC_Packet);
				}
				
				/* Clear packet pointer now that packet has been handled */
				DCC_Packet = NULL;
			}
		}
		
		if (Sig & DCC_SIGNAL_IDLE)
		{
			/* Check packet isn't already waiting */
			if (DCC_Packet == NULL)
			{
				/* Pull next packet from list */
				DCC_Packet = (DCC_Packet_t *)OS_ListRemoveHead(&DCC_PacketList);
				if (DCC_Packet == NULL)
				{
					/* Send idle packet if nothing else to send */
					static uint8_t IdlePacket[] = { 0xFF, 0x00, 0xFF };
					DCC_Packet = MEM_Create(DCC_Packet_t);
					if (DCC_Packet)
						DCC_InitPacket(DCC_Packet, IdlePacket, sizeof(IdlePacket), 14, 1, 0, 0, 0);
				}
			}
		}
	}
}



void DCC_SetFunctions(uint8_t Address, uint8_t Functions)
{
	uint8_t Packet[3];

	// Function Group One Instruction (100)
	// The format of this instruction is 100DDDDD
	// Up to 5 auxiliary functions (functions FL and F1-F4) can be controlled by the Function Group One
	// instruction.  Bits 0-3 shall define the value of functions F1-F4 with function F1 being controlled
	// by bit 0 and function F4 being controlled by bit 3.  A value of "1" shall indicate that the function
	// is "on" while a value of "0" shall indicate that the function is "off".  If Bit 1 of CV#29 has a
	// value of one (1), then bit 4 controls function FL, otherwise bit 4 has no meaning. When operations
	// mode acknowledgment is enabled, receipt of a function group 1 packet must be acknowledged according
	// with an operations mode acknowledgment.
	Packet[0] = Address;
	Packet[1] = 0b10000000 | (Functions & 0b11111);
	Packet[2] = Packet[0] ^ Packet[1];
	DCC_UpdatePacket(Packet, sizeof(Packet), 3, 1000, 0);	
}

void DCC_SetLocomotiveSpeed28(uint8_t Address, int8_t Speed)
{
	uint8_t Packet[3];

	// Speed and Direction Instructions (010 and 011)
	// These two instructions have these formats:
	//     for Reverse Operation   010DDDDD
	//     for Forward Operation   011DDDDD
		
	// A speed and direction instruction is used send information to motors connected to Multi Function Digital Decoders.
	// Instruction "010" indicates a Speed and Direction Instruction for reverse operation and instruction "011" indicates
	// a Speed and Direction Instruction for forward operation.  In these instructions the data is used to control speed
	// with 245 bits 0-3 being defined exactly as in S-9.2 Section B.

	Packet[0] = Address;
	Packet[1] = 0b01000000;
	if (Speed < 0)
		Speed = -Speed;
	else
		Packet[1] |= 0b01000000;

	/* Skip E-Stop value of 0x01 */
	if (Speed > 0)
		Speed += 1;
				
	Packet[1] |= (Speed >> 1) | ((Speed & 0x01) << 4);
	Packet[2] = Packet[0] ^ Packet[1];
	
	DCC_UpdatePacket(Packet, sizeof(Packet), 3, 500, 0);	
}

void DCC_SetLocomotiveSpeed128(uint8_t Address, int8_t Speed)
{
	uint8_t Packet[4];

	// The format of this instruction is 001CCCCC  0  DDDDDDDD
	// The 5-bit sub-instruction CCCCC allows for 32 separate Advanced Operations Sub-Instructions.
		
	// CCCCC = 11111: 128 Speed Step Control - Instruction "11111" is used to send one of 126 Digital
	// Decoder speed steps.  The subsequent single byte shall define speed and direction with bit 7
	// being direction ("1" is forward and "0" is reverse) and the remaining bits used to indicate
	// speed.  The most significant speed bit is bit 6. A data-byte value of U0000000 is used for stop,
	// and a data-byte value of U0000001 is used for emergency stop. This allows up to 126 speed steps.
	// When operations mode acknowledgment is enabled, receipt of a 128 Speed Step Control packet must
	// be acknowledged with an operations mode acknowledgment.
	Packet[0] = Address;
	Packet[1] = 0b00111111;
	if (Speed < 0)
	{
		Packet[2] = 0x00;
		Speed = -Speed;
	}
	else
		Packet[2] = 0x80;

	/* Skip E-Stop value of 0x01 */
	if (Speed > 0)
		Speed += 1;

	Packet[2] |= Speed;
	Packet[3] = Packet[0] ^ Packet[1] ^ Packet[2];
		
	DCC_UpdatePacket(Packet, sizeof(Packet), 3, 500, 0);
}


void DCC_Init(DCC_Mode_t Mode)
{
	static uint32_t DCC_TaskStack[256];
	DCC_Mode = Mode;
	
	OS_ListInit(&DCC_PacketList);
	
	/* Initialize GPIO (PORT) */
	PIO_EnableOutput(PIN_PA08);
	PIO_Clear(PIN_PA08);
	PIO_SetPeripheral(PIN_PA08, PIO_PERIPHERAL_E);	// TCC0/WO[0]
	PIO_EnablePeripheral(PIN_PA08);
	PIO_EnableOutput(PIN_PA09);
	PIO_Clear(PIN_PA09);
	PIO_SetPeripheral(PIN_PA09, PIO_PERIPHERAL_E);	// TCC0/WO[1]
	PIO_EnablePeripheral(PIN_PA09);

	/* Enable TCC0 Bus clock */
	MCLK->APBCMASK.reg |= MCLK_APBCMASK_TCC0;

	/* Enable 1MHz GCLK1 for TCC0 */
	GCLK->PCHCTRL[TCC0_GCLK_ID].reg &= ~GCLK_PCHCTRL_CHEN;
	while (GCLK->PCHCTRL[TCC0_GCLK_ID].reg & GCLK_PCHCTRL_CHEN);
	GCLK->PCHCTRL[TCC0_GCLK_ID].reg = GCLK_PCHCTRL_GEN_GCLK1;
	GCLK->PCHCTRL[TCC0_GCLK_ID].reg |= GCLK_PCHCTRL_CHEN;
	while (!(GCLK->PCHCTRL[TCC0_GCLK_ID].reg & GCLK_PCHCTRL_CHEN));

	/* Initialize TCC0 */
	TCC0->CTRLA.reg &=~(TCC_CTRLA_ENABLE);
	TCC0->WAVE.reg |= TCC_WAVE_WAVEGEN_NFRQ;

	/* Enable update interrupt */
	TCC0->INTENSET.reg |= TCC_INTENSET_OVF;
	NVIC_SetPriority(TCC0_IRQn, 1);
	NVIC_EnableIRQ(TCC0_IRQn);

	/* Set TCC0 timer interrupt for every 58uS */
	TCC0->PERBUF.reg = TCC0->PER.reg = 58;

	/* Enable TCC0 */
	TCC0->CTRLA.reg |= TCC_CTRLA_ENABLE;
	
	OS_TaskInit(DCC_TASK_ID, DCC_Task, NULL, DCC_TaskStack, sizeof(DCC_TaskStack));
}
