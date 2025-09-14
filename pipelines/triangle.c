#include <stdint.h>
#include <stdbool.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../utils/pipeline.h"
#include "../perframe.h"

extern VkuContext_t vkContext;
extern VkRenderPass renderPass;

Pipeline_t trianglePipeline;

bool CreateTrianglePipeline(void)
{
	PipelineOverrideRasterizationSamples(config.MSAA);

	if(!CreatePipeline(&vkContext, &trianglePipeline, renderPass, "pipelines/triangle.pipeline"))
		return false;

	PipelineOverrideRasterizationSamples(VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM);

	return true;
}

void DestroyTriangle(void)
{
	DestroyPipeline(&vkContext, &trianglePipeline);
}

void DrawTriangle(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, vec3 verts[3], vec4 color)
{
	struct
	{
		matrix mvp;
		vec4 color;
		vec4 verts[3];
	} trianglePC;

	trianglePC.mvp=MatrixMult(MatrixMult(perFrame[index].mainUBO[eye]->modelView, perFrame[index].mainUBO[eye]->HMD), perFrame[index].mainUBO[eye]->projection);
	trianglePC.color=color;
	trianglePC.verts[0]=Vec4_Vec3(verts[0], 1.0f);
	trianglePC.verts[1]=Vec4_Vec3(verts[1], 1.0f);
	trianglePC.verts[2]=Vec4_Vec3(verts[2], 1.0f);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline.pipeline.pipeline);
	vkCmdPushConstants(commandBuffer, trianglePipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(trianglePC), &trianglePC);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}

void DrawTrianglePushConstant(VkCommandBuffer commandBuffer, size_t constantSize, void *constant)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, trianglePipeline.pipeline.pipeline);
	vkCmdPushConstants(commandBuffer, trianglePipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, constantSize, constant);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}
