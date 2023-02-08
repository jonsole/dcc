/*
 * dcc.h
 *
 * Created: 19/03/2021 09:20:55
 *  Author: jonso
 */ 


#ifndef DCC_H_
#define DCC_H_

#include "os.h"

typedef enum
{
	DCC_MODE_NORMAL,
	DCC_MODE_SERVICE,
} DCC_Mode_t;

void DCC_Init(DCC_Mode_t Mode);
void DCC_Disable(void);

void DCC_SendPacket(const uint8_t *Data, uint8_t Size, uint8_t InitialBurstCount, uint16_t ResendPeriodMs, uint8_t Priority, OS_SignalSet_t Signal);
void DCC_SendServicePacket(const uint8_t *Data, uint8_t Size, uint8_t InitialBurstCount, uint8_t Priority, OS_SignalSet_t Signal);
void DCC_UpdatePacket(const uint8_t *Data, uint8_t Size, uint8_t InitialBurstCount, uint16_t ResendPeriodMs, uint8_t Priority);

void DCC_DirectWriteByte(uint16_t CvId, uint8_t Value);
void DCC_DirectVerifyByte(uint16_t CvId, uint8_t Value);

void DCC_SetLocomotiveSpeed28(uint8_t Address, int8_t Speed);
void DCC_SetLocomotiveSpeed128(uint8_t Address, int8_t Speed);
void DCC_SetFunctions(uint8_t Address, uint8_t Functions);


#define DCC_SetLocomotiveSpeed(Address, Speed) DCC_SetLocomotiveSpeed128((Address), (Speed))

typedef struct DCC_Loco 
{
	uint8_t Address;
	uint8_t Speed;	
} DCC_Loco_t;

DCC_Loco_t *DCC_AddLocomotive(uint8_t Address);


#endif /* DCC_H_ */