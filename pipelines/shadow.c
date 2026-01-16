#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <float.h>
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

matrix shadowMVP[NUM_CASCADES];
float cascadeSplits[NUM_CASCADES+1];

static Pipeline_t shadowPipeline;
static VkRenderPass shadowRenderPass;

static const uint32_t shadowSize=4096;

VkuImage_t shadowDepth;
static VkImageView shadowDepthView[NUM_CASCADES];
static VkFramebuffer shadowFrameBuffer[NUM_CASCADES];
static VkFormat shadowDepthFormat=VK_FORMAT_D32_SFLOAT;

static matrix BuildShadowMatrix(float fovyDeg, float aspect, float zNear, float zFar, const matrix cameraView, const vec3 lightDir)
{
	// Camera frustum corners in view space
	float tanY=tanf(deg2rad(fovyDeg)*0.5f);
	float tanX=tanY*aspect;

	float xn=zNear*tanX;
	float yn=zNear*tanY;
	float xf=zFar*tanX;
	float yf=zFar*tanY;

	vec3 vsCorners[8]={
		// Near
		{ -xn, -yn, -zNear },
		{ +xn, -yn, -zNear },
		{ -xn, +yn, -zNear },
		{ +xn, +yn, -zNear },
		// Far
		{ -xf, -yf, -zFar },
		{ +xf, -yf, -zFar },
		{ -xf, +yf, -zFar },
		{ +xf, +yf, -zFar },
	};

	// Transform to world space
	matrix invView=MatrixInverse(cameraView);
	vec3 wsCorners[8]={ 0 };

	for(int i=0;i<8;i++)
		wsCorners[i]=Matrix4x4MultVec3(vsCorners[i], invView);

	// Find the center of the frustum
	vec3 center={ 0 };
	for(int i=0;i<8;i++)
		center=Vec3_Addv(center, wsCorners[i]);

	center=Vec3_Muls(center, 1.0f/8);

	// Bounding sphere radius
	float radius=0;
	for(int i=0;i<8;i++)
	{
		float dist=Vec3_Distance(wsCorners[i], center);

		if(dist>radius)
			radius=dist;
	}

	// Light view
	vec3 eye=Vec3_Subv(center, Vec3_Muls(lightDir, radius*2));
	matrix lightView=MatrixLookAt(eye, center, Vec3(0, 1, 0));

	// Transform center to light space
	vec3 centerLS=Matrix4x4MultVec3(center, lightView);

	// Snap in light space
	float texelSize=(radius*2)/(float)shadowSize;
	centerLS.x=floorf(centerLS.x/texelSize)*texelSize;
	centerLS.y=floorf(centerLS.y/texelSize)*texelSize;

	// Build ortho around snapped center
	matrix lightProj=MatrixOrtho(
		centerLS.x-radius, centerLS.x+radius,
		centerLS.y-radius, centerLS.y+radius,
		-centerLS.z-radius*2, -centerLS.z+radius*2
	);

	return MatrixMult(lightView, lightProj);
}

static void CalculateCascadeSplits(float cameraNear, float cameraFar, int cascadeCount, float lambda, float *splits) 
{
	splits[0]=cameraNear;

	for(int i=1;i<=cascadeCount;i++)
	{
		float p=(float)i/(float)cascadeCount;

		// linear split: evenly in view-space distance
		float linear=cameraNear+(cameraFar-cameraNear)*p;

		// logarithmic split: preserves ratio (good for perspective)
		float logarithmic=cameraNear*powf(cameraFar/cameraNear, p);

		// final split: blend
		splits[i]=linear*(1.0f-lambda)+logarithmic*lambda;
	}
}

