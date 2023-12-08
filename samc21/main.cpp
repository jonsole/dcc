/*
 * toyotune_denso.c
 *
 * Created: 27/05/2016 20:20:53
 * Author : Jon
 */ 


#include <sam.h>
#include <memory.h>
#include <stdint.h>
#include <stdbool.h>

#include "debug.h"
#include "mem.h"
#include "dmac.h"
#include "clk.h"
#include "pio.h"
#include "dcc.h"
#include "eic.h"
#include "esp.h"
#include "sercom.h"
#include "ac.h"
#include "dcc_msg.h"

#define ENABLE_ESP
/*

PA00 -
PA01 -
PA02 - 
PA03
PA04
PA05
PA06
PA07
PA08 - DCC
PA09 - DCC
PA10 - ESP1 SERCOM2 PAD2
PA11 - ESP1 SERCOM2 PAD3
PA12 - 
PA13 - 
PA14
PA15
PA16 - 
PA17 - 
PA18
PA19 
PA20 - Debug
PA21
PA22 - PCA9685 SERCOM3 I2C
PA23 - PCA9685 SERCOM3 I2C

PB00
PB01 - 
PB02 - 
PB03
PB04 - 
PB05 - Current Sensor AC/AIN6
PB06 - 
PB07 - 
PB08 - 
PB09 - Debug

PB30 - ESP2 SERCOM5
PB31 - ESP2 SERCOM5

*/



extern void CLI_Init(void);
extern void CLI_InputChar(uint8_t Char);
extern uint32_t CLI_GetLocoFunctions(uint8_t Loco);
extern void CLI_SetLocoFunctions(uint8_t Loco, uint32_t Functions);


#define MAIN_SIGNAL_TIMER		(1 << (OS_SIGNAL_USER + 0))
#define CLI_SIGNAL_DEBUG_INPUT  (1 << (OS_SIGNAL_USER + 0))


ESP_t ESP[2];



void MAIN_Task(void *Instance)
{
	for (;;)
	{
		OS_SignalSet_t Sig = OS_SignalWait(0xFFFFUL);
			
		if (Sig & MAIN_SIGNAL_TIMER)
		{
#ifdef ENABLE_ESP
			ESP_TimerTick(&ESP[0]);
			ESP_Task(&ESP[0]);
			ESP_TimerTick(&ESP[1]);
			ESP_Task(&ESP[1]);
#endif			
		}
	}
}

void CALLBACK_ESP_LinkActive(ESP_t *Esp)
{
	Debug("Controller %d active\n", &ESP[0] - Esp);
	
}
	
void CALLBACK_ESP_LinkReset(ESP_t *Esp)
{
	Debug("Controller %d reset\n", &ESP[0] - Esp);
}


void CALLBACK_ESP_PacketReceived(ESP_t *Esp, uint8_t *Msg, uint8_t MsgSize)
{
	const uint8_t Id = Msg[0];
	Esp = (Esp == &ESP[0]) ? &ESP[1] : &ESP[0];
	switch (Id)
	{
		case DCC_SET_LOCO_SPEED:
		{
			DCC_SetLocoSpeed_t *Speed = (DCC_SetLocoSpeed_t *)Msg;
			DCC_SetLocomotiveSpeed(Speed->Address, Speed->Speed, Speed->Forward);
			if (ESP_IsSynced(Esp))
			{
				ESP_Packet_t *Packet = ESP_CreatePacket(1, Msg, sizeof(DCC_SetLocoSpeed_t), false);
				if (Packet)
					ESP_TxPacket(Esp, Packet);
			}
			else
				MEM_Free(Msg);
		}
		break;

		case DCC_STOP_LOCO:
		{
			DCC_StopLoco_t *Stop = (DCC_StopLoco_t *)Msg;
			DCC_StopLocomotive(Stop->Address, Stop->Forward);
			if (ESP_IsSynced(Esp))
			{
				ESP_Packet_t *Packet = ESP_CreatePacket(1, Msg, sizeof(DCC_StopLoco_t), false);
				if (Packet)
					ESP_TxPacket(Esp, Packet);
			}
			else
				MEM_Free(Msg);
			
		}
		break;
		
		case DCC_SET_LOCO_FUNCTIONS:
		{
			DCC_SetLocoFunction_t *Func = (DCC_SetLocoFunction_t *)Msg;

			uint32_t Functions = CLI_GetLocoFunctions(Func->Address);
			if (Func->Set)
				Functions |= 1UL << Func->Function;
			else
				Functions &= ~(1UL << Func->Function);				
			CLI_SetLocoFunctions(Func->Address, Functions);
			
			if (Func->Function <= 4)
				DCC_SetLocomotiveFunctions(Func->Address, ((Functions >> 1) & 0b01111) | ((Functions & 0b00001) << 4), 0); /* F1 - F4 */
			else if (Func->Function <= 8)
				DCC_SetLocomotiveFunctions(Func->Address, ((Functions >> 5) & 0b01111), 5); /* F5 - F8 */
			else if (Func->Function <= 12)
				DCC_SetLocomotiveFunctions(Func->Address, ((Functions >> 9) & 0b01111), 9); /* F9 - F12 */
			else if (Func->Function <= 20)
				DCC_SetLocomotiveFunctions(Func->Address, ((Functions >> 13) & 0b1111111), 13); /* F13 - F20 */
			else if (Func->Function <= 28)
				DCC_SetLocomotiveFunctions(Func->Address, ((Functions >> 21) & 0b1111111), 21); /* F21 - F28 */
			else if (Func->Function <= 36)
				DCC_SetLocomotiveFunctions(Func->Address, ((Functions >> 29) & 0b1111111), 29); /* F29 - F36 */

			if (ESP_IsSynced(Esp))
			{
				ESP_Packet_t *Packet = ESP_CreatePacket(1, Msg, sizeof(DCC_SetLocoFunction_t), false);
				if (Packet)
					ESP_TxPacket(Esp, Packet);
			}
			else
				MEM_Free(Msg);
		}
		break;
		
		default:
			MEM_Free(Msg);
	}
}

