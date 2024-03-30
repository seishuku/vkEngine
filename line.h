#ifndef __LINE_H__
#define __LINE_H__

extern VkPipelineLayout linePipelineLayout;
extern VkuPipeline_t linePipeline;

bool CreateLinePipeline(void);
void DestroyLine(void);
void DrawLine(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, vec3 start, vec3 end, vec4 color);

#endif
