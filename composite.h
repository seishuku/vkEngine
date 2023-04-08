#ifndef __COMPOSITE_H__
#define __COMPOSITE_H__

#include "vulkan/vulkan.h"

extern VkuImage_t ColorResolve[2];
extern VkuImage_t ColorBlur[2];
extern VkuImage_t ColorTemp[2];

extern VkPipelineLayout CompositePipelineLayout;
extern VkuPipeline_t CompositePipeline;

bool CreateCompositePipeline(void);
void CreateCompositeFramebuffers(uint32_t Eye, uint32_t targetWidth, uint32_t targetHeight);
void DestroyComposite(void);
void CompositeDraw(uint32_t Index, uint32_t Eye);

#endif
