#ifndef __SHADOW_H__
#define __SHADOW_H__

#include "vulkan/vulkan.h"
#include "math/math.h"

typedef struct
{
	matrix mvp;
} Shadow_UBO_t;

extern Shadow_UBO_t shadowUBO;

extern VkuImage_t shadowDepth;

void CreateShadowMap(void);
bool CreateShadowPipeline(void);
void ShadowUpdateMap(VkCommandBuffer commandBuffer, uint32_t frameIndex);
void DestroyShadow(void);

#endif
