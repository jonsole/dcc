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
PA20  
PA21
PA22 - PCA9685 SERCOM3 I2C
PA23 - PCA9685 SERCOM3 I2C

PB00
PB01 - 
PB02 - 
PB03
PB04 - 
PB05 - 
PB06 - 
PB07 - 

PB30 - ESP2 SERCOM5
PB31 - ESP2 SERCOM5

*/



extern void CLI_Init(void);
extern void CLI_InputChar(uint8_t Char);


#define MAIN_SIGNAL_TIMER		(1 << (OS_SIGNAL_USER + 0))
#define MAIN_SIGNAL_DEBUG_INPUT (1 << (OS_SIGNAL_USER + 1))

ESP_t ESP[2];



void MAIN_Task(void *Instance)
{
	for (;;)
	{
		OS_SignalSet_t Sig = OS_SignalWait(0xFFFFUL);
		
		if (Sig & MAIN_SIGNAL_DEBUG_INPUT)
		{
			uint8_t Char = Debug_GetChar();
			while (Char)
			{
				CLI_InputChar(Char);		
				Char = Debug_GetChar();
			}
		}
		
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
	uint8_t Functions;
} DCC_SetLocoFunctions_t;


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
		
		case DCC_SET_LOCO_FUNCTIONS:
		{
			DCC_SetLocoFunctions_t *Func = (DCC_SetLocoFunctions_t *)Msg;
			DCC_SetLocomotiveFunctions(Func->Address, Func->Functions, 0);
			if (ESP_IsSynced(Esp))
			{
				ESP_Packet_t *Packet = ESP_CreatePacket(1, Msg, sizeof(DCC_SetLocoFunctions_t), false);
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
	Debug_Init(MAIN_TASK_ID, MAIN_SIGNAL_DEBUG_INPUT);

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
	
	DCC_Init(DCC_MODE_NORMAL);
	
	CLI_Init();

	Debug("DCC Encoder v0.1\n");

	/* Enable 1ms tick */
	SysTick_Config(48000000 / 1000);
	NVIC_EnableIRQ(SysTick_IRQn);
		
	static uint32_t MAIN_TaskStack[256];
	OS_TaskInit(MAIN_TASK_ID, MAIN_Task, NULL, MAIN_TaskStack, sizeof(MAIN_TaskStack));
	
	OS_Start();	
}

#ifdef __cplusplus
}
#endif
