#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "../math/math.h"
#include "vr.h"

#ifndef ANDROID
#include "../vulkan/vulkan.h"

#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

PFN_xrGetVulkanInstanceExtensionsKHR xrGetVulkanInstanceExtensionsKHR=XR_NULL_HANDLE;
PFN_xrGetVulkanDeviceExtensionsKHR xrGetVulkanDeviceExtensionsKHR=XR_NULL_HANDLE;
PFN_xrGetVulkanGraphicsDeviceKHR xrGetVulkanGraphicsDeviceKHR=XR_NULL_HANDLE;
PFN_xrGetVulkanGraphicsRequirementsKHR xrGetVulkanGraphicsRequirementsKHR=XR_NULL_HANDLE;

XrInstance xrInstance=XR_NULL_HANDLE;
XrSystemId xrSystemID=XR_NULL_SYSTEM_ID;
XrSession xrSession=XR_NULL_HANDLE;
XrSpace xrRefSpace=XR_NULL_HANDLE;

typedef struct
{
	XrSwapchain Swapchain;
	XrSwapchainImageVulkanKHR Images[VKU_MAX_FRAME_COUNT];
	XrView View;
	XrCompositionLayerProjectionView projectionView;
} XruSwapchain_t;

XruSwapchain_t xrSwapchain[2];

uint32_t rtWidth;
uint32_t rtHeight;

matrix EyeProjection[2];

bool VR_SendEyeImages(VkCommandBuffer CommandBuffer, VkImage Eyes[2], VkFormat Format, uint32_t Width, uint32_t Height)
{
	XrFrameState frameState=
	{
		.type=XR_TYPE_FRAME_STATE,
		.next=XR_NULL_HANDLE
	};

	if(xrWaitFrame(xrSession, &(XrFrameWaitInfo) {.type=XR_TYPE_FRAME_WAIT_INFO, .next=XR_NULL_HANDLE }, &frameState)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: xrWaitFrame() was not successful.\n");
		return false;
	}

	if(!frameState.shouldRender)
		return false;

	XrResult Result=xrBeginFrame(xrSession, &(XrFrameBeginInfo) {.type=XR_TYPE_FRAME_BEGIN_INFO, .next=XR_NULL_HANDLE });
	if(Result!=XR_SUCCESS&&Result!=XR_SESSION_LOSS_PENDING&&Result!=XR_FRAME_DISCARDED)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: xrBeginFrame() was not successful.\n");
		return false;
	}

	for(uint32_t i=0;i<2;i++)
	{
		uint32_t imageIndex=0;
		if(xrAcquireSwapchainImage(xrSwapchain[i].Swapchain, &(XrSwapchainImageAcquireInfo) {.type=XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, .next=XR_NULL_HANDLE }, &imageIndex)!=XR_SUCCESS)
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to acquire swapchain image.\n");
			return false;
		}

		if(xrWaitSwapchainImage(xrSwapchain[i].Swapchain, &(XrSwapchainImageWaitInfo) {.type=XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, .next=XR_NULL_HANDLE, .timeout=1000 })!=XR_SUCCESS)
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to wait for swapchain image.\n");
			return false;
		}

		vkCmdBlitImage(CommandBuffer, Eyes[i], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, xrSwapchain[i].Images[imageIndex].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkImageBlit)
		{
			.srcOffsets[0]={ 0, 0, 0 },
			.srcOffsets[1]={ Width, Height, 1 },
			.srcSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.srcSubresource.mipLevel=0,
			.srcSubresource.baseArrayLayer=0,
			.srcSubresource.layerCount=1,
			.dstOffsets[0]={ 0, 0, 0 },
			.dstOffsets[1]={ Width, Height, 1 },
			.dstSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.dstSubresource.mipLevel=0,
			.dstSubresource.baseArrayLayer=0,
			.dstSubresource.layerCount=1,
		}, VK_FILTER_LINEAR);

		if(xrReleaseSwapchainImage(xrSwapchain[i].Swapchain, &(XrSwapchainImageReleaseInfo) {.type=XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, .next=XR_NULL_HANDLE })!=XR_SUCCESS)
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to release swapchain image.\n");
			return false;
		}

		xrSwapchain[i].projectionView.pose=xrSwapchain[i].View.pose;
		xrSwapchain[i].projectionView.fov=xrSwapchain[i].View.fov;
	}

	XrCompositionLayerProjection projectionLayer={
		.type=XR_TYPE_COMPOSITION_LAYER_PROJECTION,
		.next=NULL,
		.layerFlags=0,
		.space=xrRefSpace,
		.viewCount=2,
		.views=(const XrCompositionLayerProjectionView[]) { xrSwapchain[0].projectionView, xrSwapchain[1].projectionView },
	};

	const XrCompositionLayerBaseHeader *layers[1]={ (const XrCompositionLayerBaseHeader *const)&projectionLayer };

	XrFrameEndInfo frameEndInfo=
	{
		.type=XR_TYPE_FRAME_END_INFO,
		.next=XR_NULL_HANDLE,
		.displayTime=frameState.predictedDisplayTime,
		.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
		.layerCount=1,
		.layers=layers,
	};

	if(xrEndFrame(xrSession, &frameEndInfo)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: xrEndFrame() was not successful.\n");
		return false;
	}

	return false;
}

