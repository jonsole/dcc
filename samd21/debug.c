#include "debug.h"
#include "dmac.h"
#include "buffer.h"
#include "sercom.h"
#include "os.h"
#include "ltp305.h"

Debug_t DebugState;


static void Debug_TxKick(void)
{
	return;
}

void Debug_FormatPutChar(char Char, void *Context)
{
	return;
	if (BufferSpace(DebugState.TxBuffer) < 2)
		Panic(); 

	BufferWrite(DebugState.TxBuffer, Char);
	if (Char == '\n')
		Debug_TxKick();
	else if (BufferAmount(DebugState.TxBuffer) > 80)
		Debug_TxKick();
}

uint8_t Debug_GetChar(void)
{
	if (BufferIsEmpty(DebugState.RxBuffer))
		return 0;
		
	return BufferRead(DebugState.RxBuffer);
}

void Debug_PutChar(char Char)
{
	return;
	if (BufferSpace(DebugState.TxBuffer) < 2)
		Panic();

	BufferWrite(DebugState.TxBuffer, Char);
	Debug_TxKick();
}

void Debug_Init(OS_TaskId_t TaskId, OS_SignalSet_t DebugSignal)
{
	DebugState.Usart = &SERCOM4->USART;
	DebugState.Instance = 4;
	DebugState.ClientTask = TaskId;
	DebugState.ClientSignal = DebugSignal;
}

void Debug_SetLevel(uint8_t Level)
{
	DebugState.Level = Level;
}

uint8_t Debug_GetLevel(void)
{
	return DebugState.Level;
}


void Debug_Panic(const char *Msg, const char *File, int Line)
{
	OS_InterruptDisable();
	
	LTP305_DrawChar(0, 'E');
	LTP305_DrawChar(1, 'E');
	LTP305_Update();
	for (;;)
		__WFI();		
}
