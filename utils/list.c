// A "vector" style list manager.

// Care must be taken when using List_Add, data being passed *must* coinside
//		with stride used when initializing the list.

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "list.h"

bool List_Init(List_t *list, const size_t stride, const size_t count, const void *data)
{
	if(list==NULL)
		return false;

	// Require at least a stride
	if(!stride)
		return false;

	// Save stride
	list->stride=stride;

	// If initial data was specified, allocate and copy it
	if(data)
	{
		// Actual list size is stride*count
		list->size=stride*count;
		// Actual buffer size is 2x list size to help avoid reallocation stalls at the cost of more memory usage
		list->bufSize=list->size*2;

		list->buffer=(uint8_t *)Zone_Malloc(zone, list->bufSize);

		if(list->buffer==NULL)
			return false;

		memcpy(list->buffer, data, list->size);
	}
	// Otherwise, initialize the buffer to at least stride*2 for starters
	// Or if count is specified and no data, use that as a pre-allocation.
	else
	{
		list->size=0;
		list->bufSize=stride*2;

		if(count)
			list->bufSize*=count;

		list->buffer=(uint8_t *)Zone_Malloc(zone, list->bufSize);

		if(list->buffer==NULL)
			return false;
	}

	return true;
}

bool List_Add(List_t *list, void *data)
{
	if(list==NULL)
		return false;

	if(!data)
		return false;

	// Save the current size for inserting data at the end of the list
	size_t oldSize=list->size;

	// Increment list size by the item size (stride)
	list->size+=list->stride;

	// If list size is larger than the buffer, resize it
	if(list->size>=list->bufSize)
	{
		// Over allocate memory to save from having to resize later
		list->bufSize=list->size*2;

		// Reallocate the buffer
		uint8_t *ptr=(uint8_t *)Zone_Realloc(zone, list->buffer, list->bufSize);

		if(ptr==NULL)
			return false;

		list->buffer=ptr;
	}

	// Copy the data into the new memory
	memcpy(&list->buffer[oldSize], (uint8_t *)data, list->stride);

	return true;
}

bool List_Del(List_t *list, const size_t index)
{
	if(list==NULL)
		return false;

	// Check buffer bounds
	if((index*list->stride)>=list->size)
		return false;

	// Shift data from index to end, overwriting the item to be removed
	memcpy(&list->buffer[index*list->stride], &list->buffer[(index+1)*list->stride], list->size-(index*list->stride));
	// Update list size
	list->size-=list->stride;

	return true;
}

void *List_GetPointer(List_t *list, const size_t index)
{
	if(list==NULL)
		return NULL;

	// Check buffer bounds
	if((index*list->stride)>=list->size)
		return NULL;

	// Return pointer based on index and stride
	return (void *)&list->buffer[list->stride*index];
}

void List_GetCopy(List_t *list, const size_t index, void *data)
{
	if(list==NULL)
		return;

	// Check buffer bounds
	if((index*list->stride)>=list->size)
		return;

	// Copy data based on index and stride
	if(data)
		memcpy(data, &list->buffer[list->stride*index], list->stride);
}

size_t List_GetCount(List_t *list)
{
	// count=size/stride
	if(list&&list->size)
		return list->size/list->stride;

	return 0;
}

void *List_GetBufferPointer(List_t *list)
{
	if(list)
		return (void *)list->buffer;

	return NULL;
}

bool List_ShrinkFit(List_t *list)
{
	if(list==NULL)
		return false;

	uint8_t *temp=(uint8_t *)Zone_Realloc(zone, list->buffer, list->size);

	if(temp==NULL)
		return false;

	list->buffer=temp;
	list->bufSize=list->size;

	return true;
}

void List_Clear(List_t *list)
{
	if(list==NULL)
		return;

	// Clearing list just zeros size, doesn't free memory
	list->size=0;
}

void List_Destroy(List_t *list)
{
	if(list==NULL)
		return;

	// Free memory and zero list structure
	Zone_Free(zone, list->buffer);
    memset(list, 0, sizeof(List_t));
}
