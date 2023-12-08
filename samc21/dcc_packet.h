/*
 * dcc_packet.h
 *
 * Created: 25/02/2023 13:45:14
 *  Author: jonso
 */ 

#ifndef DCC_PACKET_H_
#define DCC_PACKET_H_

#include "os.h"
#include "rtime.h"

#define DCC_SCHEDULE_END (-1)
#define DCC_SCHEDULE_LOW (-2)
#define DCC_MS(x)	((x) * 1000)

class DCC_Packet_t;

typedef struct DCC_AddressInfo
{
	struct DCC_AddressInfo *Next;
	uint8_t Address;
	Time_t HoldOffTime;	// Earliest time that a packet for this address can be sent
	DCC_Packet_t *List;
} DCC_AddressInfo_t;


class DCC_Packet_t
{
public:
	DCC_Packet_t(uint16_t Address);
	virtual ~DCC_Packet_t();
	
	virtual bool Schedule(uint32_t &Time) { return true; }
	virtual bool IsSame(const DCC_Packet_t *Packet) { return false; }
	virtual void PacketStart(void) { return; }
	virtual uint16_t DataEnd(void) { return 0; }
	virtual void PacketEnd(void) { return; }
	
	DCC_Packet_t *Next;
	
	uint16_t Address;
	DCC_AddressInfo_t *AddressInfo;
	
	Time_t Time;

	uint8_t Size;
	uint8_t Data[6];

	OS_TaskId_t TaskId;
	OS_SignalSet_t Signal;

	uint8_t PreambleBits;
	
	enum
	{
		CREATED,
		SCHEDULED,
		ACTIVE,
		COMPLETE
	} State;
	
	bool Cancelled;
	
	void Init(const uint8_t *Data, uint8_t Size, uint8_t PreambleBits, OS_SignalSet_t Signal);
};


class DCC_SpeedPacket_t : public DCC_Packet_t
{
public:
	DCC_SpeedPacket_t(uint8_t Address, uint8_t Speed, uint8_t Forward);
	virtual bool Schedule(uint32_t &Time);
	virtual bool IsSame(const DCC_Packet_t *Packet);
	virtual void PacketStart(void);
	virtual void PacketEnd(void);
protected:
	uint8_t TxCount;
	uint32_t Scheduled;
	bool Repeat;
};



class DCC_FunctionPacket_t : public DCC_Packet_t
{
public:
	DCC_FunctionPacket_t(uint8_t Address, uint8_t Functions, uint8_t StartFunction = 0);
	virtual bool Schedule(uint32_t &Time);
	virtual bool IsSame(const DCC_Packet_t *Packet);
	virtual void PacketStart(void);
	virtual void PacketEnd(void);
protected:
	uint8_t TxCount;
	uint32_t Scheduled;
	uint8_t Mask;
};

class DCC_FunctionExpansionPacket_t : public DCC_Packet_t
{
public:
	DCC_FunctionExpansionPacket_t(uint8_t Address, uint8_t Functions, uint8_t StartFunction);
	virtual bool Schedule(uint32_t &Time);
	virtual bool IsSame(const DCC_Packet_t *Packet);
protected:
	uint8_t TxCount;
	uint32_t Scheduled;
	uint8_t Mask;
};


class DCC_IdlePacket_t : public DCC_Packet_t
{
public:
	DCC_IdlePacket_t(void);
	virtual bool Schedule(uint32_t &Time);
	virtual void PacketStart(void);
	virtual void PacketEnd(void);
protected:
	bool Scheduled;
};



class DCC_ServicePacket_t : public DCC_Packet_t
{
public:
	DCC_ServicePacket_t(const uint8_t *Data, uint8_t DataSize, OS_SignalSet_t Signal);
	virtual bool Schedule(uint32_t &Time);
protected:
	uint8_t TxCount;
};


class DCC_CvWritePacket_t : public DCC_Packet_t
{
public:
	DCC_CvWritePacket_t(uint16_t CvId, uint8_t Value, bool *ResultPtr, OS_SignalSet_t Signal);
	virtual bool Schedule(uint32_t &Time);
	virtual void PacketStart(void);
	virtual uint16_t DataEnd(void);
	virtual void PacketEnd(void);
protected:
	uint8_t TxCount;
	bool *AckPtr;
};

class DCC_CvReadPacket_t : public DCC_Packet_t
{
public:
	DCC_CvReadPacket_t(uint16_t CvId, uint8_t *ResultPtr, OS_SignalSet_t Signal);
	virtual bool Schedule(uint32_t &Time);
	virtual void PacketStart(void);
	virtual uint16_t DataEnd(void);
	virtual void PacketEnd(void);
protected:
	uint8_t TxCount;
	uint8_t *ValuePtr;
};

#endif /* DCC_PACKET_H_ */