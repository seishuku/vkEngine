// Vulkan helper functions

#include "vulkan.h"
#include <windows.h>

VkShaderModule vkuCreateShaderModule(VkDevice Device, const char *shaderFile)
{
	VkShaderModule shaderModule=VK_NULL_HANDLE;

	HANDLE hFile=CreateFile(shaderFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

	if(hFile==INVALID_HANDLE_VALUE)
		return VK_NULL_HANDLE;

	LARGE_INTEGER size;

	GetFileSizeEx(hFile, &size);

	HANDLE hMapping=CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);

	CloseHandle(hFile);

	if(!hMapping)
		return VK_NULL_HANDLE;

	const uint32_t *data=(const uint32_t *)MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);

	if(data==NULL)
		return VK_NULL_HANDLE;

	vkCreateShaderModule(Device, &(VkShaderModuleCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize=size.LowPart,
		.pCode=data,
	}, 0, &shaderModule);

	UnmapViewOfFile(data);
	CloseHandle(hMapping);

	return shaderModule;
}

uint32_t vkuMemoryTypeFromProperties(VkPhysicalDeviceMemoryProperties memory_properties, uint32_t typeBits, VkFlags requirements_mask)
{
	uint32_t i;

	// Search memtypes to find first index with those properties
	for(i=0;i<memory_properties.memoryTypeCount;i++)
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

int vkuCreateBuffer(VkDevice Device, const uint32_t *QueueFamilyIndices, VkPhysicalDeviceMemoryProperties MemoryProperties, VkBuffer *Buffer, VkDeviceMemory *Memory, uint32_t Size, VkBufferUsageFlags Flags, VkFlags RequirementsMask)
{
	VkMemoryRequirements memoryRequirements;

	vkCreateBuffer(Device, &(VkBufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size=Size,
		.usage=Flags,
		.sharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount=1,
		.pQueueFamilyIndices=QueueFamilyIndices,
	}, NULL, Buffer);

	vkGetBufferMemoryRequirements(Device, *Buffer, &memoryRequirements);

	vkAllocateMemory(Device, &(VkMemoryAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize=memoryRequirements.size,
		.memoryTypeIndex=vkuMemoryTypeFromProperties(MemoryProperties, memoryRequirements.memoryTypeBits, RequirementsMask),
	}, NULL, Memory);

	vkBindBufferMemory(Device, *Buffer, *Memory, 0);

	return 0;
}

int vkuCopyBuffer(VkDevice Device, VkQueue Queue, VkCommandPool CommandPool, VkBuffer Src, VkBuffer Dest, uint32_t Size)
{
	VkCommandBuffer copyCmd=VK_NULL_HANDLE;
	VkFence fence=VK_NULL_HANDLE;

	// Create a command buffer to submit a copy command from the staging buffer into the static vertex buffer
	vkAllocateCommandBuffers(Device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=CommandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=1,
	}, &copyCmd);
	vkBeginCommandBuffer(copyCmd, &(VkCommandBufferBeginInfo) { .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO });

	// Copy command
	vkCmdCopyBuffer(copyCmd, Src, Dest, 1, &(VkBufferCopy) { .srcOffset=0, .dstOffset=0, .size=Size });

	// End command buffer and submit
	vkEndCommandBuffer(copyCmd);
		
	vkCreateFence(Device, &(VkFenceCreateInfo) { .sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=0 }, VK_NULL_HANDLE, &fence);

	vkQueueSubmit(Queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&copyCmd,
	}, fence);

	// Wait for it to finish
	vkWaitForFences(Device, 1, &fence, VK_TRUE, UINT64_MAX);
	vkDestroyFence(Device, fence, VK_NULL_HANDLE);
	vkFreeCommandBuffers(Device, CommandPool, 1, &copyCmd);

	return 0;
}