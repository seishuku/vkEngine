#ifndef __VR_H__
#define __VR_H__

#include "../vulkan/vulkan.h"

// undefine __WIN32 for mingw/msys building, otherwise it tries to define bool
#undef __WIN32

extern uint32_t rtWidth;
extern uint32_t rtHeight;

extern matrix EyeProjection[2];

bool VR_SendEyeImages(VkCommandBuffer CommandBuffer, VkImage Eyes[2], VkFormat Format, uint32_t Width, uint32_t Height);
matrix VR_GetEyeProjection(uint32_t Eye);
matrix VR_GetHeadPose(void);
bool VR_Init(VkInstance Instance, VkuContext_t *Context);
void VR_Destroy(void);

#endif
