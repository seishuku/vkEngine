#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../vr/vr.h"
#include "../model/bmodel.h"
#include "../utils/pipeline.h"
#include "../models.h"
#include "../perframe.h"
#include "skybox.h"

extern VkuContext_t vkContext;
extern VkRenderPass renderPass;
extern VkuSwapchain_t swapchain;

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
