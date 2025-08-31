#ifndef __VR_H__
#define __VR_H__

#include "../vulkan/vulkan.h"

#ifdef ANDROID
#ifndef XR_USE_PLATFORM_ANDROID
#define XR_USE_PLATFORM_ANDROID
#endif
#endif

#ifdef WIN32
#ifndef XR_USE_PLATFORM_WIN32
#define XR_USE_PLATFORM_WIN32
#endif
#endif

#ifdef LINUX
#ifdef WAYLAND
#ifndef XR_USE_PLATFORM_WAYLAND
#define XR_USE_PLATFORM_WAYLAND
#endif
#else
#ifndef XR_USE_PLATFORM_XLIB
#define XR_USE_PLATFORM_XLIB
#endif
#endif
#endif

#ifndef XR_USE_GRAPHICS_API_VULKAN
#define XR_USE_GRAPHICS_API_VULKAN
#endif

#include <openxr/openxr.h>
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

bool VR_StartFrame(XruContext_t *xrContext, uint32_t *imageIndex);
bool VR_EndFrame(XruContext_t *xrContext);
matrix VR_GetEyeProjection(XruContext_t *xrContext, uint32_t eye);
matrix VR_GetHeadPose(XruContext_t *xrContext, uint32_t eye);
XrPosef VR_GetActionPose(XruContext_t *xrContext, const XrAction action, const XrSpace actionSpace, uint32_t hand);
bool VR_GetActionBoolean(XruContext_t *xrContext, XrAction action, uint32_t hand);
float VR_GetActionFloat(XruContext_t *xrContext, XrAction action, uint32_t hand);
vec2 VR_GetActionVec2(XruContext_t *xrContext, XrAction action, uint32_t hand);
bool VR_Init(XruContext_t *xrContext, VkInstance instance, VkuContext_t *context);
void VR_Destroy(XruContext_t *xrContext);

#endif
