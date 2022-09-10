// Vulkan helper functions
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "vulkan.h"
#include "vulkan_mem.h"
#include "../image/image.h"
#include "../math/math.h"

extern VulkanMemZone_t *VkZone;

uint32_t vkuMemoryTypeFromProperties(VkPhysicalDeviceMemoryProperties memory_properties, uint32_t typeBits, VkFlags requirements_mask)
{
	// Search memtypes to find first index with those properties
	for(uint32_t i=0;i<memory_properties.memoryTypeCount;i++)
	{
		if((typeBits&1)==1)
		{
			// Type is available, does it match user properties?
			if((memory_properties.memoryTypes[i].propertyFlags&requirements_mask)==requirements_mask)
				return i;
		}

		typeBits>>=1;
	}

	// No memory types matched, return failure
	return 0;
}

VkBool32 vkuCreateImageBuffer(VkuContext_t *Context, Image_t *Image,
	VkImageType ImageType, VkFormat Format, uint32_t MipLevels, uint32_t Layers, uint32_t Width, uint32_t Height, uint32_t Depth,
	VkImageTiling Tiling, VkBufferUsageFlags Flags, VkFlags RequirementsMask, VkImageCreateFlags CreateFlags)
{
	VkImageCreateInfo ImageInfo=
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType=ImageType,
		.format=Format,
		.mipLevels=MipLevels,
		.arrayLayers=Layers,
		.samples=VK_SAMPLE_COUNT_1_BIT,
		.tiling=Tiling,
		.usage=Flags,
		.sharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
		.extent.width=Width,
		.extent.height=Height,
		.extent.depth=Depth,
		.flags=CreateFlags,
	};

	if(vkCreateImage(Context->Device, &ImageInfo, NULL, &Image->Image)!=VK_SUCCESS)
		return VK_FALSE;

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(Context->Device, Image->Image, &memoryRequirements);

	VkMemoryAllocateInfo AllocateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize=memoryRequirements.size,
		.memoryTypeIndex=vkuMemoryTypeFromProperties(Context->DeviceMemProperties, memoryRequirements.memoryTypeBits, RequirementsMask),
	};

	// Quick hack: getting it to use the vulkan memory allocator
	Image->DeviceMemory=VulkanMem_Malloc(VkZone, memoryRequirements);

	if(Image->DeviceMemory==NULL)
		return VK_FALSE;

	if(vkBindImageMemory(Context->Device, Image->Image, VkZone->DeviceMemory, Image->DeviceMemory->Offset)!=VK_SUCCESS)
		return VK_FALSE;

	return VK_TRUE;
}

VkBool32 vkuCreateBuffer(VkuContext_t *Context, VkBuffer *Buffer, VkDeviceMemory *Memory, uint32_t Size, VkBufferUsageFlags Flags, VkFlags RequirementsMask)
{
	VkMemoryRequirements memoryRequirements;

	VkBufferCreateInfo BufferInfo=
	{
		.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size=Size,
		.usage=Flags,
		.sharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount=1,
		.pQueueFamilyIndices=&Context->QueueFamilyIndex,
	};

	if(vkCreateBuffer(Context->Device, &BufferInfo, NULL, Buffer)!=VK_SUCCESS)
		return VK_FALSE;

	vkGetBufferMemoryRequirements(Context->Device, *Buffer, &memoryRequirements);

	VkMemoryAllocateInfo AllocateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize=memoryRequirements.size,
		.memoryTypeIndex=vkuMemoryTypeFromProperties(Context->DeviceMemProperties, memoryRequirements.memoryTypeBits, RequirementsMask),
	};

	if(vkAllocateMemory(Context->Device, &AllocateInfo, NULL, Memory)!=VK_SUCCESS)
		return VK_FALSE;

	if(vkBindBufferMemory(Context->Device, *Buffer, *Memory, 0)!=VK_SUCCESS)
			return VK_FALSE;

	return VK_TRUE;
}

// Copy from one buffer to another
VkBool32 vkuCopyBuffer(VkuContext_t *Context, VkBuffer Src, VkBuffer Dest, uint32_t Size)
{
	VkCommandBuffer CopyCmd=VK_NULL_HANDLE;
	VkFence Fence=VK_NULL_HANDLE;

	// Create a command buffer to submit a copy command from the staging buffer into the static vertex buffer
	vkAllocateCommandBuffers(Context->Device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=Context->CommandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=1,
	}, &CopyCmd);

	vkBeginCommandBuffer(CopyCmd, &(VkCommandBufferBeginInfo) { .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO });

	// Copy command
	vkCmdCopyBuffer(CopyCmd, Src, Dest, 1, &(VkBufferCopy) { .srcOffset=0, .dstOffset=0, .size=Size });

	// End command buffer and submit
	vkEndCommandBuffer(CopyCmd);
		
	vkCreateFence(Context->Device, &(VkFenceCreateInfo) { .sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=0 }, VK_NULL_HANDLE, &Fence);

	vkQueueSubmit(Context->Queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&CopyCmd,
	}, Fence);

	// Wait for it to finish
	vkWaitForFences(Context->Device, 1, &Fence, VK_TRUE, UINT64_MAX);
	vkDestroyFence(Context->Device, Fence, VK_NULL_HANDLE);
	vkFreeCommandBuffers(Context->Device, Context->CommandPool, 1, &CopyCmd);

	return VK_TRUE;
}
