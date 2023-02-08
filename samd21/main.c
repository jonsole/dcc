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
#include "eic.h"
#include "rot.h"
#include "button.h"
#include "ltp305.h"
#include "esp.h"
#include "dcc_msg.h"

typedef enum
{
	STATE_SHUTDOWN,
	STATE_STARTUP,
	STATE_TRAIN_SELECT,
	STATE_SPEED_SELECT,
	STATE_TRAIN_PROGRAM,	
} TRCON_State_t;

typedef struct
{
	uint8_t Address;
	uint8_t Speed;
	uint8_t IsForward;
	uint8_t Lights;
} TRCON_TrainState_t;

typedef struct 
{	
	int16_t DeltaRaw;
	uint8_t ActiveTrain;
	
	uint8_t ProgramAddress;
	uint8_t ProgramIndex;	
	uint16_t Counter;
	uint16_t DirCounter;
	
	TRCON_TrainState_t Train[4];

	uint16_t ButtonPressed[6];
	TRCON_State_t State;
} TRCON_ControllerState_t;



TRCON_ControllerState_t Controller[1];


static int8_t TRCON_RawSpeedToPercent(int16_t Raw)
{
	return (int8_t)(Raw);
}

static int8_t TRCON_RawSpeedToSpeed(int16_t Raw)
{
	return (int8_t)((int32_t)Raw * 126 / (99));
}

static int8_t TRCON_SpeedToRawSpeed(int16_t Raw)
{
	if (Raw > 0)
		return (int8_t)((int32_t)(Raw * 99 + 125) / (126));
	else
		return (int8_t)((int32_t)(Raw * 99 - 125) / (126));	
}


void TRCON_ChangeSpeed(uint8_t Instance, int8_t Delta)
{	
	TRCON_ControllerState_t *c = &Controller[Instance];
	TRCON_TrainState_t *t = &c->Train[c->ActiveTrain];
	
	int8_t Speed = TRCON_RawSpeedToSpeed(t->Speed);
	
	c->DeltaRaw += Delta;
	while (c->DeltaRaw > 3)
	{
		if (t->Speed < 99)
			t->Speed += 1;
		c->DeltaRaw -= 4;
		c->DirCounter = 0;
	}
	while (c->DeltaRaw < 0)
	{
		if (t->Speed > 0)
			t->Speed -= 1;
		c->DeltaRaw += 4;
		c->DirCounter = 0;
	}
		
	int8_t NewSpeed = TRCON_RawSpeedToSpeed(t->Speed);
	if (NewSpeed != Speed)
	{
		Debug_PrintF("Train %u, speed %d\n", t->Address, NewSpeed);
		DCC_SetLocomotiveSpeed(t->Address, NewSpeed, t->IsForward);
	}
}


void TRCON_AdjustAddress(uint8_t Instance, int8_t Delta)
{
	TRCON_ControllerState_t *c = &Controller[Instance];
	c->DeltaRaw += Delta;
	while (c->DeltaRaw > 3)
	{
		c->ProgramAddress += 1;
		if (c->ProgramAddress > 99)
			c->ProgramAddress = 1;
		c->DeltaRaw -= 4;
		c->DirCounter = 0;
	}
	while (c->DeltaRaw < 0)
	{
		c->ProgramAddress -= 1;
		if (c->ProgramAddress < 1)
			c->ProgramAddress = 99;
		c->DeltaRaw += 4;
		c->DirCounter = 0;
	}
			
	Debug_PrintF("Address %u, Raw %d\n", c->ProgramAddress, c->DeltaRaw);
}


void TRCON_ToggleLights(uint8_t Instance)
{
	TRCON_ControllerState_t *c = &Controller[Instance];
	TRCON_TrainState_t *t = &c->Train[c->ActiveTrain];
	t->Lights ^= 0x01;	
	DCC_SetLocomotiveFunctions(t->Address, t->Lights ? 0b10000 : 0b00000);	
	Debug_PrintF("Train %u, lights %u\n", t->Address, t->Lights);
}


void TRCON_Stop(uint8_t Instance)
{
	TRCON_ControllerState_t *c = &Controller[Instance];
	TRCON_TrainState_t *t = &c->Train[c->ActiveTrain];
	t->Speed = 0;
	Debug_PrintF("Train %u, stop\n", t->Address);
	DCC_SetLocomotiveSpeed(t->Address, 0, t->IsForward);
}


