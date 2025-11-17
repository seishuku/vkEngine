#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include "../system/system.h"
#include "../math/math.h"
#include "../vulkan/vulkan.h"
#include "vr.h"

PFN_xrGetVulkanInstanceExtensionsKHR xrGetVulkanInstanceExtensionsKHR=XR_NULL_HANDLE;
PFN_xrGetVulkanDeviceExtensionsKHR xrGetVulkanDeviceExtensionsKHR=XR_NULL_HANDLE;
PFN_xrGetVulkanGraphicsDeviceKHR xrGetVulkanGraphicsDeviceKHR=XR_NULL_HANDLE;
PFN_xrGetVulkanGraphicsRequirementsKHR xrGetVulkanGraphicsRequirementsKHR=XR_NULL_HANDLE;

static bool xruCheck(XrInstance xrInstance, XrResult Result)
{
	if(XR_SUCCEEDED(Result))
		return true;

	char resultString[XR_MAX_RESULT_STRING_SIZE];
	xrResultToString(xrInstance, Result, resultString);
	DBGPRINTF(DEBUG_ERROR, "VR: %s\n", resultString);

	return false;
}

static void xruPollEvents(XruContext_t *xrContext)
{
	XrEventDataBuffer event_buffer={ XR_TYPE_EVENT_DATA_BUFFER };

	while(xrPollEvent(xrContext->instance, &event_buffer)==XR_SUCCESS)
	{
		switch(event_buffer.type)
		{
			case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
			{
				xrContext->sessionState=((XrEventDataSessionStateChanged *)&event_buffer)->state;

				switch(xrContext->sessionState)
				{
					case XR_SESSION_STATE_READY:
					{
						XrSessionBeginInfo sessionBeginInfo=
						{
							.type=XR_TYPE_SESSION_BEGIN_INFO,
							.primaryViewConfigurationType=xrContext->viewType
						};

						xruCheck(xrContext->instance, xrBeginSession(xrContext->session, &sessionBeginInfo));

						DBGPRINTF(DEBUG_INFO, "VR: State ready.\n");
						xrContext->sessionRunning=true;
						break;
					}

					case XR_SESSION_STATE_SYNCHRONIZED:
						DBGPRINTF(DEBUG_INFO, "VR: State synchronized.\n");
						break;

					case XR_SESSION_STATE_FOCUSED:
						DBGPRINTF(DEBUG_INFO, "VR: State focused.\n");
						break;

					case XR_SESSION_STATE_STOPPING:
					{
						xrContext->sessionRunning=false;
						xruCheck(xrContext->instance, xrEndSession(xrContext->session));
						DBGPRINTF(DEBUG_WARNING, "VR: State stopping.\n");
						break;
					}

					case XR_SESSION_STATE_EXITING:
						DBGPRINTF(DEBUG_WARNING, "VR: State exiting.\n");
						xrContext->exitRequested=true;
						break;

					case XR_SESSION_STATE_LOSS_PENDING:
						DBGPRINTF(DEBUG_WARNING, "VR: State loss pending.\n");
						xrContext->exitRequested=true;
						break;

					default:
						break;
				}
			}
			break;

			case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
				DBGPRINTF(DEBUG_WARNING, "VR: Event data instance loss.\n");
				xrContext->exitRequested=true;
				return;

			default:
				break;
		}

		event_buffer.type=XR_TYPE_EVENT_DATA_BUFFER;
	}
}

