#ifndef __NEBULA_H__
#define __NEBULA_H__

VkBool32 GenNebulaVolume(VkuImage_t *image);
bool CreateVolumePipeline(void);
void DestroyVolume(void);
void DrawVolume(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, VkDescriptorPool descriptorPool);

#endif
