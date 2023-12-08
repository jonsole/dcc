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

void DCC_SetLocomotiveFunction(uint8_t Address, uint8_t Function, uint8_t Set)
{
	DCC_SetLocoFunction_t *Msg = MEM_Create(DCC_SetLocoFunction_t);
	if (Msg)
	{
		Msg->Id = DCC_SET_LOCO_FUNCTIONS;
		Msg->Address = Address;
		Msg->Function = Function;
		Msg->Set = Set;
		ESP_Packet_t *Packet = ESP_CreatePacket(1, Msg, sizeof(DCC_SetLocoFunction_t), false);
		if (Packet)
			ESP_TxPacket(&Esp, Packet);
		else
			MEM_Free(Msg);
	}	
}

void DCC_StopLocomotive(uint8_t Address,  uint8_t Forward)
{
	DCC_StopLoco_t *Msg = MEM_Create(DCC_StopLoco_t);
	if (Msg)
	{
		Msg->Id = DCC_STOP_LOCO;
		Msg->Address = Address;
		Msg->Forward = Forward;
		ESP_Packet_t *Packet = ESP_CreatePacket(1, Msg, sizeof(DCC_StopLoco_t), false);
		if (Packet)
			ESP_TxPacket(&Esp, Packet);
		else
			MEM_Free(Msg);
	}	
}

void DCC_PowerOff(void)
{
}

void DCC_PowerOn(void)
{
}

void DCC_Init(void)
{
}