// Get the current projection and transform for selected eye and output a projection matrix for vulkan
matrix VR_GetEyeProjection(uint32_t Eye)
{
	return MatrixIdentity();
}

// Get current inverse head pose matrix
matrix VR_GetHeadPose(void)
{
	return MatrixIdentity();
}

bool VR_Init(VkInstance Instance, VkuContext_t *Context)
{
	const XrFormFactor formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
	const XrViewConfigurationType viewType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
	const XrReferenceSpaceType refSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;

	int extensionsCount=1;
	const char *enabledExtensions[1]={ XR_KHR_VULKAN_ENABLE_EXTENSION_NAME };
	// same can be done for API layers, but API layers can also be enabled by env var

	XrInstanceCreateInfo instanceCreateInfo=
	{
		.type=XR_TYPE_INSTANCE_CREATE_INFO,
		.next=NULL,
		.createFlags=0,
		.enabledExtensionCount=extensionsCount,
		.enabledExtensionNames=enabledExtensions,
		.enabledApiLayerCount=0,
		.enabledApiLayerNames=NULL,
		.applicationInfo=
		{
			.applicationName="vkEngine",
			.engineName="vkEngine",
			.applicationVersion=1,
			.engineVersion=0,
			.apiVersion=XR_CURRENT_API_VERSION,
		},
	};

	if(xrCreateInstance(&instanceCreateInfo, &xrInstance)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR_Init: Failed to create OpenXR instance.\n");
		return false;
	}

	if(xrGetInstanceProcAddr(xrInstance, "xrGetVulkanInstanceExtensionsKHR", (PFN_xrVoidFunction *)&xrGetVulkanInstanceExtensionsKHR)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get proc address for xrGetVulkanInstanceExtensionsKHR.\n");
		return false;
	}
	if(xrGetInstanceProcAddr(xrInstance, "xrGetVulkanDeviceExtensionsKHR", (PFN_xrVoidFunction *)&xrGetVulkanDeviceExtensionsKHR)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get proc address for xrGetVulkanDeviceExtensionsKHR.\n");
		return false;
	}
	if(xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsDeviceKHR", (PFN_xrVoidFunction *)&xrGetVulkanGraphicsDeviceKHR)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get proc address for xrGetVulkanGraphicsDeviceKHR.\n");
		return false;
	}
	if(xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsRequirementsKHR", (PFN_xrVoidFunction *)&xrGetVulkanGraphicsRequirementsKHR)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get proc address for xrGetVulkanGraphicsRequirementsKHR.\n");
		return false;
	}
	
	XrInstanceProperties instanceProps={
		.type=XR_TYPE_INSTANCE_PROPERTIES,
		.next=NULL,
	};

	if(xrGetInstanceProperties(xrInstance, &instanceProps)!=XR_SUCCESS)
		return false;

	DBGPRINTF(DEBUG_INFO, "VR: Runtime Name: %s\n\tRuntime Version: %d.%d.%d\n",
			  instanceProps.runtimeName,
			  XR_VERSION_MAJOR(instanceProps.runtimeVersion),
			  XR_VERSION_MINOR(instanceProps.runtimeVersion),
			  XR_VERSION_PATCH(instanceProps.runtimeVersion));

	XrSystemGetInfo systemGetInfo=
	{
		.type=XR_TYPE_SYSTEM_GET_INFO,
		.next=NULL,
		.formFactor=formFactor,
	};

	if(xrGetSystem(xrInstance, &systemGetInfo, &xrSystemID)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get system for HMD form factor.\n");
		return false;
	}

	DBGPRINTF(DEBUG_INFO, "VR: Successfully got system with ID %llu for HMD form factor.\n", xrSystemID);

	typedef XrResult(XRAPI_PTR *PFN_xrGetVulkanInstanceExtensionsKHR)(XrInstance instance, XrSystemId systemId, uint32_t bufferCapacityInput, uint32_t *bufferCountOutput, char *buffer);

	uint32_t instanceExtensionNamesSize=0;
	if(xrGetVulkanInstanceExtensionsKHR(xrInstance, xrSystemID, 0, &instanceExtensionNamesSize, XR_NULL_HANDLE)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get Vulkan Device Extensions.\n");
		return false;
	}

	char *instanceExtensionNames=Zone_Malloc(Zone, instanceExtensionNamesSize);

	if(instanceExtensionNames==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to allocate memory for extension names.\n");
		return false;
	}

	if(xrGetVulkanInstanceExtensionsKHR(xrInstance, xrSystemID, instanceExtensionNamesSize, &instanceExtensionNamesSize, instanceExtensionNames)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get Vulkan Device Extensions.\n");
		return false;
	}

	DBGPRINTF(DEBUG_INFO, "VR: Instance extension requirements: %s\n", instanceExtensionNames);
	Zone_Free(Zone, instanceExtensionNames);

	uint32_t extensionNamesSize=0;
	if(xrGetVulkanDeviceExtensionsKHR(xrInstance, xrSystemID, 0, &extensionNamesSize, XR_NULL_HANDLE)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get Vulkan Device Extensions.\n");
		return false;
	}

	char *extensionNames=Zone_Malloc(Zone, extensionNamesSize);

	if(extensionNames==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to allocate memory for extension names.\n");
		return false;
	}

	if(xrGetVulkanDeviceExtensionsKHR(xrInstance, xrSystemID, extensionNamesSize, &extensionNamesSize, extensionNames)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get Vulkan Device Extensions.\n");
		return false;
	}

	DBGPRINTF(DEBUG_INFO, "VR: Extension requirements: %s\n", extensionNames);
	Zone_Free(Zone, extensionNames);

	XrSystemProperties systemProps=
	{
		.type=XR_TYPE_SYSTEM_PROPERTIES,
		.next=NULL,
	};

	if(xrGetSystemProperties(xrInstance, xrSystemID, &systemProps)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get System properties\n");
		return false;
	}

	DBGPRINTF(DEBUG_INFO, "VR: System properties for system %llu: \"%s\", vendor ID %d\n",
			  systemProps.systemId,
			  systemProps.systemName,
			  systemProps.vendorId);
	DBGPRINTF(DEBUG_INFO, "\tMax layers          : %d\n", systemProps.graphicsProperties.maxLayerCount);
	DBGPRINTF(DEBUG_INFO, "\tMax swapchain height: %d\n", systemProps.graphicsProperties.maxSwapchainImageHeight);
	DBGPRINTF(DEBUG_INFO, "\tMax swapchain width : %d\n", systemProps.graphicsProperties.maxSwapchainImageWidth);
	DBGPRINTF(DEBUG_INFO, "\tOrientation Tracking: %d\n", systemProps.trackingProperties.orientationTracking);
	DBGPRINTF(DEBUG_INFO, "\tPosition Tracking   : %d\n", systemProps.trackingProperties.positionTracking);

	uint32_t viewCount=0;
	if(xrEnumerateViewConfigurationViews(xrInstance, xrSystemID, viewType, 0, &viewCount, NULL)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get view configuration view count.\n");
		return false;
	}

	XrViewConfigurationView *viewConfigViews=Zone_Malloc(Zone, sizeof(XrViewConfigurationView)*viewCount);

	if(viewConfigViews==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Unable to allocate memory for view configuration views.\n");
		return false;
	}

	for(uint32_t i=0;i<viewCount;i++)
	{
		viewConfigViews[i].type=XR_TYPE_VIEW_CONFIGURATION_VIEW;
		viewConfigViews[i].next=NULL;
	}

	if(xrEnumerateViewConfigurationViews(xrInstance, xrSystemID, viewType, viewCount, &viewCount, viewConfigViews)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to enumerate view configuration views.\n");
		return false;
	}

	for(uint32_t i=0;i<viewCount;i++)
	{
		DBGPRINTF(DEBUG_INFO, "VR: View Configuration View %d:\n", i);
		DBGPRINTF(DEBUG_INFO, "\tResolution: Recommended %dx%d, Max: %dx%d\n",
				  viewConfigViews[i].recommendedImageRectWidth,
				  viewConfigViews[i].recommendedImageRectHeight,
				  viewConfigViews[i].maxImageRectWidth,
				  viewConfigViews[i].maxImageRectHeight);
		DBGPRINTF(DEBUG_INFO, "\tSwapchain Samples: Recommended: %d, Max: %d)\n",
				  viewConfigViews[i].recommendedSwapchainSampleCount,
				  viewConfigViews[i].maxSwapchainSampleCount);
	}

	if(viewCount>2)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Device supports more views than application supports.\n");
		return false;
	}

	rtWidth=viewConfigViews[0].recommendedImageRectWidth;
	rtHeight=viewConfigViews[0].recommendedImageRectHeight;

	Zone_Free(Zone, viewConfigViews);

	XrGraphicsRequirementsVulkanKHR graphicsRequirements=
	{
		.type=XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR,
		.next=NULL
	};

	// this function pointer was loaded with xrGetInstanceProcAddr
	if(xrGetVulkanGraphicsRequirementsKHR(xrInstance, xrSystemID, &graphicsRequirements)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get Vulkan graphics requirements.\n");
		return false;
	}

	XrGraphicsBindingVulkanKHR graphicsBindingVulkan=
	{
		.type=XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR,
		.next=XR_NULL_HANDLE,
		.instance=Instance,
		.physicalDevice=Context->PhysicalDevice,
		.device=Context->Device,
		.queueFamilyIndex=Context->QueueFamilyIndex,
		.queueIndex=0
	};

	XrSessionCreateInfo sessionCreateInfo=
	{
		.type=XR_TYPE_SESSION_CREATE_INFO,
		.next=&graphicsBindingVulkan,
		.systemId=xrSystemID
	};

	if(xrCreateSession(xrInstance, &sessionCreateInfo, &xrSession)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to create session.\n");
		return false;
	}

	DBGPRINTF(DEBUG_INFO, "VR: Successfully created a Vulkan session.\n");

	XrReferenceSpaceCreateInfo refSpaceCreateInfo=
	{
		.type=XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
		.next=NULL,
		.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL,
		.poseInReferenceSpace=
		{
			.orientation={ .x=0.0f, .y=0.0f, .z=0.0f, .w=1.0},
			.position={ .x=0.0f, .y=0.0f, .z=0.0f }
		}
	};

	if(xrCreateReferenceSpace(xrSession, &refSpaceCreateInfo, &xrRefSpace)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_INFO, "VR: Failed to create play space.\n");
		return false;
	}

	uint32_t swapchainFormatCount;
	if(xrEnumerateSwapchainFormats(xrSession, 0, &swapchainFormatCount, NULL)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get number of supported swapchain formats.\n");
		return false;
	}

	DBGPRINTF(DEBUG_INFO, "VR: Runtime supports %d swapchain formats.\n", swapchainFormatCount);

	int64_t *swapchainFormats=Zone_Malloc(Zone, sizeof(int64_t)*swapchainFormatCount);

	if(swapchainFormats==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to allocate memory for swapchain formats.\n");
		return false;
	}

	if(xrEnumerateSwapchainFormats(xrSession, swapchainFormatCount, &swapchainFormatCount, swapchainFormats))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to enumerate swapchain formats.\n");
		return false;
	}

	int64_t surfaceFormat;
	VkFormat PreferredFormats[]=
	{
		VK_FORMAT_B8G8R8A8_SRGB,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_R8G8B8A8_UNORM
	};
	bool FoundFormat=false;

	for(uint32_t i=0;i<sizeof(PreferredFormats);i++)
	{
		for(uint32_t j=0;j<swapchainFormatCount;j++)
		{
			if(swapchainFormats[j]==PreferredFormats[i])
			{
				surfaceFormat=swapchainFormats[j];
				FoundFormat=true;
				break;
			}
		}

		if(FoundFormat)
			break;
	}

	Zone_Free(Zone, swapchainFormats);

	for(uint32_t i=0;i<viewCount;i++)
	{
		xrSwapchain[i].View.type=XR_TYPE_VIEW;
		xrSwapchain[i].View.next=XR_NULL_HANDLE;

		xrSwapchain[i].projectionView.type=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
		xrSwapchain[i].projectionView.next=XR_NULL_HANDLE;
		xrSwapchain[i].projectionView.subImage.swapchain=xrSwapchain[i].Swapchain;
		xrSwapchain[i].projectionView.subImage.imageArrayIndex=0;
		xrSwapchain[i].projectionView.subImage.imageRect.offset.x=0;
		xrSwapchain[i].projectionView.subImage.imageRect.offset.y=0;
		xrSwapchain[i].projectionView.subImage.imageRect.extent.width=rtWidth;
		xrSwapchain[i].projectionView.subImage.imageRect.extent.height=rtHeight;

		XrSwapchainCreateInfo swapchainCreateInfo=
		{
			.type=XR_TYPE_SWAPCHAIN_CREATE_INFO,
			.next=NULL,
			.createFlags=0,
			.usageFlags=XR_SWAPCHAIN_USAGE_SAMPLED_BIT|XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_TRANSFER_DST_BIT,
			.format=surfaceFormat,
			.sampleCount=1,
			.width=rtWidth,
			.height=rtHeight,
			.faceCount=1,
			.arraySize=1,
			.mipCount=1,
		};

		if(xrCreateSwapchain(xrSession, &swapchainCreateInfo, &xrSwapchain[i].Swapchain)!=XR_SUCCESS)
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to create swapchain %d.\n", i);
			return false;
		}

		uint32_t imageCount=0;
		if(xrEnumerateSwapchainImages(xrSwapchain[i].Swapchain, 0, &imageCount, NULL)!=XR_SUCCESS)
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to enumerate swapchain image count.\n");
			return false;
		}

		if(imageCount>VKU_MAX_FRAME_COUNT)
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Swapchain image count is higher than application supports.\n");
			return false;
		}

		for(uint32_t j=0;j<imageCount;j++)
		{
			xrSwapchain[i].Images[j].type=XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
			xrSwapchain[i].Images[j].next=NULL;
		}

		if(xrEnumerateSwapchainImages(xrSwapchain[i].Swapchain, imageCount, &imageCount, (XrSwapchainImageBaseHeader *)xrSwapchain[i].Images)!=XR_SUCCESS)
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to enumerate swapchain images.\n");
			return false;
		}
	}

	XrSessionBeginInfo session_begin_info=
	{
		.type=XR_TYPE_SESSION_BEGIN_INFO,
		.next=XR_NULL_HANDLE,
		.primaryViewConfigurationType=viewType
	};

	if(xrBeginSession(xrSession, &session_begin_info)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to begin session.\n");
		return false;
	}

	return true;
}

void VR_Destroy(void)
{
	xrEndSession(xrSession);
	xrDestroySession(xrSession);

	xrDestroySwapchain(xrSwapchain[0].Swapchain);
	xrDestroySwapchain(xrSwapchain[1].Swapchain);

	xrDestroyInstance(xrInstance);
}
#else
matrix VR_GetEyeProjection(uint32_t Eye)
{
	return MatrixIdentity();
}

matrix VR_GetHeadPose(void)
{
	return MatrixIdentity();
}

bool VR_Init(void)
{
	return false;
}

void VR_Destroy(void)
{
}
#endif
