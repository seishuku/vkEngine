#ifndef __TRIANGLE_H__
#define __TRIANGLE_H__

bool CreateTrianglePipeline(void);
void DestroyTriangle(void);
void DrawTriangle(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, vec3 verts[3], vec4 color);
void DrawTrianglePushConstant(VkCommandBuffer commandBuffer, size_t constantSize, void *constant);

#endif
