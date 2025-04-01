#include <stdint.h>
#include <stdbool.h>
#include "vulkan/vulkan.h"
#include "../math/math.h"
#include "../utils/pipeline.h"
#include "../perframe.h"

extern VkuContext_t vkContext;
extern VkuSwapchain_t swapchain;
extern VkRenderPass renderPass;

//VkPipelineLayout spherePipelineLayout;
Pipeline_t spherePipeline;

bool CreateSpherePipeline(void)
{
	if(!CreatePipeline(&vkContext, &spherePipeline, renderPass, "pipelines/sphere.pipeline"))
		return false;

	return true;
}

void DestroySphere(void)
{
	DestroyPipeline(&vkContext, &spherePipeline);
}

void DrawSphere(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, vec3 position, float radius, vec4 color)
{
	struct
	{
		matrix mvp;
		vec4 color;
	} spherePC;

	matrix local=MatrixIdentity();
	local=MatrixMult(local, MatrixScale(radius, radius, radius));
	local=MatrixMult(local, MatrixTranslatev(position));
	local=MatrixMult(local, perFrame[index].mainUBO[eye]->modelView);
	local=MatrixMult(local, perFrame[index].mainUBO[eye]->HMD);
	spherePC.mvp=MatrixMult(local, perFrame[index].mainUBO[eye]->projection);
	spherePC.color=color;

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spherePipeline.pipeline.pipeline);
	vkCmdPushConstants(commandBuffer, spherePipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(spherePC), &spherePC);
	vkCmdDraw(commandBuffer, 60, 1, 0, 0);
}

void DrawSpherePushConstant(VkCommandBuffer commandBuffer, uint32_t index, size_t constantSize, void *constant)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spherePipeline.pipeline.pipeline);
	vkCmdPushConstants(commandBuffer, spherePipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, constantSize, constant);
	vkCmdDraw(commandBuffer, 60, 1, 0, 0);
}
