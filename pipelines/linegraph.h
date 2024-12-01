#ifndef __LINEGRAPH_H__
#define __LINEGRAPH_H__

typedef struct
{
	vec2 size, position;
	vec4 color;
	VkuBuffer_t buffer;
	float currentTime, updateRate;
	float currentValue, min, max;
} LineGraph_t;

bool CreateLineGraphPipeline(void);
void DestroyLineGraphPipeline(void);

bool CreateLineGraph(LineGraph_t *lineGraph, const uint32_t numPoints, const float updateRate, const float min, const float max, const vec2 size, const vec2 position, vec4 color);
void DestroyLineGraph(LineGraph_t *lineGraph);
void UpdateLineGraph(LineGraph_t *lineGraph, const float value, const float deltaTime);
void DrawLineGraph(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, LineGraph_t *lineGraph);

#endif
