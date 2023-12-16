#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "../math/math.h"
#include "vr.h"

#ifndef ANDROID
#include "../vulkan/vulkan.h"

#ifdef WIN32
#define XR_USE_PLATFORM_WIN32
#endif

#ifdef LINUX
#define XR_USE_PLATFORM_XLIB
#endif

#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

extern VkuContext_t Context;

PFN_xrGetVulkanInstanceExtensionsKHR xrGetVulkanInstanceExtensionsKHR=XR_NULL_HANDLE;
PFN_xrGetVulkanDeviceExtensionsKHR xrGetVulkanDeviceExtensionsKHR=XR_NULL_HANDLE;
PFN_xrGetVulkanGraphicsDeviceKHR xrGetVulkanGraphicsDeviceKHR=XR_NULL_HANDLE;
PFN_xrGetVulkanGraphicsRequirementsKHR xrGetVulkanGraphicsRequirementsKHR=XR_NULL_HANDLE;

static XrInstance xrInstance=XR_NULL_HANDLE;
static XrSystemId xrSystemID=XR_NULL_SYSTEM_ID;
static uint32_t viewCount=0;
static XrViewConfigurationView *xrViewConfigViews=NULL;
static XrView xrViews[2]={ {.type=XR_TYPE_VIEW }, {.type=XR_TYPE_VIEW } };
static XrCompositionLayerProjectionView xrProjViews[2]={ { .type=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW }, {.type=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW } };
static XrSession xrSession=XR_NULL_HANDLE;
static XrSpace xrRefSpace=XR_NULL_HANDLE;

XruSwapchain_t xrSwapchain[2];

uint32_t rtWidth;
uint32_t rtHeight;

static XrFrameState frameState=
{
	.type=XR_TYPE_FRAME_STATE,
	.next=XR_NULL_HANDLE
};

static bool xruCheck(XrResult Result)
{
	if(XR_SUCCEEDED(Result))
		return true;

	char resultString[XR_MAX_RESULT_STRING_SIZE];
	xrResultToString(xrInstance, Result, resultString);
	DBGPRINTF(DEBUG_ERROR, "VR: %s\n", resultString);

	return false;
}

static XrSessionState xrSessionState=XR_SESSION_STATE_UNKNOWN;

static void xruPollEvents(bool *exit, bool *xr_running)
{
	XrEventDataBuffer event_buffer={ XR_TYPE_EVENT_DATA_BUFFER };

	while(xrPollEvent(xrInstance, &event_buffer)==XR_SUCCESS)
	{
		switch(event_buffer.type)
		{
			case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
			{
				XrEventDataSessionStateChanged *changed=(XrEventDataSessionStateChanged *)&event_buffer;
				xrSessionState=changed->state;

				switch(xrSessionState)
				{
					case XR_SESSION_STATE_READY:
					{
						XrSessionBeginInfo sessionBeginInfo=
						{
							.type=XR_TYPE_SESSION_BEGIN_INFO,
							.next=XR_NULL_HANDLE,
							.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO
						};

						xrBeginSession(xrSession, &sessionBeginInfo);

						DBGPRINTF(DEBUG_INFO, "VR: State ready, begin session.\n");
						*xr_running=true;

						break;
					}

					case XR_SESSION_STATE_STOPPING:
					{
						*xr_running=false;
						xrEndSession(xrSession);
						DBGPRINTF(DEBUG_WARNING, "VR: State stopping, end session.\n");
						break;
					}

					case XR_SESSION_STATE_EXITING:
						DBGPRINTF(DEBUG_WARNING, "VR: State exiting.\n");
						*exit=true;
						break;

					case XR_SESSION_STATE_LOSS_PENDING:
						DBGPRINTF(DEBUG_WARNING, "VR: State loss pending.\n");
						*exit=true;
						break;
				}
			}
			break;

			case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
				DBGPRINTF(DEBUG_WARNING, "VR: Event data instance loss.\n");
				*exit=true;
				return;
		}

		event_buffer.type=XR_TYPE_EVENT_DATA_BUFFER;
	}
}

static bool xr_exit=false, xr_running=false;

