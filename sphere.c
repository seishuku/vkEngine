#include <stdint.h>
#include <stdbool.h>
#include "vulkan/vulkan.h"
#include "math/math.h"

#include "perframe.h"

extern VkuContext_t vkContext;
extern VkuSwapchain_t swapchain;
extern VkSampleCountFlags MSAA;
extern VkFormat colorFormat;
extern VkFormat depthFormat;
extern VkRenderPass renderPass;

VkPipelineLayout spherePipelineLayout;
VkuPipeline_t spherePipeline;

bool CreateSpherePipeline(void)
{
	vkCreatePipelineLayout(vkContext.device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=sizeof(matrix)+sizeof(vec4),
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},
	}, 0, &spherePipelineLayout);

	vkuInitPipeline(&spherePipeline, vkContext.device, vkContext.pipelineCache);

	vkuPipeline_SetPipelineLayout(&spherePipeline, spherePipelineLayout);
	vkuPipeline_SetRenderPass(&spherePipeline, renderPass);

	spherePipeline.depthTest=VK_TRUE;
	spherePipeline.cullMode=VK_CULL_MODE_BACK_BIT;
	spherePipeline.depthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
	spherePipeline.rasterizationSamples=MSAA;
	//spherePipeline.polygonMode=VK_POLYGON_MODE_LINE;

	if(!vkuPipeline_AddStage(&spherePipeline, "shaders/sphere.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&spherePipeline, "shaders/sphere.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	//VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo=
	//{
	//	.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
	//	.colorAttachmentCount=1,
	//	.pColorAttachmentFormats=&colorFormat,
	//	.depthAttachmentFormat=depthFormat,
	//};

	if(!vkuAssemblePipeline(&spherePipeline, VK_NULL_HANDLE/*&pipelineRenderingCreateInfo*/))
		return false;

	return true;
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

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, spherePipeline.pipeline);
	vkCmdPushConstants(commandBuffer, spherePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(spherePC), &spherePC);
	vkCmdDraw(commandBuffer, 60, 1, 0, 0);
}
