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

MemZone_t *Zone_Init(size_t Size)
{
	// Allocate all the needed memory into the Zone structure pointer.
	MemZone_t *Zone=(MemZone_t *)malloc(Size+sizeof(MemZone_t));

	if(Zone==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Zone_Init: Unable to allocate memory for zone.\n");
		return NULL;
	}

	// Set the memory pointer to the allocations to just off the end of Zone's structure.
	Zone->Memory=(uint8_t *)Zone+sizeof(MemZone_t);

	// Set up initial free block header.
	size_t *Header=Zone->Memory;
	*Header=FREE_MASK|(Size&SIZE_MASK);

	Zone->Allocations=1;
	Zone->Size=Size;

	// Create a mutex for thread safety
	if(pthread_mutex_init(&Zone->Mutex, NULL))
	{
		DBGPRINTF("Zone_Init: Unable to create mutex.\n");
		return false;
	}

#ifdef _DEBUG
	DBGPRINTF(DEBUG_INFO, "Zone_Init: Allocated at %p, size: %0.3fMB\n", Zone, (float)Size/1000.0f/1000.0f);
#endif

	return Zone;
}

void Zone_Destroy(MemZone_t *Zone)
{
	if(Zone)
		free(Zone);
}

void *Zone_Malloc(MemZone_t *Zone, size_t Size)
{
	Size+=sizeof(size_t);	// Block header
	Size=(Size+7)&~7;		// Align to 8-byte boundary

	if(Size==sizeof(size_t))
	{
#ifdef _DEBUG
		DBGPRINTF(DEBUG_WARNING, "Zone_Malloc: Attempted to allocate 0 bytes\n");
#endif
		return NULL;
	}

	pthread_mutex_lock(&Zone->Mutex);

	// Search for free blocks
	size_t *Block=Zone->Memory;

	for(size_t i=0;i<Zone->Allocations;i++)
	{
		bool isFree=((*Block&FREE_MASK)>>FREE_SHIFT)==true;
		size_t blockSize=*Block&SIZE_MASK;

		// Is block free and large enough?
		if(isFree&&Size<=blockSize)
		{
			size_t newFreeSize=blockSize-Size;

			// Set the new location and header for free block, only if we haven't consumed entire block
			if(Size<blockSize&&newFreeSize>sizeof(size_t))
			{
				size_t *newBlock=(size_t *)((uint8_t *)Block+Size);
				*newBlock=FREE_MASK|(newFreeSize&SIZE_MASK);

				Zone->Allocations++;
			}
			else
				Size+=newFreeSize; // Consume the remainder of the block

			// Set the new block size
			*Block=Size&SIZE_MASK;

			pthread_mutex_unlock(&Zone->Mutex);

#ifdef _DEBUG
			DBGPRINTF(DEBUG_WARNING, "Zone_Malloc: Allocated block, location: %p, size: %0.3fKB\n", Block, (float)(Size-sizeof(size_t))/1000.0f);
#endif
			return (void *)((uint8_t *)Block+sizeof(size_t));
		}

		Block=(size_t *)((uint8_t *)Block+blockSize);
	}

	pthread_mutex_unlock(&Zone->Mutex);

	DBGPRINTF(DEBUG_ERROR, "Zone_Malloc: Unable locate large enough free block (%0.3fKB).\n", (float)(Size-sizeof(size_t))/1000.0f);
	return NULL;
}

void *Zone_Calloc(MemZone_t *Zone, size_t Size, size_t Count)
{
	void *ptr=Zone_Malloc(Zone, Size*Count);

	if(ptr)
		memset(ptr, 0, Size*Count);

	return ptr;
}

