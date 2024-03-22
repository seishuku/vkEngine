#include <stdint.h>
#include <stdbool.h>
#include "../system/system.h"
#include "vulkan.h"

VkBool32 VkuGetComputeContext(VkuComputeContext_t *computeContext, VkPhysicalDevice physicalDevice)
{
	if(computeContext==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "NULL compute context.\n");
		return VK_FALSE;
	}

	computeContext->physicalDevice=physicalDevice;

	// Get the number of queue families for this device
	uint32_t queueFamilyCount=0;
	vkGetPhysicalDeviceQueueFamilyProperties(computeContext->physicalDevice, &queueFamilyCount, VK_NULL_HANDLE);

	VkQueueFamilyProperties *queueFamilyProperties=(VkQueueFamilyProperties *)Zone_Malloc(zone, sizeof(VkQueueFamilyProperties)*queueFamilyCount);

	if(queueFamilyProperties==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to allocate memory for queue family properties.\n");
		return VK_FALSE;
	}

	// Get the queue family properties
	vkGetPhysicalDeviceQueueFamilyProperties(computeContext->physicalDevice, &queueFamilyCount, queueFamilyProperties);

	// Find a queue index on the device that has compute support
	VkBool32 found=VK_FALSE;
	for(uint32_t i=0;i<queueFamilyCount;i++)
	{
		if(queueFamilyProperties[i].queueFlags&VK_QUEUE_COMPUTE_BIT)
		{
			computeContext->queueFamilyIndex=i;
			found=VK_TRUE;
			break;
		}
	}

	if(!found)
	{
		DBGPRINTF(DEBUG_ERROR, "No compute only queue found.\n");
		Zone_Free(zone, queueFamilyProperties);
		return VK_FALSE;
	}

	// Done with queue family properties
	Zone_Free(zone, queueFamilyProperties);

	if(vkCreateDevice(computeContext->physicalDevice, &(VkDeviceCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext=VK_NULL_HANDLE,
		.queueCreateInfoCount=1,
		.pQueueCreateInfos=&(VkDeviceQueueCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex=computeContext->queueFamilyIndex,
			.queueCount=1,
			.pQueuePriorities=(const float[]){ 1.0f }
		}
	}, VK_NULL_HANDLE, &computeContext->device)!=VK_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "vkCreateDevice failed.\n");
		return VK_FALSE;
	}

	// Get device queue
	vkGetDeviceQueue(computeContext->device, computeContext->queueFamilyIndex, 0, &computeContext->queue);

	// Create a general command pool
	vkCreateCommandPool(computeContext->device, &(VkCommandPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex=computeContext->queueFamilyIndex,
	}, VK_NULL_HANDLE, &computeContext->commandPool);

	return VK_TRUE;
}
