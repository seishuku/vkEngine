#include <stdint.h>
#include <stdbool.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../utils/pipeline.h"
#include "../perframe.h"
#include "../models.h"
#include "../textures.h"
#include "shadow.h"

extern VkuContext_t vkContext;
extern VkuSwapchain_t swapchain;

VkRenderPass renderPass;
Pipeline_t mainPipeline;

bool CreateLightingPipeline(void)
{
	vkCreateRenderPass(vkContext.device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=3,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=config.colorFormat,
				.samples=config.MSAA,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			},
			{
				.format=config.depthFormat,
				.samples=config.MSAA,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				.finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			},
			{
				.format=config.colorFormat,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			},
		},
		.subpassCount=2,
		.pSubpasses=(VkSubpassDescription[])
		{
			{
				.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,
				.colorAttachmentCount=1,
				.pColorAttachments=&(VkAttachmentReference)
				{
					.attachment=0,
					.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				},
				.pDepthStencilAttachment=&(VkAttachmentReference)
				{
					.attachment=1,
					.layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
				},
				.pResolveAttachments=&(VkAttachmentReference)
				{
					.attachment=2,
					.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				},
			},
			{
				.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,
				.inputAttachmentCount=1,
				.pInputAttachments=&(VkAttachmentReference)
				{
					.attachment=1,
					.layout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				},
				.colorAttachmentCount=1,
				.pColorAttachments=&(VkAttachmentReference)
				{
					.attachment=0,
					.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				},
				.pResolveAttachments=&(VkAttachmentReference)
				{
					.attachment=2,
					.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				},
			},
		},
		.dependencyCount=5, // Increased to 2 for layout transition synchronization
		.pDependencies=(VkSubpassDependency[])
		{
			{
				.srcSubpass=0,
				.dstSubpass=1,
				.srcStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
			},
			{
				.srcSubpass=0,
				.dstSubpass=1,
				.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
			},
			{
				.srcSubpass=0,
				.dstSubpass=1,
				.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
			},
			{
				.srcSubpass=0,
				.dstSubpass=1,
				.srcStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
			},
			{
				.srcSubpass=1,
				.dstSubpass=VK_SUBPASS_EXTERNAL,
				.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
				.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_MEMORY_READ_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
			}
		}
	}, 0, &renderPass);

	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
	{
		vkuCreateHostBuffer(&vkContext, &perFrame[i].mainUBOBuffer[0], sizeof(Main_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		perFrame[i].mainUBO[0]=perFrame[i].mainUBOBuffer[0].memory->mappedPointer;

		vkuCreateHostBuffer(&vkContext, &perFrame[i].mainUBOBuffer[1], sizeof(Main_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		perFrame[i].mainUBO[1]=perFrame[i].mainUBOBuffer[1].memory->mappedPointer;
	}

	PipelineOverrideRasterizationSamples(config.MSAA);

	if(!CreatePipeline(&vkContext, &mainPipeline, renderPass, "pipelines/lighting.pipeline"))
		return false;

	PipelineOverrideRasterizationSamples(VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM);

	return true;
}

void DestroyLighting(void)
{
	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
	{
		vkuDestroyBuffer(&vkContext, &perFrame[i].mainUBOBuffer[0]);
		vkuDestroyBuffer(&vkContext, &perFrame[i].mainUBOBuffer[1]);
	}

	DestroyPipeline(&vkContext, &mainPipeline);
}

void DrawLighting(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, VkDescriptorPool descriptorPool)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipeline.pipeline.pipeline);

	vkCmdBindVertexBuffers(commandBuffer, 1, 1, &perFrame[index].asteroidInstance.buffer, &(VkDeviceSize) { 0 });

	for(uint32_t i=0;i<NUM_MODELS;i++)
	{
		vkuDescriptorSet_UpdateBindingImageInfo(&mainPipeline.descriptorSet, 0, textures[2*i+0].image.sampler, textures[2*i+0].image.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vkuDescriptorSet_UpdateBindingImageInfo(&mainPipeline.descriptorSet, 1, textures[2*i+1].image.sampler, textures[2*i+1].image.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vkuDescriptorSet_UpdateBindingImageInfo(&mainPipeline.descriptorSet, 2, shadowDepth.sampler, shadowDepth.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vkuDescriptorSet_UpdateBindingBufferInfo(&mainPipeline.descriptorSet, 3, perFrame[index].mainUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
		vkuDescriptorSet_UpdateBindingBufferInfo(&mainPipeline.descriptorSet, 4, perFrame[index].skyboxUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
		vkuAllocateUpdateDescriptorSet(&mainPipeline.descriptorSet, descriptorPool);

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipeline.pipelineLayout, 0, 1, &mainPipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &models[i].vertexBuffer.buffer, &(VkDeviceSize) { 0 });

		for(uint32_t j=0;j<models[i].numMesh;j++)
		{
			vkCmdBindIndexBuffer(commandBuffer, models[i].mesh[j].indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(commandBuffer, models[i].mesh[j].numFace*3, NUM_ASTEROIDS/NUM_MODELS, 0, 0, (NUM_ASTEROIDS/NUM_MODELS)*i);
		}
	}

	vkuDescriptorSet_UpdateBindingImageInfo(&mainPipeline.descriptorSet, 0, textures[TEXTURE_CUBE].image.sampler, textures[TEXTURE_CUBE].image.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&mainPipeline.descriptorSet, 1, textures[TEXTURE_CUBE_NORMAL].image.sampler, textures[TEXTURE_CUBE_NORMAL].image.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&mainPipeline.descriptorSet, 2, shadowDepth.sampler, shadowDepth.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingBufferInfo(&mainPipeline.descriptorSet, 3, perFrame[index].mainUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
	vkuDescriptorSet_UpdateBindingBufferInfo(&mainPipeline.descriptorSet, 4, perFrame[index].skyboxUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
	vkuAllocateUpdateDescriptorSet(&mainPipeline.descriptorSet, descriptorPool);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipeline.pipelineLayout, 0, 1, &mainPipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &cube.vertexBuffer.buffer, &(VkDeviceSize) { 0 });
	vkCmdBindVertexBuffers(commandBuffer, 1, 1, &perFrame[index].cubeInstance.buffer, &(VkDeviceSize) { 0 });

	for(uint32_t j=0;j<cube.numMesh;j++)
	{
		vkCmdBindIndexBuffer(commandBuffer, cube.mesh[j].indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffer, cube.mesh[j].numFace*3, NUM_CUBE, 0, 0, 0);
	}
}
