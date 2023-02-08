/*
 * button.h
 *
 * Created: 22/10/2021 21:48:39
 *  Author: jonso
 */ 


#ifndef BUTTON_H_
#define BUTTON_H_

#include <stdint.h>
#include <stdbool.h>

void BUT_InitButton(uint8_t Instance, uint8_t Pin);
void BUT_ScanButtons(void);
bool BUT_IsButtonPressed(uint8_t Instance);

#endif /* BUTTON_H_ */