void TRCON_ChangeDirection(uint8_t Instance)
{
	TRCON_ControllerState_t *c = &Controller[Instance];
	TRCON_TrainState_t *t = &c->Train[c->ActiveTrain];
	if (t->Speed == 0)
	{
		t->IsForward ^= 1;
		Debug_PrintF("Train %u, direction\n", t->IsForward);
		DCC_SetLocomotiveSpeed(t->Address, 0, t->IsForward);
	}
}


void TRCON_ChangeAddress(uint8_t Instance, uint8_t Index, uint8_t Address)
{
	TRCON_ControllerState_t *c = &Controller[Instance];
	TRCON_TrainState_t *t = &c->Train[Index];
	t->Speed = 0;
	Debug_PrintF("Train %u, stop\n", t->Address);
	DCC_SetLocomotiveSpeed(t->Address, 0, t->IsForward);	
	t->Address = Address;
	
	t = &c->Train[c->ActiveTrain];
	DCC_SetLocomotiveFunctions(t->Address, t->Lights ? 0b10000 : 0b00000);	
	Debug_PrintF("Train new address %u\n", t->Address);
}


void TRCON_Select(uint8_t Instance, uint8_t Index)
{
	TRCON_ControllerState_t *c = &Controller[Instance];
	TRCON_TrainState_t *t = &c->Train[c->ActiveTrain];
	
	c->ActiveTrain = Index;
	
	int8_t Speed = TRCON_RawSpeedToSpeed(t->Speed);
	Debug_PrintF("Control %u now controls Train %u\n", Instance, t->Address);
	Debug_PrintF("Train %u, speed %d\n", t->Address, Speed);
	DCC_SetLocomotiveSpeed(t->Address, Speed, t->IsForward);
	DCC_SetLocomotiveFunctions(t->Address, t->Lights ? 0b10000 : 0b00000);		
}


void TRCON_Init(uint8_t Instance, const uint8_t *Addr, const uint8_t *Pin)
{
	TRCON_ControllerState_t *c = &Controller[Instance];
	c->State = STATE_SHUTDOWN;
	c->DeltaRaw = 0;
	c->Counter = 0;
	c->DirCounter = 0;
	
	for (int Index = 0; Index < 4; Index++)
	{
		TRCON_TrainState_t *t = &c->Train[Index];
		t->Address = Addr[Index];
		t->Speed = 0;
		t->IsForward = 1;
		t->Lights = 0;
		DCC_SetLocomotiveSpeed(Addr[Index], t->Speed, t->IsForward);
		DCC_SetLocomotiveFunctions(Addr[Index], 0b00000);			
		DCC_SetLocomotiveFunctions(t->Address, t->Lights ? 0b10000 : 0b00000);	
	}
	
	for (int Index = 0; Index < 6; Index++)
	{
		BUT_InitButton(Index, Pin[Index]);
		c->ButtonPressed[Index] = 0;
	}
}


