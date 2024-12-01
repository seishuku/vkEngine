#ifndef __COMPOSITE_H__
#define __COMPOSITE_H__

#include "../vulkan/vulkan.h"

extern VkuImage_t colorResolve[2];
extern VkuImage_t colorBlur[2];
extern VkuImage_t colorTemp[2];

extern VkPipelineLayout compositePipelineLayout;
extern VkuPipeline_t compositePipeline;

bool CreateCompositePipeline(void);
void CreateCompositeFramebuffers(uint32_t eye);
void DestroyCompositeFramebuffers(void);
void DestroyComposite(void);
void CompositeDraw(uint32_t index, uint32_t eye);

#endif
