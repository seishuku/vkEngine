#ifndef __FONT_H__
#define __FONT_H__

#include "../vulkan/vulkan.h"

void Font_Print(VkCommandBuffer CommandBuffer, uint32_t Eye, float x, float y, char *string, ...);
void Font_Destroy(void);

#endif