void TRCON_Update(uint8_t Instance)
{
	TRCON_ControllerState_t *c = &Controller[Instance];
	TRCON_TrainState_t *t = &c->Train[c->ActiveTrain];
	switch (c->State)
	{
		case STATE_SHUTDOWN:
		{
			LTP305_DrawChar(0, '-');
			LTP305_DrawChar(1, '-');
			LTP305_Update();					
		}
		return;
		
		case STATE_STARTUP:
		{
			LTP305_DrawChar(0, 'O');
			LTP305_DrawChar(1, 'K');
			LTP305_Update();
			c->Counter -= 1;
			if (c->Counter == 0)
			{
				TRCON_Select(Instance, 0);
				c->State = STATE_TRAIN_SELECT;
				c->Counter = 512;
			}
		}
		break;
		
		case STATE_TRAIN_SELECT:
		{
			c->Counter -= 1;
			if (c->Counter & 0x80)
			{
				LTP305_DrawChar(0, '0' + t->Address / 10);
				LTP305_DrawChar(1, '0' + t->Address % 10);
			}
			else
			{
				LTP305_DrawChar(0, ' ');
				LTP305_DrawChar(1, ' ');
			}
			LTP305_Update();		
			if (c->Counter == 0)
				c->State = STATE_SPEED_SELECT;
		}
		break;
		
		case STATE_SPEED_SELECT:
		{
			if (c->DirCounter)
			{
				c->DirCounter -= 1;

				if (t->IsForward)
				{
					LTP305_DrawChar(0, '>');
					LTP305_DrawChar(1, '>');					
				}				
				else
				{
					LTP305_DrawChar(0, '<');
					LTP305_DrawChar(1, '<');					
				}
			}
			else
			{
				int8_t Percent = TRCON_RawSpeedToPercent(t->Speed);
				if (Percent < 0)
					Percent = -Percent;
		
				LTP305_DrawChar(0, '0' + Percent / 10);
				LTP305_DrawChar(1, '0' + Percent % 10);
			}
			LTP305_Update();				
		}
		break;
		
		case STATE_TRAIN_PROGRAM:
		{
			c->Counter -= 1;
			if (c->Counter & 0x40)
			{
				LTP305_DrawChar(0, '0' + c->ProgramAddress / 10);
				LTP305_DrawChar(1, '0' + c->ProgramAddress % 10);
			}
			else
			{
				LTP305_DrawChar(0, ' ');
				LTP305_DrawChar(1, ' ');
			}
			LTP305_Update();		
		}
		break;
	}

	LPT305_SetBrightness(20 + t->Lights * 108);
	
	/* Check for light toggle button */
	if (BUT_IsButtonPressed(5))
	{
		if (c->ButtonPressed[5] == 0)
			TRCON_ToggleLights(Instance);
		
		c->ButtonPressed[5] = 1;
	}
	else
	{
		c->ButtonPressed[5] = 0;			
	}
	
	/* Check for emergency stop button */
	if (BUT_IsButtonPressed(4))		
	{
		if (c->ButtonPressed[Index] < 512)
			c->ButtonPressed[Index] += 1;		
		else
		{
			
			
		}
		
		if (c->ButtonPressed[4] == 0)
			DCC_PowerOff();
		c->ButtonPressed[4] = 1;		
	}
	else
	{
		if (c->ButtonPressed[4] == 1)
			DCC_PowerOn();
		c->ButtonPressed[4] = 0;		
	}
	
	
	for (int Index = 0; Index < 4; Index++)
	{			
		if (BUT_IsButtonPressed(Index))
		{
			if (c->ButtonPressed[Index] == 0)
			{
				/* Check if button pressed again when in programming mode */
				if (c->State == STATE_TRAIN_PROGRAM && Index == c->ProgramIndex)
				{
					/* Re-program address */
					TRCON_ChangeAddress(Instance, c->ProgramIndex, c->ProgramAddress);
					
					/* Go back to speed select state */
					c->State = STATE_SPEED_SELECT;
				}
				else
				{
					/* Change active locomotive */
					TRCON_Select(Instance, Index);
					c->State = STATE_TRAIN_SELECT;
					c->Counter = 512;
				}
			}
			if (c->ButtonPressed[Index] < 512)
				c->ButtonPressed[Index] += 1;
			else
			{
				/* Long button press, enter programming mode */
				c->ProgramAddress = c->Train[Index].Address;
				c->ProgramIndex = Index;
				c->State = STATE_TRAIN_PROGRAM;
			}
		}
		else
		{
			c->ButtonPressed[Index] = 0;			
		}
	}
}

#define MAIN_SIGNAL_TIMER		(1 << (OS_SIGNAL_USER + 0))
#define MAIN_SIGNAL_DEBUG_INPUT (1 << (OS_SIGNAL_USER + 1))
#define MAIN_SIGNAL_ROT0_CHANGE	(1 << (OS_SIGNAL_USER + 2))
#define MAIN_SIGNAL_ROT0_CLICK	(1 << (OS_SIGNAL_USER + 3))
#define MAIN_SIGNAL_ROT1_CHANGE	(1 << (OS_SIGNAL_USER + 4))
#define MAIN_SIGNAL_ROT1_CLICK	(1 << (OS_SIGNAL_USER + 5))

ESP_t Esp;

void CALLBACK_ESP_LinkActive(ESP_t *Esp)
{
	const uint8_t Addr[4] = {8, 37, 43, 47};
	const uint8_t Pin[6]  = {PIN_PA12, PIN_PB10, PIN_PB11, PIN_PB02, PIN_PA05, PIN_PA04};
	TRCON_Init(0, Addr, Pin);

	TRCON_ControllerState_t *t = &Controller[0];
	t->State = STATE_STARTUP;
	t->Counter = 1024;
}

