#ifndef __NEBULA_H__
#define __NEBULA_H__

// Volume rendering vulkan stuff
extern VkuDescriptorSet_t VolumeDescriptorSet;
extern VkPipelineLayout VolumePipelineLayout;
extern VkuPipeline_t VolumePipeline;
//////

VkBool32 GenNebulaVolume(VkuContext_t *Context, VkuImage_t *Image);
bool CreateVolumePipeline(void);

#endif
