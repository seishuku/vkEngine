#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include "../system/system.h"
#include "../math/math.h"
#include "vulkan.h"
#include "vulkan_mem.h"

VulkanMemZone_t *VulkanMem_Init(VkuContext_t *Context, size_t Size)
{
	VulkanMemZone_t *VkZone=(VulkanMemZone_t *)Zone_Malloc(Zone, sizeof(VulkanMemZone_t));

	if(VkZone==NULL)
	{
		DBGPRINTF("Unable to allocate memory for vulkan memory zone.\n");
		return false;
	}

	VulkanMemBlock_t *Block=(VulkanMemBlock_t *)Zone_Malloc(Zone, sizeof(VulkanMemBlock_t));
	Block->Prev=&VkZone->Blocks;
	Block->Next=&VkZone->Blocks;
	Block->Free=false;
	Block->Size=Size;
	Block->Offset=0;

	VkZone->Blocks.Next=Block;
	VkZone->Blocks.Prev=Block;
	VkZone->Blocks.Free=true;
	VkZone->Blocks.Size=0;
	VkZone->Blocks.Offset=0;

	VkZone->Current=Block;

	VkZone->Size=Size;

	VkMemoryAllocateInfo AllocateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize=Size,
		.memoryTypeIndex=vkuMemoryTypeFromProperties(Context->DeviceMemProperties, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT|VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
	};

	// Why does this only work with uncached bit memory typed heaps?
	VkResult Result=vkAllocateMemory(Context->Device, &AllocateInfo, VK_NULL_HANDLE, &VkZone->DeviceMemory);

	if(Result!=VK_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "Failed to allocate vulakn memory zone (Result=%d).\n", Result);
		return NULL;
	}

	DBGPRINTF(DEBUG_INFO, "Vulakn memory zone allocated (OBJ: 0x%p), size: %0.3fMB\n", VkZone->DeviceMemory, (float)Size/1000.0f/1000.0f);
	return VkZone;
}

void VulkanMem_Destroy(VkuContext_t *Context, VulkanMemZone_t *VkZone)
{
	if(VkZone)
	{
		for(VulkanMemBlock_t *Block=VkZone->Blocks.Next;;Block=Block->Next)
		{
			Zone_Free(Zone, Block);

			if(Block->Next==&VkZone->Blocks)
				break;
		}

		vkFreeMemory(Context->Device, VkZone->DeviceMemory, VK_NULL_HANDLE);
		Zone_Free(Zone, VkZone);
	}
}

void VulkanMem_Free(VulkanMemZone_t *VkZone, VulkanMemBlock_t *Ptr)
{
	if(Ptr==NULL)
	{
		DBGPRINTF(DEBUG_WARNING, "Attempting to free NULL pointer\n");
		return;
	}

	VulkanMemBlock_t *Block=Ptr;

	if(!Block->Free)
	{
		DBGPRINTF(DEBUG_WARNING, "Attempting to free already freed pointer.\n");
		return;
	}

	Block->Free=false;

	VulkanMemBlock_t *Last=Block->Prev;

	if(!Last->Free)
	{
		Last->Size+=Block->Size;
		Last->Next=Block->Next;
		Last->Next->Prev=Last;

		if(Block==VkZone->Current)
			VkZone->Current=Last;

		Block=Last;
	}

	VulkanMemBlock_t *Next=Block->Next;

	if(!Next->Free)
	{
		Block->Size+=Next->Size;
		Block->Next=Next->Next;
		Block->Next->Prev=Block;

		if(Next==VkZone->Current)
			VkZone->Current=Block;
	}

	Zone_Free(Zone, Next);
}

VulkanMemBlock_t *VulkanMem_Malloc(VulkanMemZone_t *VkZone, VkMemoryRequirements Requirements)
{
	const size_t MinimumBlockSize=64;

	size_t Size=Requirements.size+Requirements.alignment;
	Size=(Size+7)&~7;				// Align to 64bit boundary

	VulkanMemBlock_t *Base=VkZone->Current;
	VulkanMemBlock_t *Current=VkZone->Current;
	VulkanMemBlock_t *Start=Base->Prev;

	do
	{
		if(Current==Start)
		{
			DBGPRINTF(DEBUG_WARNING, "Vulkan mem: Unable to find large enough free block.\n");
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
		VulkanMemBlock_t *New=(VulkanMemBlock_t *)Zone_Malloc(Zone, sizeof(VulkanMemBlock_t));
		New->Size=Extra;
		New->Offset=0;
		New->Free=false;
		New->Prev=Base;
		New->Next=Base->Next;
		New->Next->Prev=New;

		Base->Next=New;
		Base->Size=Size;
		Base->Offset=(size_t)(ceilf((float)(Base->Prev->Size+Base->Prev->Offset)/Requirements.alignment)*Requirements.alignment);
	}

	Base->Free=true;

	VkZone->Current=Base->Next;

#ifdef _DEBUG
	DBGPRINTF(DEBUG_WARNING, "Vulkan mem allocate block - Location offset: %lld Size: %0.3fKB\n", Base->Offset, (float)Base->Size/1000.0f);
#endif
	return Base;
}

void VulkanMem_Print(VulkanMemZone_t *VkZone)
{
	DBGPRINTF(DEBUG_WARNING, "Vulkan zone size: %0.2fMB  Location: 0x%p (Vulkan Object Address)\n", (float)(VkZone->Size/1000.0f/1000.0f), VkZone->DeviceMemory);

	for(VulkanMemBlock_t *Block=VkZone->Blocks.Next;;Block=Block->Next)
	{
		DBGPRINTF(DEBUG_WARNING, "\tPointer: 0x%p Offset: %0.4fMB Size: %0.4fMB Block free: %s\n", Block, (float)Block->Offset/1000.0f/1000.0f, (float)Block->Size/1000.0f/1000.0f, Block->Free?"no":"yes");

		if(Block->Next==&VkZone->Blocks)
			break;
	}
}