bool VR_StartFrame(XruContext_t *xrContext, uint32_t *imageIndex)
{
	xruPollEvents(xrContext);

	if(!xrContext->sessionRunning)
		return false;

	XrFrameWaitInfo waitFrame={ .type=XR_TYPE_FRAME_WAIT_INFO };

	if(!xruCheck(xrContext->instance, xrWaitFrame(xrContext->session, &waitFrame, &xrContext->frameState)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: xrWaitFrame() was not successful.\n");
		return false;
	}

	XrFrameBeginInfo frameBegin={ .type=XR_TYPE_FRAME_BEGIN_INFO };

	if(!xruCheck(xrContext->instance, xrBeginFrame(xrContext->session, &frameBegin)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: xrBeginFrame() was not successful.\n");
		return false;
	}
	
	if(xrContext->frameState.shouldRender)
	{
		uint32_t viewCount=0;
		XrViewState viewState={ .type=XR_TYPE_VIEW_STATE };
		XrViewLocateInfo locateInfo=
		{
			.type=XR_TYPE_VIEW_LOCATE_INFO,
			.viewConfigurationType=xrContext->viewType,
			.displayTime=xrContext->frameState.predictedDisplayTime,
			.space=xrContext->refSpace
		};
		XrView views[2]=
		{
			{.type=XR_TYPE_VIEW },
			{.type=XR_TYPE_VIEW }
		};

		if(!xruCheck(xrContext->instance, xrLocateViews(xrContext->session, &locateInfo, &viewState, 2, &viewCount, views)))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: xrLocateViews failed.\n");
			return false;
		}

		if(!xruCheck(xrContext->instance, xrAcquireSwapchainImage(xrContext->swapchain[0].swapchain, &(XrSwapchainImageAcquireInfo) {.type=XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO }, &imageIndex[0])))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to acquire swapchain image 0.\n");
			return false;
		}

		if(!xruCheck(xrContext->instance, xrAcquireSwapchainImage(xrContext->swapchain[1].swapchain, &(XrSwapchainImageAcquireInfo) {.type=XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO }, &imageIndex[1])))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to acquire swapchain image 1.\n");
			return false;
		}

		if(!xruCheck(xrContext->instance, xrWaitSwapchainImage(xrContext->swapchain[0].swapchain, &(XrSwapchainImageWaitInfo) {.type=XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, .timeout=INT64_MAX })))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to wait for swapchain image 0.\n");
			return false;
		}

		if(!xruCheck(xrContext->instance, xrWaitSwapchainImage(xrContext->swapchain[1].swapchain, &(XrSwapchainImageWaitInfo) {.type=XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO, .timeout=INT64_MAX })))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to wait for swapchain image 1.\n");
			return false;
		}

		xrContext->projViews[0].subImage.swapchain=xrContext->swapchain[0].swapchain;
		xrContext->projViews[0].subImage.imageRect.offset=(XrOffset2Di){ 0, 0 };
		xrContext->projViews[0].subImage.imageRect.extent=xrContext->swapchainExtent;

		xrContext->projViews[1].subImage.swapchain=xrContext->swapchain[1].swapchain;
		xrContext->projViews[1].subImage.imageRect.offset=(XrOffset2Di){ 0, 0 };
		xrContext->projViews[1].subImage.imageRect.extent=xrContext->swapchainExtent;

		// Only transfer over the pose tracking if it's valid
		if(viewState.viewStateFlags&(XR_VIEW_STATE_ORIENTATION_VALID_BIT|XR_VIEW_STATE_POSITION_VALID_BIT))
		{
			xrContext->projViews[0].pose=views[0].pose;
			xrContext->projViews[1].pose=views[1].pose;
		}

		xrContext->projViews[0].fov=views[0].fov;
		xrContext->projViews[1].fov=views[1].fov;

		XrActiveActionSet activeActionSet={ xrContext->actionSet, XR_NULL_PATH };
		XrActionsSyncInfo syncInfo={ .type=XR_TYPE_ACTIONS_SYNC_INFO, .countActiveActionSets=1, .activeActionSets=&activeActionSet };

		if(!xruCheck(xrContext->instance, xrSyncActions(xrContext->session, &syncInfo)))
			DBGPRINTF(DEBUG_WARNING, "VR: Failed to sync actions.\n");
	}

	return true;
}

bool VR_EndFrame(XruContext_t *xrContext)
{
	if(!xrContext->sessionRunning)
		return false;
		
	XrCompositionLayerProjection projectionLayer={
		.type=XR_TYPE_COMPOSITION_LAYER_PROJECTION,
		.next=NULL,
		.layerFlags=0,
		.space=xrContext->refSpace,
		.viewCount=2,
		.views=(const XrCompositionLayerProjectionView[]) { xrContext->projViews[0], xrContext->projViews[1] },
	};

	const XrCompositionLayerBaseHeader *layers[1]={ (const XrCompositionLayerBaseHeader *const)&projectionLayer };

	XrFrameEndInfo frameEndInfo=
	{
		.type=XR_TYPE_FRAME_END_INFO,
		.next=XR_NULL_HANDLE,
		.displayTime=xrContext->frameState.predictedDisplayTime,
		.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
	};

	if(xrContext->frameState.shouldRender)
	{
		if(!xruCheck(xrContext->instance, xrReleaseSwapchainImage(xrContext->swapchain[0].swapchain, &(XrSwapchainImageReleaseInfo) {.type=XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, .next=XR_NULL_HANDLE })))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to release swapchain image 0.\n");
			return false;
		}

		if(!xruCheck(xrContext->instance, xrReleaseSwapchainImage(xrContext->swapchain[1].swapchain, &(XrSwapchainImageReleaseInfo) {.type=XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, .next=XR_NULL_HANDLE })))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to release swapchain image 1.\n");
			return false;
		}

		frameEndInfo.layerCount=1;
		frameEndInfo.layers=layers;
	}

	if(!xruCheck(xrContext->instance, xrEndFrame(xrContext->session, &frameEndInfo)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: xrEndFrame() was not successful.\n");
		return false;
	}

	return true;
}

