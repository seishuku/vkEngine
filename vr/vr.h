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

#ifdef __MINGW32__
#include <unknwnbase.h>
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
	XrExtent2Di extent;
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

	int64_t swapchainFormat;
	XruSwapchain_t swapchain[2];

	XruSwapchain_t uiSwapchain;
	VkFramebuffer uiFramebuffer[VKU_MAX_FRAME_COUNT];
	VkRenderPass uiRenderPass;

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

typedef struct
{
	char *instanceExtensions;
	char *deviceExtensions;
} XruExtensionRequirements_t;

const char **xruSplitExtensionString(char *str, uint32_t *outCount);

bool xruStartFrame(XruContext_t *xrContext, uint32_t *imageIndex);
bool xruEndFrame(XruContext_t *xrContext);

matrix xruGetEyeProjection(XruContext_t *xrContext, uint32_t eye);
matrix xruGetHeadPose(XruContext_t *xrContext, uint32_t eye);
XrPosef xruGetActionPose(XruContext_t *xrContext, const XrAction action, const XrSpace actionSpace, uint32_t hand);
bool xruGetActionBoolean(XruContext_t *xrContext, XrAction action, uint32_t hand);
float xruGetActionFloat(XruContext_t *xrContext, XrAction action, uint32_t hand);
vec2 xruGetActionVec2(XruContext_t *xrContext, XrAction action, uint32_t hand);

bool xruInitSystem(XruContext_t *xrContext, const XrFormFactor formFactor, XruExtensionRequirements_t *requiredExtensions);
bool xruInit(XruContext_t *xrContext, VkInstance instance, VkuContext_t *context);

void xruDestroy(XruContext_t *xrContext);

#endif
