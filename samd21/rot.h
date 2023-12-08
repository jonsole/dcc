/*
 * rot.h
 *
 * Created: 22/10/2021 21:53:31
 *  Author: jonso
 */ 

#ifndef ROT_H_
#define ROT_H_

#include <stdint.h>
#include <stdbool.h>

#include "os.h"

void ROT_IntHandler(void *UserVoid, uint32_t Status);
void ROT_IntClickHandler(void *UserVoid, uint32_t Status);
void ROT_InitEncoder(uint8_t Instance, uint8_t PioA, uint8_t ExtIntA, uint8_t PioB, uint8_t ExtIntB, OS_TaskId_t Task, OS_SignalSet_t ChangeSignal);
void ROT_Init(void);
int8_t ROT_Read(uint8_t Instance, bool Reset);

#endif /* ROT_H_ */