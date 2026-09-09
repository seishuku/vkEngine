#ifndef __CUBE_H__
#define __CUBE_H__

bool CreateCubePipeline(void);
void DestroyCube(void);
void DrawCube(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, vec3 position, vec3 size, vec4 color);
void DrawCubePushConstant(VkCommandBuffer commandBuffer, uint32_t index, size_t constantSize, void *constant);

#endif
