/*
 * dcc_proxy.h
 *
 * Created: 01/11/2023 18:04:24
 *  Author: jonso
 */ 


#ifndef DCC_PROXY_H_
#define DCC_PROXY_H_

void DCC_SetLocomotiveSpeed(uint8_t Address, uint8_t Speed, uint8_t Forward);
void DCC_SetLocomotiveFunction(uint8_t Address, uint8_t Function, uint8_t Set);
void DCC_StopLocomotive(uint8_t Address);
void DCC_PowerOff(void);
void DCC_PowerOn(void);
void DCC_Init(void);

#endif /* DCC_PROXY_H_ */