#ifndef __COMPOSITE_H__
#define __COMPOSITE_H__

extern VkPipelineLayout CompositePipelineLayout;
extern VkuPipeline_t CompositePipeline;
extern VkRenderPass CompositeRenderPass;

bool CreateCompositePipeline(void);
void CompositeDraw(uint32_t Index);

#endif