void *Zone_Realloc(MemZone_t *Zone, void *Ptr, size_t Size)
{
	size_t *Block=(size_t *)((uint8_t *)Ptr-sizeof(size_t));
	size_t currentSize=*Block&SIZE_MASK;

	Size+=sizeof(size_t);	// Block header
	Size=(Size+7)&~7;		// Align to 8-byte boundary

	if(Size==sizeof(size_t))
	{
		// Attempting to reallocate to 0 size (frees pointer)
		Zone_Free(Zone, Ptr);
		return NULL;
	}
	else if(!Ptr)
	{
		// NULL pointer, just allocate a new block.
		return Zone_Malloc(Zone, Size);
	}
	else if(Size<=currentSize)
	{
		// Shrinking Block:

		// If the sizes are equal, just bail.
		if(Size==currentSize)
		{
#ifdef _DEBUG
			DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Location: %p, new size (%0.3fKB) == old size (%0.3fKB).\n", Ptr, (float)Size/1000.0f, (float)currentSize/1000.0f);
#endif
			return Ptr;
		}

		pthread_mutex_lock(&Zone->Mutex);

		// Otherwise, if there is a free block after, expand that block otherwise, create a new free block if possible.
#ifdef _DEBUG
		DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Location: %p, new size (%0.3fKB) < old size (%0.3fKB).\n", Ptr, (float)Size/1000.0f, (float)currentSize/1000.0f);
#endif
		size_t *nextBlock=(size_t *)((uint8_t *)Block+currentSize);
		size_t nextSize=*nextBlock&SIZE_MASK;
		bool nextIsFree=((*nextBlock&FREE_MASK)>>FREE_SHIFT)==true;

		// Check if nextBlock is a free block.
		if(nextBlock&&nextIsFree)
		{
			// Calculate the total new free size.
			size_t freeSize=currentSize-Size+nextSize;

			// Resize the Block's size to the new size.
			*Block=Size&SIZE_MASK;

			// Create a new block at the end of Block's new size.
			size_t *newBlock=(size_t *)((uint8_t *)Block+Size);
			*newBlock=FREE_MASK|(freeSize&SIZE_MASK);

#ifdef _DEBUG
			DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Free block merge.\n");
#endif
		}
		else
		{
			// If there isn't a free nextBlock, insert a free block in the size of the remaining size.
			// But only if the new size is enough to also fit a block header in it's place.
			size_t freeSize=currentSize-Size;

			if(freeSize>sizeof(size_t))
			{
				// Resize the original block's size to the new size.
				*Block=Size&SIZE_MASK;

				// Create a newBlock at the end of Block's new size.
				size_t *newBlock=(size_t *)((uint8_t *)Block+Size);
				*newBlock=FREE_MASK|(freeSize&SIZE_MASK);

				Zone->Allocations++;

#ifdef _DEBUG
				DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Free block insert.\n");
#endif
			}
		}

		pthread_mutex_unlock(&Zone->Mutex);

		// Return the adjusted block's address after adjusting free blocks.
		return Ptr;
	}
	else
	{
		// Enlarging block:

		size_t *nextBlock=(size_t *)((uint8_t *)Block+currentSize);
		size_t nextSize=*nextBlock&SIZE_MASK;
		bool nextIsFree=((*nextBlock&FREE_MASK)>>FREE_SHIFT)==true;

		// If there is an adjacent free block.
		if(nextBlock&&nextIsFree)
		{
			// Calculate total size of this block and the free block.
			size_t totalSize=currentSize+nextSize;

			// If it is large enough to expand this block into.
			if(Size<=totalSize)
			{
				size_t freeSize=totalSize-Size;

				pthread_mutex_lock(&Zone->Mutex);

				// Free size must be larger than the header in order to split a free block,
				//     otherwise consume the whole block.
				if(freeSize>sizeof(size_t))
				{
					size_t *newBlock=(size_t *)((uint8_t *)Block+Size);
					*newBlock=FREE_MASK|(freeSize&SIZE_MASK);

					*Block=Size&SIZE_MASK;

#ifdef _DEBUG
					DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Enlarging block (%p) into adjacent free block.\n", Ptr);
#endif
				}
				else
				{
					*Block=totalSize&SIZE_MASK;

#ifdef _DEBUG
					DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Enlarging block (%p), consuming free block.\n", Ptr);
#endif
				}

				pthread_mutex_unlock(&Zone->Mutex);

				return Ptr;
			}
		}

		// If there isn't a a free block to use, just allocate a new block and copy original data.
		void *New=Zone_Malloc(Zone, Size);

		if(New)
		{
			memcpy(New, Ptr, currentSize);
			Zone_Free(Zone, Ptr);
		}

#ifdef _DEBUG
		DBGPRINTF(DEBUG_WARNING, "Zone_Realloc: Allocating new block (%p) and copying.\n", New);
#endif

		return New;
	}
}