matrix VR_GetEyeProjection(XruContext_t *xrContext, uint32_t Eye)
{
	const float Left=tanf(xrContext->projViews[Eye].fov.angleLeft);
	const float Right=tanf(xrContext->projViews[Eye].fov.angleRight);
	const float Down=tanf(xrContext->projViews[Eye].fov.angleDown);
	const float Up=tanf(xrContext->projViews[Eye].fov.angleUp);
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
matrix VR_GetHeadPose(XruContext_t *xrContext, uint32_t Eye)
{
	XrPosef Pose=xrContext->projViews[Eye].pose;

	// Get a matrix from the orientation quaternion
	matrix PoseMat=QuatToMatrix(Vec4(
		Pose.orientation.x,
		Pose.orientation.y,
		Pose.orientation.z,
		Pose.orientation.w
	));

	// And set the translation directly, no need for an extra matrix multiply
	PoseMat.w=Vec4(Pose.position.x, Pose.position.y, Pose.position.z, 1.0f);

	return MatrixInverse(PoseMat);
}

static bool VR_InitSystem(XruContext_t *xrContext, const XrFormFactor formFactor)
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

	if(!xruCheck(xrContext->instance, xrCreateInstance(&instanceCreateInfo, &xrContext->instance)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR_Init: Failed to create OpenXR instance.\n");
		return false;
	}

	if(!xruCheck(xrContext->instance, xrGetInstanceProcAddr(xrContext->instance, "xrGetVulkanInstanceExtensionsKHR", (PFN_xrVoidFunction *)&xrGetVulkanInstanceExtensionsKHR)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get proc address for xrGetVulkanInstanceExtensionsKHR.\n");
		return false;
	}

	if(!xruCheck(xrContext->instance, xrGetInstanceProcAddr(xrContext->instance, "xrGetVulkanDeviceExtensionsKHR", (PFN_xrVoidFunction *)&xrGetVulkanDeviceExtensionsKHR)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get proc address for xrGetVulkanDeviceExtensionsKHR.\n");
		return false;
	}

	if(!xruCheck(xrContext->instance, xrGetInstanceProcAddr(xrContext->instance, "xrGetVulkanGraphicsDeviceKHR", (PFN_xrVoidFunction *)&xrGetVulkanGraphicsDeviceKHR)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get proc address for xrGetVulkanGraphicsDeviceKHR.\n");
		return false;
	}

	if(!xruCheck(xrContext->instance, xrGetInstanceProcAddr(xrContext->instance, "xrGetVulkanGraphicsRequirementsKHR", (PFN_xrVoidFunction *)&xrGetVulkanGraphicsRequirementsKHR)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get proc address for xrGetVulkanGraphicsRequirementsKHR.\n");
		return false;
	}

	XrInstanceProperties instanceProps={ .type=XR_TYPE_INSTANCE_PROPERTIES };

	if(!xruCheck(xrContext->instance, xrGetInstanceProperties(xrContext->instance, &instanceProps)))
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
		.formFactor=formFactor,
	};

	if(!xruCheck(xrContext->instance, xrGetSystem(xrContext->instance, &systemGetInfo, &xrContext->systemID)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get system for HMD form factor.\n");
		return false;
	}

	DBGPRINTF(DEBUG_INFO, "VR: Successfully got system with ID %llu for HMD form factor.\n", xrContext->systemID);

	uint32_t instanceExtensionNamesSize=0;
	if(!xruCheck(xrContext->instance, xrGetVulkanInstanceExtensionsKHR(xrContext->instance, xrContext->systemID, 0, &instanceExtensionNamesSize, XR_NULL_HANDLE)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get Vulkan Device Extensions.\n");
		return false;
	}

	char *instanceExtensionNames=Zone_Malloc(zone, instanceExtensionNamesSize);

	if(instanceExtensionNames==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to allocate memory for extension names.\n");
		return false;
	}

	if(!xruCheck(xrContext->instance, xrGetVulkanInstanceExtensionsKHR(xrContext->instance, xrContext->systemID, instanceExtensionNamesSize, &instanceExtensionNamesSize, instanceExtensionNames)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get Vulkan Device Extensions.\n");
		return false;
	}

	DBGPRINTF(DEBUG_INFO, "VR: Instance extension requirements: %s\n", instanceExtensionNames);
	Zone_Free(zone, instanceExtensionNames);

	uint32_t extensionNamesSize=0;
	if(!xruCheck(xrContext->instance, xrGetVulkanDeviceExtensionsKHR(xrContext->instance, xrContext->systemID, 0, &extensionNamesSize, XR_NULL_HANDLE)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get Vulkan Device Extensions.\n");
		return false;
	}

	char *extensionNames=Zone_Malloc(zone, extensionNamesSize);

	if(extensionNames==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to allocate memory for extension names.\n");
		return false;
	}

	if(!xruCheck(xrContext->instance, xrGetVulkanDeviceExtensionsKHR(xrContext->instance, xrContext->systemID, extensionNamesSize, &extensionNamesSize, extensionNames)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get Vulkan Device Extensions.\n");
		return false;
	}

	DBGPRINTF(DEBUG_INFO, "VR: Extension requirements: %s\n", extensionNames);
	Zone_Free(zone, extensionNames);

	XrSystemProperties systemProps={ .type=XR_TYPE_SYSTEM_PROPERTIES };

	if(!xruCheck(xrContext->instance, xrGetSystemProperties(xrContext->instance, xrContext->systemID, &systemProps)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get System properties\n");
		return false;
	}

	DBGPRINTF(DEBUG_INFO, "VR: System properties for system %llu: \"%s\", vendor ID %X\n", systemProps.systemId, systemProps.systemName, systemProps.vendorId);
	DBGPRINTF(DEBUG_INFO, "\tMax layers          : %d\n", systemProps.graphicsProperties.maxLayerCount);
	DBGPRINTF(DEBUG_INFO, "\tMax swapchain height: %d\n", systemProps.graphicsProperties.maxSwapchainImageHeight);
	DBGPRINTF(DEBUG_INFO, "\tMax swapchain width : %d\n", systemProps.graphicsProperties.maxSwapchainImageWidth);
	DBGPRINTF(DEBUG_INFO, "\tOrientation Tracking: %d\n", systemProps.trackingProperties.orientationTracking);
	DBGPRINTF(DEBUG_INFO, "\tPosition Tracking   : %d\n", systemProps.trackingProperties.positionTracking);

	return true;
}

static bool VR_GetViewConfig(XruContext_t *xrContext, const XrViewConfigurationType viewType)
{
	if(xrEnumerateViewConfigurationViews(xrContext->instance, xrContext->systemID, viewType, 0, &xrContext->viewCount, NULL)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get view configuration view count.\n");
		return false;
	}

	if(xrContext->viewCount>2)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Device supports more views than application supports.\n");
		return false;
	}

	if(xrEnumerateViewConfigurationViews(xrContext->instance, xrContext->systemID, viewType, xrContext->viewCount, &xrContext->viewCount, xrContext->viewConfigViews)!=XR_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to enumerate view configuration views.\n");
		return false;
	}

	for(uint32_t i=0;i<xrContext->viewCount;i++)
	{
		DBGPRINTF(DEBUG_INFO, "VR: View Configuration View %d:\n", i);
		DBGPRINTF(DEBUG_INFO, "\tResolution: Recommended %dx%d, Max: %dx%d\n",
				  xrContext->viewConfigViews[i].recommendedImageRectWidth,
				  xrContext->viewConfigViews[i].recommendedImageRectHeight,
				  xrContext->viewConfigViews[i].maxImageRectWidth,
				  xrContext->viewConfigViews[i].maxImageRectHeight);
		DBGPRINTF(DEBUG_INFO, "\tSwapchain Samples: Recommended: %d, Max: %d)\n",
				  xrContext->viewConfigViews[i].recommendedSwapchainSampleCount,
				  xrContext->viewConfigViews[i].maxSwapchainSampleCount);
	}

	xrContext->viewType=viewType;

	// Set half recommended res
	xrContext->swapchainExtent.width=xrContext->viewConfigViews[0].recommendedImageRectWidth/2;
	xrContext->swapchainExtent.height=xrContext->viewConfigViews[0].recommendedImageRectHeight/2;

	return true;
}

static bool VR_InitSession(XruContext_t *xrContext, VkInstance instance, VkuContext_t *context)
{
	// Check required Vulkan extensions
	if(!(context->externalMemoryExtension&&context->externalFenceExtension&&context->externalSemaphoreExtension&&context->getMemoryRequirements2Extension&&context->dedicatedAllocationExtension))
		return false;

	XrGraphicsRequirementsVulkanKHR graphicsRequirements={ .type=XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR };

	if(!xruCheck(xrContext->instance, xrGetVulkanGraphicsRequirementsKHR(xrContext->instance, xrContext->systemID, &graphicsRequirements)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get Vulkan graphics requirements.\n");
		return false;
	}

	VkPhysicalDevice physicalDevice=VK_NULL_HANDLE;
	xrGetVulkanGraphicsDeviceKHR(xrContext->instance, xrContext->systemID, instance, &physicalDevice);

	if(physicalDevice!=context->physicalDevice)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Physical device mismatch.\n");
		return false;
	}

	XrGraphicsBindingVulkanKHR graphicsBindingVulkan=
	{
		.type=XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR,
		.next=XR_NULL_HANDLE,
		.instance=instance,
		.physicalDevice=context->physicalDevice,
		.device=context->device,
		.queueFamilyIndex=context->graphicsQueueIndex,
		.queueIndex=0
	};

	XrSessionCreateInfo sessionCreateInfo=
	{
		.type=XR_TYPE_SESSION_CREATE_INFO,
		.next=(void *)&graphicsBindingVulkan,
		.systemId=xrContext->systemID
	};

	if(!xruCheck(xrContext->instance, xrCreateSession(xrContext->instance, &sessionCreateInfo, &xrContext->session)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to create session.\n");
		return false;
	}

	DBGPRINTF(DEBUG_INFO, "VR: Successfully created a Vulkan session.\n");

	return true;
}

static bool VR_InitReferenceSpace(XruContext_t *xrContext, XrReferenceSpaceType spaceType)
{
	XrReferenceSpaceCreateInfo refSpaceCreateInfo=
	{
		.type=XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
		.next=NULL,
		.referenceSpaceType=spaceType,
		.poseInReferenceSpace.orientation={ .x=0.0f, .y=0.0f, .z=0.0f, .w=1.0f },
		.poseInReferenceSpace.position={ .x=0.0f, .y=0.0f, .z=0.0f }
	};

	if(!xruCheck(xrContext->instance, xrCreateReferenceSpace(xrContext->session, &refSpaceCreateInfo, &xrContext->refSpace)))
	{
		DBGPRINTF(DEBUG_INFO, "VR: Failed to create play space.\n");
		return false;
	}

	return true;
}

static bool VR_InitSwapchain(XruContext_t *xrContext, VkuContext_t *context)
{
	uint32_t swapchainFormatCount;
	if(!xruCheck(xrContext->instance, xrEnumerateSwapchainFormats(xrContext->session, 0, &swapchainFormatCount, NULL)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get number of supported swapchain formats.\n");
		return false;
	}

	DBGPRINTF(DEBUG_INFO, "VR: Runtime supports %d swapchain formats.\n", swapchainFormatCount);

	int64_t *swapchainFormats=Zone_Malloc(zone, sizeof(int64_t)*swapchainFormatCount);

	if(swapchainFormats==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to allocate memory for swapchain formats.\n");
		return false;
	}

	if(!xruCheck(xrContext->instance, xrEnumerateSwapchainFormats(xrContext->session, swapchainFormatCount, &swapchainFormatCount, swapchainFormats)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to enumerate swapchain formats.\n");
		return false;
	}

	VkFormat PreferredFormats[]=
	{
		VK_FORMAT_B8G8R8A8_SRGB,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_R8G8B8A8_UNORM
	};
	bool FoundFormat=false;

	for(uint32_t i=0;i<sizeof(PreferredFormats)/sizeof(PreferredFormats[0]);i++)
	{
		for(uint32_t j=0;j<swapchainFormatCount;j++)
		{
			if(swapchainFormats[j]==PreferredFormats[i])
			{
				xrContext->swapchainFormat=swapchainFormats[j];
				FoundFormat=true;
				break;
			}
		}

		if(FoundFormat)
			break;
	}

	Zone_Free(zone, swapchainFormats);

	// Create swapchain images, imageviews, and transition to correct image layout
	VkCommandBuffer CommandBuffer=vkuOneShotCommandBufferBegin(context);

	for(uint32_t i=0;i<xrContext->viewCount;i++)
	{
		XrSwapchainCreateInfo swapchainCreateInfo=
		{
			.type=XR_TYPE_SWAPCHAIN_CREATE_INFO, .next=NULL,
			.createFlags=0, .usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT,
			.format=xrContext->swapchainFormat, .sampleCount=VK_SAMPLE_COUNT_1_BIT,
			.width=xrContext->swapchainExtent.width, .height=xrContext->swapchainExtent.height,
			.faceCount=1, .arraySize=1, .mipCount=1,
		};

		if(!xruCheck(xrContext->instance, xrCreateSwapchain(xrContext->session, &swapchainCreateInfo, &xrContext->swapchain[i].swapchain)))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to create swapchain %d.\n", i);
			return false;
		}

		xrContext->swapchain[i].numImages=0;
		if(!xruCheck(xrContext->instance, xrEnumerateSwapchainImages(xrContext->swapchain[i].swapchain, 0, &xrContext->swapchain[i].numImages, NULL)))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to enumerate swapchain image count.\n");
			return false;
		}

		if(xrContext->swapchain[i].numImages>VKU_MAX_FRAME_COUNT)
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Swapchain image count is higher than application supports.\n");
			return false;
		}

		for(uint32_t j=0;j<xrContext->swapchain[i].numImages;j++)
			xrContext->swapchain[i].images[j].type=XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR;

		if(!xruCheck(xrContext->instance, xrEnumerateSwapchainImages(xrContext->swapchain[i].swapchain, xrContext->swapchain[i].numImages, &xrContext->swapchain[i].numImages, (XrSwapchainImageBaseHeader *)xrContext->swapchain[i].images)))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to enumerate swapchain images.\n");
			return false;
		}

		for(uint32_t j=0;j<xrContext->swapchain[i].numImages;j++)
		{
			vkuTransitionLayout(CommandBuffer, xrContext->swapchain[i].images[j].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

			vkCreateImageView(context->device, &(VkImageViewCreateInfo)
			{
				.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext=VK_NULL_HANDLE,
				.image=xrContext->swapchain[i].images[j].image,
				.format=xrContext->swapchainFormat,
				.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
				.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
				.subresourceRange.baseMipLevel=0,
				.subresourceRange.levelCount=1,
				.subresourceRange.baseArrayLayer=0,
				.subresourceRange.layerCount=1,
				.viewType=VK_IMAGE_VIEW_TYPE_2D,
				.flags=0,
			}, VK_NULL_HANDLE, &xrContext->swapchain[i].imageView[j]);
		}
	}

	vkuOneShotCommandBufferEnd(context, CommandBuffer);

	return true;
}

static XrAction VR_CreateAction(XruContext_t *xrContext, const char *name, XrActionType type, const uint32_t numSubPaths, XrPath *subPaths)
{
	XrActionCreateInfo actionCreateInfo={ .type=XR_TYPE_ACTION_CREATE_INFO, .actionType=type };

	strcpy(actionCreateInfo.actionName, name);
	strcpy(actionCreateInfo.localizedActionName, name);

	actionCreateInfo.countSubactionPaths=numSubPaths;
	actionCreateInfo.subactionPaths=subPaths;

	XrAction action=XR_NULL_HANDLE;
	if(!xruCheck(xrContext->instance, xrCreateAction(xrContext->actionSet, &actionCreateInfo, &action)))
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to create action %s %d.\n", name, type);

	return action;
}

static XrActionSet VR_CreateActionSet(XruContext_t *xrContext, const char *name)
{
	XrActionSetCreateInfo actionSetCreateInfo={ .type=XR_TYPE_ACTION_SET_CREATE_INFO, .priority=0 };

	strcpy(actionSetCreateInfo.actionSetName, name);
	strcpy(actionSetCreateInfo.localizedActionSetName, name);

	XrActionSet actionSet=XR_NULL_HANDLE;
	if(!xruCheck(xrContext->instance, xrCreateActionSet(xrContext->instance, &actionSetCreateInfo, &actionSet)))
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to initialize action set.\n");

	return actionSet;
}

static XrSpace VR_CreateActionSpace(XruContext_t *xrContext, XrAction action, uint32_t hand)
{
	XrActionSpaceCreateInfo actionSpaceCreateInfo=
	{
		.type=XR_TYPE_ACTION_SPACE_CREATE_INFO,
		.poseInActionSpace={ { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } },
		.action=action,
		.subactionPath=xrContext->handPath[hand]
	};

	XrSpace space=XR_NULL_HANDLE;
	if(!xruCheck(xrContext->instance, xrCreateActionSpace(xrContext->session, &actionSpaceCreateInfo, &space)))
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to create action space.\n");

	return space;
}

static XrPath xruGetPath(XrInstance instance, const char *path)
{
	XrPath xrPath=XR_NULL_HANDLE;
	if(!xruCheck(instance, xrStringToPath(instance, path, &xrPath)))
		DBGPRINTF(DEBUG_WARNING, "VR: xruGetPath invalid path (%s).\n", path);

	return xrPath;
}

static void VR_SuggestBindings(XruContext_t *xrContext)
{
	XrActionSuggestedBinding suggestedBindings[]=
	{
		{ xrContext->handPose, xruGetPath(xrContext->instance, "/user/hand/left/input/aim/pose") },
		{ xrContext->handTrigger, xruGetPath(xrContext->instance, "/user/hand/left/input/trigger/value") },
		{ xrContext->handGrip, xruGetPath(xrContext->instance, "/user/hand/left/input/squeeze/value") },
		{ xrContext->handThumbstick, xruGetPath(xrContext->instance, "/user/hand/left/input/thumbstick") },

		{ xrContext->handPose, xruGetPath(xrContext->instance, "/user/hand/right/input/aim/pose") },
		{ xrContext->handTrigger, xruGetPath(xrContext->instance, "/user/hand/right/input/trigger/value") },
		{ xrContext->handGrip, xruGetPath(xrContext->instance, "/user/hand/right/input/squeeze/value") },
		{ xrContext->handThumbstick, xruGetPath(xrContext->instance, "/user/hand/right/input/thumbstick") },
	};

	XrInteractionProfileSuggestedBinding suggestedBinding=
	{
		.type=XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
		.countSuggestedBindings=sizeof(suggestedBindings)/sizeof(XrActionSuggestedBinding),
		.suggestedBindings=suggestedBindings
	};

	suggestedBinding.interactionProfile=xruGetPath(xrContext->instance, "/interaction_profiles/oculus/touch_controller");
	if(!xruCheck(xrContext->instance, xrSuggestInteractionProfileBindings(xrContext->instance, &suggestedBinding)))
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to suggest bindings for Oculus touch controller.\n");

	suggestedBinding.interactionProfile=xruGetPath(xrContext->instance, "/interaction_profiles/htc/vive_controller");
	if(!xruCheck(xrContext->instance, xrSuggestInteractionProfileBindings(xrContext->instance, &suggestedBinding)))
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to suggest bindings for HTC Vive controller.\n");

	suggestedBinding.interactionProfile=xruGetPath(xrContext->instance, "/interaction_profiles/khr/simple_controller");
	if(!xruCheck(xrContext->instance, xrSuggestInteractionProfileBindings(xrContext->instance, &suggestedBinding)))
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to suggest bindings for KHRONOS simple controller.\n");
}

static void VR_AttachActionSet(XruContext_t *xrContext, XrActionSet actionSet)
{
	XrSessionActionSetsAttachInfo actionSetsAttachInfo=
	{
		.type=XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
		.countActionSets=1, .actionSets=&actionSet
	};

	if(!xruCheck(xrContext->instance, xrAttachSessionActionSets(xrContext->session, &actionSetsAttachInfo)))
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to attach action set.\n");
}

XrPosef VR_GetActionPose(XruContext_t *xrContext, const XrAction action, const XrSpace actionSpace, uint32_t hand)
{
	const XrPosef defaultPose={ { 0.0f, 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 0.0f } };
	XrActionStateGetInfo getInfo=
	{
		.type=XR_TYPE_ACTION_STATE_GET_INFO,
		.action=action,
		.subactionPath=xrContext->handPath[hand]
	};
	XrActionStatePose state={ .type=XR_TYPE_ACTION_STATE_POSE };

	if(!xruCheck(xrContext->instance, xrGetActionStatePose(xrContext->session, &getInfo, &state)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get action pose state.\n");
		return defaultPose;
	}

	if(state.isActive)
	{
		XrSpaceLocation location={ .type=XR_TYPE_SPACE_LOCATION };

		if(!xruCheck(xrContext->instance, xrLocateSpace(actionSpace, xrContext->refSpace, xrContext->frameState.predictedDisplayTime, &location)))
		{
			DBGPRINTF(DEBUG_ERROR, "VR: Failed to get action pose.\n");
			return defaultPose;
		}

		if(!(location.locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT))
			return defaultPose;

		return location.pose;
	}

	return defaultPose;
}

bool VR_GetActionBoolean(XruContext_t *xrContext, XrAction action, uint32_t hand)
{
	XrActionStateGetInfo getInfo=
	{
		.type=XR_TYPE_ACTION_STATE_GET_INFO,
		.action=action,
		.subactionPath=xrContext->handPath[hand]
	};
	XrActionStateBoolean state={ .type=XR_TYPE_ACTION_STATE_BOOLEAN };

	if(!xruCheck(xrContext->instance, xrGetActionStateBoolean(xrContext->session, &getInfo, &state)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get boolean action state.\n");
		return false;
	}

	return state.currentState;
}

float VR_GetActionFloat(XruContext_t *xrContext, XrAction action, uint32_t hand)
{
	XrActionStateGetInfo getInfo=
	{
		.type=XR_TYPE_ACTION_STATE_GET_INFO,
		.action=action,
		.subactionPath=xrContext->handPath[hand]
	};
	XrActionStateFloat state={ .type=XR_TYPE_ACTION_STATE_FLOAT };

	if(!xruCheck(xrContext->instance, xrGetActionStateFloat(xrContext->session, &getInfo, &state)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get boolean action state.\n");
		return false;
	}

	return state.currentState;
}

vec2 VR_GetActionVec2(XruContext_t *xrContext, XrAction action, uint32_t hand)
{
	XrActionStateGetInfo getInfo=
	{
		.type=XR_TYPE_ACTION_STATE_GET_INFO,
		.action=action,
		.subactionPath=xrContext->handPath[hand]
	};
	XrActionStateVector2f state={ .type=XR_TYPE_ACTION_STATE_VECTOR2F };

	if(!xruCheck(xrContext->instance, xrGetActionStateVector2f(xrContext->session, &getInfo, &state)))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: Failed to get boolean action state.\n");
		return Vec2(0.0f, 0.0f);
	}

	return Vec2(state.currentState.x, state.currentState.y);
}

bool VR_Init(XruContext_t *xrContext, VkInstance instance, VkuContext_t *context)
{
	xrContext->instance=XR_NULL_HANDLE;

	xrContext->systemID=XR_NULL_SYSTEM_ID;

	xrContext->viewCount=0;

	xrContext->viewConfigViews[0].type=XR_TYPE_VIEW_CONFIGURATION_VIEW;
	xrContext->viewConfigViews[1].type=XR_TYPE_VIEW_CONFIGURATION_VIEW;

	xrContext->projViews[0].type=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
	xrContext->projViews[0].pose.orientation=(XrQuaternionf){ 0.0f, 0.0f, 0.0f, 1.0f };
	xrContext->projViews[0].pose.position=(XrVector3f){ 0.0f, 0.0f, 0.0f };
	xrContext->projViews[1].type=XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
	xrContext->projViews[1].pose.orientation=(XrQuaternionf){ 0.0f, 0.0f, 0.0f, 1.0f };
	xrContext->projViews[1].pose.position=(XrVector3f){ 0.0f, 0.0f, 0.0f };

	xrContext->session=XR_NULL_HANDLE;

	xrContext->refSpace=XR_NULL_HANDLE;

	xrContext->frameState.type=XR_TYPE_FRAME_STATE;

	xrContext->sessionState=XR_SESSION_STATE_UNKNOWN;

	xrContext->exitRequested=false;
	xrContext->sessionRunning=false;

	xrContext->actionSet=XR_NULL_HANDLE;

	xrContext->handPose=XR_NULL_HANDLE;
	xrContext->handTrigger=XR_NULL_HANDLE;
	xrContext->handThumbstick=XR_NULL_HANDLE;

	xrContext->leftHandSpace=XR_NULL_HANDLE;
	xrContext->rightHandSpace=XR_NULL_HANDLE;

	if(!VR_InitSystem(xrContext, XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: VR_InitSystem failed.\n");
		return false;
	}

	if(!VR_GetViewConfig(xrContext, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: VR_GetViewConfig failed.\n");
		return false;
	}

	if(!VR_InitSession(xrContext, instance, context))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: VR_InitSession failed.\n");
		return false;
	}

	if(!VR_InitReferenceSpace(xrContext, XR_REFERENCE_SPACE_TYPE_LOCAL))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: VR_InitReferenceSpace failed.\n");
		return false;
	}

	if(!VR_InitSwapchain(xrContext, context))
	{
		DBGPRINTF(DEBUG_ERROR, "VR: VR_InitSwapchain failed.\n");
		return false;
	}

	xrContext->actionSet=VR_CreateActionSet(xrContext, "vkengine_actions");

	xrContext->handPath[0]=xruGetPath(xrContext->instance, "/user/hand/left");
	xrContext->handPath[1]=xruGetPath(xrContext->instance, "/user/hand/right");

	xrContext->handPose=VR_CreateAction(xrContext, "pose", XR_ACTION_TYPE_POSE_INPUT, 2, xrContext->handPath);
	xrContext->leftHandSpace=VR_CreateActionSpace(xrContext, xrContext->handPose, 0);
	xrContext->rightHandSpace=VR_CreateActionSpace(xrContext, xrContext->handPose, 1);

	xrContext->handTrigger=VR_CreateAction(xrContext, "trigger", XR_ACTION_TYPE_FLOAT_INPUT, 2, xrContext->handPath);
	xrContext->handGrip=VR_CreateAction(xrContext, "squeeze", XR_ACTION_TYPE_FLOAT_INPUT, 2, xrContext->handPath);
	xrContext->handThumbstick=VR_CreateAction(xrContext, "thumbstick", XR_ACTION_TYPE_VECTOR2F_INPUT, 2, xrContext->handPath);

	VR_SuggestBindings(xrContext);

	VR_AttachActionSet(xrContext, xrContext->actionSet);

	return true;
}

void VR_Destroy(XruContext_t *xrContext)
{
	if(xrContext->session)
	{
		xrEndSession(xrContext->session);
		xrDestroySession(xrContext->session);
	}

	if(xrContext->swapchain[0].swapchain)
		xrDestroySwapchain(xrContext->swapchain[0].swapchain);

	if(xrContext->swapchain[1].swapchain)
		xrDestroySwapchain(xrContext->swapchain[1].swapchain);

	if(xrContext->instance)
		xrDestroyInstance(xrContext->instance);
}
