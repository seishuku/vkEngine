#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "system/system.h"
#include "vulkan/vulkan.h"
#include "math/math.h"
#include "vr/vr.h"
#include "model/bmodel.h"
#include "utils/pipeline.h"
#include "skybox.h"
#include "models.h"
#include "perframe.h"

extern VkuContext_t vkContext;
extern VkRenderPass renderPass;
extern VkSampleCountFlags MSAA;
extern VkuSwapchain_t swapchain;
extern VkFormat colorFormat, depthFormat;

//VkuDescriptorSet_t skyboxDescriptorSet;
//VkPipelineLayout skyboxPipelineLayout;
Pipeline_t skyboxPipeline;

bool CreateSkyboxPipeline(void)
{
	for(uint32_t i=0;i<swapchain.numImages;i++)
	{
		vkuCreateHostBuffer(&vkContext, &perFrame[i].skyboxUBOBuffer[0], sizeof(Skybox_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		perFrame[i].skyboxUBO[0]=(Skybox_UBO_t *)perFrame[i].skyboxUBOBuffer[0].memory->mappedPointer;

		vkuCreateHostBuffer(&vkContext, &perFrame[i].skyboxUBOBuffer[1], sizeof(Skybox_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		perFrame[i].skyboxUBO[1]=(Skybox_UBO_t *)perFrame[i].skyboxUBOBuffer[1].memory->mappedPointer;
	}

#if 0
	vkuInitDescriptorSet(&skyboxDescriptorSet, vkContext.device);
	vkuDescriptorSet_AddBinding(&skyboxDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
	vkuDescriptorSet_AddBinding(&skyboxDescriptorSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuAssembleDescriptorSetLayout(&skyboxDescriptorSet);

	vkCreatePipelineLayout(vkContext.device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&skyboxDescriptorSet.descriptorSetLayout,
		.pushConstantRangeCount=0,
	}, 0, &skyboxPipelineLayout);

	vkuInitPipeline(&skyboxPipeline, vkContext.device, vkContext.pipelineCache);

	vkuPipeline_SetPipelineLayout(&skyboxPipeline, skyboxPipelineLayout);
	vkuPipeline_SetRenderPass(&skyboxPipeline, renderPass);

	skyboxPipeline.depthTest=VK_TRUE;
	skyboxPipeline.cullMode=VK_CULL_MODE_BACK_BIT;
	skyboxPipeline.depthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
	skyboxPipeline.rasterizationSamples=MSAA;

	if(!vkuPipeline_AddStage(&skyboxPipeline, "shaders/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&skyboxPipeline, "shaders/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	//VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo=
	//{
	//	.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
	//	.colorAttachmentCount=1,
	//	.pColorAttachmentFormats=&ColorFormat,
	//	.depthAttachmentFormat=DepthFormat,
	//};

	if(!vkuAssemblePipeline(&skyboxPipeline, VK_NULL_HANDLE/*&PipelineRenderingCreateInfo*/))
		return false;
#endif

	if(!CreatePipeline(&vkContext, &skyboxPipeline, renderPass, "pipelines/skybox.pipeline"))
		return false;

	return true;
}

void DestroySkybox(void)
{
	for(uint32_t i=0;i<swapchain.numImages;i++)
	{
		vkuDestroyBuffer(&vkContext, &perFrame[i].skyboxUBOBuffer[0]);
		vkuDestroyBuffer(&vkContext, &perFrame[i].skyboxUBOBuffer[1]);
	}

	//vkDestroyDescriptorSetLayout(vkContext.device, skyboxDescriptorSet.descriptorSetLayout, VK_NULL_HANDLE);
	//vkDestroyPipeline(vkContext.device, skyboxPipeline.pipeline.pipeline, VK_NULL_HANDLE);
	//vkDestroyPipelineLayout(vkContext.device, skyboxPipelineLayout, VK_NULL_HANDLE);
	DestroyPipeline(&vkContext, &skyboxPipeline);
}

void DrawSkybox(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, VkDescriptorPool descriptorPool)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline.pipeline.pipeline);

	vkuDescriptorSet_UpdateBindingBufferInfo(&skyboxPipeline.descriptorSet, 0, perFrame[index].mainUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
	vkuDescriptorSet_UpdateBindingBufferInfo(&skyboxPipeline.descriptorSet, 1, perFrame[index].skyboxUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
	vkuAllocateUpdateDescriptorSet(&skyboxPipeline.descriptorSet, descriptorPool);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline.pipelineLayout, 0, 1, &skyboxPipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	// This has no bound vertex data, it's baked into the vertex shader
	vkCmdDraw(commandBuffer, 60, 1, 0, 0);
}
