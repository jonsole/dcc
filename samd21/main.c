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
#include "dcc_proxy.h"

typedef struct
{
	uint8_t R;
	uint8_t G;
	uint8_t B;
} APA102_LED_t;

#define APA102_CLK	PIN_PB11
#define APA102_DATA PIN_PB10

void APA102_Init(void)
{
	PIO_EnableOutput(APA102_DATA); // DATA
	PIO_Clear(APA102_CLK);
	PIO_EnableOutput(APA102_CLK); // CLK
}


static void APA102_WriteByte(uint8_t Byte)
{
	for (int Index = 0; Index < 8; Index++)
	{
		if (Byte & 0x80)
			PIO_Set(APA102_DATA);
		else
			PIO_Clear(APA102_DATA);
	  
		PIO_Set(APA102_CLK);
		Byte = Byte << 1;
		
		PIO_Clear(APA102_CLK);
	}
}


void APA102_Update(const APA102_LED_t *Leds, uint16_t NumLeds)
{
	for (int Index = 0; Index < 4; Index++)
		APA102_WriteByte(0);
	
	for (int Led = 0; Led < NumLeds; Led++)
	{
		APA102_WriteByte(0xFF);  // Maximum global brightness
		APA102_WriteByte(Leds[Led].B);
		APA102_WriteByte(Leds[Led].G);
		APA102_WriteByte(Leds[Led].R);
	}

	for (int Index = 0; Index < NumLeds; Index += 16)
		APA102_WriteByte(0xFF);
}


APA102_LED_t TRCON_Leds[8];

typedef enum
{
	STATE_SHUTDOWN,
	STATE_STARTUP,
	STATE_TRAIN_SELECT,
	STATE_SPEED_SELECT,
	STATE_TRAIN_PROGRAM,
	STATE_FUNCTION1_PROGRAM,
	STATE_FUNCTION2_PROGRAM,	
	STATE_FUNCTION3_PROGRAM,
	STATE_FUNCTION4_PROGRAM
} TRCON_State_t;

