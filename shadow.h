#ifndef __SHADOW_H__
#define __SHADOW_H__

#include "vulkan/vulkan.h"
#include "math/math.h"

typedef struct
{
	matrix mvp;
	matrix local;
} Shadow_UBO_t;

extern Shadow_UBO_t Shadow_UBO;

extern VkuImage_t ShadowDepth;

void CreateShadowMap(void);
bool CreateShadowPipeline(void);
void ShadowUpdateMap(VkCommandBuffer CommandBuffer, uint32_t FrameIndex);
void DestroyShadow(void);

#endif
