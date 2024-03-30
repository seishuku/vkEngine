#ifndef __SPHERE_H__
#define __SPHERE_H__

extern VkPipelineLayout spherePipelineLayout;
extern VkuPipeline_t spherePipeline;

bool CreateSpherePipeline(void);
void DestroySphere(void);
void DrawSphere(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, vec3 position, float radius, vec4 color);

#endif
