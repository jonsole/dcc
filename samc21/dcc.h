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

bool DCC_CvWrite(uint16_t CvId, uint8_t Value);
void DCC_CvVerify(uint16_t CvId, uint8_t Value);
uint8_t DCC_CvRead(uint16_t CvId);
void DCC_TimerTick(void);
void DCC_SetLocomotiveSpeed(uint8_t Loco, uint8_t Speed, uint8_t Forward);
void DCC_SetLocomotiveFunctions(uint8_t Loco, uint8_t Functions, uint8_t Group);
void DCC_StopLocomotive(uint8_t Loco, uint8_t Forward);


bool DCC_TxIsComplete(void);
bool DCC_TxIsIdle(void);
void DCC_TxIdle(void);


#ifdef __cplusplus
}
#endif

#endif /* DCC_H_ */