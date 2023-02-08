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

volatile uint32_t DCC_TimerTime;

#define DCC_MS(x)	((x) * 1000)

#define DCC_SCHEDULED_END (-1)
#define DCC_PACKET_END_DELAY (5000)

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
} DCC_State_t;


volatile static DCC_State_t DCC_State;

static uint8_t  DCC_PreambleCount;
static uint8_t  DCC_EndCount;
static uint8_t	DCC_Mode;

static uint8_t DCC_PayloadIndex;
static uint8_t DCC_PayloadByte;
static uint8_t DCC_PayloadBitCount;

static const uint8_t ResetPacket[] = { 0x00, 0x00, 0x00 };

static const uint16_t StatePattern[] = 
{
	TCC_PATT_PGV0, //	DCC_STATE_IDLE_1
	TCC_PATT_PGV0, //	DCC_STATE_IDLE_2

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
};

#define DCC_SIGNAL_PKT_COMPLETE (1 << (OS_SIGNAL_USER + 1))
#define DCC_SIGNAL_PKT_READY	(1 << (OS_SIGNAL_USER + 2))


class DCC_Packet
{
public:
	DCC_Packet();
	virtual ~DCC_Packet();
	
	virtual int32_t Schedule(uint32_t Time) { return 0; }
	virtual bool IsSame(const DCC_Packet *Packet) { return false; }

	static DCC_Packet *List;
	static uint8_t PreviousAddress;
	static uint32_t PreviousEndTime;
	
	uint32_t Completed;
	DCC_Packet *Next;
	int32_t Delta;
	uint8_t Size;
	uint8_t Data[6];
	OS_TaskId_t TaskId;
	OS_SignalSet_t Signal;
	uint8_t PreambleBits;
	enum {CREATED, SCHEDULED, ACTIVE, COMPLETE} State;
	bool DisableReschedule;
	
	void Init(const uint8_t *Data, uint8_t Size, uint8_t PreambleBits, OS_SignalSet_t Signal);
};


DCC_Packet::DCC_Packet()
{
	Next = NULL;
	State = CREATED;
	DisableReschedule = false;
}

DCC_Packet::~DCC_Packet()
{
}

void DCC_Packet::Init(const uint8_t *Data, uint8_t Size, uint8_t PreambleBits, OS_SignalSet_t Signal)
{
	memcpy(this->Data, Data, Size);
	this->Size = Size;
	this->PreambleBits = PreambleBits;
	this->TaskId = OS_TaskId();
	this->Signal = Signal;
}


class DCC_SpeedPacket : public DCC_Packet
{
public:
	DCC_SpeedPacket(uint8_t Address, uint8_t Speed, uint8_t Forward);
	virtual int32_t Schedule(uint32_t Time);
	virtual bool IsSame(const DCC_Packet *Packet);
protected:
	uint8_t TxCount;
	uint32_t Scheduled;
	bool Repeat;
};

DCC_SpeedPacket::DCC_SpeedPacket(uint8_t Address, uint8_t Speed, uint8_t Forward)
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
	if (Forward)
		Packet[2] = 0x80;
	else
		Packet[2] = 0x00;
	
	/* Skip E-Stop value of 0x01 */
	if (Speed >= 0)
		Speed += 1;

	Packet[2] |= Speed;
	Packet[3] = Packet[0] ^ Packet[1] ^ Packet[2];
		
	Repeat = (Speed != 0);
	TxCount = 0;
	
	Init(Packet, sizeof(Packet), 14, 0);
}

int32_t DCC_SpeedPacket::Schedule(uint32_t Time)
{
	TxCount += 1;
	if (TxCount < 4)
	{
		Scheduled = Time;
		return 0;
	}
	else
	{
		if (Repeat)
		{
			Scheduled = Time_Add(Scheduled, DCC_MS(500));
			int32_t Delta = Time_Sub(Scheduled, Time);
			return (Delta >= 0) ? Delta : 0;
		}
		else
			return DCC_SCHEDULED_END;
	}
}

bool DCC_SpeedPacket::IsSame(const DCC_Packet *Packet)
{	
	return (Packet->Data[0] == Data[0]) &&
		   (Packet->Data[1] == 0b00111111);
}


class DCC_FunctionPacket : public DCC_Packet
{
public:
	DCC_FunctionPacket(uint8_t Address, uint8_t Functions, uint8_t Group = 0);
	virtual int32_t Schedule(uint32_t Time);
	virtual bool IsSame(const DCC_Packet *Packet);
protected:
	uint8_t TxCount;
	uint32_t Scheduled;
	uint8_t Mask;
};