typedef struct
{
	uint8_t Address;
	uint8_t Speed;
	uint8_t IsForward;
	uint32_t Functions;
	uint8_t FunctionIndex[4];
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

void TRCON_AdjustFunction(uint8_t Instance, int8_t Delta)
{
	TRCON_ControllerState_t *c = &Controller[Instance];
	TRCON_TrainState_t *t = &c->Train[c->ActiveTrain];
	int Index = c->State - STATE_FUNCTION1_PROGRAM;
	c->DeltaRaw += Delta;
	while (c->DeltaRaw > 3)
	{
		if (t->FunctionIndex[Index] == 36)
			t->FunctionIndex[Index] = 0;
		else
			t->FunctionIndex[Index] += 1;
		c->DeltaRaw -= 4;
		c->DirCounter = 0;
	}
	while (c->DeltaRaw < 0)
	{
		if (t->FunctionIndex[Index] == 0)
			t->FunctionIndex[Index] = 36;
		else
			t->FunctionIndex[Index] -= 1;	
		c->DeltaRaw += 4;
		c->DirCounter = 0;
	}
	
	Debug_PrintF("Function %u, Raw %d\n", t->FunctionIndex[Index], c->DeltaRaw);
}


void TRCON_ToggleFunction(uint8_t Instance, uint8_t Function)
{
	TRCON_ControllerState_t *c = &Controller[Instance];
	TRCON_TrainState_t *t = &c->Train[c->ActiveTrain];
	t->Functions ^= (1UL << Function);	
	DCC_SetLocomotiveFunction(t->Address, Function, t->Functions & (1UL << Function) ? 1 : 0);	
	Debug_PrintF("Train %u, functions %u\n", t->Address, t->Functions);
}


void TRCON_SetFunction(uint8_t Instance, uint8_t Function)
{
	TRCON_ControllerState_t *c = &Controller[Instance];
	TRCON_TrainState_t *t = &c->Train[c->ActiveTrain];
	t->Functions |= (1UL << Function);
	DCC_SetLocomotiveFunction(t->Address, Function, t->Functions & (1UL << Function) ? 1 : 0);
	Debug_PrintF("Train %u, functions %u\n", t->Address, t->Functions);
}


void TRCON_ClearFunction(uint8_t Instance, uint8_t Function)
{
	TRCON_ControllerState_t *c = &Controller[Instance];
	TRCON_TrainState_t *t = &c->Train[c->ActiveTrain];
	t->Functions &= ~(1UL << Function);
	DCC_SetLocomotiveFunction(t->Address, Function, t->Functions & (1UL << Function) ? 1 : 0);
	Debug_PrintF("Train %u, functions %u\n", t->Address, t->Functions);
}


void TRCON_Stop(uint8_t Instance, bool Emergency)
{
	TRCON_ControllerState_t *c = &Controller[Instance];
	TRCON_TrainState_t *t = &c->Train[c->ActiveTrain];
	t->Speed = 0;
	Debug_PrintF("Train %u, stop\n", t->Address);
	if (Emergency)
		DCC_StopLocomotive(t->Address);
	else
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
	DCC_SetLocomotiveFunction(t->Address, 0, t->Functions & 1);
	Debug_PrintF("Train new address %u\n", t->Address);
}


void TRCON_UpdateLeds(uint8_t Instance)
{
	TRCON_ControllerState_t *c = &Controller[Instance];
	TRCON_TrainState_t *t = &c->Train[c->ActiveTrain];

	/* Inactive trains LED which are moving are red */
	for (int Index = 0; Index < 4; Index++)
	{
		TRCON_Leds[Index].R = c->Train[Index].Speed ? 10 : 0;
		TRCON_Leds[Index].G = 0;
		TRCON_Leds[Index].B = 0;
	}	
	
	/* Active train LED are yellow or green */
	TRCON_Leds[c->ActiveTrain].G = 10;

	for (int Index = 0; Index < 4; Index++)
	{
		int FunctionIndex = t->FunctionIndex[Index];
		TRCON_Leds[7 - Index].R = 0;
		TRCON_Leds[7 - Index].G = (t->Functions & (1UL << FunctionIndex)) ? 10 : 0;
		TRCON_Leds[7 - Index].B = 0;
	}

	switch (c->State)
	{
		case STATE_FUNCTION1_PROGRAM:
		case STATE_FUNCTION2_PROGRAM:
		case STATE_FUNCTION3_PROGRAM:
		case STATE_FUNCTION4_PROGRAM:
		{
			int Led = 7 - (c->State - STATE_FUNCTION1_PROGRAM);
			if (c->Counter & 0x40)
			{
				TRCON_Leds[Led].R = 0;
				TRCON_Leds[Led].G = 10;
				TRCON_Leds[Led].B = 0;
			}
			else
			{
				TRCON_Leds[Led].R = 0;
				TRCON_Leds[Led].G = 0;
				TRCON_Leds[Led].B = 0;
			}
		}
		break;
		
		case STATE_TRAIN_PROGRAM:
		{			
			int Led = c->ProgramIndex;
			if (c->Counter & 0x40)
			{
				TRCON_Leds[Led].R = 0;
				TRCON_Leds[Led].G = 10;
				TRCON_Leds[Led].B = 0;
			}
			else
			{
				TRCON_Leds[Led].R = 0;
				TRCON_Leds[Led].G = 0;
				TRCON_Leds[Led].B = 0;
			}
		}
		break;
	}
	
	APA102_Update(TRCON_Leds, 8);	
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
	DCC_SetLocomotiveFunction(t->Address, 0, t->Functions & 1);	
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
		t->Functions = 0;
		t->FunctionIndex[0] = 0;
		t->FunctionIndex[1] = 1;
		t->FunctionIndex[2] = 2;
		t->FunctionIndex[3] = 3;
		DCC_SetLocomotiveSpeed(Addr[Index], t->Speed, t->IsForward);
		DCC_SetLocomotiveFunction(Addr[Index], 0, 0);			
		DCC_SetLocomotiveFunction(t->Address, 0, t->Functions & 1);	
	}
	
	for (int Index = 0; Index < 9; Index++)
		BUT_InitButton(Index, Pin[Index], Index == 8);
}

