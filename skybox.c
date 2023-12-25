#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "system/system.h"
#include "vulkan/vulkan.h"
#include "math/math.h"
#include "vr/vr.h"
#include "model/bmodel.h"
#include "skybox.h"
#include "models.h"
#include "perframe.h"

extern VkuContext_t Context;
extern VkRenderPass RenderPass;
extern VkSampleCountFlags MSAA;
extern VkuSwapchain_t Swapchain;
extern VkFormat colorFormat, depthFormat;

VkuDescriptorSet_t skyboxDescriptorSet;
VkPipelineLayout skyboxPipelineLayout;
VkuPipeline_t skyboxPipeline;

bool CreateSkyboxPipeline(void)
{
	for(uint32_t i=0;i<Swapchain.NumImages;i++)
	{
		vkuCreateHostBuffer(&Context, &perFrame[i].skyboxUBOBuffer[0], sizeof(Skybox_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		vkMapMemory(Context.Device, perFrame[i].skyboxUBOBuffer[0].DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&perFrame[i].skyboxUBO[0]);

		vkuCreateHostBuffer(&Context, &perFrame[i].skyboxUBOBuffer[1], sizeof(Skybox_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		vkMapMemory(Context.Device, perFrame[i].skyboxUBOBuffer[1].DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&perFrame[i].skyboxUBO[1]);
	}

	vkuInitDescriptorSet(&skyboxDescriptorSet, &Context);
	vkuDescriptorSet_AddBinding(&skyboxDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&skyboxDescriptorSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuAssembleDescriptorSetLayout(&skyboxDescriptorSet);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&skyboxDescriptorSet.DescriptorSetLayout,
		.pushConstantRangeCount=0,
	}, 0, &skyboxPipelineLayout);

	vkuInitPipeline(&skyboxPipeline, &Context);

	vkuPipeline_SetPipelineLayout(&skyboxPipeline, skyboxPipelineLayout);
	vkuPipeline_SetRenderPass(&skyboxPipeline, RenderPass);

	skyboxPipeline.DepthTest=VK_TRUE;
	skyboxPipeline.CullMode=VK_CULL_MODE_BACK_BIT;
	skyboxPipeline.DepthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
	skyboxPipeline.RasterizationSamples=MSAA;

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

	return true;
}

void DestroySkybox(void)
{
	for(uint32_t i=0;i<Swapchain.NumImages;i++)
	{
		vkUnmapMemory(Context.Device, perFrame[i].skyboxUBOBuffer[0].DeviceMemory);
		vkuDestroyBuffer(&Context, &perFrame[i].skyboxUBOBuffer[0]);

		vkUnmapMemory(Context.Device, perFrame[i].skyboxUBOBuffer[1].DeviceMemory);
		vkuDestroyBuffer(&Context, &perFrame[i].skyboxUBOBuffer[1]);
	}

	vkDestroyDescriptorSetLayout(Context.Device, skyboxDescriptorSet.DescriptorSetLayout, VK_NULL_HANDLE);
	vkDestroyPipeline(Context.Device, skyboxPipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, skyboxPipelineLayout, VK_NULL_HANDLE);
}
