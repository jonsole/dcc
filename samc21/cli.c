/*
 * cli.c
 *
 * Created: 29/06/2021 13:11:48
 *  Author: jonso
 */ 
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "buffer.h"
#include "debug.h"
#include "dcc.h"

typedef struct
{
	uint8_t Index;
	uint8_t Outdex;
	char Buffer[64];	
}  CLI_Buffer_t;	

CLI_Buffer_t CLI_Buffer;

typedef struct 
{
	int (*command)(int arvc, const char *argv[]);
	const char *name;
	const char *args;
	const char *help;
} CLI_Command_t;


static bool CLI_ArgToInt(const char *Arg, int *Value)
{
	char *end;
	long l = strtol(Arg, &end, 0);		
	*Value = l;
	return 1;
}

int CLI_CommandCv(int argc, const char *argv[])
{
	if (argc != 2)
		return -1;
		
	int CvId, CvValue;
	if (CLI_ArgToInt(argv[1], &CvId) && CLI_ArgToInt(argv[2], &CvValue))
	{
		Debug("Setting CV %u to %u\n", CvId, CvValue);
		DCC_DirectWriteByte(CvId, CvValue);
		return 0;
	}	
	else
		return -1;
}


int CLI_CommandSpeed(int argc, const char *argv[])
{
	if (argc != 2)
		return -1;
		
	int Loco, Speed;
	if (CLI_ArgToInt(argv[1], &Loco) && CLI_ArgToInt(argv[2], &Speed))
	{
		if (Speed < 0)
			DCC_SetLocomotiveSpeed(Loco, -Speed, 0);
		else
			DCC_SetLocomotiveSpeed(Loco, Speed, 1);
		return 0;
	}	
	else
		return -1;
}

int CLI_CommandLights(int argc, const char *argv[])
{
	if (argc != 2)
		return -1;
		
	int Loco, Lights;
	if (CLI_ArgToInt(argv[1], &Loco) && CLI_ArgToInt(argv[2], &Lights))
	{
		DCC_SetLocomotiveFunctions(Loco, Lights & 0b11111, 0);
		DCC_SetLocomotiveFunctions(Loco, (Lights >> 5) & 0b01111, 1);
		//DCC_SetLocomotiveFunctions(Loco, (Lights >> 9) & 0b01111, 2);
		return 0;
	}	
	else
		return -1;
}

int CLI_CommandControlLoco(int argc, const char *argv[])
{
	if (argc != 2)
		return -1;
		
	int Control, Addr;
	if (CLI_ArgToInt(argv[1], &Control) && CLI_ArgToInt(argv[2], &Addr))
	{
		//TRCON_SetAddress(Control, Addr);
		return 0;
	}	
	else
		return -1;
}


const CLI_Command_t CLI_CommandTable[] = 
{
	{ CLI_CommandCv, "CV", "ID/N VALUE/N", "Write CV value"	},
	{ CLI_CommandSpeed, "SP", "LOCO/N SPEED/N", "Set locomotive speed" },
	{ CLI_CommandLights,  "LI", "LOCO/N ON/B", "Turn Front/Rear lights on or off (FL4)" },
	{ CLI_CommandControlLoco,  "SCL", "CONTROL/N LOCO/R", "Set control locomotive address" },
//	{ CLI_CommandLights,  "FU", "LOCO/N FUNCTION/N", "Set function outputs" },
	{ 0, 0, 0, 0 }
};

void CLI_Init(void)
{	
	BufferInit(CLI_Buffer);
}


static void CLI_ParseLine(void)
{
	const char *argv[10] = { NULL };
	int8_t argc = -1;
	
	char *str = CLI_Buffer.Buffer;
	char *token = strtok_r(str, " ", &str);
	while (token)
	{
		argv[++argc] = token;
		token = strtok_r(str, " ", &str);
	}
	
	if (argc >= 0)
	{
		const CLI_Command_t *Command = CLI_CommandTable;
		while (Command->command)
		{
			if (strcasecmp(Command->name, argv[0]) == 0)
				break;
			Command += 1;
		}
		
		if (Command->command)
		{
			int Result = Command->command(argc, argv);
			if (Result)
				Debug_PrintF("'%s' returned %d\n", argv[0], Result);
		}
		else
			Debug_PrintF("Unknown command '%s'\n", argv[0]);
	}
	
	BufferInit(CLI_Buffer);
}
	

void CLI_InputChar(uint8_t Char)
{
	switch (Char)
	{
		case '\r':
		{
			BufferWrite(CLI_Buffer, 0);		
			Debug_PutChar('\n');
			CLI_ParseLine();
		}
		break;
		
		case 127:
		{
			if (BufferAmount(CLI_Buffer) > 0)
			{
				BufferUnWrite(CLI_Buffer);
				Debug_PutChar(127);
			}
			else
				Debug_PutChar(7);			
		}
		break;
		
		default:
		{
			if (isprint(Char) && BufferSpace(CLI_Buffer) >= 1)
			{
				Debug_PutChar(Char);
				BufferWrite(CLI_Buffer, Char);
			}		
			else
				Debug_PutChar(7);
		}
		break;	
	}
}