#define BUTTON_FUNC_1 (4)
#define BUTTON_FUNC_2 (5)
#define BUTTON_FUNC_3 (6)
#define BUTTON_FUNC_4 (7)
#define BUTTON_ROT    (8)

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
		
		case STATE_FUNCTION1_PROGRAM:
		case STATE_FUNCTION2_PROGRAM:
		case STATE_FUNCTION3_PROGRAM:
		case STATE_FUNCTION4_PROGRAM:
		{
			c->Counter -= 1;
			int Index = c->State - STATE_FUNCTION1_PROGRAM;
			if (c->Counter & 0x40)
			{
				LTP305_DrawChar(0, '0' + t->FunctionIndex[Index] / 10);
				LTP305_DrawChar(1, '0' + t->FunctionIndex[Index] % 10);
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

	TRCON_UpdateLeds(0);	
	LPT305_SetBrightness(20 + (t->Functions & 1) * 108);
			
	BUT_ScanButtons();

	for (int Index = BUTTON_FUNC_1; Index <= BUTTON_FUNC_4; Index++)
	{
		switch (BUT_GetEvent(Index))
		{	
			case BUT_EVENT_HELD_RELEASED:
			{
				if (c->State == STATE_SPEED_SELECT)
				{
					switch (Index)
					{
						case BUTTON_FUNC_1:
						case BUTTON_FUNC_2:
							TRCON_ToggleFunction(Instance, t->FunctionIndex[Index - BUTTON_FUNC_1]);
							break;
						case BUTTON_FUNC_3:
						case BUTTON_FUNC_4:
							TRCON_ClearFunction(Instance, t->FunctionIndex[Index - BUTTON_FUNC_1]);
							break;
					}
				}
			}
			break;
			
			case BUT_EVENT_RELEASED:
			{
				if (c->State == STATE_SPEED_SELECT)
				{
					switch (Index)
					{
						case BUTTON_FUNC_1:
						case BUTTON_FUNC_2:
							TRCON_ToggleFunction(Instance, t->FunctionIndex[Index - BUTTON_FUNC_1]);			
							break;
						case BUTTON_FUNC_3:
						case BUTTON_FUNC_4:
							TRCON_ClearFunction(Instance, t->FunctionIndex[Index - BUTTON_FUNC_1]);
							break;
					}
				}			
				else if (c->State == (STATE_FUNCTION1_PROGRAM + (Index - BUTTON_FUNC_1)))
					c->State = STATE_SPEED_SELECT;
			}
			break;

			case BUT_EVENT_HELD:
			{
				if (BUT_IsPressed(BUTTON_ROT))
				{
					c->State = STATE_FUNCTION1_PROGRAM + (Index - BUTTON_FUNC_1);			
					switch (Index)
					{
						case BUTTON_FUNC_3:
						case BUTTON_FUNC_4:
							TRCON_ClearFunction(Instance, t->FunctionIndex[Index - BUTTON_FUNC_1]);
							break;
					}
				}
			}
			break;
			
			case BUT_EVENT_PRESSED:
			{
				if (c->State == STATE_SPEED_SELECT)
				{
					switch (Index)
					{
						case BUTTON_FUNC_3:
						case BUTTON_FUNC_4:
							TRCON_SetFunction(Instance, t->FunctionIndex[Index - BUTTON_FUNC_1]);			
							break;
					}
				}
				
				if (c->State == (STATE_FUNCTION1_PROGRAM + (Index - BUTTON_FUNC_1)))
					c->State = STATE_SPEED_SELECT;			
			}
			break;

		}
	}

	switch (BUT_GetEvent(BUTTON_ROT))
	{
		case BUT_EVENT_RELEASED:
		{
			TRCON_ControllerState_t *c = &Controller[0];
			if (c->Train[c->ActiveTrain].Speed > 0)
				TRCON_Stop(0, false);
			else
			{
				TRCON_ChangeDirection(0);
				c->DirCounter = 1000;
			}
		}
		break;
		
		case BUT_EVENT_HELD:
		{
			TRCON_Stop(0, true);
		}
		break;	
	}
		
			
	for (int Index = 0; Index < 4; Index++)
	{			
		switch (BUT_GetEvent(Index))
		{
			case BUT_EVENT_PRESSED:
			{
				/* Check if button has been pressed again when in programming mode */
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
					c->Counter = 512;
					c->State = STATE_TRAIN_SELECT;
				}
			}
			break;
			
			case BUT_EVENT_HELD:
			{
				/* Long button press, enter programming mode */
				c->ProgramAddress = c->Train[Index].Address;
				c->ProgramIndex = Index;
				c->State = STATE_TRAIN_PROGRAM;
			}
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
	const uint8_t Addr[4] = {3, 37, 43, 47};
	const uint8_t Pin[9]  = {PIN_PA16, PIN_PA18, PIN_PA07, PIN_PA20, PIN_PB02, PIN_PA05, PIN_PA04, PIN_PB09, PIN_PA15};
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

		case DCC_STOP_LOCO:
		{
			DCC_StopLoco_t *Stop = (DCC_StopLoco_t *)Msg;
			TRCON_ControllerState_t *c = &Controller[0];
			for (int Index = 0; Index < 4; Index++)
			{
				if (Stop->Address == c->Train[Index].Address)
				{
					c->Train[Index].Speed = TRCON_SpeedToRawSpeed(0);
					c->Train[Index].IsForward = Stop->Forward;
				}
			}
		}
		break;
		
		case DCC_SET_LOCO_FUNCTIONS:
		{
			DCC_SetLocoFunction_t *Func = (DCC_SetLocoFunction_t *)Msg;
			TRCON_ControllerState_t *c = &Controller[0];
			for (int Index = 0; Index < 4; Index++)
			{
				if (Func->Address == c->Train[Index].Address)
				{
					if (Func->Set)
						c->Train[Index].Functions |= (1 << Func->Function);
					else
						c->Train[Index].Functions &= ~(1 << Func->Function);
				}
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
					
				case STATE_FUNCTION1_PROGRAM:
				case STATE_FUNCTION2_PROGRAM:
				case STATE_FUNCTION3_PROGRAM:
				case STATE_FUNCTION4_PROGRAM:
					TRCON_AdjustFunction(0, ROT_Read(0, true));
					break;
									
				default:
					break;
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
	ROT_InitEncoder(0, PIN_PA11, 11, PIN_PA10, 10, MAIN_TASK_ID, MAIN_SIGNAL_ROT0_CHANGE);
		
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
	
	APA102_Init();
	
	/* Start OS task scheduler */
	OS_Start();	
}
