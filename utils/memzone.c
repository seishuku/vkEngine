#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "../system/system.h"
#include "memzone.h"

static const size_t CHUNK_SIZE=8;
static const size_t HEADER_SIZE=sizeof(MemBlock_t);

MemZone_t *Zone_Init(size_t Size)
{
	MemZone_t *Zone=(MemZone_t *)malloc(Size);

	if(Zone==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to allocate memory for zone.\n");
		return false;
	}

	MemBlock_t *Block=(MemBlock_t *)((uint8_t *)Zone+sizeof(MemZone_t));
	Block->Prev=&Zone->Blocks;
	Block->Next=&Zone->Blocks;
	Block->Free=false;
	Block->Size=Size-sizeof(MemZone_t);

	Zone->Blocks.Next=Block;
	Zone->Blocks.Prev=Block;
	Zone->Blocks.Free=true;
	Zone->Blocks.Size=0;

	Zone->Current=Block;

	Zone->Size=Size;

	DBGPRINTF(DEBUG_INFO, "Zone allocated at 0x%p, size: %0.3fMB\n", Zone, (float)Size/1000.0f/1000.0f);
	return Zone;
}

void Zone_Destroy(MemZone_t *Zone)
{
	if(Zone)
		free(Zone);
}

void Zone_Free(MemZone_t *Zone, void *Ptr)
{
	if(Ptr==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Attempting to free NULL pointer\n");
		return;
	}

	MemBlock_t *Block=(MemBlock_t *)((uint8_t *)Ptr-HEADER_SIZE);

#ifdef _DEBUG
	DBGPRINTF(DEBUG_WARNING, "Zone freed block - Location: 0x%p Size: %0.3fKB\n", Ptr, (float)Block->Size/1000.0f);
#endif

	if(!Block->Free)
	{
		DBGPRINTF(DEBUG_ERROR, "Attempting to free already freed pointer.\n");
		return;
	}

	Block->Free=false;

	MemBlock_t *Last=Block->Prev;

	if(!Last->Free)
	{
		Last->Size+=Block->Size;
		Last->Next=Block->Next;
		Last->Next->Prev=Last;

		if(Block==Zone->Current)
			Zone->Current=Last;

		Block=Last;
	}

	MemBlock_t *Next=Block->Next;

	if(!Next->Free)
	{
		Block->Size+=Next->Size;
		Block->Next=Next->Next;
		Block->Next->Prev=Block;

		if(Next==Zone->Current)
			Zone->Current=Block;
	}
}

void *Zone_Malloc(MemZone_t *Zone, size_t Size)
{
	const size_t MinimumBlockSize=64;

	Size+=(HEADER_SIZE+CHUNK_SIZE-1)&~(CHUNK_SIZE-1);	// Align to chunk boundary

	MemBlock_t *Base=Zone->Current;
	MemBlock_t *Current=Zone->Current;
	MemBlock_t *Start=Base->Prev;

	do
	{
		if(Current==Start)
		{
			DBGPRINTF(DEBUG_ERROR, "Out of zone memory, unable to allocate block.\n");
			return NULL;
		}

		if(Current->Free)
		{
			Base=Current->Next;
			Current=Current->Next;
		}
		else
			Current=Current->Next;
	}
	while(Base->Free||Base->Size<Size);

	size_t Extra=Base->Size-Size;

	if(Extra>MinimumBlockSize)
	{
		MemBlock_t *New=(MemBlock_t *)((uint8_t *)Base+Size);
		New->Size=Extra;
		New->Free=false;
		New->Prev=Base;
		New->Next=Base->Next;
		New->Next->Prev=New;

		Base->Next=New;
		Base->Size=Size;
	}

	Base->Free=true;

	Zone->Current=Base->Next;

#ifdef _DEBUG
	DBGPRINTF(DEBUG_WARNING, "Zone allocate block - Location: 0x%p Size: %0.3fKB\n", (void *)((uint8_t *)Base+HEADER_SIZE), (float)Size/1000.0f);
#endif
	return (void *)((uint8_t *)Base+HEADER_SIZE);
}

void *Zone_Realloc(MemZone_t *Zone, void *Ptr, size_t Size)
{
	MemBlock_t *Block=(MemBlock_t *)((uint8_t *)Ptr-HEADER_SIZE);
	size_t PreviousSize=Block->Size-HEADER_SIZE;

	if(Size==0)
	{
		Zone_Free(Zone, Ptr);
		return NULL;
	}
	else if(!Ptr)
		return Zone_Malloc(Zone, Size);
	else if(Size<=PreviousSize)
		return Ptr;
	else
	{
		assert((Ptr)&&(Size>PreviousSize));

#ifdef _DEBUG
		DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: ");
#endif
		void *New=Zone_Malloc(Zone, Size);

		if(New)
		{
			memcpy(New, Ptr, PreviousSize);
			Zone_Free(Zone, Ptr);
		}

		return New;
	}
}

void Zone_Print(MemZone_t *Zone)
{
	DBGPRINTF(DEBUG_WARNING, "Zone size: %0.2fMB  Location: 0x%p\n", (float)(Zone->Size/1000.0f/1000.0f), Zone);

	for(MemBlock_t *Block=Zone->Blocks.Next;;Block=Block->Next)
	{
		DBGPRINTF(DEBUG_WARNING, "\tBlock: 0x%p Size: %0.2fKB Block free: %s\n", Block, (float)(Block->Size/1000.0f), Block->Free?"no":"yes");

		if(Block->Next==&Zone->Blocks)
			break;
	}
}
