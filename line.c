#include <stdint.h>
#include <stdbool.h>
#include "vulkan/vulkan.h"
#include "math/math.h"
#include "utils/pipeline.h"

#include "perframe.h"

extern VkuContext_t vkContext;
extern VkuSwapchain_t swapchain;
extern VkSampleCountFlags MSAA;
extern VkFormat colorFormat;
extern VkFormat depthFormat;
extern VkRenderPass renderPass;

//VkPipelineLayout linePipelineLayout;
//VkuPipeline_t linePipeline;
Pipeline_t linePipeline;

bool CreateLinePipeline(void)
{
#if 0
	vkCreatePipelineLayout(vkContext.device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=sizeof(matrix)+(sizeof(vec4)*3),
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},
	}, 0, &linePipelineLayout);

	vkuInitPipeline(&linePipeline, vkContext.device, vkContext.pipelineCache);

	vkuPipeline_SetPipelineLayout(&linePipeline, linePipelineLayout);
	vkuPipeline_SetRenderPass(&linePipeline, renderPass);

	linePipeline.topology=VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	linePipeline.depthTest=VK_TRUE;
	linePipeline.cullMode=VK_CULL_MODE_BACK_BIT;
	linePipeline.depthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
	linePipeline.rasterizationSamples=MSAA;

	if(!vkuPipeline_AddStage(&linePipeline, "shaders/line.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&linePipeline, "shaders/line.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	//VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo=
	//{
	//	.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
	//	.colorAttachmentCount=1,
	//	.pColorAttachmentFormats=&colorFormat,
	//	.depthAttachmentFormat=depthFormat,
	//};

	if(!vkuAssemblePipeline(&linePipeline, VK_NULL_HANDLE/*&pipelineRenderingCreateInfo*/))
		return false;
#endif

	if(!CreatePipeline(&vkContext, &linePipeline, renderPass, "pipelines/line.pipeline"))
		return false;

	return true;
}

void DestroyLine(void)
{
	DestroyPipeline(&vkContext, &linePipeline);
	//vkDestroyPipeline(vkContext.device, linePipeline.pipeline, VK_NULL_HANDLE);
	//vkDestroyPipelineLayout(vkContext.device, linePipelineLayout, VK_NULL_HANDLE);
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
