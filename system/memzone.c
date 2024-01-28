#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "../system/system.h"
#include "memzone.h"

#define SIZE_MASK (((size_t)-1)>>1)
#define SIZE_SHIFT (0)

#define FREE_MASK (~SIZE_MASK)
#define FREE_SHIFT (sizeof(size_t)*8-1)

MemZone_t *Zone_Init(size_t size)
{
	// Allocate all the needed memory into the Zone structure pointer.
	MemZone_t *zone=(MemZone_t *)malloc(size+sizeof(MemZone_t));

	if(zone==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Zone_Init: Unable to allocate memory for zone.\n");
		return NULL;
	}

	// Set the memory pointer to the allocations to just off the end of Zone's structure.
	zone->memory=(uint8_t *)zone+sizeof(MemZone_t);

	// Set up initial free block header.
	size_t *header=zone->memory;
	*header=FREE_MASK|(size&SIZE_MASK);

	zone->allocations=1;
	zone->size=size;

	// Create a mutex for thread safety
	if(mtx_init(&zone->mutex, mtx_plain))
	{
		DBGPRINTF(DEBUG_ERROR, "Zone_Init: Unable to create mutex.\n");
		return false;
	}

#ifdef _DEBUG
	DBGPRINTF(DEBUG_INFO, "Zone_Init: Allocated at %p, size: %0.3fMB\n", zone, (float)size/1000.0f/1000.0f);
#endif

	return zone;
}

void Zone_Destroy(MemZone_t *zone)
{
	if(zone)
		free(zone);
}

void *Zone_Malloc(MemZone_t *zone, size_t size)
{
	size+=sizeof(size_t);	// Block header
	size=(size+7)&~7;		// Align to 8-byte boundary

	if(size==sizeof(size_t))
	{
#ifdef _DEBUG
		DBGPRINTF(DEBUG_WARNING, "Zone_Malloc: Attempted to allocate 0 bytes\n");
#endif
		return NULL;
	}

	mtx_lock(&zone->mutex);

	// Search for free blocks
	size_t *block=zone->memory;

	for(size_t i=0;i<zone->allocations;i++)
	{
		bool isFree=((*block&FREE_MASK)>>FREE_SHIFT)==true;
		size_t blockSize=*block&SIZE_MASK;

		// Is block free and large enough?
		if(isFree&&size<=blockSize)
		{
			size_t newFreeSize=blockSize-size;

			// Set the new location and header for free block, only if we haven't consumed entire block
			if(size<blockSize&&newFreeSize>sizeof(size_t))
			{
				size_t *newBlock=(size_t *)((uint8_t *)block+size);
				*newBlock=FREE_MASK|(newFreeSize&SIZE_MASK);

				zone->allocations++;
			}
			else
				size+=newFreeSize; // Consume the remainder of the block

			// Set the new block size
			*block=size&SIZE_MASK;

			mtx_unlock(&zone->mutex);

#ifdef _DEBUG
			DBGPRINTF(DEBUG_WARNING, "Zone_Malloc: Allocated block, location: %p, size: %0.3fKB\n", block, (float)(size-sizeof(size_t))/1000.0f);
#endif
			return (void *)((uint8_t *)block+sizeof(size_t));
		}

		block=(size_t *)((uint8_t *)block+blockSize);
	}

	mtx_unlock(&zone->mutex);

	DBGPRINTF(DEBUG_ERROR, "Zone_Malloc: Unable locate large enough free block (%0.3fKB).\n", (float)(size-sizeof(size_t))/1000.0f);
	return NULL;
}

void *Zone_Calloc(MemZone_t *zone, size_t size, size_t count)
{
	void *ptr=Zone_Malloc(zone, size*count);

	if(ptr)
		memset(ptr, 0, size*count);

	return ptr;
}

