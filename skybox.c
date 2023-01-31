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
extern VkRenderPass RenderPass;
extern VkSampleCountFlags MSAA;

VkPipelineLayout SkyboxPipelineLayout;
VkuPipeline_t SkyboxPipeline;

bool CreateSkyboxPipeline(void)
{
	for(uint32_t i=0;i<VKU_MAX_FRAME_COUNT;i++)
	{
		vkuCreateHostBuffer(&Context, &PerFrame[i].Skybox_UBO_Buffer[0], sizeof(Skybox_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		vkMapMemory(Context.Device, PerFrame[i].Skybox_UBO_Buffer[0].DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&PerFrame[i].Skybox_UBO[0]);

		vkuCreateHostBuffer(&Context, &PerFrame[i].Skybox_UBO_Buffer[1], sizeof(Skybox_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		vkMapMemory(Context.Device, PerFrame[i].Skybox_UBO_Buffer[1].DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&PerFrame[i].Skybox_UBO[1]);

		vkuInitDescriptorSet(&PerFrame[i].SkyboxDescriptorSet, &Context);
		vkuDescriptorSet_AddBinding(&PerFrame[i].SkyboxDescriptorSet, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);
		vkuAssembleDescriptorSetLayout(&PerFrame[i].SkyboxDescriptorSet);
	}

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&PerFrame[0].SkyboxDescriptorSet.DescriptorSetLayout,
		.pushConstantRangeCount=0,
	}, 0, &SkyboxPipelineLayout);

	vkuInitPipeline(&SkyboxPipeline, &Context);

	vkuPipeline_SetPipelineLayout(&SkyboxPipeline, SkyboxPipelineLayout);
	vkuPipeline_SetRenderPass(&SkyboxPipeline, RenderPass);

	SkyboxPipeline.DepthTest=VK_TRUE;
	SkyboxPipeline.CullMode=VK_CULL_MODE_BACK_BIT;
	SkyboxPipeline.DepthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
	SkyboxPipeline.RasterizationSamples=MSAA;

	if(!vkuPipeline_AddStage(&SkyboxPipeline, "./shaders/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&SkyboxPipeline, "./shaders/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	//vkuPipeline_AddVertexBinding(&SkyboxPipeline, 0, sizeof(float)*4, VK_VERTEX_INPUT_RATE_VERTEX);
	//vkuPipeline_AddVertexAttribute(&SkyboxPipeline, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);

	if(!vkuAssemblePipeline(&SkyboxPipeline))
		return false;

	return true;
}

void DestroySkybox(void)
{
	for(uint32_t i=0;i<VKU_MAX_FRAME_COUNT;i++)
	{
		vkUnmapMemory(Context.Device, PerFrame[i].Skybox_UBO_Buffer[0].DeviceMemory);
		vkuDestroyBuffer(&Context, &PerFrame[i].Skybox_UBO_Buffer[0]);

		vkUnmapMemory(Context.Device, PerFrame[i].Skybox_UBO_Buffer[1].DeviceMemory);
		vkuDestroyBuffer(&Context, &PerFrame[i].Skybox_UBO_Buffer[1]);

		vkDestroyDescriptorSetLayout(Context.Device, PerFrame[i].SkyboxDescriptorSet.DescriptorSetLayout, VK_NULL_HANDLE);
	}

	vkDestroyPipeline(Context.Device, SkyboxPipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, SkyboxPipelineLayout, VK_NULL_HANDLE);
}
