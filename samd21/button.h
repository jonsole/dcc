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

typedef enum
{
	BUT_EVENT_NONE,
	BUT_EVENT_PRESSED,
	BUT_EVENT_HELD,
	BUT_EVENT_RELEASED,
	BUT_EVENT_HELD_RELEASED,
} BUT_Event_t;

void BUT_InitButton(uint8_t Instance, uint8_t Pin, bool PullDown);
void BUT_ScanButtons(void);
BUT_Event_t BUT_GetEvent(uint8_t Instance);
bool BUT_IsPressed(uint8_t Instance);

#endif /* BUTTON_H_ */