bool VR_StartFrame(uint32_t *eyeIndex1, uint32_t *eyeIndex2)
{
	xruPollEvents(&xr_exit, &xr_running);

	if(!xr_running)
		return false;
	
	memset(&frameState, 0, sizeof(XrFrameState));
	frameState.type=XR_TYPE_FRAME_STATE;

	if(!xruCheck(xrWaitFrame(xrSession, XR_NULL_HANDLE, &frameState)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: xrWaitFrame() was not successful.\n");
		return false;
	}

	if(!xruCheck(xrBeginFrame(xrSession, XR_NULL_HANDLE)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: xrBeginFrame() was not successful.\n");
		return false;
	}

	if(xrSessionState==XR_SESSION_STATE_VISIBLE||xrSessionState==XR_SESSION_STATE_FOCUSED)
	{
		uint32_t viewCount=0;
		XrViewState viewState={ .type=XR_TYPE_VIEW_STATE };
		XrViewLocateInfo locateInfo={ .type=XR_TYPE_VIEW_LOCATE_INFO };

		locateInfo.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
		locateInfo.displayTime=frameState.predictedDisplayTime;
		locateInfo.space=xrRefSpace;

		if(!xruCheck(xrLocateViews(xrSession, &locateInfo, &viewState, 2, &viewCount, xrViews)))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: xrLocateViews failed.\n");
			return false;
		}

		if(!xruCheck(xrAcquireSwapchainImage(xrSwapchain[0].Swapchain, &(XrSwapchainImageAcquireInfo) {.type=XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, .next=XR_NULL_HANDLE }, eyeIndex1)))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to acquire swapchain image 0.\n");
			return false;
		}

		if(!xruCheck(xrWaitSwapchainImage(xrSwapchain[0].Swapchain, &(XrSwapchainImageWaitInfo) {.type=XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, .next=XR_NULL_HANDLE, .timeout=INT64_MAX })))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to wait for swapchain image 0.\n");
			return false;
		}

		if(!xruCheck(xrAcquireSwapchainImage(xrSwapchain[1].Swapchain, &(XrSwapchainImageAcquireInfo) {.type=XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, .next=XR_NULL_HANDLE }, eyeIndex2)))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to acquire swapchain image 1.\n");
			return false;
		}

		if(!xruCheck(xrWaitSwapchainImage(xrSwapchain[1].Swapchain, &(XrSwapchainImageWaitInfo) {.type=XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, .next=XR_NULL_HANDLE, .timeout=INT64_MAX })))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to wait for swapchain image 1.\n");
			return false;
		}

		xrProjViews[0].type=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
		xrProjViews[0].pose=xrViews[0].pose;
		xrProjViews[0].fov=xrViews[0].fov;
		xrProjViews[0].subImage.swapchain=xrSwapchain[0].Swapchain;
		xrProjViews[0].subImage.imageRect.offset.x=0;
		xrProjViews[0].subImage.imageRect.offset.y=0;
		xrProjViews[0].subImage.imageRect.extent.width=rtWidth;
		xrProjViews[0].subImage.imageRect.extent.height=rtHeight;

		xrProjViews[1].type=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
		xrProjViews[1].pose=xrViews[1].pose;
		xrProjViews[1].fov=xrViews[1].fov;
		xrProjViews[1].subImage.swapchain=xrSwapchain[1].Swapchain;
		xrProjViews[1].subImage.imageRect.offset.x=0;
		xrProjViews[1].subImage.imageRect.offset.y=0;
		xrProjViews[1].subImage.imageRect.extent.width=rtWidth;
		xrProjViews[1].subImage.imageRect.extent.height=rtHeight;
	}

	return true;
}

bool VR_EndFrame(void)
{
	if((!xr_running)||(!frameState.shouldRender))
		return false;

	if(xrSessionState==XR_SESSION_STATE_VISIBLE||xrSessionState==XR_SESSION_STATE_FOCUSED)
	{
		if(!xruCheck(xrReleaseSwapchainImage(xrSwapchain[0].Swapchain, &(XrSwapchainImageReleaseInfo) {.type=XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, .next=XR_NULL_HANDLE })))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to release swapchain image 0.\n");
			return false;
		}

		if(!xruCheck(xrReleaseSwapchainImage(xrSwapchain[1].Swapchain, &(XrSwapchainImageReleaseInfo) {.type=XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, .next=XR_NULL_HANDLE })))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to release swapchain image 1.\n");
			return false;
		}

		XrCompositionLayerProjection projectionLayer={
			.type=XR_TYPE_COMPOSITION_LAYER_PROJECTION,
			.next=NULL,
			.layerFlags=0,
			.space=xrRefSpace,
			.viewCount=2,
			.views=(const XrCompositionLayerProjectionView[]) { xrProjViews[0], xrProjViews[1] },
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

		if(!xruCheck(xrEndFrame(xrSession, &frameEndInfo)))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: xrEndFrame() was not successful.\n");
			return false;
		}
	}

	return true;
}

