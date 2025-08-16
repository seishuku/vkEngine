#ifndef __NEBULA_H__
#define __NEBULA_H__

VkBool32 InitNebulaVolume(void);
VkBool32 GenNebulaVolume(void);
VkBool32 FluidStep(void);
bool CreateVolumePipeline(void);
void DestroyVolume(void);
void DrawVolume(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, VkDescriptorPool descriptorPool);

#endif
