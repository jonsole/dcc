#ifndef __DCC_MSG_H
#define __DCC_MSG_H

#include <stdint.h>

#define DCC_SET_LOCO_SPEED	(0x10)
typedef struct
{
	uint8_t Id;
	uint8_t Address;
	uint8_t Speed;
	uint8_t Forward;
} DCC_SetLocoSpeed_t;

#define DCC_SET_LOCO_FUNCTIONS (0x11)
typedef struct
{
	uint8_t Id;
	uint8_t Address;
	uint8_t Function;
	uint8_t Set;
} DCC_SetLocoFunction_t;

#define DCC_STOP_LOCO (0x12)
typedef struct
{
	uint8_t Id;
	uint8_t Address;
	uint8_t Forward;
} DCC_StopLoco_t;

#endif 