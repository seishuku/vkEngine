#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "../system/system.h"
#include "../math/math.h"
#include "vulkan.h"

bool vkuMem_Init(VkuContext_t *context, VkuMemZone_t *vkZone, uint32_t typeIndex, size_t size)
{
	// Set up create info with slab size
	VkMemoryAllocateInfo allocateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize=size,
		.memoryTypeIndex=typeIndex,
	};

	// Attempt to allocate it
	VkResult result=vkAllocateMemory(context->device, &allocateInfo, VK_NULL_HANDLE, &vkZone->deviceMemory);

	if(result!=VK_SUCCESS)
	{
#ifdef _DEBUG
		DBGPRINTF(DEBUG_ERROR, "Failed to allocate vulakn memory zone (Result=%d).\n", result);
#endif
		return false;
	}

	vkZone->blocks=(VkuMemBlock_t *)Zone_Malloc(zone, sizeof(VkuMemBlock_t));

	if(vkZone->blocks==NULL)
	{
		vkFreeMemory(context->device, vkZone->deviceMemory, VK_NULL_HANDLE);

		DBGPRINTF(DEBUG_ERROR, "Failed to allocate memory for memory block.\n");
		return false;
	}

	// Set up the initial free block
	vkZone->blocks->offset=0;
	vkZone->blocks->size=size;
	vkZone->blocks->free=true;
	vkZone->blocks->deviceMemory=vkZone->deviceMemory;
	vkZone->blocks->mappedPointer=NULL;
	vkZone->blocks->prev=NULL;
	vkZone->blocks->next=NULL;

	vkZone->size=size;

	// If this is a host memory heap, map a pointer to it
	if(context->deviceMemProperties.memoryTypes[typeIndex].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT|VK_MEMORY_PROPERTY_HOST_CACHED_BIT))
	{
		result=vkMapMemory(context->device, vkZone->deviceMemory, 0, VK_WHOLE_SIZE, 0, &vkZone->mappedPointer);

		if(result!=VK_SUCCESS)
		{
			DBGPRINTF(DEBUG_ERROR, "Failed to map vulakn device memory (Result=%d).\n", result);
			vkZone->mappedPointer=NULL;
		}

		if(vkZone->mappedPointer!=NULL)
			vkZone->blocks->mappedPointer=vkZone->mappedPointer;
	}

	DBGPRINTF(DEBUG_INFO, "Vulakn memory zone allocated (OBJ: 0x%p), size: %0.3fMB\n", vkZone->deviceMemory, (float)size/1000.0f/1000.0f);

	return true;
}

void vkuMem_Destroy(VkuContext_t *context, VkuMemZone_t *vkZone)
{
	if(vkZone->blocks)
	{
		VkuMemBlock_t *block=vkZone->blocks;

		while(block!=NULL)
		{
			Zone_Free(zone, block);
			block=block->next;
		}

		if(vkZone->mappedPointer)
			vkUnmapMemory(context->device, vkZone->deviceMemory);

		vkFreeMemory(context->device, vkZone->deviceMemory, VK_NULL_HANDLE);
	}
}

void vkuMem_Free(VkuMemZone_t *vkZone, VkuMemBlock_t *block)
{
	if(block==NULL)
	{
		DBGPRINTF(DEBUG_WARNING, "VkuMem_Free: Attempting to free NULL pointer\n");
		return;
	}

	if(block->free)
	{
		DBGPRINTF(DEBUG_WARNING, "VkuMem_Free: Attempting to free already freed pointer.\n");
		return;
	}

	block->free=true;

	if(block->prev&&block->prev->free)
	{
		VkuMemBlock_t *Last=block->prev;

		Last->size+=block->size;
		Last->next=block->next;

		if(Last->next)
			Last->next->prev=Last;

		if(block==vkZone->blocks)
			vkZone->blocks=Last;

		Zone_Free(zone, block);
		block=Last;
	}

	if(block->next&&block->next->free)
	{
		VkuMemBlock_t *next=block->next;

		block->size+=next->size;
		block->next=next->next;

		if(block->next)
			block->next->prev=block;

		if(next==vkZone->blocks)
			vkZone->blocks=block;

		Zone_Free(zone, next);
	}
}

VkuMemBlock_t *vkuMem_Malloc(VkuMemZone_t *vkZone, VkMemoryRequirements memoryRequirements)
{
	const size_t minimumBlockSize=64;

	// Does size also need this alignment?
	size_t size=(memoryRequirements.size+(memoryRequirements.alignment-1))&~(memoryRequirements.alignment-1);

	VkuMemBlock_t *baseBlock=vkZone->blocks;

	while(baseBlock!=NULL)
	{
		if(baseBlock->free&&baseBlock->size>=size)
		{
			size_t extra=baseBlock->size-size;

			if(extra>minimumBlockSize)
			{
				VkuMemBlock_t *newBlock=(VkuMemBlock_t *)Zone_Malloc(zone, sizeof(VkuMemBlock_t));
				newBlock->size=extra;
				newBlock->offset=size;
				newBlock->free=true;
				newBlock->prev=baseBlock;
				newBlock->next=baseBlock->next;
				newBlock->deviceMemory=vkZone->deviceMemory;

				if(vkZone->mappedPointer)
					newBlock->mappedPointer=(void*)((uint8_t *)vkZone->mappedPointer+newBlock->offset);

				baseBlock->next=newBlock;
				baseBlock->size=size;

				if(baseBlock->prev)
				{
					baseBlock->offset=(baseBlock->prev->size+baseBlock->prev->offset+(memoryRequirements.alignment-1))&~(memoryRequirements.alignment-1);

					if(vkZone->mappedPointer)
						baseBlock->mappedPointer=(void *)((uint8_t *)vkZone->mappedPointer+baseBlock->offset);
				}
			}

			baseBlock->free=false;

#ifdef _DEBUG
			DBGPRINTF(DEBUG_WARNING, "Vulkan mem allocate block - Location offset: %zu Size: %0.3fKB\n", baseBlock->offset, (float)baseBlock->size/1000.0f);
#endif
			return baseBlock;
		}

		baseBlock=baseBlock->next;
	}

	DBGPRINTF(DEBUG_WARNING, "Vulkan mem: Unable to find large enough free block.\n");
	return NULL;
}

void vkuMem_Print(VkuMemZone_t *vkZone)
{
	DBGPRINTF(DEBUG_WARNING, "Vulkan zone size: %0.2fMB  Location: 0x%p (Vulkan Object Address)\n", (float)(vkZone->size/1000.0f/1000.0f), vkZone->deviceMemory);

	VkuMemBlock_t *block=vkZone->blocks;

	while(block!=NULL)
	{
		DBGPRINTF(DEBUG_WARNING, "\tOffset: %0.4fMB Size: %0.4fMB Block free: %s\n", (float)block->offset/1000.0f/1000.0f, (float)block->size/1000.0f/1000.0f, block->free?"yes":"no");
		block=block->next;
	}
}
