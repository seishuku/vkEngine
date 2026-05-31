// Vulkan helper functions
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "vulkan.h"

PFN_vkCmdPushDescriptorSetKHR _vkCmdPushDescriptorSetKHR=VK_NULL_HANDLE;

void PrintMemoryTypeFlags(VkMemoryPropertyFlags propertyFlags)
{
	if(propertyFlags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
	{
		fprintf(stderr, "<DEVICE LOCAL>");
		propertyFlags&=~VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	}
	if(propertyFlags&VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
	{
		fprintf(stderr, "<HOST VISIBLE>");
		propertyFlags&=~VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
	}
	if(propertyFlags&VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
	{
		fprintf(stderr, "<HOST COHERENT>");
		propertyFlags&=~VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
	}
	if(propertyFlags&VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
	{
		fprintf(stderr, "<HOST CACHED>");
		propertyFlags&=~VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
	}
	if(propertyFlags&VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
	{
		fprintf(stderr, "<LAZILY ALLOCATED>");
		propertyFlags&=~VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;
	}
	if(propertyFlags&VK_MEMORY_PROPERTY_PROTECTED_BIT)
	{
		fprintf(stderr, "<PROTECTED>");
		propertyFlags&=~VK_MEMORY_PROPERTY_PROTECTED_BIT;
	}
	if(propertyFlags&VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD)
	{
		fprintf(stderr, "<DEVICE COHERENT>");
		propertyFlags&=~VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD;
	}
	if(propertyFlags&VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD)
	{
		fprintf(stderr, "<DEVICE UNCACHED>");
		propertyFlags&=~VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD;
	}
	if(propertyFlags&VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV)
	{
		fprintf(stderr, "<RDMA CAPABLE>");
		propertyFlags&=~VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV;
	}

	if(propertyFlags)
		fprintf(stderr, "<UNKNOWN: %X>", propertyFlags);

	fprintf(stderr, "\n");
}

// Creates a Vulkan Context
VkBool32 vkuCreateContext(VkInstance instance, VkuContext_t *context, const char *extensions[], const uint32_t extensionCount)
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
#ifdef WAYLAND
	if(vkCreateWaylandSurfaceKHR(instance, &(VkWaylandSurfaceCreateInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR,
		.display=context->wlDisplay,
		.surface=context->wlSurface,
	}, NULL, &context->surface)!=VK_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "vkCreateWaylandSurfaceKHR failed.\n");
		return VK_FALSE;
	}
#else
	if(vkCreateXlibSurfaceKHR(instance, &(VkXlibSurfaceCreateInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR,
		.dpy=context->display,
		.window=context->window,
	}, VK_NULL_HANDLE, &context->surface)!=VK_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "vkCreateXlibSurfaceKHR failed.\n");
		return VK_FALSE;
	}
#endif
#elif ANDROID
	if(vkCreateAndroidSurfaceKHR(instance, &(VkAndroidSurfaceCreateInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
		.pNext=NULL,
		.flags=0,
		.window=context->window,
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

	// Get the handles to the devices
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, deviceHandles);

	// Print out the available devices
	DBGPRINTF(DEBUG_INFO, "Found devices:\n");

	for(uint32_t i=0;i<physicalDeviceCount;i++)
	{
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(deviceHandles[i], &deviceProperties);
		DBGPRINTF(DEBUG_INFO, "\t#%d: %s VendorID: 0x%0.4X ProductID: 0x%0.4X\n", i, deviceProperties.deviceName, deviceProperties.vendorID, deviceProperties.deviceID);
	}

	// Get the number of queue families for this device
	uint32_t queueFamilyCount=0;
	vkGetPhysicalDeviceQueueFamilyProperties(deviceHandles[context->deviceIndex], &queueFamilyCount, VK_NULL_HANDLE);

	// Allocate the memory for the structs 
	VkQueueFamilyProperties *queueFamilyProperties=(VkQueueFamilyProperties *)Zone_Malloc(zone, sizeof(VkQueueFamilyProperties)*queueFamilyCount);

	if(queueFamilyProperties==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to allocate memory for queue family properties.\n");
		Zone_Free(zone, deviceHandles);
		return VK_FALSE;
	}

	// Get the queue family properties
	vkGetPhysicalDeviceQueueFamilyProperties(deviceHandles[context->deviceIndex], &queueFamilyCount, queueFamilyProperties);

	// Find a queue index on a device that supports both graphics rendering and present support
	for(uint32_t i=0;i<queueFamilyCount;i++)
	{
		VkBool32 supportsPresent=VK_TRUE;

		//vkGetPhysicalDeviceSurfaceSupportKHR(deviceHandles[deviceIndex], i, context->surface, &SupportsPresent);

		if(supportsPresent&&(queueFamilyProperties[i].queueFlags&VK_QUEUE_GRAPHICS_BIT))
		{
			context->graphicsQueueIndex=i;
			break;
		}
	}

	// Find a queue index on the device that has compute support
	VkBool32 computeFound=VK_FALSE;
	for(uint32_t i=0;i<queueFamilyCount;i++)
	{
		if(queueFamilyProperties[i].queueFlags&VK_QUEUE_COMPUTE_BIT&&!(queueFamilyProperties[i].queueFlags&VK_QUEUE_GRAPHICS_BIT))
		{
			context->computeQueueIndex=i;
			computeFound=VK_TRUE;
			break;
		}
	}

	if(!computeFound)
	{
		DBGPRINTF(DEBUG_ERROR, "No dedicated compute only queue found, looking for any supported compute queue...\n");

		for(uint32_t i=0;i<queueFamilyCount;i++)
		{
			if(queueFamilyProperties[i].queueFlags&VK_QUEUE_COMPUTE_BIT)
			{
				context->computeQueueIndex=i;
				computeFound=VK_TRUE;
				break;
			}
		}

		if(!computeFound)
			DBGPRINTF(DEBUG_ERROR, "No compute queue found.\n");
	}

	// Done with queue family properties
	Zone_Free(zone, queueFamilyProperties);

	context->physicalDevice=deviceHandles[context->deviceIndex];

	// Free allocated handles
	Zone_Free(zone, deviceHandles);

	uint32_t extensionPropertyCount=0;
	vkEnumerateDeviceExtensionProperties(context->physicalDevice, VK_NULL_HANDLE, &extensionPropertyCount, VK_NULL_HANDLE);

	VkExtensionProperties *extensionProperties=(VkExtensionProperties *)Zone_Malloc(zone, sizeof(VkExtensionProperties)*extensionPropertyCount);

	if(extensionProperties==VK_NULL_HANDLE)
	{
		Zone_Free(zone, extensionProperties);
		DBGPRINTF(DEBUG_ERROR, "Failed to allocate memory for extension properties.\n");
		return VK_FALSE;
	}

	vkEnumerateDeviceExtensionProperties(context->physicalDevice, VK_NULL_HANDLE, &extensionPropertyCount, extensionProperties);

	//DBGPRINTF(DEBUG_INFO, "device extensions:\n");
	//for(uint32_t i=0;i<extensionPropertyCount;i++)
	//	DBGPRINTF(DEBUG_INFO, "\t%s\n", extensionProperties[i].extensionName);

	context->swapchainExtension=VK_FALSE;
	context->pushDescriptorExtension=VK_FALSE;
	context->dynamicRenderingExtension=VK_FALSE;
	context->getPhysicalDeviceProperties2Extension=VK_FALSE;
	context->depthStencilResolveExtension=VK_FALSE;
	context->createRenderPass2Extension=VK_FALSE;

	// Enable extensions that are supported
	const char **enabledExtensions=(const char **)Zone_Malloc(zone, sizeof(const char **)*(extensionCount+6));
	uint32_t numEnabledExtensions=0;

	for(uint32_t i=0;i<extensionPropertyCount;i++)
	{
		if(strcmp(extensionProperties[i].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)==0)
		{
			DBGPRINTF(DEBUG_INFO, VK_KHR_SWAPCHAIN_EXTENSION_NAME" extension is supported!\n");
			enabledExtensions[numEnabledExtensions++]=VK_KHR_SWAPCHAIN_EXTENSION_NAME;
			context->swapchainExtension=VK_TRUE;
			continue;
		}

		if(strcmp(extensionProperties[i].extensionName, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)==0)
		{
			if((_vkCmdPushDescriptorSetKHR=(PFN_vkCmdPushDescriptorSetKHR)vkGetInstanceProcAddr(instance, "vkCmdPushDescriptorSetKHR"))==VK_NULL_HANDLE)
				DBGPRINTF(DEBUG_ERROR, "vkGetInstanceProcAddr failed on vkCmdPushDescriptorSetKHR.\n");
			else
			{
				DBGPRINTF(DEBUG_INFO, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME" extension is supported!\n");
				enabledExtensions[numEnabledExtensions++]=VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME;
				context->pushDescriptorExtension=VK_TRUE;
				continue;
			}
		}

		if(strcmp(extensionProperties[i].extensionName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)==0)
		{
			DBGPRINTF(DEBUG_INFO, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME" extension is supported!\n");
			context->dynamicRenderingExtension=VK_TRUE;
			enabledExtensions[numEnabledExtensions++]=VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME;
			continue;
		}		

		if(strcmp(extensionProperties[i].extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)==0)
		{
			DBGPRINTF(DEBUG_INFO, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME" extension is supported!\n");
			context->getPhysicalDeviceProperties2Extension=VK_TRUE;
			enabledExtensions[numEnabledExtensions++]=VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;
			continue;
		}

		if(strcmp(extensionProperties[i].extensionName, VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME)==0)
		{
			DBGPRINTF(DEBUG_INFO, VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME" extension is supported!\n");
			context->depthStencilResolveExtension=VK_TRUE;
			enabledExtensions[numEnabledExtensions++]=VK_KHR_DEPTH_STENCIL_RESOLVE_EXTENSION_NAME;
			continue;
		}

		if(strcmp(extensionProperties[i].extensionName, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME)==0)
		{
			DBGPRINTF(DEBUG_INFO, VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME" extension is supported!\n");
			context->createRenderPass2Extension=VK_TRUE;
			enabledExtensions[numEnabledExtensions++]=VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME;
			continue;
		}

		for(uint32_t j=0;j<extensionCount;j++)
		{
			if(strcmp(extensionProperties[i].extensionName, extensions[j])==0)
			{
				DBGPRINTF(DEBUG_INFO, "%s extension is supported!\n", extensions[j]);
				enabledExtensions[numEnabledExtensions++]=extensions[j];
				break;
			}
		}
	}

	// Report missing external extensions
	for(uint32_t i=0;i<extensionCount;i++)
	{
		VkBool32 found=VK_FALSE;
		for(uint32_t j=0;j<numEnabledExtensions;j++)
		{
			if(strcmp(enabledExtensions[j], extensions[i])==0)
			{
				found=VK_TRUE;
				break;
			}
		}

		if(!found)
			DBGPRINTF(DEBUG_WARNING, "%s extension not supported!\n", extensions[i]);
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

	DBGPRINTF(DEBUG_INFO, "Vulkan memory information: \n");

	for(uint32_t i=0;i<context->deviceMemProperties.memoryTypeCount;i++)
	{
		VkDeviceSize heapSize=context->deviceMemProperties.memoryHeaps[context->deviceMemProperties.memoryTypes[i].heapIndex].size;
		VkMemoryPropertyFlags flags=context->deviceMemProperties.memoryTypes[i].propertyFlags;

		if(flags&VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
		{
			DBGPRINTF(DEBUG_INFO, "\t#%d: Heap index: %d\tSize: %0.3fGB (device local only)\n", i, context->deviceMemProperties.memoryTypes[i].heapIndex, (float)heapSize/1000.0f/1000.0f/1000.0f);
			context->localMemSize=heapSize;
			context->localMemIndex=i;
			break;
		}
	}

	for(uint32_t i=0;i<context->deviceMemProperties.memoryTypeCount;i++)
	{
		VkDeviceSize heapSize=context->deviceMemProperties.memoryHeaps[context->deviceMemProperties.memoryTypes[i].heapIndex].size;
		VkMemoryPropertyFlags flags=context->deviceMemProperties.memoryTypes[i].propertyFlags;

		if(flags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT|VK_MEMORY_PROPERTY_HOST_CACHED_BIT))
		{
			DBGPRINTF(DEBUG_INFO, "\t#%d: Heap index: %d\tSize: %0.3fGB (host visible/coherent/cached)\n", i, context->deviceMemProperties.memoryTypes[i].heapIndex, (float)heapSize/1000.0f/1000.0f/1000.0f);
			context->hostMemSize=heapSize;
			context->hostMemIndex=i;
			break;
		}
	}

	VkPhysicalDeviceFeatures features;
	vkGetPhysicalDeviceFeatures(context->physicalDevice, &features);

	if(!features.imageCubeArray)
	{
		DBGPRINTF(DEBUG_ERROR, "Missing cubemap arrays feature.\n");
		return VK_FALSE;
	}

	features.robustBufferAccess=false;

	void *pNext=VK_NULL_HANDLE;
	VkPhysicalDeviceDynamicRenderingFeatures deviceDynamicRenderingFeatures={ 0 };

	if(context->dynamicRenderingExtension&&
	   context->getPhysicalDeviceProperties2Extension&&
	   context->depthStencilResolveExtension&&
	   context->createRenderPass2Extension)
	{
		deviceDynamicRenderingFeatures=(VkPhysicalDeviceDynamicRenderingFeatures)
		{
			.sType=VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
			.dynamicRendering=VK_TRUE,
		};
		pNext=&deviceDynamicRenderingFeatures;
	}

	// Create the logical device from the physical device and queue index from above
	VkResult result=VK_SUCCESS;
	if((result=vkCreateDevice(context->physicalDevice, &(VkDeviceCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.pNext=pNext,
		.pEnabledFeatures=&features,
		.enabledExtensionCount=numEnabledExtensions,
		.ppEnabledExtensionNames=enabledExtensions,
		.queueCreateInfoCount=2,
		.pQueueCreateInfos=(VkDeviceQueueCreateInfo [])
		{
			{
				.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex=context->graphicsQueueIndex,
				.queueCount=1,
				.pQueuePriorities=(const float[]) { 1.0f }
			},
			{
				.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex=context->computeQueueIndex,
				.queueCount=1,
				.pQueuePriorities=(const float[]) { 1.0f }
			}
		}
	}, VK_NULL_HANDLE, &context->device))!=VK_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "vkCreateDevice failed. (%d)\n", result);
		return VK_FALSE;
	}

	// Done with enabled extension list
	Zone_Free(zone, enabledExtensions);

	// Get device queues
	vkGetDeviceQueue(context->device, context->graphicsQueueIndex, 0, &context->graphicsQueue);
	vkGetDeviceQueue(context->device, context->computeQueueIndex, 0, &context->computeQueue);

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
		.queueFamilyIndex=context->graphicsQueueIndex,
	}, VK_NULL_HANDLE, &context->commandPool);

	return VK_TRUE;
}

// Destroys a Vulkan context
void vkuDestroyContext(VkInstance instance, VkuContext_t *context)
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
