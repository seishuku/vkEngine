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

static const uint32_t skyboxWidth=512;
static const uint32_t skyboxHeight=512;

VkuImage_t skyboxTex;

Pipeline_t skyboxPipeline;
Pipeline_t skyboxGenPipeline;

static VkCommandPool computeCommandPool;
static VkCommandBuffer computeCommand;
static VkDescriptorPool computeDescriptorPool;

static VkFence fence=VK_NULL_HANDLE;

bool CreateSkyboxPipeline(void)
{
	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
	{
		vkuCreateHostBuffer(&vkContext, &perFrame[i].skyboxUBOBuffer[0], sizeof(Skybox_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		perFrame[i].skyboxUBO[0]=(Skybox_UBO_t *)perFrame[i].skyboxUBOBuffer[0].memory->mappedPointer;

		vkuCreateHostBuffer(&vkContext, &perFrame[i].skyboxUBOBuffer[1], sizeof(Skybox_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		perFrame[i].skyboxUBO[1]=(Skybox_UBO_t *)perFrame[i].skyboxUBOBuffer[1].memory->mappedPointer;
	}

	PipelineOverrideRasterizationSamples(config.MSAA);

	if(!CreatePipeline(&vkContext, &skyboxPipeline, renderPass, "pipelines/skybox.pipeline"))
		return false;

	if(!CreatePipeline(&vkContext, &skyboxGenPipeline, renderPass, "pipelines/skybox_gen.pipeline"))
		return false;

	PipelineOverrideRasterizationSamples(VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM);

	// Create skybox texture
	VkBufferUsageFlags usageFlags=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_STORAGE_BIT;
	VkImageAspectFlags aspectFlags=VK_IMAGE_ASPECT_COLOR_BIT;
	VkFormat format=VK_FORMAT_R16G16B16A16_SFLOAT;

	if(!vkuCreateImageBuffer(&vkContext, &skyboxTex, VK_IMAGE_TYPE_2D, format, 1, 6, skyboxWidth, skyboxHeight, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, usageFlags, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT))
		return VK_FALSE;

	VkCommandBuffer commandBuffer=vkuOneShotCommandBufferBegin(&vkContext);
	vkuTransitionLayout(commandBuffer, skyboxTex.image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkuTransitionLayout(commandBuffer, skyboxTex.image, 1, 0, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkuTransitionLayout(commandBuffer, skyboxTex.image, 1, 0, 1, 2, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkuTransitionLayout(commandBuffer, skyboxTex.image, 1, 0, 1, 3, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkuTransitionLayout(commandBuffer, skyboxTex.image, 1, 0, 1, 4, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkuTransitionLayout(commandBuffer, skyboxTex.image, 1, 0, 1, 5, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	vkuOneShotCommandBufferEnd(&vkContext, commandBuffer);

	if(vkCreateImageView(vkContext.device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image=skyboxTex.image,
		.viewType=VK_IMAGE_VIEW_TYPE_CUBE,
		.format=format,
		.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
		.subresourceRange.aspectMask=aspectFlags,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.levelCount=1,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=6,
	}, NULL, &skyboxTex.imageView)!=VK_SUCCESS)
		return VK_FALSE;

	if(vkCreateSampler(vkContext.device, &(VkSamplerCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter=VK_FILTER_LINEAR,
		.minFilter=VK_FILTER_LINEAR,
		.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.mipLodBias=0.0f,
		.maxAnisotropy=1.0f,
		.compareOp=VK_COMPARE_OP_NEVER,
		.minLod=0.0f,
		.maxLod=VK_LOD_CLAMP_NONE,
		.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	}, VK_NULL_HANDLE, &skyboxTex.sampler)!=VK_SUCCESS)
		return VK_FALSE;
	//////

	// Set up compute resources
	vkCreateDescriptorPool(vkContext.device, &(VkDescriptorPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets=1024, // Max number of descriptor sets that can be allocated from this pool
		.poolSizeCount=1,
		.pPoolSizes=(VkDescriptorPoolSize[]){ {.type=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount=1024, }, },
	}, VK_NULL_HANDLE, &computeDescriptorPool);

	if(vkCreateCommandPool(vkContext.device, &(VkCommandPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags=0,
		.queueFamilyIndex=vkContext.computeQueueIndex,
	}, VK_NULL_HANDLE, &computeCommandPool)!=VK_SUCCESS)
		return VK_FALSE;

	if(vkAllocateCommandBuffers(vkContext.device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=computeCommandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=1,
	}, &computeCommand)!=VK_SUCCESS)
		return VK_FALSE;

	if(vkCreateFence(vkContext.device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=0 }, VK_NULL_HANDLE, &fence)!=VK_SUCCESS)
		return VK_FALSE;
	//////

	return true;
}

void DestroySkybox(void)
{
	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
	{
		vkuDestroyBuffer(&vkContext, &perFrame[i].skyboxUBOBuffer[0]);
		vkuDestroyBuffer(&vkContext, &perFrame[i].skyboxUBOBuffer[1]);
	}

	vkuDestroyImageBuffer(&vkContext, &skyboxTex);

	DestroyPipeline(&vkContext, &skyboxPipeline);
}

VkBool32 GenSkybox(void)
{
	vkResetDescriptorPool(vkContext.device, computeDescriptorPool, 0);
	vkResetCommandPool(vkContext.device, computeCommandPool, 0);
	vkResetFences(vkContext.device, 1, (VkFence []){ fence });

	vkBeginCommandBuffer(computeCommand, &(VkCommandBufferBeginInfo) { .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO });

	vkCmdPipelineBarrier(computeCommand, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_NONE, .dstAccessMask=VK_ACCESS_SHADER_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED, .newLayout=VK_IMAGE_LAYOUT_GENERAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=skyboxTex.image,
		.subresourceRange={ .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel=0, .levelCount=1, .baseArrayLayer=0, .layerCount=1 }
	});

	vkCmdBindPipeline(computeCommand, VK_PIPELINE_BIND_POINT_COMPUTE, skyboxGenPipeline.pipeline.pipeline);

	vkuDescriptorSet_UpdateBindingBufferInfo(&skyboxGenPipeline.descriptorSet, 0, perFrame[0].skyboxUBOBuffer[0].buffer, 0, VK_WHOLE_SIZE);
	vkuDescriptorSet_UpdateBindingImageInfo(&skyboxGenPipeline.descriptorSet, 1, VK_NULL_HANDLE, skyboxTex.imageView, VK_IMAGE_LAYOUT_GENERAL);
	vkuAllocateUpdateDescriptorSet(&skyboxGenPipeline.descriptorSet, computeDescriptorPool);

	vkCmdBindDescriptorSets(computeCommand, VK_PIPELINE_BIND_POINT_COMPUTE, skyboxGenPipeline.pipelineLayout, 0, 1, &skyboxGenPipeline.descriptorSet.descriptorSet, 0, 0);

	vkCmdDispatch(computeCommand, skyboxWidth/8, skyboxHeight/8, 6);

	vkCmdPipelineBarrier(computeCommand, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT, .dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_GENERAL, .newLayout=VK_IMAGE_LAYOUT_GENERAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED, .dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=skyboxTex.image,
		.subresourceRange={ .aspectMask=VK_IMAGE_ASPECT_COLOR_BIT, .baseMipLevel=0, .levelCount=1, .baseArrayLayer=0, .layerCount=1 }
	});

	// End command buffer and submit
	if(vkEndCommandBuffer(computeCommand)!=VK_SUCCESS)
		return VK_FALSE;

	VkSubmitInfo submitInfo=
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&computeCommand,
	};

	vkQueueSubmit(vkContext.computeQueue, 1, &submitInfo, fence);

	if(vkWaitForFences(vkContext.device, 1, &fence, VK_TRUE, UINT64_MAX)!=VK_SUCCESS)
		return VK_FALSE;

	VkCommandBuffer commandBuffer=vkuOneShotCommandBufferBegin(&vkContext);
	vkuTransitionLayout(commandBuffer, skyboxTex.image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuTransitionLayout(commandBuffer, skyboxTex.image, 1, 0, 1, 1, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuTransitionLayout(commandBuffer, skyboxTex.image, 1, 0, 1, 2, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuTransitionLayout(commandBuffer, skyboxTex.image, 1, 0, 1, 3, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuTransitionLayout(commandBuffer, skyboxTex.image, 1, 0, 1, 4, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuTransitionLayout(commandBuffer, skyboxTex.image, 1, 0, 1, 5, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuOneShotCommandBufferEnd(&vkContext, commandBuffer);

	return VK_TRUE;
}

void DrawSkybox(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, VkDescriptorPool descriptorPool)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline.pipeline.pipeline);

	vkuDescriptorSet_UpdateBindingBufferInfo(&skyboxPipeline.descriptorSet, 0, perFrame[index].mainUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
	vkuDescriptorSet_UpdateBindingImageInfo(&skyboxPipeline.descriptorSet, 1, skyboxTex.sampler, skyboxTex.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuAllocateUpdateDescriptorSet(&skyboxPipeline.descriptorSet, descriptorPool);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline.pipelineLayout, 0, 1, &skyboxPipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	// This has no bound vertex data, it's baked into the vertex shader
	vkCmdDraw(commandBuffer, 60, 1, 0, 0);
}
