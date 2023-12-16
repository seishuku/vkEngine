#ifndef __VR_H__
#define __VR_H__

#include "../vulkan/vulkan.h"
#include <openxr/openxr.h>
#define XR_USE_GRAPHICS_API_VULKAN
#include <openxr/openxr_platform.h>

// undefine __WIN32 for mingw/msys building, otherwise it tries to define bool
#undef __WIN32

extern uint32_t rtWidth;
extern uint32_t rtHeight;

typedef struct
{
	XrSwapchain Swapchain;
	uint32_t NumImages;
	XrSwapchainImageVulkanKHR Images[VKU_MAX_FRAME_COUNT];
	VkImageView ImageView[VKU_MAX_FRAME_COUNT];
} XruSwapchain_t;

extern XruSwapchain_t xrSwapchain[2];

bool VR_StartFrame(uint32_t *eyeIndex1, uint32_t *eyeIndex2);
bool VR_EndFrame(void);
matrix VR_GetEyeProjection(uint32_t Eye);
matrix VR_GetHeadPose(void);
bool VR_Init(VkInstance Instance, VkuContext_t *Context);
void VR_Destroy(void);

#endif
