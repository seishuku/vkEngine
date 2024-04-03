#ifndef __SPHERE_H__
#define __SPHERE_H__

bool CreateSpherePipeline(void);
void DestroySphere(void);
void DrawSphere(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, vec3 position, float radius, vec4 color);
void DrawSpherePushConstant(VkCommandBuffer commandBuffer, uint32_t index, size_t constantSize, void *constant);

#endif
