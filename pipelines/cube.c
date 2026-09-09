#include <stdint.h>
#include <stdbool.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../utils/pipeline.h"
#include "../perframe.h"

extern VkuContext_t vkContext;
extern VkuSwapchain_t swapchain;
extern VkRenderPass renderPass;

//VkPipelineLayout spherePipelineLayout;
static Pipeline_t cubePipeline;

bool CreateCubePipeline(void)
{
	PipelineOverrideRasterizationSamples(config.MSAA);

	if(!CreatePipeline(&vkContext, &cubePipeline, renderPass, "pipelines/cube.pipeline"))
		return false;

	PipelineOverrideRasterizationSamples(VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM);

	return true;
}

void DestroyCube(void)
{
	DestroyPipeline(&vkContext, &cubePipeline);
}

void DrawCube(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, vec3 position, vec3 size, vec4 color)
{
	struct
	{
		matrix mvp;
		vec4 color;
	} cubePC;

	matrix local=MatrixIdentity();
	local=MatrixMult(local, MatrixScale(size.x, size.y, size.z));
	local=MatrixMult(local, MatrixTranslatev(position));
	local=MatrixMult(local, perFrame[index].mainUBO[eye]->modelView);
	local=MatrixMult(local, perFrame[index].mainUBO[eye]->HMD);
	cubePC.mvp=MatrixMult(local, perFrame[index].mainUBO[eye]->projection);
	cubePC.color=color;

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cubePipeline.pipeline.pipeline);
	vkCmdPushConstants(commandBuffer, cubePipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(cubePC), &cubePC);
	vkCmdDraw(commandBuffer, 36, 1, 0, 0);
}

void DrawCubePushConstant(VkCommandBuffer commandBuffer, uint32_t index, size_t constantSize, void *constant)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, cubePipeline.pipeline.pipeline);
	vkCmdPushConstants(commandBuffer, cubePipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, constantSize, constant);
	vkCmdDraw(commandBuffer, 36, 1, 0, 0);
}
