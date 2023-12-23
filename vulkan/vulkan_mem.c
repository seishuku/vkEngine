#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "../system/system.h"
#include "../math/math.h"
#include "vulkan.h"

VkuMemZone_t *vkuMem_Init(VkuContext_t *Context, size_t Size)
{
	VkuMemZone_t *VkZone=(VkuMemZone_t *)Zone_Malloc(Zone, sizeof(VkuMemZone_t));

	if(VkZone==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to allocate memory for vulkan memory zone.\n");
		return false;
	}

	VkuMemBlock_t *Block=(VkuMemBlock_t *)Zone_Malloc(Zone, sizeof(VkuMemBlock_t));
	Block->Offset=0;
	Block->Size=Size;
	Block->Free=true;
	Block->Prev=NULL;
	Block->Next=NULL;

	VkZone->Blocks=Block;

	VkZone->Size=Size;

	// Set up create info with slab size
	VkMemoryAllocateInfo AllocateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize=Size,
	};

	// Search for the first memory type on the local memory heap and set it
	for(uint32_t Index=0;Index<Context->DeviceMemProperties.memoryTypeCount;Index++)
	{
		if(Context->DeviceMemProperties.memoryTypes[Index].propertyFlags&(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT|VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD))
		{
			AllocateInfo.memoryTypeIndex=Index;
			break;
		}
	}

	// Attempt to allocate it
	VkResult Result=vkAllocateMemory(Context->Device, &AllocateInfo, VK_NULL_HANDLE, &VkZone->DeviceMemory);

	if(Result!=VK_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "Failed to allocate vulakn memory zone (Result=%d).\n", Result);
		return NULL;
	}

	DBGPRINTF(DEBUG_INFO, "Vulakn memory zone allocated (OBJ: 0x%p), size: %0.3fMB\n", VkZone->DeviceMemory, (float)Size/1000.0f/1000.0f);
	return VkZone;
}

void vkuMem_Destroy(VkuContext_t *Context, VkuMemZone_t *VkZone)
{
	if(VkZone)
	{
		VkuMemBlock_t *Block=VkZone->Blocks;

		while(Block!=NULL)
		{
			Zone_Free(Zone, Block);
			Block=Block->Next;
		}

		vkFreeMemory(Context->Device, VkZone->DeviceMemory, VK_NULL_HANDLE);
		Zone_Free(Zone, VkZone);
	}
}

void vkuMem_Free(VkuMemZone_t *VkZone, VkuMemBlock_t *Block)
{
	if(Block==NULL)
	{
		DBGPRINTF(DEBUG_WARNING, "VkuMem_Free: Attempting to free NULL pointer\n");
		return;
	}

	if(Block->Free)
	{
		DBGPRINTF(DEBUG_WARNING, "VkuMem_Free: Attempting to free already freed pointer.\n");
		return;
	}

	Block->Free=true;

	if(Block->Prev&&Block->Prev->Free)
	{
		VkuMemBlock_t *Last=Block->Prev;

		Last->Size+=Block->Size;
		Last->Next=Block->Next;

		if(Last->Next)
			Last->Next->Prev=Last;

		if(Block==VkZone->Blocks)
			VkZone->Blocks=Last;

		Zone_Free(Zone, Block);
		Block=Last;
	}

	if(Block->Next&&Block->Next->Free)
	{
		VkuMemBlock_t *Next=Block->Next;

		Block->Size+=Next->Size;
		Block->Next=Next->Next;

		if(Block->Next)
			Block->Next->Prev=Block;

		if(Next==VkZone->Blocks)
			VkZone->Blocks=Block;

		Zone_Free(Zone, Next);
	}
}

VkuMemBlock_t *vkuMem_Malloc(VkuMemZone_t *VkZone, VkMemoryRequirements Requirements)
{
	const size_t MinimumBlockSize=64;

	// Does size also need this alignment?
	size_t Size=(Requirements.size+(Requirements.alignment-1))&~(Requirements.alignment-1);

	VkuMemBlock_t *Base=VkZone->Blocks;

	while(Base!=NULL)
	{
		if(Base->Free&&Base->Size>=Size)
		{
			size_t Extra=Base->Size-Size;

			if(Extra>MinimumBlockSize)
			{
				VkuMemBlock_t *New=(VkuMemBlock_t *)Zone_Malloc(Zone, sizeof(VkuMemBlock_t));
				New->Size=Extra;
				New->Offset=Size;
				New->Free=true;
				New->Prev=Base;
				New->Next=Base->Next;

				Base->Next=New;
				Base->Size=Size;

				if(Base->Prev)
					Base->Offset=(Base->Prev->Size+Base->Prev->Offset+(Requirements.alignment-1))&~(Requirements.alignment-1);
			}

			Base->Free=false;

#ifdef _DEBUG
			DBGPRINTF(DEBUG_WARNING, "Vulkan mem allocate block - Location offset: %zu Size: %0.3fKB\n", Base->Offset, (float)Base->Size/1000.0f);
#endif
			return Base;
		}

		Base=Base->Next;
	}

	DBGPRINTF(DEBUG_WARNING, "Vulkan mem: Unable to find large enough free block.\n");
	return NULL;
}

void vkuMem_Print(VkuMemZone_t *VkZone)
{
	DBGPRINTF(DEBUG_WARNING, "Vulkan zone size: %0.2fMB  Location: 0x%p (Vulkan Object Address)\n", (float)(VkZone->Size/1000.0f/1000.0f), VkZone->DeviceMemory);

	VkuMemBlock_t *Block=VkZone->Blocks;

	while(Block!=NULL)
	{
		DBGPRINTF(DEBUG_WARNING, "\tOffset: %0.4fMB Size: %0.4fMB Block free: %s\n", (float)Block->Offset/1000.0f/1000.0f, (float)Block->Size/1000.0f/1000.0f, Block->Free?"yes":"no");
		Block=Block->Next;
	}
}
