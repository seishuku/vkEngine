#include <stdbool.h>
#include <stdint.h>
#include "../system/system.h"
#include "vulkan.h"

// Max number of device local heap slices
#define VKU_MAX_LOCAL_HEAPS 8

// 1GB for host data (staging buffers, UBOs, etc)
#define VKU_HOST_HEAP_SIZE (1024*1024*1024)

typedef struct
{
	VkuContext_t *context;

	uint32_t numDeviceLocal;
	VkDeviceSize localHeapSize;
	VkuMemZone_t deviceLocal[VKU_MAX_LOCAL_HEAPS];
	VkuMemZone_t hostLocal;
} VkuMemAllocator_t;

VkuMemAllocator_t vkHeaps;

bool vkuMemAllocator_Init(VkuContext_t *context)
{
	vkHeaps.context=context;

	if(!vkuMem_Init(context, &vkHeaps.hostLocal, context->hostMemIndex, VKU_HOST_HEAP_SIZE))
	{
		DBGPRINTF(DEBUG_ERROR, "Failed to allocate host local Vulkan memory.\n");
		return false;
	}

	DBGPRINTF(DEBUG_INFO, "Host heap size: %0.3fGB\n", (float)VKU_HOST_HEAP_SIZE/1000.0f/1000.0f/1000.0f);

	vkHeaps.localHeapSize=context->localMemSize/VKU_MAX_LOCAL_HEAPS;

	if(vkHeaps.localHeapSize>context->deviceProperties2.maxMemoryAllocationSize)
		vkHeaps.localHeapSize=context->deviceProperties2.maxMemoryAllocationSize;

	if(!vkuMem_Init(context, &vkHeaps.deviceLocal[0], context->localMemIndex, vkHeaps.localHeapSize))
	{
		DBGPRINTF(DEBUG_ERROR, "Failed to allocate device local Vulkan memory.\n");
		return false;
	}
	
	vkHeaps.numDeviceLocal++;

	DBGPRINTF(DEBUG_INFO, "Local heap size: %0.3fGB\n", (float)(vkHeaps.localHeapSize*vkHeaps.numDeviceLocal)/1000.0f/1000.0f/1000.0f);

	return true;
}

void VkuMemAllocator_Destroy(void)
{
	vkuMem_Destroy(vkHeaps.context, &vkHeaps.hostLocal);

	for(uint32_t i=0;i<vkHeaps.numDeviceLocal;i++)
		vkuMem_Destroy(vkHeaps.context, &vkHeaps.deviceLocal[i]);
}

VkuMemBlock_t *VkuMemAllocator_Malloc(VkMemoryRequirements memoryRequirements)
{
	VkuMemBlock_t *block=NULL;

	if(memoryRequirements.memoryTypeBits&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	{
		uint32_t heap=0;

		while(1)
		{
			// If the selected heap is valid
			if(vkHeaps.deviceLocal[heap].deviceMemory)
			{
				// Attempt to allocate the memory
				block=vkuMem_Malloc(&vkHeaps.deviceLocal[heap], memoryRequirements);

				// If it fails, try the next heap
				if(block==NULL)
				{
					heap++;
					continue;
				}

				break;
			}
			// Otherwise allocate a new heap and try again
			else
			{
				if(!vkuMem_Init(vkHeaps.context, &vkHeaps.deviceLocal[heap], vkHeaps.context->localMemIndex, vkHeaps.localHeapSize))
				{
					DBGPRINTF(DEBUG_ERROR, "Failed to allocate device local Vulkan memory.\n");
					return NULL;
				}
				else
				{
					vkHeaps.numDeviceLocal++;
					DBGPRINTF(DEBUG_INFO, "Added local device heap - Local heap total size: %0.3fGB\n", (float)(vkHeaps.localHeapSize*vkHeaps.numDeviceLocal)/1000.0f/1000.0f/1000.0f);
				}
			}
		}

	}
	else if(memoryRequirements.memoryTypeBits&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT|VK_MEMORY_PROPERTY_HOST_CACHED_BIT))
		block=vkuMem_Malloc(&vkHeaps.hostLocal, memoryRequirements);

	return block;
}

void VkuMemAllocator_Free(VkuMemBlock_t *block)
{
	// Search device local heaps for the block
	for(uint32_t i=0;i<vkHeaps.numDeviceLocal;i++)
	{
		VkuMemBlock_t *current=vkHeaps.deviceLocal[i].blocks;

		while(current!=NULL)
		{
			if(current==block)
			{
				vkuMem_Free(&vkHeaps.deviceLocal[i], block);
				return;
			}

			current=current->next;
		}
	}

	// Didn't find it in device local, try host local:
	vkuMem_Free(&vkHeaps.hostLocal, block);
}