void CreateShadowMap(void)
{
	// Depth render target
	vkuCreateImageBuffer(&vkContext, &shadowDepth, VK_IMAGE_TYPE_2D, shadowDepthFormat, 1, NUM_CASCADES, shadowSize, shadowSize, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);

	vkCreateImageView(vkContext.device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image=shadowDepth.image,
		.viewType=VK_IMAGE_VIEW_TYPE_2D,
		.format=shadowDepthFormat,
		.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.levelCount=1,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=NUM_CASCADES,
	}, NULL, &shadowDepth.imageView);

	VkCommandBuffer commandBuffer=vkuOneShotCommandBufferBegin(&vkContext);
	for(uint32_t i=0;i<NUM_CASCADES;i++)
		vkuTransitionLayout(commandBuffer, shadowDepth.image, 1, 0, 1, i, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuOneShotCommandBufferEnd(&vkContext, commandBuffer);

	// Need compare ops
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

	for(uint32_t i=0;i<NUM_CASCADES;i++)
	{
		vkCreateImageView(vkContext.device, &(VkImageViewCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image=shadowDepth.image,
			.viewType=VK_IMAGE_VIEW_TYPE_2D,
			.format=shadowDepthFormat,
			.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
			.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
			.subresourceRange.baseMipLevel=0,
			.subresourceRange.levelCount=1,
			.subresourceRange.baseArrayLayer=i,
			.subresourceRange.layerCount=1,
		}, NULL, &shadowDepthView[i]);

		vkCreateFramebuffer(vkContext.device, &(VkFramebufferCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass=shadowRenderPass,
			.attachmentCount=1,
			.pAttachments=(VkImageView[]){ shadowDepthView[i] },
			.width=shadowSize,
			.height=shadowSize,
			.layers=1,
		}, 0, &shadowFrameBuffer[i]);
	}
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

	CalculateCascadeSplits(0.01, 3000.0f, NUM_CASCADES, 0.9f, cascadeSplits);

	return true;
}

void ShadowUpdateMap(VkCommandBuffer commandBuffer, uint32_t frameIndex)
{
	for(uint32_t cascade=0;cascade<NUM_CASCADES;cascade++)
	{
		vkCmdBeginRenderPass(commandBuffer, &(VkRenderPassBeginInfo)
		{
			.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass=shadowRenderPass,
			.framebuffer=shadowFrameBuffer[cascade],
			.clearValueCount=1,
			.pClearValues=(VkClearValue[]){ {{{ 1.0f, 0 }}} },
			.renderArea.offset=(VkOffset2D){ 0, 0 },
			.renderArea.extent=(VkExtent2D){ shadowSize, shadowSize },
		}, VK_SUBPASS_CONTENTS_INLINE);

		// Bind the pipeline descriptor, this sets the pipeline states (blend, depth/stencil tests, etc)
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline.pipeline.pipeline);

		vkCmdSetViewport(commandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)shadowSize, (float)shadowSize, 0.0f, 1.0f });
		vkCmdSetScissor(commandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { shadowSize, shadowSize } });

		matrix modelview=perFrame[frameIndex].mainUBO[0]->modelView;
		vec3 lightDir={
			-perFrame[frameIndex].skyboxUBO[0]->uSunPosition.x,
			-perFrame[frameIndex].skyboxUBO[0]->uSunPosition.y,
			-perFrame[frameIndex].skyboxUBO[0]->uSunPosition.z
		};
		Vec3_Normalize(&lightDir);

		shadowMVP[cascade]=BuildShadowMatrix(90.0f, (float)swapchain.extent.width/(float)swapchain.extent.height, cascadeSplits[cascade], cascadeSplits[cascade+1], modelview, lightDir);

		vkCmdPushConstants(commandBuffer, shadowPipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(matrix), &shadowMVP[cascade]);

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
}

void DestroyShadow(void)
{
	vkuDestroyImageBuffer(&vkContext, &shadowDepth);

	for(uint32_t i=0;i<NUM_CASCADES;i++)
	{
		vkDestroyImageView(vkContext.device, shadowDepthView[i], VK_NULL_HANDLE);
		vkDestroyFramebuffer(vkContext.device, shadowFrameBuffer[i], VK_NULL_HANDLE);
	}

	DestroyPipeline(&vkContext, &shadowPipeline);
	vkDestroyRenderPass(vkContext.device, shadowRenderPass, VK_NULL_HANDLE);
}
