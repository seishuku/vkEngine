#include <stdint.h>
#include <stdbool.h>
#include "vulkan/vulkan.h"
#include "../math/math.h"
#include "../utils/pipeline.h"
#include "../perframe.h"

extern VkuContext_t vkContext;
extern VkuSwapchain_t swapchain;
extern VkSampleCountFlags MSAA;
extern VkFormat colorFormat;
extern VkFormat depthFormat;
extern VkRenderPass renderPass;

Pipeline_t linePipeline;

bool CreateLinePipeline(void)
{
	if(!CreatePipeline(&vkContext, &linePipeline, renderPass, "pipelines/line.pipeline"))
		return false;

	return true;
}

void DestroyLine(void)
{
	DestroyPipeline(&vkContext, &linePipeline);
}

void DrawLine(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, vec3 start, vec3 end, vec4 color)
{
	struct
	{
		matrix mvp;
		vec4 color;
		vec4 verts[2];
	} linePC;

	matrix local=MatrixMult(perFrame[index].mainUBO[eye]->modelView, perFrame[index].mainUBO[eye]->HMD);
	linePC.mvp=MatrixMult(local, perFrame[index].mainUBO[eye]->projection);
	linePC.color=color;
	linePC.verts[0]=Vec4_Vec3(start, 1.0f);
	linePC.verts[1]=Vec4_Vec3(end, 1.0f);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline.pipeline.pipeline);
	vkCmdPushConstants(commandBuffer, linePipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(linePC), &linePC);
	vkCmdDraw(commandBuffer, 2, 1, 0, 0);
}

void DrawLinePushConstant(VkCommandBuffer commandBuffer, size_t constantSize, void *constant)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, linePipeline.pipeline.pipeline);
	vkCmdPushConstants(commandBuffer, linePipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, constantSize, constant);
	vkCmdDraw(commandBuffer, 2, 1, 0, 0);
}
