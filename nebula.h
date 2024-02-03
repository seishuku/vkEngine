#ifndef __NEBULA_H__
#define __NEBULA_H__

// Volume rendering vulkan stuff
extern VkuDescriptorSet_t volumeDescriptorSet;
extern VkPipelineLayout volumePipelineLayout;
extern VkuPipeline_t volumePipeline;
extern VkRenderPass volumeRenderPass;
//////

VkBool32 GenNebulaVolume(VkuContext_t *Context, VkuImage_t *image);
bool CreateVolumePipeline(void);

#endif
