#include <stdint.h>
#include <stdbool.h>
#include "vulkan/vulkan.h"
#include "math/math.h"

#include "perframe.h"
#include "models.h"
#include "textures.h"
#include "shadow.h"

#define NUM_ASTEROIDS 1000

extern VkuContext_t vkContext;
extern VkuSwapchain_t swapchain;
extern VkSampleCountFlags MSAA;
extern VkFormat colorFormat;
extern VkFormat depthFormat;
extern VkRenderPass renderPass;

VkuDescriptorSet_t mainDescriptorSet;
VkPipelineLayout mainPipelineLayout;
VkuPipeline_t mainPipeline;

bool CreateLightingPipeline(void)
{
	vkCreateRenderPass(vkContext.device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=3,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=colorFormat,
				.samples=MSAA,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			},
			{
				.format=depthFormat,
				.samples=MSAA,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
			{
				.format=colorFormat,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
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
		.dependencyCount=3,
		.pDependencies=(VkSubpassDependency[])
		{
			{
				.srcSubpass=VK_SUBPASS_EXTERNAL,
				.dstSubpass=0,
				.srcStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
			},
			{
				.srcSubpass=0,
				.dstSubpass=VK_SUBPASS_EXTERNAL,
				.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
			},
			{
				.srcSubpass=0,
				.dstSubpass=1,
				.srcStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
			},
		}
	}, 0, &renderPass);

	for(uint32_t i=0;i<swapchain.numImages;i++)
	{
		vkuCreateHostBuffer(&vkContext, &perFrame[i].mainUBOBuffer[0], sizeof(Main_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		vkMapMemory(vkContext.device, perFrame[i].mainUBOBuffer[0].deviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&perFrame[i].mainUBO[0]);

		vkuCreateHostBuffer(&vkContext, &perFrame[i].mainUBOBuffer[1], sizeof(Main_UBO_t), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
		vkMapMemory(vkContext.device, perFrame[i].mainUBOBuffer[1].deviceMemory, 0, VK_WHOLE_SIZE, 0, (void **)&perFrame[i].mainUBO[1]);
	}

	vkuInitDescriptorSet(&mainDescriptorSet, vkContext.device);

	vkuDescriptorSet_AddBinding(&mainDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&mainDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&mainDescriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&mainDescriptorSet, 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&mainDescriptorSet, 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);

	vkuAssembleDescriptorSetLayout(&mainDescriptorSet);

	vkCreatePipelineLayout(vkContext.device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&mainDescriptorSet.descriptorSetLayout,
	}, 0, &mainPipelineLayout);

	vkuInitPipeline(&mainPipeline, vkContext.device, vkContext.pipelineCache);

	vkuPipeline_SetPipelineLayout(&mainPipeline, mainPipelineLayout);
	vkuPipeline_SetRenderPass(&mainPipeline, renderPass);

	mainPipeline.depthTest=VK_TRUE;
	mainPipeline.cullMode=VK_CULL_MODE_BACK_BIT;
	mainPipeline.depthCompareOp=VK_COMPARE_OP_GREATER_OR_EQUAL;
	mainPipeline.rasterizationSamples=MSAA;

	if(!vkuPipeline_AddStage(&mainPipeline, "shaders/lighting.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&mainPipeline, "shaders/lighting.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	vkuPipeline_AddVertexBinding(&mainPipeline, 0, sizeof(vec4)*5, VK_VERTEX_INPUT_RATE_VERTEX);
	vkuPipeline_AddVertexAttribute(&mainPipeline, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*0);
	vkuPipeline_AddVertexAttribute(&mainPipeline, 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*1);
	vkuPipeline_AddVertexAttribute(&mainPipeline, 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*2);
	vkuPipeline_AddVertexAttribute(&mainPipeline, 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*3);
	vkuPipeline_AddVertexAttribute(&mainPipeline, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*4);

	vkuPipeline_AddVertexBinding(&mainPipeline, 1, sizeof(matrix), VK_VERTEX_INPUT_RATE_INSTANCE);
	vkuPipeline_AddVertexAttribute(&mainPipeline, 5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*0);
	vkuPipeline_AddVertexAttribute(&mainPipeline, 6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*1);
	vkuPipeline_AddVertexAttribute(&mainPipeline, 7, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*2);
	vkuPipeline_AddVertexAttribute(&mainPipeline, 8, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*3);

	//VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo=
	//{
	//	.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
	//	.colorAttachmentCount=1,
	//	.pColorAttachmentFormats=&colorFormat,
	//	.depthAttachmentFormat=depthFormat,
	//};

	if(!vkuAssemblePipeline(&mainPipeline, VK_NULL_HANDLE/*&pipelineRenderingCreateInfo*/))
		return false;

	return true;
}

void DestroyLighting(void)
{
	for(uint32_t i=0;i<swapchain.numImages;i++)
	{
		vkUnmapMemory(vkContext.device, perFrame[i].mainUBOBuffer[0].deviceMemory);
		vkuDestroyBuffer(&vkContext, &perFrame[i].mainUBOBuffer[0]);

		vkUnmapMemory(vkContext.device, perFrame[i].mainUBOBuffer[1].deviceMemory);
		vkuDestroyBuffer(&vkContext, &perFrame[i].mainUBOBuffer[1]);
	}

	vkDestroyDescriptorSetLayout(vkContext.device, mainDescriptorSet.descriptorSetLayout, VK_NULL_HANDLE);
	vkDestroyPipeline(vkContext.device, mainPipeline.pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(vkContext.device, mainPipelineLayout, VK_NULL_HANDLE);
}

void DrawLighting(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, VkDescriptorPool descriptorPool)
{
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipeline.pipeline);

	vkCmdBindVertexBuffers(commandBuffer, 1, 1, &perFrame[index].asteroidInstance.buffer, &(VkDeviceSize) { 0 });

	for(uint32_t i=0;i<NUM_MODELS;i++)
	{
		vkuDescriptorSet_UpdateBindingImageInfo(&mainDescriptorSet, 0, textures[2*i+0].sampler, textures[2*i+0].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vkuDescriptorSet_UpdateBindingImageInfo(&mainDescriptorSet, 1, textures[2*i+1].sampler, textures[2*i+1].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vkuDescriptorSet_UpdateBindingImageInfo(&mainDescriptorSet, 2, shadowDepth.sampler, shadowDepth.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vkuDescriptorSet_UpdateBindingBufferInfo(&mainDescriptorSet, 3, perFrame[index].mainUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
		vkuDescriptorSet_UpdateBindingBufferInfo(&mainDescriptorSet, 4, perFrame[index].skyboxUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
		vkuAllocateUpdateDescriptorSet(&mainDescriptorSet, descriptorPool);

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mainPipelineLayout, 0, 1, &mainDescriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &models[i].vertexBuffer.buffer, &(VkDeviceSize) { 0 });

		for(uint32_t j=0;j<models[i].numMesh;j++)
		{
			vkCmdBindIndexBuffer(commandBuffer, models[i].mesh[j].indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(commandBuffer, models[i].mesh[j].numFace*3, NUM_ASTEROIDS/NUM_MODELS, 0, 0, (NUM_ASTEROIDS/NUM_MODELS)*i);
		}
	}
}
