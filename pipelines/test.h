#ifndef __TEST_H__
#define __TEST_H__

#include <stdint.h>
#include "../vulkan/vulkan.h"
#include "../utils/pipeline.h"
#include "../entitylist.h"

extern VkRenderPass renderPass;

bool CreateTestPipeline(void);
void DestroyTest(void);
void DrawTest(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, VkDescriptorPool descriptorPool);

#endif
