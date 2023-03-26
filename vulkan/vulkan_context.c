// Vulkan helper functions
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "vulkan.h"

PFN_vkCmdPushDescriptorSetKHR _vkCmdPushDescriptorSetKHR=VK_NULL_HANDLE;

// Debug messenger callback function
#ifdef _DEBUG
VkDebugUtilsMessengerEXT debugMessenger;

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
	if(messageSeverity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		DBGPRINTF(DEBUG_ERROR, "\n%s\n", pCallbackData->pMessage);
	else if(messageSeverity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		DBGPRINTF(DEBUG_WARNING, "\n%s\n", pCallbackData->pMessage);
	else if(messageSeverity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		DBGPRINTF(DEBUG_INFO, "\n%s\n", pCallbackData->pMessage);
	else
		DBGPRINTF(DEBUG_WARNING, "\n%s\n", pCallbackData->pMessage);

	return VK_FALSE;
}
#endif

// Creates a Vulkan Context
VkBool32 CreateVulkanContext(VkInstance Instance, VkuContext_t *Context)
{
#ifdef WIN32
	if(vkCreateWin32SurfaceKHR(Instance, &(VkWin32SurfaceCreateInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hinstance=GetModuleHandle(0),
		.hwnd=Context->hWnd,
	}, VK_NULL_HANDLE, &Context->Surface)!=VK_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "vkCreateWin32SurfaceKHR failed.\n");
		return VK_FALSE;
	}
#else
	if(vkCreateXlibSurfaceKHR(Instance, &(VkXlibSurfaceCreateInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
		.dpy=Context->Dpy,
		.window=Context->Win,
	}, VK_NULL_HANDLE, &Context->Surface)!=VK_SUCCESS)
	{
		DBGPRINTF("vkCreateXlibSurfaceKHR failed.\n");
		return VK_FALSE;
	}
#endif

	// Get the number of physical devices in the system
	uint32_t PhysicalDeviceCount=0;
	vkEnumeratePhysicalDevices(Instance, &PhysicalDeviceCount, VK_NULL_HANDLE);

	// Allocate an array of handles
	VkPhysicalDevice *DeviceHandles=(VkPhysicalDevice *)Zone_Malloc(Zone, sizeof(VkPhysicalDevice)*PhysicalDeviceCount);

	if(DeviceHandles==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to allocate memory for physical device handles.\n");
		return VK_FALSE;
	}

	uint32_t *QueueIndices=(uint32_t *)Zone_Malloc(Zone, sizeof(uint32_t)*PhysicalDeviceCount);

	if(QueueIndices==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to allocate memory for queue indices.\n");
		return VK_FALSE;
	}

	// Get the handles to the devices
	vkEnumeratePhysicalDevices(Instance, &PhysicalDeviceCount, DeviceHandles);

	DBGPRINTF(DEBUG_INFO, "Found devices:\n");

	for(uint32_t i=0;i<PhysicalDeviceCount;i++)
	{
		uint32_t QueueFamilyCount=0;

		VkPhysicalDeviceProperties DeviceProperties;
		vkGetPhysicalDeviceProperties(DeviceHandles[i], &DeviceProperties);
		DBGPRINTF(DEBUG_INFO, "\t#%d: %s VendorID: 0x%0.4X ProductID: 0x%0.4X\n", i, DeviceProperties.deviceName, DeviceProperties.vendorID, DeviceProperties.deviceID);

		// Get the number of queue families for this device
		vkGetPhysicalDeviceQueueFamilyProperties(DeviceHandles[i], &QueueFamilyCount, VK_NULL_HANDLE);

		// Allocate the memory for the structs 
		VkQueueFamilyProperties *QueueFamilyProperties=(VkQueueFamilyProperties *)Zone_Malloc(Zone, sizeof(VkQueueFamilyProperties)*QueueFamilyCount);

		if(QueueFamilyProperties==NULL)
		{
			DBGPRINTF(DEBUG_ERROR, "Unable to allocate memory for queue family properties.\n");
			Zone_Free(Zone, DeviceHandles);
			return VK_FALSE;
		}

		// Get the queue family properties
		vkGetPhysicalDeviceQueueFamilyProperties(DeviceHandles[i], &QueueFamilyCount, QueueFamilyProperties);

		// Find a queue index on a device that supports both graphics rendering and present support
		for(uint32_t j=0;j<QueueFamilyCount;j++)
		{
			VkBool32 SupportsPresent=VK_FALSE;

			vkGetPhysicalDeviceSurfaceSupportKHR(DeviceHandles[i], j, Context->Surface, &SupportsPresent);

			if(SupportsPresent&&(QueueFamilyProperties[j].queueFlags&VK_QUEUE_GRAPHICS_BIT))
			{
				QueueIndices[i]=j;
				break;
			}
		}

		// Done with queue family properties
		Zone_Free(Zone, QueueFamilyProperties);
	}

	// Select which device to use
	uint32_t DeviceIndex=0;
	Context->QueueFamilyIndex=QueueIndices[DeviceIndex];
	Context->PhysicalDevice=DeviceHandles[DeviceIndex];

	// Free allocated handles and queue indices
	Zone_Free(Zone, DeviceHandles);
	Zone_Free(Zone, QueueIndices);

	uint32_t ExtensionCount=0;

	vkEnumerateDeviceExtensionProperties(Context->PhysicalDevice, VK_NULL_HANDLE, &ExtensionCount, VK_NULL_HANDLE);

	VkExtensionProperties *ExtensionProperties=(VkExtensionProperties *)Zone_Malloc(Zone, sizeof(VkExtensionProperties)*ExtensionCount);

	if(ExtensionProperties==VK_NULL_HANDLE)
	{
		Zone_Free(Zone, ExtensionProperties);
		DBGPRINTF(DEBUG_ERROR, "Failed to allocate memory for extension properties.\n");
		return VK_FALSE;
	}

	vkEnumerateDeviceExtensionProperties(Context->PhysicalDevice, VK_NULL_HANDLE, &ExtensionCount, ExtensionProperties);

	VkBool32 SwapchainExtension=VK_FALSE;
	VkBool32 PushDescriptorExtension=VK_FALSE;

	for(uint32_t i=0;i<ExtensionCount;i++)
	{
		if(strcmp(ExtensionProperties[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)==0)
		{
			DBGPRINTF(DEBUG_INFO, VK_KHR_SWAPCHAIN_EXTENSION_NAME" extension is supported!\n");
			SwapchainExtension=VK_TRUE;
		}
		else if(strcmp(ExtensionProperties[i].extensionName, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)==0)
		{
			if((_vkCmdPushDescriptorSetKHR=(PFN_vkCmdPushDescriptorSetKHR)vkGetInstanceProcAddr(Instance, "vkCmdPushDescriptorSetKHR"))==VK_NULL_HANDLE)
				DBGPRINTF(DEBUG_ERROR, "vkGetInstanceProcAddr failed on vkCmdPushDescriptorSetKHR.\n");
			else
			{
				DBGPRINTF(DEBUG_INFO, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME" extension is supported!\n");
				PushDescriptorExtension=VK_TRUE;
			}
		}
	}

	Zone_Free(Zone, ExtensionProperties);

	if(!SwapchainExtension)
	{
		DBGPRINTF(DEBUG_ERROR, "Missing required device extensions!\n");
		return VK_FALSE;
	}

	Context->DeviceProperties2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;
	Context->DeviceProperties.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	Context->DeviceProperties.pNext=&Context->DeviceProperties2;
	vkGetPhysicalDeviceProperties2(Context->PhysicalDevice, &Context->DeviceProperties);

	DBGPRINTF(DEBUG_INFO, "Vulkan device name: %s\nVulkan API version: %d.%d.%d\n",
			  Context->DeviceProperties.properties.deviceName,
			  VK_API_VERSION_MAJOR(Context->DeviceProperties.properties.apiVersion),
			  VK_API_VERSION_MINOR(Context->DeviceProperties.properties.apiVersion),
			  VK_API_VERSION_PATCH(Context->DeviceProperties.properties.apiVersion));

	// Get device physical memory properties
	vkGetPhysicalDeviceMemoryProperties(Context->PhysicalDevice, &Context->DeviceMemProperties);

	DBGPRINTF(DEBUG_INFO, "Vulkan memory heaps: \n");
	for(uint32_t i=0;i<Context->DeviceMemProperties.memoryHeapCount;i++)
		DBGPRINTF(DEBUG_INFO, "\t#%d: Size: %0.3fGB\n", i, (float)Context->DeviceMemProperties.memoryHeaps[i].size/1000.0f/1000.0f/1000.0f);

	DBGPRINTF(DEBUG_INFO, "Vulkan memory types: \n");
	for(uint32_t i=0;i<Context->DeviceMemProperties.memoryTypeCount;i++)
		DBGPRINTF(DEBUG_INFO, "\t#%d: Heap index: %d Flags: 0x%X\n", i, Context->DeviceMemProperties.memoryTypes[i].heapIndex, Context->DeviceMemProperties.memoryTypes[i].propertyFlags);

	VkPhysicalDeviceFeatures Features;
	vkGetPhysicalDeviceFeatures(Context->PhysicalDevice, &Features);

	if(!Features.imageCubeArray)
	{
		DBGPRINTF(DEBUG_WARNING, "Missing cubemap arrays feature.\n");
		return VK_FALSE;
	}

	// Extensions we're going to use
	const char *Extensions[]=
	{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME,
//		VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,
	};

	// Create the logical device from the physical device and queue index from above
	if(vkCreateDevice(Context->PhysicalDevice, &(VkDeviceCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pEnabledFeatures=&Features,
		.enabledExtensionCount=sizeof(Extensions)/sizeof(void *),
		.ppEnabledExtensionNames=Extensions,
		.queueCreateInfoCount=1,
		.pQueueCreateInfos=&(VkDeviceQueueCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex=Context->QueueFamilyIndex,
			.queueCount=1,
			.pQueuePriorities=(const float[]) { 1.0f }
		}
	}, VK_NULL_HANDLE, &Context->Device)!=VK_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "vkCreateDevice failed.\n");
		return VK_FALSE;
	}

	// Get device queue
	vkGetDeviceQueue(Context->Device, Context->QueueFamilyIndex, 0, &Context->Queue);

	FILE *Stream=fopen("pipelinecache.bin", "rb");

	if(Stream)
	{
		DBGPRINTF(DEBUG_INFO, "Reading pipeline cache data...\n");

		fseek(Stream, 0, SEEK_END);
		size_t PipelineCacheSize=ftell(Stream);
		fseek(Stream, 0, SEEK_SET);

		uint8_t *PipelineCacheData=(uint8_t *)Zone_Malloc(Zone, PipelineCacheSize);

		if(PipelineCacheData)
		{
			VkResult Result=vkCreatePipelineCache(Context->Device, &(VkPipelineCacheCreateInfo)
			{
				.sType=VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
				.initialDataSize=PipelineCacheSize,
				.pInitialData=PipelineCacheData,
			}, VK_NULL_HANDLE, &Context->PipelineCache);

			if(Result!=VK_SUCCESS)
			{
				DBGPRINTF(DEBUG_ERROR, "Corrupted pipeline cache data, creating new. (Result=%d)\n", Result);

				vkCreatePipelineCache(Context->Device, &(VkPipelineCacheCreateInfo)
				{
					.sType=VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
				}, VK_NULL_HANDLE, &Context->PipelineCache);
			}

			Zone_Free(Zone, PipelineCacheData);
		}
		else
		{
			DBGPRINTF(DEBUG_ERROR, "Failed to allocate memory for pipeline cache data, creating new pipeline cache.\n");

			vkCreatePipelineCache(Context->Device, &(VkPipelineCacheCreateInfo)
			{
				.sType=VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
			}, VK_NULL_HANDLE, &Context->PipelineCache);
		}
	}
	else
	{
		DBGPRINTF(DEBUG_INFO, "No pipeline cache data file found, creating new.\n");

		vkCreatePipelineCache(Context->Device, &(VkPipelineCacheCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
		}, VK_NULL_HANDLE, &Context->PipelineCache);
	}

	// Create a general command pool
	vkCreateCommandPool(Context->Device, &(VkCommandPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex=Context->QueueFamilyIndex,
	}, VK_NULL_HANDLE, &Context->CommandPool);

#ifdef _DEBUG
	if(vkCreateDebugUtilsMessengerEXT(Instance, &(VkDebugUtilsMessengerCreateInfoEXT)
	{
		.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback=debugCallback
	}, VK_NULL_HANDLE, &debugMessenger)!=VK_SUCCESS)
		return false;
#endif

	return VK_TRUE;
}

// Destroys a Vulkan context
void DestroyVulkan(VkInstance Instance, VkuContext_t *Context)
{
	if(!Context)
		return;

	// Destroy general command pool
	vkDestroyCommandPool(Context->Device, Context->CommandPool, VK_NULL_HANDLE);

	// Destroy pipeline cache
	vkDestroyPipelineCache(Context->Device, Context->PipelineCache, VK_NULL_HANDLE);

	// Destroy logical device
	vkDestroyDevice(Context->Device, VK_NULL_HANDLE);

	// Destroy rendering surface
	vkDestroySurfaceKHR(Instance, Context->Surface, VK_NULL_HANDLE);

#ifdef _DEBUG
	vkDestroyDebugUtilsMessengerEXT(Instance, debugMessenger, VK_NULL_HANDLE);
#endif
}