void Zone_Free(MemZone_t *Zone, void *Ptr)
{
	if(Ptr==NULL)
	{
#ifdef _DEBUG
		DBGPRINTF(DEBUG_ERROR, "Zone_Free: Attempted to free NULL pointer.\n");
#endif
		return;
	}

	size_t *Block=(size_t *)((uint8_t *)Ptr-sizeof(size_t));

	bool isFree=((*Block&FREE_MASK)>>FREE_SHIFT)==true;
	size_t blockSize=*Block&SIZE_MASK;

	if(isFree)
	{
#ifdef _DEBUG
		DBGPRINTF(DEBUG_ERROR, "Zone_Free: Attempted to free already freed pointer.\n");
#endif
		return;
	}

#ifdef _DEBUG
	DBGPRINTF(DEBUG_WARNING, "Zone_Free: Freed block, location: %p, size: %0.3fKB\n", Block, (float)(blockSize-sizeof(size_t))/1000.0f);
#endif

	pthread_mutex_lock(&Zone->Mutex);

	// Freeing a block is as simple as setting it to free.
	*Block|=FREE_MASK;

	// Check previous and next blocks to see if either are free blocks,
	//     if they are, merge them all into one large free block.
	size_t *nextBlock=(size_t *)((uint8_t *)Block+blockSize);
	bool isNextFree=((*nextBlock&FREE_MASK)>>FREE_SHIFT)==true;

	// If next is valid and free, enlarge Block by nextBlock's size
	//     and decrement allocations (removes next).
	if(nextBlock&&isNextFree)
	{
		blockSize+=*nextBlock&SIZE_MASK;
		*Block=FREE_MASK|(blockSize&SIZE_MASK);

		Zone->Allocations--;
	}

	// Search for the previous block.
	//
	// TODO: Change this to use "previous pointers" or offsets to the previous block,
	//     so this doesn't have to iterate over the list. It's not exactly slow, but can be avoided.
	size_t *prevBlock=NULL;
	size_t *currentBlock=Zone->Memory;

	for(size_t i=0;i<Zone->Allocations;i++)
	{
		size_t currentSize=*currentBlock&SIZE_MASK;

		if(currentBlock==Block)
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

			Zone->Allocations--;
		}
	}

	pthread_mutex_unlock(&Zone->Mutex);
}

// Iterate over the allocations and print out some stats.
void Zone_Print(MemZone_t *Zone)
{
	DBGPRINTF(DEBUG_WARNING, "Zone size: %0.2fMB  Location: 0x%p\n", (float)(Zone->Size/1000.0f/1000.0f), Zone);

	size_t *Block=Zone->Memory;

	for(size_t i=0;i<Zone->Allocations;i++)
	{
		bool isFree=((*Block&FREE_MASK)>>FREE_SHIFT)==true;
		size_t blockSize=*Block&SIZE_MASK;

		DBGPRINTF(DEBUG_WARNING, "\tBlock: %p, Address: %p, Size: %0.3fKB, Free: %s\n", Block, (uint8_t *)Block+sizeof(size_t), (float)(blockSize-sizeof(size_t))/1000.0f, isFree?"yes":"no");

		Block=(size_t *)((uint8_t *)Block+blockSize);
	}
}