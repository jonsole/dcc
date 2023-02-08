/*
 * dcc_proxy.c
 *
 * Created: 26/10/2021 12:39:40
 *  Author: jonso
 */ 

#include "sercom.h"
#include "esp.h"
#include "mem.h"
#include "dcc_msg.h"

#include <stdint.h>

extern ESP_t Esp;

void DCC_SetLocomotiveSpeed(uint8_t Address, uint8_t Speed, uint8_t Forward)
{
	DCC_SetLocoSpeed_t *Msg = MEM_Create(DCC_SetLocoSpeed_t);
	if (Msg)
	{
		Msg->Id = DCC_SET_LOCO_SPEED;
		Msg->Address = Address;
		Msg->Speed = Speed;
		Msg->Forward = Forward;
		ESP_Packet_t *Packet = ESP_CreatePacket(1, Msg, sizeof(DCC_SetLocoSpeed_t), false);
		if (Packet)
			ESP_TxPacket(&Esp, Packet);
		else
			MEM_Free(Msg);
	}
}

void DCC_SetLocomotiveFunctions(uint8_t Address, uint8_t Functions)
{
	DCC_SetLocoFunctions_t *Msg = MEM_Create(DCC_SetLocoFunctions_t);
	if (Msg)
	{
		Msg->Id = DCC_SET_LOCO_FUNCTIONS;
		Msg->Address = Address;
		Msg->Functions = Functions;
		ESP_Packet_t *Packet = ESP_CreatePacket(1, Msg, sizeof(DCC_SetLocoFunctions_t), false);
		if (Packet)
			ESP_TxPacket(&Esp, Packet);
		else
			MEM_Free(Msg);
	}	
}

void DCC_PowerOff(void)
{
	DCC_SetLocoFunctions_t *Msg = MEM_Create(DCC_SetLocoFunctions_t);
	if (Msg)
	{
		Msg->Address = 0;
		Msg->Functions = 0;
		ESP_Packet_t *Packet = ESP_CreatePacket(1, Msg, sizeof(DCC_SetLocoFunctions_t), false);
		if (Packet)
			ESP_TxPacket(&Esp, Packet);
		else
			MEM_Free(Msg);
	}	
}

void DCC_PowerOn(void)
{
}

void DCC_Init(void)
{
}

