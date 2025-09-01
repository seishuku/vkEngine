#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../camera/camera.h"
#include "../model/bmodel.h"
#include "../utils/pipeline.h"
#include "../assetmanager.h"
#include "../perframe.h"
#include "skybox.h"
#include "shadow.h"

extern VkuContext_t vkContext;
extern Camera_t camera;
extern VkuSwapchain_t swapchain;

matrix shadowMVP;

static Pipeline_t shadowPipeline;
static VkRenderPass shadowRenderPass;

static const uint32_t shadowSize=4096;

VkuImage_t shadowDepth;
static VkFramebuffer shadowFrameBuffer;
static VkFormat shadowDepthFormat=VK_FORMAT_D32_SFLOAT;

void CreateShadowMap(void)
{
	// Depth render target
	vkuCreateTexture2D(&vkContext, &shadowDepth, shadowSize, shadowSize, shadowDepthFormat, VK_SAMPLE_COUNT_1_BIT);

	VkCommandBuffer commandBuffer=vkuOneShotCommandBufferBegin(&vkContext);
	vkuTransitionLayout(commandBuffer, shadowDepth.image, 1, 0, 1, 0, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuOneShotCommandBufferEnd(&vkContext, commandBuffer);

	// Need compare ops, so recreate the sampler
	vkDestroySampler(vkContext.device, shadowDepth.sampler, VK_NULL_HANDLE);
	vkCreateSampler(vkContext.device, &(VkSamplerCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter=VK_FILTER_LINEAR,
		.minFilter=VK_FILTER_LINEAR,
		.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
		.mipLodBias=0.0f,
		.compareOp=VK_COMPARE_OP_LESS_OR_EQUAL,
		.compareEnable=VK_TRUE,
		.minLod=0.0f,
		.maxLod=1.0f,
		.maxAnisotropy=1.0f,
		.anisotropyEnable=VK_FALSE,
		.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	}, VK_NULL_HANDLE, &shadowDepth.sampler);

	vkCreateFramebuffer(vkContext.device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=shadowRenderPass,
		.attachmentCount=1,
		.pAttachments=(VkImageView[]){ shadowDepth.imageView },
		.width=shadowSize,
		.height=shadowSize,
		.layers=1,
	}, 0, &shadowFrameBuffer);
}

bool CreateShadowPipeline(void)
{
	vkCreateRenderPass(vkContext.device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=1,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=shadowDepthFormat,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			},
		},
		.subpassCount=1,
		.pSubpasses=&(VkSubpassDescription)
		{
			.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,
			.pDepthStencilAttachment=&(VkAttachmentReference)
			{
				.attachment=0,
				.layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			},
		},
		.dependencyCount=2,
		.pDependencies=(VkSubpassDependency[])
		{
			{
				.srcSubpass=VK_SUBPASS_EXTERNAL,
				.dstSubpass=0,
				.srcStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				.srcAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
			},
			{
				.srcSubpass=0,
				.dstSubpass=VK_SUBPASS_EXTERNAL,
				.srcStageMask=VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
			},
		}
	}, 0, &shadowRenderPass);

	if(!CreatePipeline(&vkContext, &shadowPipeline, shadowRenderPass, "pipelines/shadow.pipeline"))
		return false;

	return true;
}

