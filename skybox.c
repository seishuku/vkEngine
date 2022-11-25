#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "system/system.h"
#include "vulkan/vulkan.h"
#include "math/math.h"
#include "skybox.h"

#define MAX_FRAME_COUNT 2

extern VkuContext_t Context;
extern VkRenderPass RenderPass;
extern VkDescriptorPool DescriptorPool[];

Skybox_UBO_t *Skybox_UBO;

VkuBuffer_t Skybox_UBO_Buffer;

VkuDescriptorSet_t SkyboxDescriptorSet[MAX_FRAME_COUNT];

VkPipelineLayout SkyboxPipelineLayout;
VkuPipeline_t SkyboxPipeline;

bool CreateSkyboxPipeline(void)
{
	for(uint32_t i=0;i<MAX_FRAME_COUNT;i++)
	{
		vkuInitDescriptorSet(&SkyboxDescriptorSet[i], &Context);
		vkuDescriptorSet_AddBinding(&SkyboxDescriptorSet[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);
		vkuAssembleDescriptorSetLayout(&SkyboxDescriptorSet[i]);
	}

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&SkyboxDescriptorSet[0].DescriptorSetLayout,
		.pushConstantRangeCount=0,
	}, 0, &SkyboxPipelineLayout);

	vkuInitPipeline(&SkyboxPipeline, &Context);

	vkuPipeline_SetPipelineLayout(&SkyboxPipeline, SkyboxPipelineLayout);
	vkuPipeline_SetRenderPass(&SkyboxPipeline, RenderPass);

	SkyboxPipeline.DepthTest=VK_TRUE;
	SkyboxPipeline.CullMode=VK_CULL_MODE_BACK_BIT;

	if(!vkuPipeline_AddStage(&SkyboxPipeline, "./shaders/skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&SkyboxPipeline, "./shaders/skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	vkuPipeline_AddVertexBinding(&SkyboxPipeline, 0, sizeof(float)*4, VK_VERTEX_INPUT_RATE_VERTEX);
	vkuPipeline_AddVertexAttribute(&SkyboxPipeline, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);

	if(!vkuAssemblePipeline(&SkyboxPipeline))
		return false;

	vkuCreateHostBuffer(&Context, &Skybox_UBO_Buffer, sizeof(Skybox_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	vkMapMemory(Context.Device, Skybox_UBO_Buffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&Skybox_UBO);

	return true;
}

void DrawSkybox(VkCommandBuffer CommandBuffer, uint32_t Index)
{
	vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, SkyboxPipeline.Pipeline);

	vkuDescriptorSet_UpdateBindingBufferInfo(&SkyboxDescriptorSet[Index], 0, Skybox_UBO_Buffer.Buffer, 0, VK_WHOLE_SIZE);
	vkuAllocateUpdateDescriptorSet(&SkyboxDescriptorSet[Index], DescriptorPool[Index]);

	vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, SkyboxPipelineLayout, 0, 1, &SkyboxDescriptorSet[Index].DescriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDrawIndexed(CommandBuffer, 60, 1, 0, 0, 0);
}

void DestroySkybox(void)
{
	vkUnmapMemory(Context.Device, Skybox_UBO_Buffer.DeviceMemory);
	vkuDestroyBuffer(&Context, &Skybox_UBO_Buffer);

	for(uint32_t i=0;i<MAX_FRAME_COUNT;i++)
		vkDestroyDescriptorSetLayout(Context.Device, SkyboxDescriptorSet[i].DescriptorSetLayout, VK_NULL_HANDLE);

	vkDestroyPipeline(Context.Device, SkyboxPipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, SkyboxPipelineLayout, VK_NULL_HANDLE);
}