void CALLBACK_ESP_LinkReset(ESP_t *Esp)
{
	TRCON_ControllerState_t *t = &Controller[0];
	t->State = STATE_SHUTDOWN;	
}

void CALLBACK_ESP_PacketReceived(ESP_t *Esp, uint8_t *Msg, uint8_t MsgSize)
{
	const uint8_t Id = Msg[0];
	switch (Id)
	{
		case DCC_SET_LOCO_SPEED:
		{
			DCC_SetLocoSpeed_t *Speed = (DCC_SetLocoSpeed_t *)Msg;
			TRCON_ControllerState_t *c = &Controller[0];
			for (int Index = 0; Index < 4; Index++)
			{
				if (Speed->Address == c->Train[Index].Address)
				{
					c->Train[Index].Speed = TRCON_SpeedToRawSpeed(Speed->Speed);
					c->Train[Index].IsForward = Speed->Forward;
				}
			}			
		}
		break;
		
		case DCC_SET_LOCO_FUNCTIONS:
		{
			DCC_SetLocoFunctions_t *Func = (DCC_SetLocoFunctions_t *)Msg;
			TRCON_ControllerState_t *c = &Controller[0];
			for (int Index = 0; Index < 4; Index++)
			{
				if (Func->Address == c->Train[Index].Address)
					c->Train[Index].Lights = (Func->Functions & 0b10000) ? 1 : 0;
			}			
		}
		break;
	}
	MEM_Free(Msg);
}

void MAIN_Task(void *Instance)
{
	for (;;)
	{
		OS_SignalSet_t Sig = OS_SignalWait(0xFFFFUL);

		if (Sig & MAIN_SIGNAL_TIMER)
		{						
			BUT_ScanButtons();
			TRCON_Update(0);
			ESP_TimerTick(&Esp);
			ESP_Task(&Esp);
		}
		
		if (Sig & MAIN_SIGNAL_ROT0_CHANGE)
		{
			TRCON_ControllerState_t *c = &Controller[0];
			switch (c->State)
			{
				case STATE_SPEED_SELECT:
					TRCON_ChangeSpeed(0, ROT_Read(0, true));
					break;
					
				case STATE_TRAIN_PROGRAM:
					TRCON_AdjustAddress(0, ROT_Read(0, true));
					break;
					
				default:
					break;
			}
		}
		
		if (Sig & MAIN_SIGNAL_ROT0_CLICK)
		{
			TRCON_ControllerState_t *c = &Controller[0];
			if (c->Train[c->ActiveTrain].Speed > 0)			
				TRCON_Stop(0);						
			else
			{
				TRCON_ChangeDirection(0);
				c->DirCounter = 1000;
			}
		}
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

	/* Initialise external interrupt controller */
	EIC_Init();
	
	/* Initialise OS */
	OS_Init();
		
	PIO_SetPeripheral(PIN_PA24D_SERCOM5_PAD2, PIO_PERIPHERAL_D);
	PIO_SetPeripheral(PIN_PA25D_SERCOM5_PAD3, PIO_PERIPHERAL_D);
	PIO_EnablePeripheral(PIN_PA24D_SERCOM5_PAD2);
	PIO_EnablePeripheral(PIN_PA25D_SERCOM5_PAD3);
	PIO_EnableOutput(PIN_PA25);
	
	ESP_Init(&Esp, 5, 2, 3);
	
	/* Initialise rotary encoder */
	ROT_Init();	
	ROT_InitEncoder(0, PIN_PA11, 11, PIN_PA10, 10, PIN_PA15, 15, MAIN_TASK_ID, MAIN_SIGNAL_ROT0_CHANGE, MAIN_SIGNAL_ROT0_CLICK);
		
	/* Initialise LTP305 display */
	LTP305_Init();	
		
	/* Initialise debug output */
	Debug_Init(MAIN_TASK_ID, MAIN_SIGNAL_DEBUG_INPUT);
	Debug("Locomotive Controller v0.1\n");

	/* Configure SYSTICK for 1ms */
	SysTick_Config(48000000 / 1000);
	NVIC_EnableIRQ(SysTick_IRQn);
	
	/* Create main task */	
	static uint32_t MAIN_TaskStack[256];
	OS_TaskInit(MAIN_TASK_ID, MAIN_Task, NULL, MAIN_TaskStack, sizeof(MAIN_TaskStack));
	
	/* Start OS task scheduler */
	OS_Start();	
}
