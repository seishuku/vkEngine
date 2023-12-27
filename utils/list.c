#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "list.h"

#ifndef FREE
#define FREE(p) { if(p) { free(p); p=NULL; } }
#endif

bool List_Init(List_t *List, const size_t Stride, const size_t Count, const void *Data)
{
	if(List==NULL)
		return false;

	// Require at least a stride
	if(!Stride)
		return false;

	// Save stride
	List->Stride=Stride;

	// If initial data was specified, allocate and copy it
	if(Data)
	{
		// Actual list size is Stride*Count
		List->Size=Stride*Count;
		// Actual buffer size is 1.5x list size to help avoid reallocation stalls at the cost of more memory usage
		List->bufSize=List->Size*2;

		List->Buffer=(uint8_t *)Zone_Malloc(zone, List->bufSize);

		if(List->Buffer==NULL)
			return false;

		memcpy(List->Buffer, Data, List->bufSize);
	}
	// Otherwise, initalize the buffer to at least stride*2 for starters
	// Or if Count is specified and no data, use that as a pre-allocation.
	else
	{
		List->Size=0;
		if(!Count)
			List->bufSize=Stride*2;
		else
			List->bufSize=Stride*Count*2;

		List->Buffer=(uint8_t *)Zone_Malloc(zone, List->bufSize);

		if(List->Buffer==NULL)
			return false;
	}

	return true;
}

bool List_Add(List_t *List, void *Data)
{
	if(List==NULL)
		return false;

	if(!Data)
		return false;

	// Save the current size for inserting data at the end of the list
	size_t oldSize=List->Size;

	// Increment list size by the item size (stride)
	List->Size+=List->Stride;

	// If list size is larger than the buffer, resize it
	if(List->Size>=List->bufSize)
	{
		// Over allocate memory to save from having to resize later
		List->bufSize=List->Size*2;

		// Reallocate the buffer
		uint8_t *Ptr=(uint8_t *)Zone_Realloc(zone, List->Buffer, List->bufSize);

		if(Ptr==NULL)
			return false;

		List->Buffer=Ptr;
	}

	// Copy the data into the new memory
	memcpy(&List->Buffer[oldSize], Data, List->Stride);

	return true;
}

bool List_Del(List_t *List, const size_t Index)
{
	if(List==NULL)
		return false;

	// Check buffer bounds
	if((Index*List->Stride)>=List->Size)
		return false;

	// Shift data from index to end, overwriting the item to be removed
	memcpy(&List->Buffer[Index*List->Stride], &List->Buffer[(Index+1)*List->Stride], List->Size-(Index*List->Stride));
	// Update list size
	List->Size-=List->Stride;

	return true;
}

void *List_GetPointer(List_t *List, const size_t Index)
{
	if(List==NULL)
		return NULL;

	// Check buffer bounds
	if((Index*List->Stride)>=List->Size)
		return NULL;

	// Return pointer based on index and stride
	return (void *)&List->Buffer[List->Stride*Index];
}

void List_GetCopy(List_t *List, const size_t Index, void *Data)
{
	if(List==NULL)
		return;

	// Check buffer bounds
	if((Index*List->Stride)>=List->Size)
		return;

	// Copy data based on index and stride
	if(Data)
		memcpy(Data, &List->Buffer[List->Stride*Index], List->Stride);
}

size_t List_GetCount(List_t *List)
{
	// Count=size/stride
	if(List&&List->Size)
		return List->Size/List->Stride;

	return 0;
}

void *List_GetBufferPointer(List_t *List)
{
	if(List)
		return (void *)List->Buffer;

	return NULL;
}

bool List_ShrinkFit(List_t *List)
{
	if(List==NULL)
		return false;

	void *temp=Zone_Realloc(zone, List->Buffer, List->Size);

	if(temp==NULL)
		return false;

	List->Buffer=temp;
	List->bufSize=List->Size;

	return true;
}

void List_Clear(List_t *List)
{
	if(List==NULL)
		return;

	// Clearing list just zeros size, doesn't free memory
	List->Size=0;
}

void List_Destroy(List_t *List)
{
	if(List==NULL)
		return;

	// Free memory and zero list structure
	Zone_Free(zone, List->Buffer);
    memset(List, 0, sizeof(List_t));
}
