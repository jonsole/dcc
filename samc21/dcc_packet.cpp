/*
 * dcc_packet.cpp
 *
 * Created: 25/02/2023 13:39:21
 *  Author: jonso
 */ 

#include <cstring>

#include "dcc_packet.h"
#include "ac.h"
#include "rtime.h"
#include "debug.h"
#include "pio.h"

uint16_t PacketCount;

DCC_Packet_t::DCC_Packet_t(uint16_t Address)
{
	Next = NULL;
	this->Address = Address;
	AddressInfo = NULL;	
	State = CREATED;
	Cancelled = false;
	PacketCount += 1;
	//Debug("Create packet %u\n", PacketCount);
}

DCC_Packet_t::~DCC_Packet_t()
{
	PacketCount -= 1;
	//Debug("Destroy packet %u\n", PacketCount);
}


void DCC_Packet_t::Init(const uint8_t *Data, uint8_t Size, uint8_t PreambleBits, OS_SignalSet_t Signal)
{
	if (Data)
		memcpy(this->Data, Data, Size);
	this->Size = Size;
	this->PreambleBits = PreambleBits;
	this->TaskId = OS_TaskId();
	this->Signal = Signal;
}



DCC_SpeedPacket_t::DCC_SpeedPacket_t(uint8_t Address, uint8_t Speed, uint8_t Forward) : DCC_Packet_t(Address)
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
	
	Packet[2] |= Speed;
	Packet[3] = Packet[0] ^ Packet[1] ^ Packet[2];
	
	Repeat = (Speed != 0);
	TxCount = 0;
	
	Init(Packet, sizeof(Packet), 14, 0);
}

bool DCC_SpeedPacket_t::Schedule(uint32_t &Time)
{
	TxCount += 1;
	if (TxCount <= 4)
	{
		Scheduled = Time;
		return true;
	}
	else
	{
		//return false;
		if (Repeat)
		{
			Scheduled = Time_Add(Scheduled, DCC_MS(500));
			Time = Scheduled;
			return true;
		}
		else
			return false;
	}
}

bool DCC_SpeedPacket_t::IsSame(const DCC_Packet_t *Packet)
{
	return (Packet->Data[0] == Data[0]) &&
		   (Packet->Data[1] == 0b00111111);
}

void DCC_SpeedPacket_t::PacketStart()
{	
	PIO_Set(PIN_PB09);
}

void DCC_SpeedPacket_t::PacketEnd()
{
	PIO_Clear(PIN_PB09);
}





DCC_FunctionPacket_t::DCC_FunctionPacket_t(uint8_t Address, uint8_t Functions, uint8_t StartFunction) : DCC_Packet_t(Address)
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
	switch (StartFunction)
	{
		case 0: /* F1 - F4 + FL */
			Packet[1] = 0b10000000 | (Functions & 0b11111);
			Mask = 0b11100000;
			break;
		
		case 5: /* F5 - F8 */
			Packet[1] = 0b10110000 | (Functions & 0b01111);
			Mask = 0b11110000;
			break;
		
		case 9: /* F9 - F12 */
			Packet[1] = 0b10100000 | (Functions & 0b01111);
			Mask = 0b11110000;
			break;
	}
	Packet[2] = Packet[0] ^ Packet[1];
	Debug("FN: %02x %02x\n", Packet[0], Packet[1]);

	TxCount = 0;
	Init(Packet, sizeof(Packet), 14, 0);
}



bool DCC_FunctionPacket_t::Schedule(uint32_t &Time)
{
	TxCount += 1;
	if (TxCount < 4)
	{
		Scheduled = Time;
		return true;
	}
	else
	{
		return false;
		Scheduled = Time_Add(Scheduled, DCC_MS(1000));
		Time = Scheduled;
		return true;
	}
}


bool DCC_FunctionPacket_t::IsSame(const DCC_Packet_t *Packet)
{
	return false;
	//return (Packet->Data[0] == Data[0]) && ((Packet->Data[1] & Mask) == (Data[1] & Mask));
}

void DCC_FunctionPacket_t::PacketStart()
{
	PIO_Set(PIN_PB09);
}

void DCC_FunctionPacket_t::PacketEnd()
{
	PIO_Clear(PIN_PB09);
}










DCC_FunctionExpansionPacket_t::DCC_FunctionExpansionPacket_t(uint8_t Address, uint8_t Functions, uint8_t StartFunction) : DCC_Packet_t(Address)
{
	uint8_t Packet[4];

	Packet[0] = Address;
	switch (StartFunction)
	{
		case 13:
			Packet[1] = 0b11011110;
			break;
		case 21:
			Packet[1] = 0b11011111;
			break;		
		case 29:
			Packet[1] = 0b11011000;
			break;
	}
	Packet[2] = Functions;
	Packet[3] = Packet[0] ^ Packet[1] ^ Packet[2];
	Debug("FNE: %02x %02x %02x\n", Packet[0], Packet[1], Packet[2]);

	TxCount = 0;
	Init(Packet, sizeof(Packet), 14, 0);
}



bool DCC_FunctionExpansionPacket_t::Schedule(uint32_t &Time)
{
	TxCount += 1;
	if (TxCount < 4)
	{
		Scheduled = Time;
		return true;
	}
	else
	{
		Scheduled = Time_Add(Scheduled, DCC_MS(1000));
		Time = Scheduled;
		return true;		
	}
}