matrix VR_GetEyeProjection(uint32_t Eye)
{
	const float Left=tanf(xrViews[Eye].fov.angleLeft);
	const float Right=tanf(xrViews[Eye].fov.angleRight);
	const float Down=tanf(xrViews[Eye].fov.angleDown);
	const float Up=tanf(xrViews[Eye].fov.angleUp);
	const float Width=Right-Left;
	const float Height=Down-Up;
	const float nearZ=0.01f;

	return (matrix)
	{
		{ 2.0f/Width, 0.0f, 0.0f, 0.0f },
		{ 0.0f, 2.0f/Height, 0.0f, 0.0f },
		{ (Right+Left)/Width, (Up+Down)/Height, 0.0f, -1.0f },
		{ 0.0f, 0.0f, nearZ, 0.0f }
	};
}

// Get current inverse head pose matrix
matrix VR_GetHeadPose(void)
{
	XrPosef Pose=xrProjViews[0].pose;

	// Get a matrix from the orientation quaternion
	matrix PoseMat=QuatMatrix(Vec4(
		Pose.orientation.x,
		Pose.orientation.y,
		Pose.orientation.z,
		Pose.orientation.w
	));

	// And set the translation directly, no need for an extra matrix multiply
	PoseMat.w=Vec4(Pose.position.x, Pose.position.y, Pose.position.z, 1.0f);

	return MatrixInverse(PoseMat);
}

