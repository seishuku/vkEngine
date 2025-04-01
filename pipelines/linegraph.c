#include <stdint.h>
#include <stdbool.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../utils/pipeline.h"
#include "../vr/vr.h"
#include "../perframe.h"
#include "linegraph.h"

extern VkuContext_t vkContext;
extern VkuSwapchain_t swapchain;
extern VkRenderPass compositeRenderPass;
extern XruContext_t xrContext;
extern matrix modelView, projection[2], headPose;

Pipeline_t lineGraphPipeline;

bool CreateLineGraphPipeline(void)
{
	if(!CreatePipeline(&vkContext, &lineGraphPipeline, compositeRenderPass, "pipelines/linegraph.pipeline"))
		return false;

	return true;
}

void DestroyLineGraphPipeline(void)
{
	DestroyPipeline(&vkContext, &lineGraphPipeline);
}

bool CreateLineGraph(LineGraph_t *lineGraph, const uint32_t numPoints, const float updateRate, const float min, const float max, const vec2 size, const vec2 position, vec4 color)
{
	lineGraph->updateRate=updateRate;
	lineGraph->currentTime=0.0f;
	lineGraph->currentValue=0.0f;
	lineGraph->min=min;
	lineGraph->max=max;
	lineGraph->size=size;
	lineGraph->position=position;
	lineGraph->color=color;

	if(!vkuCreateHostBuffer(&vkContext, &lineGraph->buffer, sizeof(vec2)*numPoints, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT))
		return false;

	vec2 *buffer=(vec2 *)lineGraph->buffer.memory->mappedPointer;
	size_t bufferSize=lineGraph->buffer.memory->size/sizeof(vec2);

	for(size_t i=0;i<bufferSize;i++)
	{
		float fi=(float)i/bufferSize;
		buffer[i]=Vec2(position.x+(fi*size.x), position.y);
	}

	return true;
}

void DestroyLineGraph(LineGraph_t *lineGraph)
{
	vkuDestroyBuffer(&vkContext, &lineGraph->buffer);
}

void UpdateLineGraph(LineGraph_t *lineGraph, const float value, const float deltaTime)
{
	lineGraph->currentValue=value;
	lineGraph->currentTime+=deltaTime;

	if(lineGraph->currentTime>=lineGraph->updateRate)
	{
		vec2 *buffer=(vec2 *)lineGraph->buffer.memory->mappedPointer;
		size_t bufferSize=lineGraph->buffer.memory->size/sizeof(vec2);

		lineGraph->currentTime-=lineGraph->updateRate;

		for(size_t i=1;i<bufferSize;i++)
		{
			// Shift the point left by a fixed amount
			float deltaX=lineGraph->size.x/(float)(bufferSize-1);
			buffer[i-1]=Vec2(buffer[i].x-deltaX, buffer[i].y);
		}

		const float normalizeValue=clampf((value-lineGraph->min)/(lineGraph->max-lineGraph->min), 0.0f, 1.0f);
		buffer[bufferSize-1]=Vec2(lineGraph->position.x+lineGraph->size.x, lineGraph->position.y+(normalizeValue*lineGraph->size.y));
	}
}

void DrawLineGraph(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, LineGraph_t *lineGraph)
{
	struct
	{
		VkExtent2D extent;
		uint32_t pad[2];
		matrix mvp;
		vec4 color;
	} linePC;

	float z=-1.0f;

	if(config.isVR)
	{
		z=-1.5f;
		linePC.extent=(VkExtent2D){ xrContext.swapchainExtent.width, xrContext.swapchainExtent.height };
	}
	else
		linePC.extent=(VkExtent2D){ config.renderWidth, config.renderHeight };

	linePC.mvp=MatrixMult(MatrixMult(MatrixMult(MatrixScale((float)linePC.extent.width/(float)linePC.extent.height, 1.0f, 1.0f), MatrixTranslate(0.0f, 0.0f, z)), headPose), projection[eye]);
	linePC.color=lineGraph->color;

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lineGraphPipeline.pipeline.pipeline);
	vkCmdPushConstants(commandBuffer, lineGraphPipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(linePC), &linePC);
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &lineGraph->buffer.buffer, &(VkDeviceSize) { 0 });
	vkCmdDraw(commandBuffer, lineGraph->buffer.memory->size/sizeof(vec2), 1, 0, 0);
}
