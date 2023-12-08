#ifndef __DEBUG_H_
#define __DEBUG_H_

//#include <stdio.h>
#include <stdint.h>
#include <sam.h>
#include "os.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG_TX_BUFFER_SIZE (4096)
#define DEBUG_RX_BUFFER_SIZE (1024)

typedef struct
{
	volatile uint16_t Index;
	volatile uint16_t Outdex;
	volatile uint8_t Buffer[DEBUG_TX_BUFFER_SIZE];
} DebugTxBuffer_t;

typedef struct
{
	volatile uint16_t Index;
	volatile uint16_t Outdex;
	volatile uint8_t Buffer[DEBUG_RX_BUFFER_SIZE];
} DebugRxBuffer_t;

typedef struct
{
	SercomUsart *Usart;
	uint8_t Instance;
	volatile uint8_t TxInProgress;

	DebugTxBuffer_t TxBuffer;
	DebugRxBuffer_t RxBuffer;

	uint8_t TxUsartDmaChannel;
	uint8_t TxUsartDmaTrigger;

	uint8_t Level;
	
	OS_TaskId_t ClientTask;
	OS_SignalSet_t ClientSignal;
	
} Debug_t;

extern Debug_t DebugState;

extern void Debug_Init(OS_TaskId_t TaskId, OS_SignalSet_t DebugSignal);
extern void Debug_SetLevel(uint8_t Level);
extern uint8_t Debug_GetLevel(void);
extern void Debug_FormatPutChar(char Char, void *Context);
extern void Debug_PutChar(char Char);
extern uint8_t Debug_GetChar(void);
extern void Debug_PrintF(const char *Format, ...);

#ifndef Debug
#define Debug(...)			Debug_PrintF(__VA_ARGS__)
#define DebugNoInt(...)		do { OS_InterruptDisable();Debug_PrintF(__VA_ARGS__);OS_InterruptEnable();} while(0)
#endif
#define Debug_Test(...)		((DebugState.Level >= 3) ? Debug_PrintF(__VA_ARGS__) : 0)
#define Debug_Verbose(...)	((DebugState.Level >= 2) ? Debug_PrintF(__VA_ARGS__) : 0)
#define Debug_Info(...)		((DebugState.Level >= 1) ? Debug_PrintF(__VA_ARGS__) : 0)
#define Debug_Error(...)	((DebugState.Level >= 0) ? Debug_PrintF(__VA_ARGS__) : 0)

extern void Debug_Panic(const char *, const char *, int);
extern void Debug_PanicFormatPutChar(char Char, void *Context);
extern void Debug_PanicPrintF(const char *Format, ...);

#define Panic()			Debug_Panic("Panic()", __FILE__, __LINE__)
#define PanicMessage(m)		Debug_Panic(#m, __FILE__, __LINE__)
#define PanicFalse(e)	do { if (!(e)) Debug_Panic("PanicFalse(" #e ")", __FILE__, __LINE__); } while(0)
#define PanicNull(e)	do { if ((e) == NULL) Debug_Panic("PanicNull(" #e ")", __FILE__, __LINE__); } while(0)

#ifdef __cplusplus
}
#endif

#endif