static bool VR_InitSystem(const XrFormFactor formFactor)
{
	int extensionCount=0;
	const char *enabledExtensions[10];

	enabledExtensions[extensionCount++]=XR_KHR_VULKAN_ENABLE_EXTENSION_NAME;

	XrInstanceCreateInfo instanceCreateInfo=
	{
		.type=XR_TYPE_INSTANCE_CREATE_INFO,
		.next=NULL,
		.enabledExtensionCount=extensionCount,
		.enabledExtensionNames=enabledExtensions,
		.applicationInfo=
		{
			.applicationName="vkEngine",
			.engineName="vkEngine",
			.applicationVersion=1,
			.engineVersion=0,
			.apiVersion=XR_CURRENT_API_VERSION,
		},
	};

	if(!xruCheck(xrCreateInstance(&instanceCreateInfo, &xrInstance)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR_Init: Failed to create OpenXR instance.\n");
		return false;
	}

	if(!xruCheck(xrGetInstanceProcAddr(xrInstance, "xrGetVulkanInstanceExtensionsKHR", (PFN_xrVoidFunction *)&xrGetVulkanInstanceExtensionsKHR)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get proc address for xrGetVulkanInstanceExtensionsKHR.\n");
		return false;
	}

	if(!xruCheck(xrGetInstanceProcAddr(xrInstance, "xrGetVulkanDeviceExtensionsKHR", (PFN_xrVoidFunction *)&xrGetVulkanDeviceExtensionsKHR)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get proc address for xrGetVulkanDeviceExtensionsKHR.\n");
		return false;
	}

	if(!xruCheck(xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsDeviceKHR", (PFN_xrVoidFunction *)&xrGetVulkanGraphicsDeviceKHR)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get proc address for xrGetVulkanGraphicsDeviceKHR.\n");
		return false;
	}

	if(!xruCheck(xrGetInstanceProcAddr(xrInstance, "xrGetVulkanGraphicsRequirementsKHR", (PFN_xrVoidFunction *)&xrGetVulkanGraphicsRequirementsKHR)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get proc address for xrGetVulkanGraphicsRequirementsKHR.\n");
		return false;
	}

	XrInstanceProperties instanceProps={
		.type=XR_TYPE_INSTANCE_PROPERTIES,
		.next=NULL,
	};

	if(!xruCheck(xrGetInstanceProperties(xrInstance, &instanceProps)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get instance properties.\n");
		return false;
	}

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

	if(!xruCheck(xrGetSystem(xrInstance, &systemGetInfo, &xrSystemID)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get system for HMD form factor.\n");
		return false;
	}

	DBGPRINTF(DEBUG_INFO, "VR: Successfully got system with ID %llu for HMD form factor.\n", xrSystemID);

	uint32_t instanceExtensionNamesSize=0;
	if(!xruCheck(xrGetVulkanInstanceExtensionsKHR(xrInstance, xrSystemID, 0, &instanceExtensionNamesSize, XR_NULL_HANDLE)))
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

	if(!xruCheck(xrGetVulkanInstanceExtensionsKHR(xrInstance, xrSystemID, instanceExtensionNamesSize, &instanceExtensionNamesSize, instanceExtensionNames)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get Vulkan Device Extensions.\n");
		return false;
	}

	DBGPRINTF(DEBUG_INFO, "VR: Instance extension requirements: %s\n", instanceExtensionNames);
	Zone_Free(Zone, instanceExtensionNames);

	uint32_t extensionNamesSize=0;
	if(!xruCheck(xrGetVulkanDeviceExtensionsKHR(xrInstance, xrSystemID, 0, &extensionNamesSize, XR_NULL_HANDLE)))
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

	if(!xruCheck(xrGetVulkanDeviceExtensionsKHR(xrInstance, xrSystemID, extensionNamesSize, &extensionNamesSize, extensionNames)))
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

	if(!xruCheck(xrGetSystemProperties(xrInstance, xrSystemID, &systemProps)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get System properties\n");
		return false;
	}

	DBGPRINTF(DEBUG_INFO, "VR: System properties for system %llu: \"%s\", vendor ID %d\n", systemProps.systemId, systemProps.systemName, systemProps.vendorId);
	DBGPRINTF(DEBUG_INFO, "\tMax layers          : %d\n", systemProps.graphicsProperties.maxLayerCount);
	DBGPRINTF(DEBUG_INFO, "\tMax swapchain height: %d\n", systemProps.graphicsProperties.maxSwapchainImageHeight);
	DBGPRINTF(DEBUG_INFO, "\tMax swapchain width : %d\n", systemProps.graphicsProperties.maxSwapchainImageWidth);
	DBGPRINTF(DEBUG_INFO, "\tOrientation Tracking: %d\n", systemProps.trackingProperties.orientationTracking);
	DBGPRINTF(DEBUG_INFO, "\tPosition Tracking   : %d\n", systemProps.trackingProperties.positionTracking);

	return true;
}

static bool VR_GetViewConfig(const XrViewConfigurationType viewType)
{
	if(xrEnumerateViewConfigurationViews(xrInstance, xrSystemID, viewType, 0, &viewCount, NULL)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get view configuration view count.\n");
		return false;
	}

	xrViewConfigViews=Zone_Malloc(Zone, sizeof(XrViewConfigurationView)*viewCount);

	if(xrViewConfigViews==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Unable to allocate memory for view configuration views.\n");
		return false;
	}

	for(uint32_t i=0;i<viewCount;i++)
	{
		xrViewConfigViews[i].type=XR_TYPE_VIEW_CONFIGURATION_VIEW;
		xrViewConfigViews[i].next=NULL;
	}

	if(xrEnumerateViewConfigurationViews(xrInstance, xrSystemID, viewType, viewCount, &viewCount, xrViewConfigViews)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to enumerate view configuration views.\n");
		return false;
	}

	for(uint32_t i=0;i<viewCount;i++)
	{
		DBGPRINTF(DEBUG_INFO, "VR: View Configuration View %d:\n", i);
		DBGPRINTF(DEBUG_INFO, "\tResolution: Recommended %dx%d, Max: %dx%d\n",
				  xrViewConfigViews[i].recommendedImageRectWidth,
				  xrViewConfigViews[i].recommendedImageRectHeight,
				  xrViewConfigViews[i].maxImageRectWidth,
				  xrViewConfigViews[i].maxImageRectHeight);
		DBGPRINTF(DEBUG_INFO, "\tSwapchain Samples: Recommended: %d, Max: %d)\n",
				  xrViewConfigViews[i].recommendedSwapchainSampleCount,
				  xrViewConfigViews[i].maxSwapchainSampleCount);
	}

	if(viewCount>2)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Device supports more views than application supports.\n");
		return false;
	}

	rtWidth=xrViewConfigViews[0].recommendedImageRectWidth;
	rtHeight=xrViewConfigViews[0].recommendedImageRectHeight;

	return true;
}

static bool VR_InitSession(VkInstance Instance, VkuContext_t *Context)
{
	XrGraphicsRequirementsVulkanKHR graphicsRequirements={ .type=XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR };

	if(!xruCheck(xrGetVulkanGraphicsRequirementsKHR(xrInstance, xrSystemID, &graphicsRequirements)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get Vulkan graphics requirements.\n");
		return false;
	}

	// Is this needed for sure?
	// SteamVR null driver needed it, but Quest2 doesn't.
	//VkPhysicalDevice physicalDevice=VK_NULL_HANDLE;
	//xrGetVulkanGraphicsDeviceKHR(xrInstance, xrSystemID, Instance, &physicalDevice);

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
		.next=(void *)&graphicsBindingVulkan,
		.systemId=xrSystemID
	};

	if(!xruCheck(xrCreateSession(xrInstance, &sessionCreateInfo, &xrSession)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to create session.\n");
		return false;
	}

	DBGPRINTF(DEBUG_INFO, "VR: Successfully created a Vulkan session.\n");

	return true;
}

static bool VR_InitReferenceSpace(XrReferenceSpaceType spaceType)
{

	XrReferenceSpaceCreateInfo refSpaceCreateInfo=
	{
		.type=XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
		.next=NULL,
		.referenceSpaceType=spaceType,
		.poseInReferenceSpace.orientation={ .x=0.0f, .y=0.0f, .z=0.0f, .w=1.0f },
		.poseInReferenceSpace.position={ .x=0.0f, .y=0.0f, .z=0.0f }
	};

	if(!xruCheck(xrCreateReferenceSpace(xrSession, &refSpaceCreateInfo, &xrRefSpace)))
	{
		DBGPRINTF(DEBUG_INFO, "VR: Failed to create play space.\n");
		return false;
	}

	return true;
}

static bool VR_InitSwapchain(VkuContext_t *Context)
{
	uint32_t swapchainFormatCount;
	if(!xruCheck(xrEnumerateSwapchainFormats(xrSession, 0, &swapchainFormatCount, NULL)))
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

	if(!xruCheck(xrEnumerateSwapchainFormats(xrSession, swapchainFormatCount, &swapchainFormatCount, swapchainFormats)))
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

	// Create swapchain images, imageviews, and transition to correct image layout
	VkCommandBuffer CommandBuffer=vkuOneShotCommandBufferBegin(Context);

	for(uint32_t i=0;i<viewCount;i++)
	{
		XrSwapchainCreateInfo swapchainCreateInfo=
		{
			.type=XR_TYPE_SWAPCHAIN_CREATE_INFO,
			.next=NULL,
			.createFlags=0,
			.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
			.format=surfaceFormat,
			.sampleCount=VK_SAMPLE_COUNT_1_BIT,
			.width=rtWidth,
			.height=rtHeight,
			.faceCount=1,
			.arraySize=1,
			.mipCount=1,
		};

		if(!xruCheck(xrCreateSwapchain(xrSession, &swapchainCreateInfo, &xrSwapchain[i].Swapchain)))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to create swapchain %d.\n", i);
			return false;
		}

		xrSwapchain[i].NumImages=0;
		if(!xruCheck(xrEnumerateSwapchainImages(xrSwapchain[i].Swapchain, 0, &xrSwapchain[i].NumImages, NULL)))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to enumerate swapchain image count.\n");
			return false;
		}

		if(xrSwapchain[i].NumImages>VKU_MAX_FRAME_COUNT)
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Swapchain image count is higher than application supports.\n");
			return false;
		}

		for(uint32_t j=0;j<xrSwapchain[i].NumImages;j++)
		{
			xrSwapchain[i].Images[j].type=XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;
			xrSwapchain[i].Images[j].next=NULL;
		}

		if(!xruCheck(xrEnumerateSwapchainImages(xrSwapchain[i].Swapchain, xrSwapchain[i].NumImages, &xrSwapchain[i].NumImages, (XrSwapchainImageBaseHeader *)xrSwapchain[i].Images)))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to enumerate swapchain images.\n");
			return false;
		}

		for(uint32_t j=0;j<xrSwapchain[i].NumImages;j++)
		{
			vkuTransitionLayout(CommandBuffer, xrSwapchain[i].Images[j].image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

			vkCreateImageView(Context->Device, &(VkImageViewCreateInfo)
			{
				.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext=VK_NULL_HANDLE,
				.image=xrSwapchain[i].Images[j].image,
				.format=surfaceFormat,
				.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
				.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
				.subresourceRange.baseMipLevel=0,
				.subresourceRange.levelCount=1,
				.subresourceRange.baseArrayLayer=0,
				.subresourceRange.layerCount=1,
				.viewType=VK_IMAGE_VIEW_TYPE_2D,
				.flags=0,
			}, VK_NULL_HANDLE, &xrSwapchain[i].ImageView[j]);
		}
	}

	vkuOneShotCommandBufferEnd(Context, CommandBuffer);

	return true;
}

bool VR_Init(VkInstance Instance, VkuContext_t *Context)
{
	if(!VR_InitSystem(XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: VR_InitSystem failed.\n");
		return false;
	}

	if(!VR_GetViewConfig(XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: VR_GetViewConfig failed.\n");
		return false;
	}

	if(!VR_InitSession(Instance, Context))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: VR_InitSession failed.\n");
		return false;
	}

	if(!VR_InitReferenceSpace(XR_REFERENCE_SPACE_TYPE_STAGE))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: VR_InitReferenceSpace failed.\n");
		return false;
	}

	if(!VR_InitSwapchain(Context))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: VR_InitSwapchain failed.\n");
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