bool DCC_FunctionExpansionPacket_t::IsSame(const DCC_Packet_t *Packet)
{
	return (Packet->Data[0] == Data[0]) && (Packet->Data[1] == Data[1]);
}






DCC_IdlePacket_t::DCC_IdlePacket_t(void) : DCC_Packet_t(0xFF)
{
	static const uint8_t Packet[] = { 0xFF, 0x00, 0xFF };
	Init(Packet, sizeof(Packet), 14, 0);
	Scheduled = false;
}

bool DCC_IdlePacket_t::Schedule(uint32_t &Time)
{
	if (!Scheduled)
	{
		Time = Time_Add(Time, 20000);
		Scheduled = true;
		return true;
	}
	else
		return false;
}

void DCC_IdlePacket_t::PacketStart()
{
	PIO_Set(PIN_PB09);
	PIO_Clear(PIN_PB09);
}

void DCC_IdlePacket_t::PacketEnd()
{
}





DCC_ServicePacket_t::DCC_ServicePacket_t(const uint8_t *Data, uint8_t DataSize, OS_SignalSet_t Signal) : DCC_Packet_t(0)
{
	TxCount = 0;
	Init(Data, DataSize, 20, Signal);
}

bool DCC_ServicePacket_t::Schedule(uint32_t &Time)
{
	TxCount += 1;
	return (TxCount <= 5);
}


#if 0
DCC_CvMainWritePacket_t::DCC_CvMainWritePacket_t(uint8_t Address, uint16_t CvId, uint8_t Value, OS_SignalSet_t Signal) : DCC_Packet_t(0)
{
	uint8_t Packet[5];

	Data[0] = Address;

	CvId -= 1;
	
	Data[1] = 0xEC | ((CvId >> 8) & 0x03);  
	Data[2] = CvId & 0xFF;
	Data[3] = Value;
	Data[4] = Packet[0] ^ Packet[1] ^ Packet[2] ^ Packet[3];

	Init(NULL, 5, 14, Signal);
}
#endif



DCC_CvWritePacket_t::DCC_CvWritePacket_t(uint16_t CvId, uint8_t Value, bool *ResultPtr, OS_SignalSet_t Signal) : DCC_Packet_t(0)
{
	AckPtr = ResultPtr;
	*AckPtr = false;
	
	TxCount = 0;	

	/* FFS - Hidden in the documentation "The Configuration variable being addressed is
	   the provided 10 bit address plus 1.  CV #1 is defined by the address 00 00000000" */
	uint16_t Address = CvId - 1;

	Data[0] = 0x7C | ((Address >> 8) & 0x03);
	Data[1] = Address & 0xFF;
	Data[2] = Value;
	Data[3] = Data[0] ^ Data[1] ^ Data[2];
	
	Init(NULL, 4, 14, Signal);
}


void DCC_CvWritePacket_t::PacketStart(void)
{
	AC_EnableTrigger();
	AC_ResetTriggerCount();
}

uint16_t DCC_CvWritePacket_t::DataEnd(void)
{
	return 155;
}

void DCC_CvWritePacket_t::PacketEnd(void)
{
	if (AC_DisableTrigger() > 5000)
		*AckPtr = true;
	TxCount += 1;
}


bool DCC_CvWritePacket_t::Schedule(uint32_t &Time)
{
	if (*AckPtr)
		return false;
	else
		return (TxCount < 20);
}

  

DCC_CvReadPacket_t::DCC_CvReadPacket_t(uint16_t CvId, uint8_t *ResultPtr, OS_SignalSet_t Signal) : DCC_Packet_t(0)
{	
	ValuePtr = ResultPtr;
	*ValuePtr = 0;
	TxCount = 0;

	/* FFS - Hidden in the documentation "The Configuration variable being addressed is
	   the provided 10 bit address plus 1.  CV #1 is defined by the address 00 00000000" */
	const uint16_t Address = CvId - 1;

	/* Instructions packets using Direct CV Addressing are 4 byte packets of the format: 
		Long-preamble   0  0111CCAA  0  AAAAAAAA  0  111KDBBB  0  EEEEEEEE  1 */
	Data[0] = 0b01111000 | (Address >> 8);
	Data[1] = Address & 0xFF;
	Data[2] = 0b11101000 | 0;  // Verify if bit is 1
	Data[3] = Data[0] ^ Data[1] ^ Data[2];	
	
	Init(NULL, 4, 14, Signal);
}


void DCC_CvReadPacket_t::PacketStart(void)
{
	AC_EnableTrigger();
	AC_ResetTriggerCount();
	PIO_Set(PIN_PA20);
}

uint16_t DCC_CvReadPacket_t::DataEnd(void)
{
	PIO_Clear(PIN_PA20);
	return 105;
}

void DCC_CvReadPacket_t::PacketEnd(void)
{
	uint8_t Bit = TxCount / 3;
	if (AC_DisableTrigger() > 5000)
		*ValuePtr |= (1 << Bit);
	TxCount += 1;	
}

bool DCC_CvReadPacket_t::Schedule(uint32_t &Time)
{
	uint8_t Bit = TxCount / 3;
	if (Bit < 8)
	{
		Data[2] = 0b11101000 | Bit;
		Data[3] = Data[0] ^ Data[1] ^ Data[2];		
		return true;
	}
	else
		return false;
}