#ifndef __SHADOW_H__
#define __SHADOW_H__

#include "../vulkan/vulkan.h"
#include "../math/math.h"

#ifndef NUM_CASCADES
#define NUM_CASCADES 4
#endif

extern matrix shadowMVP[NUM_CASCADES];
extern float cascadeSplits[NUM_CASCADES+1];
extern VkuImage_t shadowDepth;

void CreateShadowMap(void);
bool CreateShadowPipeline(void);
void ShadowUpdateMap(VkCommandBuffer commandBuffer, uint32_t frameIndex);
void DestroyShadow(void);

#endif
