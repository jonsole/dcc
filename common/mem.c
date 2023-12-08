#include <sam.h>

#include "mem.h"
#include "os.h"
#include "debug.h"

extern uint8_t *_end;

#define NUM_POOLS (7)

typedef struct MEM_FreeBlock
{
	struct MEM_FreeBlock *Next;
} MEM_FreeBlock_t;

typedef struct 
{
	uint16_t Size;
	void *Start;
	uint16_t NumFreeBlocks;
	MEM_FreeBlock_t *Free;
} MEM_Pool_t;

typedef struct 
{
	uint16_t Size;
	uint16_t NumBlocks;
} MEM_PoolConfig_t;

MEM_Pool_t MEM_Pool[NUM_POOLS];

static const MEM_PoolConfig_t MEM_PoolConfig[NUM_POOLS] =
{
	{8,   40},
	{16,  20},
	{32,  20},
	{48,  20},
	{64,  20},
	{128, 10},
	{256, 5},
};

extern void Dummy_Handler(void);
void MEM_BlockClear(void *Ptr, uint32_t Size)
{
	uint32_t *Mem = (uint32_t *)Ptr;
	while (Size >= 4)
	{
		*Mem = 0x55555555;
		Mem ++;
		Size -= 4;		
	}
}

void MEM_Init(void)
{
	uint8_t *MemPtr = (uint8_t *)&_end;
#ifdef HMCRAMC0_ADDR
	uint8_t *MemEnd = (uint8_t *)HMCRAMC0_ADDR + HMCRAMC0_SIZE;
#else
	uint8_t *MemEnd = (uint8_t *)HSRAM_ADDR + HSRAM_SIZE;
#endif

	for (int Pool = 0; Pool < NUM_POOLS; Pool++)
	{
		/* Initialise pool */
		MEM_Pool[Pool].Free = NULL;
		MEM_Pool[Pool].Start = MemPtr;
		MEM_Pool[Pool].Size = MEM_PoolConfig[Pool].Size;
		MEM_Pool[Pool].NumFreeBlocks = 0;
		
		/* Add blocks to pool */
		for (int Block = 0; Block < MEM_PoolConfig[Pool].NumBlocks; Block++)
		{
			/* Check enough space left for this block */
			if (MemPtr + MEM_Pool[Pool].Size >= MemEnd)
			{
				break;
			}

			/* Create block and add it to pool's free list */
			MEM_BlockClear(MemPtr, MEM_Pool[Pool].Size);

			MEM_FreeBlock_t *Block = (MEM_FreeBlock_t *)MemPtr;
			Block->Next = MEM_Pool[Pool].Free;
			MEM_Pool[Pool].Free = Block;
			MEM_Pool[Pool].NumFreeBlocks += 1;

			/* Advance memory pointer */
			MemPtr += MEM_Pool[Pool].Size;
		}
	}
}


void *MEM_Alloc(uint16_t Size)
{
	for (int Index = 0; Index < NUM_POOLS; Index++)
	{
		if (MEM_Pool[Index].Size >= Size)
		{
			OS_InterruptDisable();
			MEM_FreeBlock_t *Mem = MEM_Pool[Index].Free;
			if (Mem)
			{
				MEM_Pool[Index].Free = Mem->Next;
				MEM_Pool[Index].NumFreeBlocks -= 1;
				OS_InterruptEnable();
				return Mem;
			}
			OS_InterruptEnable();
		}
	}
	Panic();
	return NULL;
}


void MEM_Free(const void *Mem)
{
	int Index = NUM_POOLS - 1;

	/* Find pool that block belongs to */
	while (Mem < MEM_Pool[Index].Start)
	{
		if (Index == 0)
			return;

		Index--;
	}
	
	/* Handle address that's not at start of block */
	uint16_t Offset = (Mem - MEM_Pool[Index].Start) % MEM_Pool[Index].Size;
	Mem -= Offset;
	
	MEM_FreeBlock_t *MemBlock = (MEM_FreeBlock_t *)Mem;

#if 0
	/* Check block isn't already in free list */
	MEM_FreeBlock_t *Block = MEM_Pool[Index].Free;
	while (Block != NULL)
	{
		if (Block == MemBlock)
			return;
		Block = Block->Next;
	}
#endif
	MEM_BlockClear(Mem, MEM_Pool[Index].Size);

	OS_InterruptDisable();
		
	/* Link block to start of free list */
	MemBlock->Next = MEM_Pool[Index].Free;
	MEM_Pool[Index].Free = MemBlock;
	MEM_Pool[Index].NumFreeBlocks += 1;

		
	OS_InterruptEnable();
}