void SysTick_Handler(void)
{
	OS_SignalSend(MAIN_TASK_ID, MAIN_SIGNAL_TIMER);
}




int main(void)
{
	CLK_Init();

    /* Initialize the SAM system */
    SystemInit();

	/* Initialise memory pools */
	MEM_Init();

	/* Initialise DMA controller */
	DMAC_Init();

	OS_Init();
			
	/* Initialise debug output */
	Debug_Init(CLI_TASK_ID, CLI_SIGNAL_DEBUG_INPUT);

#ifdef ENABLE_ESP
	PIO_EnablePullUp(PIN_PA10D_SERCOM2_PAD2);
	PIO_EnablePullUp(PIN_PA11D_SERCOM2_PAD3);
	PIO_SetPeripheral(PIN_PA10D_SERCOM2_PAD2, PIO_PERIPHERAL_D);
	PIO_SetPeripheral(PIN_PA11D_SERCOM2_PAD3, PIO_PERIPHERAL_D);
	PIO_EnablePeripheral(PIN_PA10D_SERCOM2_PAD2);
	PIO_EnablePeripheral(PIN_PA11D_SERCOM2_PAD3);
	SERCOM_UsartInit(2, 115200, 3, 1);
	ESP_Init(&ESP[0], 2/* SERCOM2 */, 2/* DMAC2 */, 2/* TC2 */);
	PIO_EnablePullUp(PIN_PB30D_SERCOM5_PAD0);
	PIO_EnablePullUp(PIN_PB31D_SERCOM5_PAD1);
 	PIO_SetPeripheral(PIN_PB30D_SERCOM5_PAD0, PIO_PERIPHERAL_D);
	PIO_SetPeripheral(PIN_PB31D_SERCOM5_PAD1, PIO_PERIPHERAL_D);
	PIO_EnablePeripheral(PIN_PB30D_SERCOM5_PAD0);
	PIO_EnablePeripheral(PIN_PB31D_SERCOM5_PAD1);
	SERCOM_UsartInit(5, 115200, 1, 0);
	ESP_Init(&ESP[1], 5/* SERCOM5 */, 3/* DMAC3 */, 3/* TC3 */);
#endif
	
	/* Debug PIOs */
	PIO_EnableOutput(PIN_PB09);
	PIO_Clear(PIN_PB09);
	PIO_EnableOutput(PIN_PA20);
	PIO_Clear(PIN_PA20);

	DCC_Init(DCC_MODE_NORMAL);

	AC_Init();

			
	CLI_Init();

	Debug("DCC Encoder v0.1\n");

	/* Enable 1ms tick */
	SysTick_Config(48000000 / 1000);
	NVIC_SetPriority(SysTick_IRQn, 2);
	NVIC_EnableIRQ(SysTick_IRQn);
		
	static uint32_t MAIN_TaskStack[512];
	OS_TaskInit(MAIN_TASK_ID, MAIN_Task, NULL, MAIN_TaskStack, sizeof(MAIN_TaskStack));
	
	OS_Start();	
}

#ifdef __cplusplus
}
#endif
