/*
 * eic.h
 *
 * Created: 02/07/2021 11:35:27
 *  Author: jonso
 */ 


#ifndef EIC_H_
#define EIC_H_

#include <sam.h>
#include <stdbool.h>

void EIC_Init(void);;
void EIC_ConfigureEdgeInterrupt(uint8_t Pio, uint8_t ExtInt, bool Filter, void (*Handler)(void *, const uint32_t), void *HandlerData);

#endif /* EIC_H_ */