DCC_FunctionPacket::DCC_FunctionPacket(uint8_t Address, uint8_t Functions, uint8_t Group)
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
	switch (Group)
	{
		case 0:
			Packet[1] = 0b10000000 | (Functions & 0b11111);
			Mask = 0b11100000;
			break;
			
		case 1:
			Packet[1] = 0b10110000 | (Functions & 0b01111);
			Mask = 0b11110000;
			break;
			
		case 2:
			Packet[1] = 0b10100000 | (Functions & 0b01111);
			Mask = 0b11110000;
			break;		
	}
	Packet[2] = Packet[0] ^ Packet[1];

	TxCount = 0;
	Init(Packet, sizeof(Packet), 14, 0);
}



int32_t DCC_FunctionPacket::Schedule(uint32_t Time)
{
	TxCount += 1;
	if (TxCount < 4)
	{
		Scheduled = Time;
		return 0;
	}
	else
	{
		Scheduled = Time_Add(Scheduled, DCC_MS(1000));
		int32_t Delta = Time_Sub(Scheduled, Time);
		return (Delta >= 0) ? Delta : 0;
	}
}

bool DCC_FunctionPacket::IsSame(const DCC_Packet *Packet)
{
	return (Packet->Data[0] == Data[0]) && ((Packet->Data[1] & Mask) == (Data[1] & Mask));
}


class DCC_IdlePacket : public DCC_Packet
{
public:
	DCC_IdlePacket(void);
	virtual int32_t Schedule(uint32_t Time);
protected:
	bool Scheduled;
};

DCC_IdlePacket::DCC_IdlePacket(void)
{
	static const uint8_t Packet[] = { 0xFF, 0x00, 0xFF };
	Init(Packet, sizeof(Packet), 14, 0);
	Scheduled = false;
}

int32_t DCC_IdlePacket::Schedule(uint32_t Time)
{
	if (!Scheduled)
	{
		Scheduled = true;
		return 0;
	}
	else
		return DCC_SCHEDULED_END;
}



class DCC_ServicePacket : public DCC_Packet
{
public:
	DCC_ServicePacket(const uint8_t *Data, uint8_t DataSize, OS_SignalSet_t Signal);
	virtual int32_t Schedule(uint32_t Time);
protected:
	uint8_t TxCount;
};

DCC_ServicePacket::DCC_ServicePacket(const uint8_t *Data, uint8_t DataSize, OS_SignalSet_t Signal)
{
	TxCount = 0;
	Init(Data, DataSize, 50, Signal);
}

int32_t DCC_ServicePacket::Schedule(uint32_t Time)
{
	TxCount += 1;
	if (TxCount <= 10)
	{
		return 0;
	}
	else
		return DCC_SCHEDULED_END;
}

DCC_Packet *DCC_Packet::List;
uint8_t DCC_Packet::PreviousAddress;
uint32_t DCC_Packet::PreviousEndTime;


static void DCC_InsertPacket(DCC_Packet *NewPacket, int32_t Delta)
{	
	PanicFalse(NewPacket->State == DCC_Packet::SCHEDULED);
	PanicFalse(Delta >= 0);
	
	/* Insert packet into time ordered list */
	DCC_Packet **PacketRef, *Packet;
	for (PacketRef = &DCC_Packet::List; (Packet = *PacketRef) != NULL; PacketRef = &Packet->Next)
	{
		/* Exit loop if packet is after new packet */
		if (Packet->Delta > Delta)
			break;
				
		/* Adjust time for current packet */
		Delta -= Packet->Delta;
	}	
		
	/* Store time delta from previous packet */
	NewPacket->Delta = Delta;

	/* Insert packet into the list */
	OS_InterruptDisable();
	NewPacket->Next = *PacketRef;
	*PacketRef = NewPacket;
	OS_InterruptEnable();		
}
	
static bool DCC_SchedulePacket(DCC_Packet *NewPacket)
{	
	PanicNull(NewPacket);	
	PanicFalse(NewPacket->State == DCC_Packet::CREATED);
	
	if (NewPacket->DisableReschedule)
		return false;
	
	/* Get time until packet is scheduled */
	int32_t Delta = NewPacket->Schedule(DCC_TimerTime);
	if (Delta != DCC_SCHEDULED_END)
	{
		NewPacket->State = DCC_Packet::SCHEDULED;
		DCC_InsertPacket(NewPacket, Delta);
		return true;
	}
	else
		return false;
}



