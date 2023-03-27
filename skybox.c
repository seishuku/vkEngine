#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "system/system.h"
#include "vulkan/vulkan.h"
#include "math/math.h"
#include "model/bmodel.h"
#include "skybox.h"
#include "models.h"
#include "perframe.h"

extern VkuContext_t Context;
extern VkuRenderPass_t RenderPass;
extern VkSampleCountFlags MSAA;
extern VkuSwapchain_t Swapchain;

VkuDescriptorSet_t SkyboxDescriptorSet;
VkPipelineLayout SkyboxPipelineLayout;
VkuPipeline_t SkyboxPipeline;

bool CreateSkyboxPipeline(void)
{
	for(uint32_t i=0;i<Swapchain.NumImages;i++)
	{
		vkuCreateHostBuffer(&Context, &PerFrame[i].Skybox_UBO_Buffer[0], sizeof(Skybox_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		vkMapMemory(Context.Device, PerFrame[i].Skybox_UBO_Buffer[0].DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&PerFrame[i].Skybox_UBO[0]);

		vkuCreateHostBuffer(&Context, &PerFrame[i].Skybox_UBO_Buffer[1], sizeof(Skybox_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		vkMapMemory(Context.Device, PerFrame[i].Skybox_UBO_Buffer[1].DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&PerFrame[i].Skybox_UBO[1]);
	}

	vkuInitDescriptorSet(&SkyboxDescriptorSet, &Context);
	vkuDescriptorSet_AddBinding(&SkyboxDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&SkyboxDescriptorSet, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuAssembleDescriptorSetLayout(&SkyboxDescriptorSet);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&SkyboxDescriptorSet.DescriptorSetLayout,
		.pushConstantRangeCount=0,
	}, 0, &SkyboxPipelineLayout);

	vkuInitPipeline(&SkyboxPipeline, &Context);

	vkuPipeline_SetPipelineLayout(&SkyboxPipeline, SkyboxPipelineLayout);
	vkuPipeline_SetRenderPass(&SkyboxPipeline, RenderPass.RenderPass);

	SkyboxPipeline.DepthTest=VK_TRUE;
	SkyboxPipeline.CullMode=VK_CULL_MODE_BACK_BIT;
	SkyboxPipeline.DepthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
	SkyboxPipeline.RasterizationSamples=MSAA;

	if(!vkuPipeline_AddStage(&SkyboxPipeline, "./shaders/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&SkyboxPipeline, "./shaders/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	if(!vkuAssemblePipeline(&SkyboxPipeline))
		return false;

	return true;
}

void DestroySkybox(void)
{
	for(uint32_t i=0;i<Swapchain.NumImages;i++)
	{
		vkUnmapMemory(Context.Device, PerFrame[i].Skybox_UBO_Buffer[0].DeviceMemory);
		vkuDestroyBuffer(&Context, &PerFrame[i].Skybox_UBO_Buffer[0]);

		vkUnmapMemory(Context.Device, PerFrame[i].Skybox_UBO_Buffer[1].DeviceMemory);
		vkuDestroyBuffer(&Context, &PerFrame[i].Skybox_UBO_Buffer[1]);
	}

	vkDestroyDescriptorSetLayout(Context.Device, SkyboxDescriptorSet.DescriptorSetLayout, VK_NULL_HANDLE);
	vkDestroyPipeline(Context.Device, SkyboxPipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, SkyboxPipelineLayout, VK_NULL_HANDLE);
}
