// Vulkan helper functions
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "vulkan.h"

#ifdef _DEBUG
PFN_vkCreateDebugUtilsMessengerEXT _vkCreateDebugUtilsMessengerEXT=VK_NULL_HANDLE;
PFN_vkDestroyDebugUtilsMessengerEXT _vkDestroyDebugUtilsMessengerEXT=VK_NULL_HANDLE;

// Debug messenger callback function
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

// Create Vulkan Instance
VkBool32 vkuCreateInstance(VkInstance *instance, const char *extensions[], const uint32_t extensionCount)
{
	uint32_t numExtensionsProperties=0;

	if(vkEnumerateInstanceExtensionProperties(NULL, &numExtensionsProperties, NULL)!=VK_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "vkEnumerateInstanceExtensionProperties failed.\n");
		return VK_FALSE;
	}

	VkExtensionProperties *extensionProperties=(VkExtensionProperties *)Zone_Malloc(zone, sizeof(VkExtensionProperties)*numExtensionsProperties);

	if(extensionProperties==VK_NULL_HANDLE)
	{
		DBGPRINTF(DEBUG_ERROR, "Failed to allocate memory for extension properties.\n");
		return VK_FALSE;
	}

	if(vkEnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &numExtensionsProperties, extensionProperties)!=VK_SUCCESS)
	{
		Zone_Free(zone, extensionProperties);
		DBGPRINTF(DEBUG_ERROR, "vkEnumerateInstanceExtensionProperties failed.\n");
		return VK_FALSE;
	}

	VkBool32 surfaceOSExtension=VK_FALSE;
	VkBool32 surfaceExtension=VK_FALSE;
	VkBool32 debugExtension=VK_FALSE;

	const char **enabledExtensions=(const char **)Zone_Malloc(zone, sizeof(const char *)*(extensionCount+3));
	uint32_t numEnabledExtensions=0;

	for(uint32_t i=0;i<numExtensionsProperties;i++)
	{
#ifdef WIN32
		if(strcmp(extensionProperties[i].extensionName, VK_KHR_WIN32_SURFACE_EXTENSION_NAME)==0)
		{
			DBGPRINTF(DEBUG_INFO, VK_KHR_WIN32_SURFACE_EXTENSION_NAME" extension is supported!\n");
			enabledExtensions[numEnabledExtensions++]=VK_KHR_WIN32_SURFACE_EXTENSION_NAME;
			surfaceOSExtension=VK_TRUE;
		}
#elif LINUX
#ifdef WAYLAND
		if(strcmp(extensionProperties[i].extensionName, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME)==0)
		{
			DBGPRINTF(DEBUG_INFO, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME" extension is supported!\n");
			enabledExtensions[numEnabledExtensions++]=VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
			surfaceOSExtension=VK_TRUE;
			continue;
		}
#else
		if(strcmp(extensionProperties[i].extensionName, VK_KHR_XLIB_SURFACE_EXTENSION_NAME)==0)
		{
			DBGPRINTF(DEBUG_INFO, VK_KHR_XLIB_SURFACE_EXTENSION_NAME" extension is supported!\n");
			enabledExtensions[numEnabledExtensions++]=VK_KHR_XLIB_SURFACE_EXTENSION_NAME;
			surfaceOSExtension=VK_TRUE;
			continue;
		}
#endif
#elif ANDROID
		if(strcmp(extensionProperties[i].extensionName, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME)==0)
		{
			DBGPRINTF(DEBUG_INFO, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME" extension is supported!\n");
			enabledExtensions[numEnabledExtensions++]=VK_KHR_ANDROID_SURFACE_EXTENSION_NAME;
			surfaceOSExtension=VK_TRUE;
			continue;
		}
#endif
		if(strcmp(extensionProperties[i].extensionName, VK_KHR_SURFACE_EXTENSION_NAME)==0)
		{
			DBGPRINTF(DEBUG_INFO, VK_KHR_SURFACE_EXTENSION_NAME" extension is supported!\n");
			enabledExtensions[numEnabledExtensions++]=VK_KHR_SURFACE_EXTENSION_NAME;
			surfaceExtension=VK_TRUE;
			continue;
		}

		if(strcmp(extensionProperties[i].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME)==0)
		{
			DBGPRINTF(DEBUG_INFO, VK_EXT_DEBUG_UTILS_EXTENSION_NAME" extension is supported!\n");
			enabledExtensions[numEnabledExtensions++]=VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
			debugExtension=VK_TRUE;
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

	// Done with extension properties
	Zone_Free(zone, extensionProperties);

	if(!surfaceExtension||!surfaceOSExtension)
	{
		DBGPRINTF(DEBUG_ERROR, "Missing required instance surface extensions!\n");
		return VK_FALSE;
	}

	VkApplicationInfo AppInfo=
	{
		.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,
		.pApplicationName="Engine",
		.applicationVersion=VK_MAKE_VERSION(1, 0, 0),
		.pEngineName="Engine",
		.engineVersion=VK_MAKE_VERSION(1, 0, 0),
		.apiVersion=VK_API_VERSION_1_1
	};

	VkInstanceCreateInfo instanceInfo=
	{
		.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo=&AppInfo,
		.enabledExtensionCount=numEnabledExtensions,
		.ppEnabledExtensionNames=enabledExtensions,
		.ppEnabledLayerNames=(const char *[]) { "VK_LAYER_KHRONOS_validation" },
	};

	// Set to 1 to enable validation layer
	instanceInfo.enabledLayerCount=0;

	if(vkCreateInstance(&instanceInfo, 0, instance)!=VK_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "vkCreateInstance failed.\n");
		return VK_FALSE;
	}

	// Done with enabled extension list
	Zone_Free(zone, enabledExtensions);

#ifdef _DEBUG
	if(debugExtension)
	{
		if((_vkCreateDebugUtilsMessengerEXT=(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(*instance, "vkCreateDebugUtilsMessengerEXT"))==VK_NULL_HANDLE)
		{
			DBGPRINTF(DEBUG_WARNING, "vkGetInstanceProcAddr failed on vkCreateDebugUtilsMessengerEXT... Disabling DebugExtension.\n");
			debugExtension=VK_FALSE;
		}
		else if((_vkDestroyDebugUtilsMessengerEXT=(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(*instance, "vkDestroyDebugUtilsMessengerEXT"))==VK_NULL_HANDLE)
		{
			DBGPRINTF(DEBUG_WARNING, "vkGetInstanceProcAddr failed on vkDestroyDebugUtilsMessengerEXT.\n");
			debugExtension=VK_FALSE;
		}

		if(vkCreateDebugUtilsMessengerEXT(*instance, &(VkDebugUtilsMessengerCreateInfoEXT)
		{
			.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback=debugCallback
		}, VK_NULL_HANDLE, &debugMessenger)!=VK_SUCCESS)
			return false;
	}
#endif

	return VK_TRUE;
}

void vkuDestroyInstance(VkInstance instance)
{
#ifdef _DEBUG
	vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, VK_NULL_HANDLE);
#endif

	vkDestroyInstance(instance, VK_NULL_HANDLE);
}
