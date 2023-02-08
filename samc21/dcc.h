/*
 * dcc.h
 *
 * Created: 19/03/2021 09:20:55
 *  Author: jonso
 */ 


#ifndef DCC_H_
#define DCC_H_

#include "os.h"
#include "rtime.h"

typedef enum
{
	DCC_MODE_NORMAL,
	DCC_MODE_SERVICE,
} DCC_Mode_t;

#ifdef __cplusplus
extern "C" {
#endif

void DCC_Init(DCC_Mode_t Mode);
void DCC_Disable(void);

void DCC_DirectWriteByte(uint16_t CvId, uint8_t Value);
void DCC_DirectVerifyByte(uint16_t CvId, uint8_t Value);
void DCC_TimerTick(void);
void DCC_SetLocomotiveSpeed(uint8_t Loco, uint8_t Speed, uint8_t Forward);
void DCC_SetLocomotiveFunctions(uint8_t Loco, uint8_t Functions, uint8_t Group);

#ifdef __cplusplus
}
#endif

//#define DCC_SetLocomotiveSpeed(Address, Speed) DCC_SetLocomotiveSpeed128((Address), (Speed))


#endif /* DCC_H_ */