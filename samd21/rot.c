/*
 * rot.c
 *
 * Created: 01/07/2021 09:55:27
 *  Author: jonso
 */ 

// Encoder 0 - PA
// Encoder 1

#include "eic.h"
#include "pio.h"
#include "debug.h"

typedef struct  
{
	int8_t Value;
	
	unsigned PioState:2;
	unsigned LastPioState:2;
	
	uint8_t ExtInt[3];
	uint8_t Pio[3];	
	
	OS_TaskId_t Task;
	OS_SignalSet_t ChangeSignal;
	OS_SignalSet_t ResetSignal;
	
} ROT_State_t;


void ROT_IntHandler(void *UserVoid, uint32_t Status)
{
	ROT_State_t *Rot = (ROT_State_t *)UserVoid;

	/* Update PIO state if external interrupt flag set */
	if (Status & (1UL << Rot->ExtInt[0]))
		Rot->PioState = (Rot->PioState & 0b10) | (PIO_Read(Rot->Pio[0]) << 0);
	if (Status & (1UL << Rot->ExtInt[1]))
		Rot->PioState = (Rot->PioState & 0b01) | (PIO_Read(Rot->Pio[1]) << 1);

	/* Combine current state and previous state */
	uint8_t State = (Rot->LastPioState << 2) | Rot->PioState; 
	
	static const int8_t Table[16] = { 0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0 };
	
	if (Table[State])
	{
		Rot->Value += Table[State];
		OS_SignalSend(Rot->Task, Rot->ChangeSignal);
		Rot->LastPioState = Rot->PioState;	
	}
}

void ROT_IntClickHandler(void *UserVoid, uint32_t Status)
{
	ROT_State_t *Rot = (ROT_State_t *)UserVoid;
	if (PIO_Read(Rot->Pio[2]))
		OS_SignalSend(Rot->Task, Rot->ResetSignal);
}


ROT_State_t ROT_State[2];

void ROT_InitEncoder(uint8_t Instance, uint8_t PioA, uint8_t ExtIntA, uint8_t PioB, uint8_t ExtIntB, uint8_t PioC, uint8_t ExtIntC, OS_TaskId_t Task, OS_SignalSet_t ChangeSignal, OS_SignalSet_t ResetSignal)
{
	ROT_State_t *Rot = &ROT_State[Instance];

	Rot->Pio[0] = PioA;
	Rot->ExtInt[0] = ExtIntA;
	Rot->Pio[1] = PioB;
	Rot->ExtInt[1] = ExtIntB;
	Rot->Pio[2] = PioC;
	Rot->ExtInt[2] = ExtIntC;
	
	Rot->Task = Task;
	Rot->ChangeSignal = ChangeSignal;
	Rot->ResetSignal = ResetSignal;
	
	Rot->PioState  = (PIO_Read(Rot->Pio[0]) << 0);
	Rot->PioState |= (PIO_Read(Rot->Pio[1]) << 1);
	Rot->LastPioState = Rot->PioState;	

	PIO_EnableInput(PioA);
	PIO_EnablePullUp(PioA);

	PIO_EnableInput(PioB);
	PIO_EnablePullUp(PioB);

	PIO_EnableInput(PioC);
	PIO_EnablePullDown(PioC);

	/* Configure PIOs as inputs with pull-ups, MUX to to route to EIC */
	EIC_ConfigureEdgeInterrupt(PioA, ExtIntA, true, ROT_IntHandler, Rot);
	EIC_ConfigureEdgeInterrupt(PioB, ExtIntB, true, ROT_IntHandler, Rot);	
	if (ResetSignal)
		EIC_ConfigureEdgeInterrupt(PioC, ExtIntC, true, ROT_IntClickHandler, Rot);	
}

void ROT_Init(void)
{
}

int8_t ROT_Read(uint8_t Instance, bool Reset)
{
	ROT_State_t *Rot = &ROT_State[Instance];
	
	OS_InterruptDisable();
	
	int8_t Value = Rot->Value;
	if (Reset)
		Rot->Value = 0;	
		
	OS_InterruptEnable();
	
	return Value;
}