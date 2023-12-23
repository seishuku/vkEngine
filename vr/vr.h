#ifndef __VR_H__
#define __VR_H__

#include "../vulkan/vulkan.h"

#ifdef ANDROID
#define XR_USE_PLATFORM_ANDROID
#endif

#ifdef WIN32
#define XR_USE_PLATFORM_WIN32
#endif

#ifdef LINUX
#define XR_USE_PLATFORM_XLIB
#endif

#include <openxr/openxr.h>
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>

// undefine __WIN32 for mingw/msys building, otherwise it tries to define bool
#undef __WIN32

typedef struct
{
	XrSwapchain swapchain;
	uint32_t numImages;
	XrSwapchainImageVulkanKHR images[VKU_MAX_FRAME_COUNT];
	VkImageView imageView[VKU_MAX_FRAME_COUNT];
} XruSwapchain_t;

typedef struct
{
	XrInstance instance;
	XrSystemId systemID;

	uint32_t viewCount;
	XrViewConfigurationView viewConfigViews[2];
	XrViewConfigurationType viewType;

	XrExtent2Di swapchainExtent;
	int64_t swapchainFormat;
	XruSwapchain_t swapchain[2];

	XrCompositionLayerProjectionView projViews[2];

	XrSpace refSpace;

	XrSession session;
	XrSessionState sessionState;

	bool exitRequested;
	bool sessionRunning;

	XrFrameState frameState;

	XrActionSet actionSet;

	XrPath handPath[2];
	XrAction handPose, handTrigger, handGrip, handThumbstick;
	XrSpace leftHandSpace, rightHandSpace;
} XruContext_t;

bool VR_StartFrame(XruContext_t *xrContext);
bool VR_EndFrame(XruContext_t *xrContext);
matrix VR_GetEyeProjection(XruContext_t *xrContext, uint32_t Eye);
matrix VR_GetHeadPose(XruContext_t *xrContext);
XrPosef VR_GetActionPose(XruContext_t *xrContext, const XrAction action, const XrSpace actionSpace, uint32_t hand);
bool VR_GetActionBoolean(XruContext_t *xrContext, XrAction action, uint32_t hand);
float VR_GetActionFloat(XruContext_t *xrContext, XrAction action, uint32_t hand);
vec2 VR_GetActionVec2(XruContext_t *xrContext, XrAction action, uint32_t hand);
bool VR_Init(XruContext_t *xrContext, VkInstance Instance, VkuContext_t *Context);
void VR_Destroy(XruContext_t *xrContext);

#endif