void ShadowUpdateMap(VkCommandBuffer commandBuffer, uint32_t frameIndex)
{
	vkCmdBeginRenderPass(commandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=shadowRenderPass,
		.framebuffer=shadowFrameBuffer,
		.clearValueCount=1,
		.pClearValues=(VkClearValue[]){ {{{ 1.0f, 0 }}} },
		.renderArea.offset=(VkOffset2D){ 0, 0 },
		.renderArea.extent=(VkExtent2D){ shadowSize, shadowSize },
	}, VK_SUBPASS_CONTENTS_INLINE);

	// Bind the pipeline descriptor, this sets the pipeline states (blend, depth/stencil tests, etc)
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline.pipeline.pipeline);

	vkCmdSetViewport(commandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)shadowSize, (float)shadowSize, 0.0f, 1.0f });
	vkCmdSetScissor(commandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { shadowSize, shadowSize } });

	matrix projection=MatrixOrtho(-2000.0f, 2000.0f, -2000.0f, 2000.0f, 0.01f, 4400.0f);

	// Looking at the asteroid field from the sun's position "a number" away
	// This should technically be an infinite distance away, but that's not possible, so "a number" is whatever best compromise.
	// (uSubPosition is a vec4, so need to recreate that in a vec3)
	vec3 position=Vec3_Muls(Vec3(
		perFrame[frameIndex].skyboxUBO[0]->uSunPosition.x,
		perFrame[frameIndex].skyboxUBO[0]->uSunPosition.y,
		perFrame[frameIndex].skyboxUBO[0]->uSunPosition.z
	), 2100.0f);

	// Following the camera's position, so we don't have to composite multiple shadow maps or have super large maps.
	matrix modelview=MatrixLookAt(position, Vec3(0.0f, 0.0f, 0.0f), Vec3(0.0f, 1.0f, 0.0f));

	// Multiply matrices together, so we can just send one matrix as a push constant.

	shadowMVP=MatrixMult(modelview, projection);
	vkCmdPushConstants(commandBuffer, shadowPipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(matrix), &shadowMVP);

	vkCmdBindVertexBuffers(commandBuffer, 1, 1, &perFrame[frameIndex].asteroidInstance.buffer, &(VkDeviceSize) { 0 });

	// Draw the models
	for(uint32_t i=0;i<4;i++)
	{
		const BModel_t *model=&AssetManager_GetAsset(assets, MODEL_ASTEROID1+i)->model;

		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &model->vertexBuffer.buffer, &(VkDeviceSize) { 0 });

		for(uint32_t j=0;j<model->numMesh;j++)
		{
			vkCmdBindIndexBuffer(commandBuffer, model->mesh[j].indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(commandBuffer, model->mesh[j].numFace*3, NUM_ASTEROIDS/4, 0, 0, (NUM_ASTEROIDS/4)*i);
		}
	}
	//////

	// Fighters
	const BModel_t *fighterModel=&AssetManager_GetAsset(assets, MODEL_FIGHTER)->model;

	vkCmdBindVertexBuffers(commandBuffer, 1, 1, &perFrame[frameIndex].fighterInstance.buffer, &(VkDeviceSize) { 0 });
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &fighterModel->vertexBuffer.buffer, &(VkDeviceSize) { 0 });

	for(uint32_t i=0;i<fighterModel->numMesh;i++)
	{
		vkCmdBindIndexBuffer(commandBuffer, fighterModel->mesh[i].indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffer, fighterModel->mesh[i].numFace*3, 2, 0, 0, 0);
	}
	//////

	// Cubes
	const BModel_t *cubeModel=&AssetManager_GetAsset(assets, MODEL_CUBE)->model;

	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &cubeModel->vertexBuffer.buffer, &(VkDeviceSize) { 0 });
	vkCmdBindVertexBuffers(commandBuffer, 1, 1, &perFrame[frameIndex].cubeInstance.buffer, &(VkDeviceSize) { 0 });

	for(uint32_t i=0;i<cubeModel->numMesh;i++)
	{
		vkCmdBindIndexBuffer(commandBuffer, cubeModel->mesh[i].indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(commandBuffer, cubeModel->mesh[i].numFace*3, NUM_CUBE, 0, 0, 0);
	}
	///////

	vkCmdEndRenderPass(commandBuffer);
}

void DestroyShadow(void)
{
	vkuDestroyImageBuffer(&vkContext, &shadowDepth);
	vkDestroyFramebuffer(vkContext.device, shadowFrameBuffer, VK_NULL_HANDLE);

	DestroyPipeline(&vkContext, &shadowPipeline);
	vkDestroyRenderPass(vkContext.device, shadowRenderPass, VK_NULL_HANDLE);
}
