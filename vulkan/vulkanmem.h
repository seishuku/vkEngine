#ifndef __VULKANMEM_H__
#define __VULKANMEM_H__

#include "vulkan.h"

typedef struct VulkanMemBlock_s
{
	size_t Offset;
	size_t Size;
	bool Free;
	struct VulkanMemBlock_s *Next, *Prev;
} VulkanMemBlock_t;

typedef struct
{
	size_t Size;
	VulkanMemBlock_t Blocks;
	VulkanMemBlock_t *Current;
	VkDeviceMemory DeviceMemory;
} VulkanMemZone_t;

VulkanMemZone_t *VulkanMem_Init(VkuContext_t *Context, size_t Size);
void VulkanMem_Destroy(VulkanMemZone_t *VkZone);
void VulkanMem_Free(VulkanMemZone_t *VkZone, VulkanMemBlock_t *Ptr);
VulkanMemBlock_t *VulkanMem_Malloc(VulkanMemZone_t *VkZone, size_t Size);
void VulkanMem_Print(VulkanMemZone_t *VkZone);

#endif