void *Zone_Realloc(MemZone_t *zone, void *ptr, size_t size)
{
	// Input pointer is NULL, just do an allocation
	if(!ptr)
		return Zone_Malloc(zone, size);

	size_t *block=(size_t *)((uint8_t *)ptr-sizeof(size_t));
	size_t currentSize=*block&SIZE_MASK;
	bool isFree=((*block&FREE_MASK)>>FREE_SHIFT)==true;

	// Block being reallocated shouldn't be free
	if(isFree)
	{
		DBGPRINTF(DEBUG_ERROR, "Zone_Realloc: attempted to reallocate a free block.\n");
		return NULL;
	}

	size+=sizeof(size_t);	// Block header
	size=(size+7)&~7;		// Align to 8-byte boundary

	if(size==sizeof(size_t))
	{
		// Size=0, free the block
		Zone_Free(zone, ptr);
		return NULL;
	}
	else if(size<=currentSize)
	{
		// Shrinking Block:

		// If the sizes are equal, just bail.
		if(size==currentSize)
		{
#ifdef _DEBUG
			DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Location: %p, new size (%0.3fKB) == old size (%0.3fKB).\n", ptr, (float)size/1000.0f, (float)currentSize/1000.0f);
#endif
			return ptr;
		}

		mtx_lock(&zone->mutex);

		// Otherwise, if there is a free block after, expand that block otherwise, create a new free block if possible.
#ifdef _DEBUG
		DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Location: %p, new size (%0.3fKB) < old size (%0.3fKB).\n", ptr, (float)size/1000.0f, (float)currentSize/1000.0f);
#endif
		size_t *nextBlock=(size_t *)((uint8_t *)block+currentSize);
		size_t nextSize=*nextBlock&SIZE_MASK;
		bool nextIsFree=((*nextBlock&FREE_MASK)>>FREE_SHIFT)==true;

		// Check if nextBlock is a free block.
		if(nextBlock&&nextIsFree)
		{
			// Calculate the total new free size.
			size_t freeSize=currentSize-size+nextSize;

			// Resize the Block's size to the new size.
			*block=size&SIZE_MASK;

			// Create a new block at the end of Block's new size.
			size_t *newBlock=(size_t *)((uint8_t *)block+size);
			*newBlock=FREE_MASK|(freeSize&SIZE_MASK);

#ifdef _DEBUG
			DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Free block merge.\n");
#endif
		}
		else
		{
			// If there isn't a free nextBlock, insert a free block in the size of the remaining size.
			// But only if the new size is enough to also fit a block header in it's place.
			size_t freeSize=currentSize-size;

			if(freeSize>sizeof(size_t))
			{
				// Resize the original block's size to the new size.
				*block=size&SIZE_MASK;

				// Create a newBlock at the end of Block's new size.
				size_t *newBlock=(size_t *)((uint8_t *)block+size);
				*newBlock=FREE_MASK|(freeSize&SIZE_MASK);

				zone->allocations++;

#ifdef _DEBUG
				DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Free block insert.\n");
#endif
			}
		}

		mtx_unlock(&zone->mutex);

		// Return the adjusted block's address after adjusting free blocks.
		return ptr;
	}
	else
	{
		// Enlarging block:

		size_t *nextBlock=(size_t *)((uint8_t *)block+currentSize);
		size_t nextSize=*nextBlock&SIZE_MASK;
		bool nextIsFree=((*nextBlock&FREE_MASK)>>FREE_SHIFT)==true;

		// If there is an adjacent free block.
		if(nextBlock&&nextIsFree)
		{
			// Calculate total size of this block and the free block.
			size_t totalSize=currentSize+nextSize;

			// If it is large enough to expand this block into.
			if(size<=totalSize)
			{
				size_t freeSize=totalSize-size;

				mtx_lock(&zone->mutex);

				// Free size must be larger than the header in order to split a free block,
				//     otherwise consume the whole block.
				if(freeSize>sizeof(size_t))
				{
					size_t *newBlock=(size_t *)((uint8_t *)block+size);
					*newBlock=FREE_MASK|(freeSize&SIZE_MASK);

					*block=size&SIZE_MASK;

#ifdef _DEBUG
					DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Enlarging block (%p) into adjacent free block.\n", ptr);
#endif
				}
				else
				{
					*block=totalSize&SIZE_MASK;

#ifdef _DEBUG
					DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Enlarging block (%p), consuming free block.\n", ptr);
#endif
				}

				mtx_unlock(&zone->mutex);

				return ptr;
			}
		}

		// If there isn't a a free block to use, just allocate a new block and copy original data.
		void *newBlock=Zone_Malloc(zone, size);

		if(newBlock)
		{
			memcpy(newBlock, ptr, currentSize);
			Zone_Free(zone, ptr);
		}

#ifdef _DEBUG
		DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Allocating new block (%p) and copying.\n", newBlock);
#endif

		return newBlock;
	}
}

