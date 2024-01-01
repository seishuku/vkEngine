// Vulkan helper functions
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "vulkan.h"

PFN_vkCmdPushDescriptorSetKHR _vkCmdPushDescriptorSetKHR=VK_NULL_HANDLE;

// Creates a Vulkan Context
VkBool32 CreateVulkanContext(VkInstance instance, VkuContext_t *context)
{
#ifdef WIN32
	if(vkCreateWin32SurfaceKHR(instance, &(VkWin32SurfaceCreateInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hinstance=GetModuleHandle(0),
		.hwnd=context->hWnd,
	}, VK_NULL_HANDLE, &context->surface)!=VK_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "vkCreateWin32SurfaceKHR failed.\n");
		return VK_FALSE;
	}
#elif LINUX
	if(vkCreateXlibSurfaceKHR(instance, &(VkXlibSurfaceCreateInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
		.dpy=context->Dpy,
		.window=context->Win,
	}, VK_NULL_HANDLE, &context->surface)!=VK_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "vkCreateXlibSurfaceKHR failed.\n");
		return VK_FALSE;
	}
#elif ANDROID
	if(vkCreateAndroidSurfaceKHR(instance, &(VkAndroidSurfaceCreateInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
		.pNext=NULL,
		.flags=0,
		.window=context->Win,
	}, VK_NULL_HANDLE, &context->surface)!=VK_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "vkCreateAndroidSurfaceKHR failed.\n");
		return VK_FALSE;
	}
