#ifndef __LIGHTING_H__
#define __LIGHTING_H__

#include <stdint.h>
#include "../vulkan/vulkan.h"
#include "../utils/pipeline.h"

extern VkRenderPass renderPass;
extern Pipeline_t mainPipeline;

bool CreateLightingPipeline(void);
void DestroyLighting(void);
void DrawLighting(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, VkDescriptorPool descriptorPool);

#endif
