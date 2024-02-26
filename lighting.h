#ifndef __LIGHTING_H__
#define __LIGHTING_H__

#include <stdint.h>
#include "vulkan/vulkan.h"

extern VkuDescriptorSet_t mainDescriptorSet;
extern VkPipelineLayout mainPipelineLayout;
extern VkuPipeline_t mainPipeline;

bool CreateLightingPipeline(void);
void DestroyLighting(void);
void DrawLighting(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, VkDescriptorPool descriptorPool);

#endif