static void DCC_GetNextByte(void)
{	
	PanicNull(DCC_Packet::List);
	if (DCC_PayloadIndex < DCC_Packet::List->Size)
	{
		DCC_PayloadByte = DCC_Packet::List->Data[DCC_PayloadIndex];
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
		{
			/* Check if packet at head of list is active */
			if (DCC_Packet::List && DCC_Packet::List->State == DCC_Packet::ACTIVE)
			{
				DCC_PreambleCount = DCC_Packet::List->PreambleBits;
				DCC_EndCount = 1;
				DCC_PayloadIndex = 0;				
				DCC_State = DCC_STATE_PREAMBLE_1;
			}
			else
			{
				/* No packet ready yet, move to other idle state */
				DCC_State = DCC_STATE_IDLE_2;			
			}
		}
		break;

		case DCC_STATE_IDLE_2:
		{
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
			DCC_EndCount -= 1;
			if (DCC_EndCount == 0)
			{
				PanicNull(DCC_Packet::List);
				PanicFalse(DCC_Packet::List->State == DCC_Packet::ACTIVE);
				DCC_Packet::List->State = DCC_Packet::COMPLETE;				
					
				DCC_State = DCC_STATE_IDLE_1;
				OS_SignalSend(DCC_TASK_ID, DCC_SIGNAL_PKT_COMPLETE);
			}
			else
				DCC_State = DCC_STATE_PAYLOAD_END_1;
			break;			
	}
	
	while (TCC0->SYNCBUSY.reg  & TCC_SYNCBUSY_PATT);
		TCC0->PATTBUF.reg = TccPattern;

	/* Clear OVF interrupt */
	TCC0->INTFLAG.reg = TCC_INTFLAG_OVF;

	/* Advance clock */
	DCC_TimerTime += 58;
	
	/* Walk packet list decrementing delta times, we need to account for 58uS */
	int32_t Delta = 58;
	DCC_Packet *Packet = DCC_Packet::List;			
	while (Packet && Delta)
	{
		if (Packet->Delta <= Delta)
		{
			Delta -= Packet->Delta;
			Packet->Delta = 0;	
			Packet = Packet->Next;
			OS_SignalSend(DCC_TASK_ID, DCC_SIGNAL_PKT_READY);
		}
		else
		{
			Packet->Delta -= Delta;
			Delta = 0;
		}
	}
}



static void DCC_SendPacket(DCC_Packet *NewPacket)
{
	PanicNull(NewPacket);
	PanicFalse(NewPacket->State == DCC_Packet::CREATED);
	PanicFalse(NewPacket->Next == NULL);
	
	/* Destroy any 'same' packets in list */
	DCC_Packet **PacketRef, *Packet;
	for (PacketRef = &DCC_Packet::List; (Packet = *PacketRef) != NULL; PacketRef = &Packet->Next)
	{
		/* Check if packets are the same */
		if (NewPacket->IsSame(Packet))
		{
			/* If packet is active (or complete) do not reschedule once it's been transmitted */
			if (Packet->State >= DCC_Packet::ACTIVE)
			{
				/* Set flag to prevent this packet being rescheduled */
				Packet->DisableReschedule = true;
			}
			else
			{
				/* Unlink packet and destroy it */
				OS_InterruptDisable();
				if (Packet->Next)
					Packet->Next->Delta += Packet->Delta;
				*PacketRef = Packet->Next;
				delete Packet;
				OS_InterruptEnable();
				
				/* Exit loop as there should only be one packet the same */
				break;
			}
		}
	}

	/* Attempt to schedule new packet, destroy it if scheduling failed */
	if (!DCC_SchedulePacket(NewPacket))
		delete NewPacket;
}

	

void DCC_SetLocomotiveSpeed(uint8_t Loco, uint8_t Speed, uint8_t Forward)
{
	DCC_SendPacket(new DCC_SpeedPacket(Loco, Speed, Forward));
}


void DCC_SetLocomotiveFunctions(uint8_t Loco, uint8_t Functions, uint8_t Group)
{
	DCC_SendPacket(new DCC_FunctionPacket(Loco, Functions, Group));	
}


void DCC_DirectWriteByte(uint16_t CvId, uint8_t Value)
{
	OS_SignalSet_t Signal = 1 << 15;

	DCC_Mode = DCC_MODE_SERVICE;
	
	/* Send 3 or more reset packets */
	DCC_SendPacket(new DCC_ServicePacket(ResetPacket, sizeof(ResetPacket), Signal));
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
	DCC_SendPacket(new DCC_ServicePacket(CvWrite, sizeof(CvWrite), Signal));
	OS_SignalWait(Signal);
	
	DCC_SendPacket(new DCC_ServicePacket(ResetPacket, sizeof(ResetPacket), Signal));
	OS_SignalWait(Signal);
	
	DCC_Mode = DCC_MODE_NORMAL;
}


void DCC_DirectVerifyByte(uint16_t CvId, uint8_t Value)
{
	OS_SignalSet_t Signal = (1 << 15);	

	DCC_Mode = DCC_MODE_SERVICE;

	/* Send 3 or more reset packets */
	DCC_SendPacket(new DCC_ServicePacket(ResetPacket, sizeof(ResetPacket), Signal));
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
	DCC_SendPacket(new DCC_ServicePacket(CvWrite, sizeof(CvWrite), Signal));
	OS_SignalWait(Signal);
	
	/* TODO: Check if current sense triggered */
	
	DCC_SendPacket(new DCC_ServicePacket(ResetPacket, sizeof(ResetPacket), Signal));
	OS_SignalWait(Signal);

	DCC_Mode = DCC_MODE_NORMAL;
}




void DCC_Task(void *Instance)
{	
	DCC_SendPacket(new DCC_IdlePacket());
	
	for (;;)
	{
		 OS_SignalWait(DCC_SIGNAL_PKT_COMPLETE | DCC_SIGNAL_PKT_READY);
		
		DCC_Packet *Packet = DCC_Packet::List;
		if (Packet && Packet->State == DCC_Packet::COMPLETE)
		{
			/* Record current packet end time and address if it's not an idle packet */				
			if (Packet->Data[0] != 0xFF)
			{
				DCC_Packet::PreviousAddress = Packet->Data[0];
				DCC_Packet::PreviousEndTime = DCC_TimerTime;
			}
				
			/* Unlink packet from list */
			OS_InterruptDisable();
			DCC_Packet::List = Packet->Next;
			OS_InterruptEnable();
			
			/* Attempt to schedule package again, destroy it if it can't be re-scheduled */
			Packet->State = DCC_Packet::CREATED;
			if (!DCC_SchedulePacket(Packet))
			{
				if (Packet->Signal)
					OS_SignalSend(Packet->TaskId, Packet->Signal);		
				delete(Packet);
			}
		}
		
		Packet = DCC_Packet::List;
		if (Packet && Packet->State == DCC_Packet::SCHEDULED)
		{
			if (Packet->Delta <= 0)
			{
				/* Check if address same as previous packet */
				int32_t Delta = Time_Sub(DCC_TimerTime, DCC_Packet::PreviousEndTime);
				if (Packet->Data[0] == DCC_Packet::PreviousAddress && (Delta < 5000))
				{
					/* Remove from list and re-insert 5ms later */	
					OS_InterruptDisable();	
					DCC_Packet::List = Packet->Next;
					OS_InterruptEnable();
					DCC_InsertPacket(Packet, 5000 - Delta);							
				}
				else
				{
					/* Packet is now active */
					Packet->State = DCC_Packet::ACTIVE;
				}					
			}
		}

		if (DCC_Mode == DCC_MODE_NORMAL)
		{
			if ((DCC_Packet::List == NULL) ||
			    (DCC_Packet::List->Delta > DCC_MS(10)))
			{
				DCC_SendPacket(new DCC_IdlePacket());
			}
		}
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
	static uint32_t DCC_TaskStack[256];
	
	DCC_Mode = Mode;
	DCC_Packet::List = 0;
		
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
	NVIC_SetPriority(TCC0_IRQn, 1);
	NVIC_EnableIRQ(TCC0_IRQn);

	/* Set TCC0 timer interrupt for every 58uS */
	TCC0->PERBUF.reg = TCC0->PER.reg = 58;

	/* Enable TCC0 */
	TCC0->CTRLA.reg |= TCC_CTRLA_ENABLE;
	
	OS_TaskInit(DCC_TASK_ID, DCC_Task, NULL, DCC_TaskStack, sizeof(DCC_TaskStack));
}