void Zone_Free(MemZone_t *zone, void *ptr)
{
	if(ptr==NULL)
	{
#ifdef _DEBUG
		DBGPRINTF(DEBUG_ERROR, "Zone_Free: Attempted to free NULL pointer.\n");
#endif
		return;
	}

	size_t *block=(size_t *)((uint8_t *)ptr-sizeof(size_t));

	bool isFree=((*block&FREE_MASK)>>FREE_SHIFT)==true;
	size_t blockSize=*block&SIZE_MASK;

	if(isFree)
	{
#ifdef _DEBUG
		DBGPRINTF(DEBUG_ERROR, "Zone_Free: Attempted to free already freed pointer.\n");
#endif
		return;
	}

#ifdef _DEBUG
	DBGPRINTF(DEBUG_WARNING, "Zone_Free: Freed block, location: %p, size: %0.3fKB\n", block, (float)(blockSize-sizeof(size_t))/1000.0f);
#endif

	mtx_lock(&zone->mutex);

	// Freeing a block is as simple as setting it to free.
	*block|=FREE_MASK;

	// Check previous and next blocks to see if either are free blocks,
	//     if they are, merge them all into one large free block.
	size_t *nextBlock=(size_t *)((uint8_t *)block+blockSize);
	bool isNextFree=((*nextBlock&FREE_MASK)>>FREE_SHIFT)==true;

	// If next is valid and free, enlarge Block by nextBlock's size
	//     and decrement allocations (removes next).
	if(nextBlock&&isNextFree)
	{
		blockSize+=*nextBlock&SIZE_MASK;
		*block=FREE_MASK|(blockSize&SIZE_MASK);

		zone->allocations--;
	}

	// Search for the previous block.
	//
	// TODO: Change this to use "previous pointers" or offsets to the previous block,
	//     so this doesn't have to iterate over the list. It's not exactly slow, but can be avoided.
	size_t *prevBlock=NULL;
	size_t *currentBlock=zone->memory;

	for(size_t i=0;i<zone->allocations;i++)
	{
		size_t currentSize=*currentBlock&SIZE_MASK;

		if(currentBlock==block)
			break;

		prevBlock=currentBlock;
		currentBlock=(size_t *)((uint8_t *)currentBlock+currentSize);
	}

	// If prevBlock is found and is free, enlarge prevBlock by Block's size
	//     and decrement allocations (removes Block).
	// Note: Block may have been also enlarged and merged by the previous operation,
	//     thus merging all 3 blocks together into a single free block.
	if(prevBlock)
	{
		bool isPrevFree=((*prevBlock&FREE_MASK)>>FREE_SHIFT)==true;
		size_t prevSize=*prevBlock&SIZE_MASK;

		if(isPrevFree)
		{
			prevSize+=blockSize;
			*prevBlock=FREE_MASK|(prevSize&SIZE_MASK);

			zone->allocations--;
		}
	}

	mtx_unlock(&zone->mutex);
}

// Walk the blocks in the heap and verify that none go out of bounds
bool Zone_VerifyHeap(MemZone_t *zone)
{
	size_t *block=zone->memory;
	size_t *endZone=(size_t *)((uint8_t *)zone->memory+zone->size);

	for(size_t i=0;i<zone->allocations;i++)
	{
		size_t blockSize=*block&SIZE_MASK;
		size_t *nextBlock=(size_t *)((uint8_t *)block+blockSize);

		if(nextBlock>endZone)
		{
			DBGPRINTF(DEBUG_ERROR, "Zone_VerifyHeap: Corrupted heap! Block (%p>%p) went out of range.\n", nextBlock, endZone);
			return false;
		}

		block=(size_t *)((uint8_t *)block+blockSize);
	}

	return true;
}

// Iterate over the allocations and print out some stats.
void Zone_Print(MemZone_t *zone)
{
	DBGPRINTF(DEBUG_WARNING, "Zone size: %0.2fMB  Location: 0x%p\n", (float)(zone->size/1000.0f/1000.0f), zone);

	size_t *block=zone->memory;

	for(size_t i=0;i<zone->allocations;i++)
	{
		bool isFree=((*block&FREE_MASK)>>FREE_SHIFT)==true;
		size_t blockSize=*block&SIZE_MASK;

		DBGPRINTF(DEBUG_WARNING, "\tBlock: %p, Address: %p, Size: %0.3fKB, Free: %s\n", block, (uint8_t *)block+sizeof(size_t), (float)(blockSize-sizeof(size_t))/1000.0f, isFree?"yes":"no");

		block=(size_t *)((uint8_t *)block+blockSize);
	}
}