#endif
	// Get the number of physical devices in the system
	uint32_t physicalDeviceCount=0;
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, VK_NULL_HANDLE);

	if(!physicalDeviceCount)
	{
		DBGPRINTF(DEBUG_ERROR, "No physical devices found.\n");
		return VK_FALSE;
	}

	// Allocate an array of handles
	VkPhysicalDevice *deviceHandles=(VkPhysicalDevice *)Zone_Malloc(zone, sizeof(VkPhysicalDevice)*physicalDeviceCount);

	if(deviceHandles==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to allocate memory for physical device handles.\n");
		return VK_FALSE;
	}

	uint32_t *queueIndices=(uint32_t *)Zone_Malloc(zone, sizeof(uint32_t)*physicalDeviceCount);

	if(queueIndices==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to allocate memory for queue indices.\n");
		return VK_FALSE;
	}

	// Get the handles to the devices
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, deviceHandles);

	DBGPRINTF(DEBUG_INFO, "Found devices:\n");

	for(uint32_t i=0;i<physicalDeviceCount;i++)
	{
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(deviceHandles[i], &deviceProperties);
		DBGPRINTF(DEBUG_INFO, "\t#%d: %s VendorID: 0x%0.4X ProductID: 0x%0.4X\n", i, deviceProperties.deviceName, deviceProperties.vendorID, deviceProperties.deviceID);

		// Get the number of queue families for this device
		uint32_t queueFamilyCount=0;
		vkGetPhysicalDeviceQueueFamilyProperties(deviceHandles[i], &queueFamilyCount, VK_NULL_HANDLE);

		// Allocate the memory for the structs 
		VkQueueFamilyProperties *queueFamilyProperties=(VkQueueFamilyProperties *)Zone_Malloc(zone, sizeof(VkQueueFamilyProperties)*queueFamilyCount);

		if(queueFamilyProperties==NULL)
		{
			DBGPRINTF(DEBUG_ERROR, "Unable to allocate memory for queue family properties.\n");
			Zone_Free(zone, deviceHandles);
			return VK_FALSE;
		}

		// Get the queue family properties
		vkGetPhysicalDeviceQueueFamilyProperties(deviceHandles[i], &queueFamilyCount, queueFamilyProperties);

		// Find a queue index on a device that supports both graphics rendering and present support
		for(uint32_t j=0;j<queueFamilyCount;j++)
		{
			VkBool32 supportsPresent=VK_TRUE;

			//vkGetPhysicalDeviceSurfaceSupportKHR(deviceHandles[i], j, context->surface, &SupportsPresent);

			if(supportsPresent&&(queueFamilyProperties[j].queueFlags&VK_QUEUE_GRAPHICS_BIT))
			{
				queueIndices[i]=j;
				break;
			}
		}

		// Done with queue family properties
		Zone_Free(zone, queueFamilyProperties);
	}

	// Select which device to use
	uint32_t deviceIndex=0;
	context->queueFamilyIndex=queueIndices[deviceIndex];
	context->physicalDevice=deviceHandles[deviceIndex];

	// Free allocated handles and queue indices
	Zone_Free(zone, deviceHandles);
	Zone_Free(zone, queueIndices);

	uint32_t extensionCount=0;
	vkEnumerateDeviceExtensionProperties(context->physicalDevice, VK_NULL_HANDLE, &extensionCount, VK_NULL_HANDLE);

	VkExtensionProperties *extensionProperties=(VkExtensionProperties *)Zone_Malloc(zone, sizeof(VkExtensionProperties)*extensionCount);

	if(extensionProperties==VK_NULL_HANDLE)
	{
		Zone_Free(zone, extensionProperties);
		DBGPRINTF(DEBUG_ERROR, "Failed to allocate memory for extension properties.\n");
		return VK_FALSE;
	}

	vkEnumerateDeviceExtensionProperties(context->physicalDevice, VK_NULL_HANDLE, &extensionCount, extensionProperties);

	//DBGPRINTF(DEBUG_INFO, "device extensions:\n");
	//for(uint32_t i=0;i<extensionCount;i++)
	//	DBGPRINTF(DEBUG_INFO, "\t%s\n", extensionProperties[i].extensionName);

	context->swapchainExtension=VK_FALSE;
	context->pushDescriptorExtension=VK_FALSE;
	context->dynamicRenderingExtension=VK_FALSE;
	context->depthStencilResolveExtension=VK_FALSE;

	for(uint32_t i=0;i<extensionCount;i++)
	{
		if(strcmp(extensionProperties[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)==0)
		{
			DBGPRINTF(DEBUG_INFO, VK_KHR_SWAPCHAIN_EXTENSION_NAME" extension is supported!\n");
			context->swapchainExtension=VK_TRUE;
		}
		else if(strcmp(extensionProperties[i].extensionName, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)==0)
		{
			if((_vkCmdPushDescriptorSetKHR=(PFN_vkCmdPushDescriptorSetKHR)vkGetInstanceProcAddr(instance, "vkCmdPushDescriptorSetKHR"))==VK_NULL_HANDLE)
				DBGPRINTF(DEBUG_ERROR, "vkGetInstanceProcAddr failed on vkCmdPushDescriptorSetKHR.\n");
			else
			{
				DBGPRINTF(DEBUG_INFO, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME" extension is supported!\n");
				context->pushDescriptorExtension=VK_TRUE;
			}
		}
		else if(strcmp(extensionProperties[i].extensionName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)==0)
		{
			DBGPRINTF(DEBUG_INFO, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME" extension is supported!\n");
			context->dynamicRenderingExtension=VK_TRUE;
		}		
		else if(strcmp(extensionProperties[i].extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)==0)
		{
			DBGPRINTF(DEBUG_INFO, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME" extension is supported!\n");
			context->getPhysicalDeviceProperties2Extension=VK_TRUE;
		}
		else if(strcmp(extensionProperties[i].extensionName, VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME)==0)
		{
			DBGPRINTF(DEBUG_INFO, VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME" extension is supported!\n");
			context->depthStencilResolveExtension=VK_TRUE;
		}
		else if(strcmp(extensionProperties[i].extensionName, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME)==0)
		{
			DBGPRINTF(DEBUG_INFO, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME" extension is supported!\n");
			context->createRenderPass2Extension=VK_TRUE;
		}
	}

	Zone_Free(zone, extensionProperties);

	if(!context->swapchainExtension)
	{
		DBGPRINTF(DEBUG_ERROR, "Missing required device extensions!\n");
		return VK_FALSE;
	}

	context->deviceProperties2.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_3_PROPERTIES;
	context->deviceProperties.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	context->deviceProperties.pNext=&context->deviceProperties2;
	vkGetPhysicalDeviceProperties2(context->physicalDevice, &context->deviceProperties);

	DBGPRINTF(DEBUG_INFO, "Vulkan device name: %s\nVulkan API version: %d.%d.%d\n",
			  context->deviceProperties.properties.deviceName,
			  VK_API_VERSION_MAJOR(context->deviceProperties.properties.apiVersion),
			  VK_API_VERSION_MINOR(context->deviceProperties.properties.apiVersion),
			  VK_API_VERSION_PATCH(context->deviceProperties.properties.apiVersion));

	// Get device physical memory properties
	vkGetPhysicalDeviceMemoryProperties(context->physicalDevice, &context->deviceMemProperties);

	DBGPRINTF(DEBUG_INFO, "Vulkan memory heaps: \n");
	for(uint32_t i=0;i<context->deviceMemProperties.memoryHeapCount;i++)
		DBGPRINTF(DEBUG_INFO, "\t#%d: Size: %0.3fGB\n", i, (float)context->deviceMemProperties.memoryHeaps[i].size/1000.0f/1000.0f/1000.0f);

	DBGPRINTF(DEBUG_INFO, "Vulkan memory types: \n");
	for(uint32_t i=0;i<context->deviceMemProperties.memoryTypeCount;i++)
		DBGPRINTF(DEBUG_INFO, "\t#%d: Heap index: %d Flags: 0x%X\n", i, context->deviceMemProperties.memoryTypes[i].heapIndex, context->deviceMemProperties.memoryTypes[i].propertyFlags);

	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceFeatures(context->physicalDevice, &features);

	if(!features.imageCubeArray)
	{
		DBGPRINTF(DEBUG_WARNING, "Missing cubemap arrays feature.\n");
		return VK_FALSE;
	}

	// Enable extensions that are supported
	const char *extensions[100];
	uint32_t numExtensions=0;

	if(context->swapchainExtension)
		extensions[numExtensions++]=VK_KHR_SWAPCHAIN_EXTENSION_NAME;

	if(context->pushDescriptorExtension)
		extensions[numExtensions++]=VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME;

	void *pNext=VK_NULL_HANDLE;
	VkPhysicalDeviceDynamicRenderingFeatures deviceDynamicRenderingFeatures={ 0 };
	if(context->dynamicRenderingExtension&&
	   context->getPhysicalDeviceProperties2Extension&&
	   context->depthStencilResolveExtension&&
	   context->createRenderPass2Extension)
	{
		extensions[numExtensions++]=VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME;

		// These are also required with dynamic rendering
		extensions[numExtensions++]=VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
		extensions[numExtensions++]=VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME;
		extensions[numExtensions++]=VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME;
		// 
	
		deviceDynamicRenderingFeatures=(VkPhysicalDeviceDynamicRenderingFeatures)
		{
			.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
			.dynamicRendering=VK_TRUE,
		};
		pNext=&deviceDynamicRenderingFeatures;
	}

#if 1
	extensions[numExtensions++]=VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME;
	extensions[numExtensions++]=VK_KHR_EXTERNAL_FENCE_EXTENSION_NAME;
	extensions[numExtensions++]=VK_KHR_EXTERNAL_SEMAPHORE_EXTENSION_NAME;
	extensions[numExtensions++]=VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME;
	extensions[numExtensions++]=VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME;
	//extensions[numExtensions++]=VK_EXT_DEBUG_MARKER_EXTENSION_NAME;

#ifdef WIN32
	extensions[numExtensions++]=VK_KHR_EXTERNAL_MEMORY_WIN32_EXTENSION_NAME;
	extensions[numExtensions++]=VK_KHR_EXTERNAL_FENCE_WIN32_EXTENSION_NAME;
	extensions[numExtensions++]=VK_KHR_EXTERNAL_SEMAPHORE_WIN32_EXTENSION_NAME;
	extensions[numExtensions++]=VK_KHR_WIN32_KEYED_MUTEX_EXTENSION_NAME;
#else
	extensions[numExtensions++]=VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME;
	extensions[numExtensions++]=VK_KHR_EXTERNAL_FENCE_FD_EXTENSION_NAME;
	extensions[numExtensions++]=VK_KHR_EXTERNAL_SEMAPHORE_FD_EXTENSION_NAME;
#endif
#endif

	// Create the logical device from the physical device and queue index from above
	if(vkCreateDevice(context->physicalDevice, &(VkDeviceCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext=pNext,
		.pEnabledFeatures=&features,
		.enabledExtensionCount=numExtensions,
		.ppEnabledExtensionNames=extensions,
		.queueCreateInfoCount=1,
		.pQueueCreateInfos=&(VkDeviceQueueCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			.queueFamilyIndex=context->queueFamilyIndex,
			.queueCount=1,
			.pQueuePriorities=(const float[]) { 1.0f }
		}
	}, VK_NULL_HANDLE, &context->device)!=VK_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "vkCreateDevice failed.\n");
		return VK_FALSE;
	}

	// Get device queue
	vkGetDeviceQueue(context->device, context->queueFamilyIndex, 0, &context->queue);

	FILE *stream=fopen("pipelinecache.bin", "rb");

	if(stream)
	{
		DBGPRINTF(DEBUG_INFO, "Reading pipeline cache data...\n");

		fseek(stream, 0, SEEK_END);
		size_t pipelineCacheSize=ftell(stream);
		fseek(stream, 0, SEEK_SET);

		uint8_t *pipelineCacheData=(uint8_t *)Zone_Malloc(zone, pipelineCacheSize);

		if(pipelineCacheData)
		{
			VkResult Result=vkCreatePipelineCache(context->device, &(VkPipelineCacheCreateInfo)
			{
				.sType=VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
				.initialDataSize=pipelineCacheSize,
				.pInitialData=pipelineCacheData,
			}, VK_NULL_HANDLE, &context->pipelineCache);

			if(Result!=VK_SUCCESS)
			{
				DBGPRINTF(DEBUG_ERROR, "Corrupted pipeline cache data, creating new. (Result=%d)\n", Result);

				vkCreatePipelineCache(context->device, &(VkPipelineCacheCreateInfo)
				{
					.sType=VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
			}, VK_NULL_HANDLE, &context->pipelineCache);
	}

			Zone_Free(zone, pipelineCacheData);
		}
		else
		{
			DBGPRINTF(DEBUG_ERROR, "Failed to allocate memory for pipeline cache data, creating new pipeline cache.\n");

			vkCreatePipelineCache(context->device, &(VkPipelineCacheCreateInfo)
			{
				.sType=VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
			}, VK_NULL_HANDLE, &context->pipelineCache);
		}
	}
	else
	{
		DBGPRINTF(DEBUG_INFO, "No pipeline cache data file found, creating new.\n");

		vkCreatePipelineCache(context->device, &(VkPipelineCacheCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
		}, VK_NULL_HANDLE, &context->pipelineCache);
	}

	// Create a general command pool
	vkCreateCommandPool(context->device, &(VkCommandPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags=0,
		.queueFamilyIndex=context->queueFamilyIndex,
	}, VK_NULL_HANDLE, &context->commandPool);

	return VK_TRUE;
}

// Destroys a Vulkan context
void DestroyVulkan(VkInstance instance, VkuContext_t *context)
{
	if(!context)
		return;

	// Destroy general command pool
	vkDestroyCommandPool(context->device, context->commandPool, VK_NULL_HANDLE);

	// Destroy pipeline cache
	vkDestroyPipelineCache(context->device, context->pipelineCache, VK_NULL_HANDLE);

	// Destroy logical device
	vkDestroyDevice(context->device, VK_NULL_HANDLE);

	// Destroy rendering surface
	vkDestroySurfaceKHR(instance, context->surface, VK_NULL_HANDLE);
}
