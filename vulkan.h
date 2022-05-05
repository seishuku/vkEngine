#ifndef __VULKAN_H__
#define __VULKAN_H__

#include <vulkan/vulkan.h>

VkShaderModule vkuCreateShaderModule(VkDevice Device, const char *shaderFile);
uint32_t vkuMemoryTypeFromProperties(VkPhysicalDeviceMemoryProperties memory_properties, uint32_t typeBits, VkFlags requirements_mask);
int vkuCreateImageBuffer(VkDevice Device, const uint32_t *QueueFamilyIndices, VkPhysicalDeviceMemoryProperties MemoryProperties,
	VkImageType ImageType, VkFormat Format, uint32_t MipLevels, uint32_t Layers, uint32_t Width, uint32_t Height, uint32_t Depth,
	VkImage *Image, VkDeviceMemory *Memory,  VkImageTiling Tiling, VkBufferUsageFlags Flags, VkFlags RequirementsMask, VkImageCreateFlags CreateFlags);
int vkuCreateBuffer(VkDevice Device, const uint32_t *QueueFamilyIndices, VkPhysicalDeviceMemoryProperties MemoryProperties, VkBuffer *Buffer, VkDeviceMemory *Memory, uint32_t Size, VkBufferUsageFlags Flags, VkFlags RequirementsMask);
int vkuCopyBuffer(VkDevice Device, VkQueue Queue, VkCommandPool CommandPool, VkBuffer Src, VkBuffer Dest, uint32_t Size);

#endif
