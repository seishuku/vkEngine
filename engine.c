#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "system/system.h"
#include "vulkan/vulkan.h"
#include "math/math.h"
#include "camera/camera.h"
#include "model/bmodel.h"
#include "model/skybox.h"
#include "image/image.h"
#include "font/font.h"
#include "utils/list.h"
#include "lights/lights.h"

uint32_t Width=1280, Height=720;

VkInstance Instance;
VkuContext_t Context;

VkuMemZone_t *VkZone;

Camera_t Camera;

extern float fps, fTimeStep, fTime;

enum
{
	MODEL_ASTEROID1,
	MODEL_ASTEROID2,
	MODEL_ASTEROID3,
	MODEL_ASTEROID4,
	NUM_MODELS
};

BModel_t Model[NUM_MODELS];

VkuBuffer_t SkyboxVertex, SkyboxIndex;

enum
{
	TEXTURE_ASTEROID1,
	TEXTURE_ASTEROID1_NORMAL,
	TEXTURE_ASTEROID2,
	TEXTURE_ASTEROID2_NORMAL,
	TEXTURE_ASTEROID3,
	TEXTURE_ASTEROID3_NORMAL,
	TEXTURE_ASTEROID4,
	TEXTURE_ASTEROID4_NORMAL,
	NUM_TEXTURES
};

VkuImage_t Textures[NUM_TEXTURES];

struct
{
	matrix projection;
	matrix modelview;
	matrix light_mvp;
	vec4 light_color;
	vec4 light_direction;
} *ubo;

VkuBuffer_t uboBuffer;

VkDebugUtilsMessengerEXT debugMessenger;

// Swapchain
#define MAX_FRAME_COUNT 3

VkSwapchainKHR Swapchain;

VkExtent2D SwapchainExtent;
VkSurfaceFormatKHR SurfaceFormat;
VkFormat DepthFormat=VK_FORMAT_D32_SFLOAT_S8_UINT;

uint32_t SwapchainImageCount=0;

VkImage SwapchainImage[MAX_FRAME_COUNT];
VkImageView SwapchainImageView[MAX_FRAME_COUNT];
VkFramebuffer FrameBuffers[MAX_FRAME_COUNT];

// Depth buffer handles
VkuImage_t DepthImage;

VkRenderPass RenderPass;
VkPipelineLayout PipelineLayout;
VkuPipeline_t Pipeline;

VkuBuffer_t Asteroid_Instance;

typedef struct
{
	vec3 Position;
	float Radius;
	vec3 Rotate;
} Asteroid_t;

#define NUM_ASTEROIDS 1000
Asteroid_t Asteroids[NUM_ASTEROIDS];

struct
{
	matrix mvp;
	vec4 uOffset;

	vec3 uNebulaAColor;
	float uNebulaADensity;
	vec3 uNebulaBColor;
	float uNebulaBDensity;

	float uStarsScale;
	float uStarDensity;
	float pad0[2];

	vec4 uSunPosition;
	float uSunSize;
	float uSunFalloff;
	float pad1[2];
	vec4 uSunColor;
} *Skybox_UBO;

VkuBuffer_t Skybox_UBO_Buffer;

VkuDescriptorSet_t SkyboxDescriptorSet[MAX_FRAME_COUNT];

VkPipelineLayout SkyboxPipelineLayout;
VkuPipeline_t SkyboxPipeline;

VkPipelineLayout LinePipelineLayout;
VkuPipeline_t LinePipeline;

const uint32_t ShadowSize=8192;

VkFramebuffer ShadowFrameBuffer;
VkuImage_t ShadowDepth;
VkFormat ShadowDepthFormat=VK_FORMAT_D32_SFLOAT;

VkuPipeline_t ShadowPipeline;
VkPipelineLayout ShadowPipelineLayout;
VkRenderPass ShadowRenderPass;

struct
{
	matrix mvp;
	matrix local;
} shadow_ubo;

VkDescriptorPool DescriptorPool[MAX_FRAME_COUNT];
VkuDescriptorSet_t DescriptorSet[MAX_FRAME_COUNT*NUM_MODELS];

VkCommandBuffer CommandBuffers[MAX_FRAME_COUNT];

VkFence FrameFences[MAX_FRAME_COUNT];
VkSemaphore PresentCompleteSemaphores[MAX_FRAME_COUNT];
VkSemaphore RenderCompleteSemaphores[MAX_FRAME_COUNT];

void RecreateSwapchain(void);

void InitShadowMap(void)
{
	// Cubemap depth render target
	vkuCreateImageBuffer(&Context, &ShadowDepth,
						 VK_IMAGE_TYPE_2D, ShadowDepthFormat, 1, 1, ShadowSize, ShadowSize, 1,
						 VK_IMAGE_TILING_OPTIMAL,
						 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
						 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);

	// Cube color sampler
	vkCreateSampler(Context.Device, &(VkSamplerCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.maxAnisotropy=1.0f,
		.magFilter=VK_FILTER_LINEAR,
		.minFilter=VK_FILTER_LINEAR,
		.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR,
		.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.mipLodBias=0.0f,
		.compareOp=VK_COMPARE_OP_LESS_OR_EQUAL,
		.compareEnable=VK_TRUE,
		.minLod=0.0f,
		.maxLod=1.0f,
		.maxAnisotropy=1.0,
		.anisotropyEnable=VK_FALSE,
		.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	}, VK_NULL_HANDLE, &ShadowDepth.Sampler);

	vkCreateImageView(Context.Device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType=VK_IMAGE_VIEW_TYPE_2D,
		.format=ShadowDepthFormat,
		.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=1,
		.subresourceRange.levelCount=1,
		.image=ShadowDepth.Image,
	}, VK_NULL_HANDLE, &ShadowDepth.View);

	vkCreateFramebuffer(Context.Device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=ShadowRenderPass,
		.attachmentCount=1,
		.pAttachments=(VkImageView[]){ ShadowDepth.View },
		.width=ShadowSize,
		.height=ShadowSize,
		.layers=1,
	}, 0, &ShadowFrameBuffer);
}

bool InitShadowPipeline(void)
{
	vkCreateRenderPass(Context.Device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
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
				.srcStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
			}
		},
		.attachmentCount=1,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=ShadowDepthFormat,
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
	}, 0, &ShadowRenderPass);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=0,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=sizeof(shadow_ubo),
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},
	}, 0, &ShadowPipelineLayout);

	vkuInitPipeline(&ShadowPipeline, &Context);

	vkuPipeline_SetPipelineLayout(&ShadowPipeline, ShadowPipelineLayout);
	vkuPipeline_SetRenderPass(&ShadowPipeline, ShadowRenderPass);

	// Add in vertex shader
	if(!vkuPipeline_AddStage(&ShadowPipeline, "./shaders/distance.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	// Set states that are different than defaults
	ShadowPipeline.CullMode=VK_CULL_MODE_BACK_BIT;
	ShadowPipeline.DepthTest=VK_TRUE;

	ShadowPipeline.DepthBias=VK_TRUE;
	ShadowPipeline.DepthBiasConstantFactor=1.25f;
	ShadowPipeline.DepthBiasSlopeFactor=1.75f;

	// Add vertex binding and attrib parameters
	vkuPipeline_AddVertexBinding(&ShadowPipeline, 0, sizeof(float)*20, VK_VERTEX_INPUT_RATE_VERTEX);
	vkuPipeline_AddVertexAttribute(&ShadowPipeline, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);

	vkuPipeline_AddVertexBinding(&ShadowPipeline, 1, sizeof(matrix), VK_VERTEX_INPUT_RATE_INSTANCE);
	vkuPipeline_AddVertexAttribute(&ShadowPipeline, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*0);
	vkuPipeline_AddVertexAttribute(&ShadowPipeline, 2, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*1);
	vkuPipeline_AddVertexAttribute(&ShadowPipeline, 3, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*2);
	vkuPipeline_AddVertexAttribute(&ShadowPipeline, 4, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*3);

	// Assemble the pipeline
	if(!vkuAssemblePipeline(&ShadowPipeline))
		return false;

	return true;
}

void ShadowUpdateMap(VkCommandBuffer CommandBuffer, uint32_t FrameIndex)
{
	vkCmdBeginRenderPass(CommandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=ShadowRenderPass,
		.framebuffer=ShadowFrameBuffer,
		.clearValueCount=1,
		.pClearValues=(VkClearValue[]){ { 1.0f, 0 } },
		.renderArea.offset=(VkOffset2D){ 0, 0 },
		.renderArea.extent=(VkExtent2D){ ShadowSize, ShadowSize },
	}, VK_SUBPASS_CONTENTS_INLINE);

	// Bind the pipeline descriptor, this sets the pipeline states (blend, depth/stencil tests, etc)
	vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ShadowPipeline.Pipeline);

	vkCmdSetViewport(CommandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)ShadowSize, (float)ShadowSize, 0.0f, 1.0f });
	vkCmdSetScissor(CommandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { ShadowSize, ShadowSize } });

	matrix Projection;
	MatrixIdentity(Projection);
	MatrixOrtho(-12500.0f, 12500.0f, -12500.0f, 12500.0f, 0.1f, 30000.0f, Projection);

	matrix ModelView;
	MatrixIdentity(ModelView);

	vec3 Position;
	Vec3_Setv(Position, Skybox_UBO->uSunPosition);
	Vec3_Muls(Position, 20000.0f);

	MatrixLookAt(Position, (vec3) { 0.0f, 0.0f, 0.0f }, (vec3) { 0.0f, 1.0f, 0.0f }, ModelView);

	MatrixMult(ModelView, Projection, shadow_ubo.mvp);

	vkCmdBindVertexBuffers(CommandBuffer, 1, 1, &Asteroid_Instance.Buffer, &(VkDeviceSize) { 0 });

	// Draw the models
	for(uint32_t j=0;j<NUM_MODELS;j++)
	{
		MatrixIdentity(shadow_ubo.local);
		MatrixRotate(fTime, 1.0f, 0.0f, 0.0f, shadow_ubo.local);
		MatrixRotate(fTime, 0.0f, 1.0f, 0.0f, shadow_ubo.local);

		vkCmdPushConstants(CommandBuffer, ShadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(shadow_ubo), &shadow_ubo);

		// Bind model data buffers and draw the triangles
		vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Model[j].VertexBuffer.Buffer, &(VkDeviceSize) { 0 });

		for(uint32_t k=0;k<Model[j].NumMesh;k++)
		{
			vkCmdBindIndexBuffer(CommandBuffer, Model[j].Mesh[k].IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(CommandBuffer, Model[j].Mesh[k].NumFace*3, NUM_ASTEROIDS/NUM_MODELS, 0, 0, (NUM_ASTEROIDS/NUM_MODELS)*j);
		}
	}

	vkCmdEndRenderPass(CommandBuffer);
}
// ---

bool CreateFramebuffers(void)
{
	vkuCreateImageBuffer(&Context, &DepthImage,
		VK_IMAGE_TYPE_2D, DepthFormat, 1, 1, Width, Height, 1,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0);

	vkCreateImageView(Context.Device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext=NULL,
		.image=DepthImage.Image,
		.format=DepthFormat,
		.components.r=VK_COMPONENT_SWIZZLE_R,
		.components.g=VK_COMPONENT_SWIZZLE_G,
		.components.b=VK_COMPONENT_SWIZZLE_B,
		.components.a=VK_COMPONENT_SWIZZLE_A,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.levelCount=1,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=1,
		.viewType=VK_IMAGE_VIEW_TYPE_2D,
		.flags=0,
	}, NULL, &DepthImage.View);

	for(uint32_t i=0;i<SwapchainImageCount;i++)
	{
		vkCreateFramebuffer(Context.Device, &(VkFramebufferCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass=RenderPass,
			.attachmentCount=2,
			.pAttachments=(VkImageView[]) { SwapchainImageView[i], DepthImage.View },
			.width=SwapchainExtent.width,
			.height=SwapchainExtent.height,
			.layers=1,
		}, 0, &FrameBuffers[i]);
	}

	return true;
}

bool CreatePipeline(void)
{
	vkCreateRenderPass(Context.Device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=2,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=SurfaceFormat.format,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			},
			{
				.format=DepthFormat,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			},
		},
		.subpassCount=1,
		.pSubpasses=&(VkSubpassDescription)
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
		},
	}, 0, &RenderPass);

	for(uint32_t i=0;i<MAX_FRAME_COUNT*NUM_MODELS;i++)
	{
		vkuInitDescriptorSet(&DescriptorSet[i], &Context);

		vkuDescriptorSet_AddBinding(&DescriptorSet[i], 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		vkuDescriptorSet_AddBinding(&DescriptorSet[i], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		vkuDescriptorSet_AddBinding(&DescriptorSet[i], 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		vkuDescriptorSet_AddBinding(&DescriptorSet[i], 3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);

		vkuAssembleDescriptorSetLayout(&DescriptorSet[i]);
	}

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&DescriptorSet[0].DescriptorSetLayout, // Just use the first in the set, they're all the same layout
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=sizeof(matrix),
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},
	}, 0, &PipelineLayout);

	vkuInitPipeline(&Pipeline, &Context);

	vkuPipeline_SetPipelineLayout(&Pipeline, PipelineLayout);
	vkuPipeline_SetRenderPass(&Pipeline, RenderPass);

	Pipeline.DepthTest=VK_TRUE;
	Pipeline.CullMode=VK_CULL_MODE_BACK_BIT;

	if(!vkuPipeline_AddStage(&Pipeline, "./shaders/lighting.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&Pipeline, "./shaders/lighting.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	vkuPipeline_AddVertexBinding(&Pipeline, 0, sizeof(vec4)*5, VK_VERTEX_INPUT_RATE_VERTEX);
	vkuPipeline_AddVertexAttribute(&Pipeline, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*0);
	vkuPipeline_AddVertexAttribute(&Pipeline, 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*1);
	vkuPipeline_AddVertexAttribute(&Pipeline, 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*2);
	vkuPipeline_AddVertexAttribute(&Pipeline, 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*3);
	vkuPipeline_AddVertexAttribute(&Pipeline, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*4);

	vkuPipeline_AddVertexBinding(&Pipeline, 1, sizeof(matrix), VK_VERTEX_INPUT_RATE_INSTANCE);
	vkuPipeline_AddVertexAttribute(&Pipeline, 5, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*0);
	vkuPipeline_AddVertexAttribute(&Pipeline, 6, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*1);
	vkuPipeline_AddVertexAttribute(&Pipeline, 7, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*2);
	vkuPipeline_AddVertexAttribute(&Pipeline, 8, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*3);

	if(!vkuAssemblePipeline(&Pipeline))
		return false;

	vkuCreateHostBuffer(&Context, &uboBuffer, sizeof(*ubo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	vkMapMemory(Context.Device, uboBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, &ubo);

	return true;
}

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

	vkuCreateHostBuffer(&Context, &Skybox_UBO_Buffer, sizeof(*Skybox_UBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
	vkMapMemory(Context.Device, Skybox_UBO_Buffer.DeviceMemory, 0, sizeof(*Skybox_UBO), 0, &Skybox_UBO);

	return true;
}

bool CreateLinePipeline(void)
{
	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=sizeof(matrix)+(sizeof(vec4)*2),
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},
	}, 0, &LinePipelineLayout);

	vkuInitPipeline(&LinePipeline, &Context);

	vkuPipeline_SetPipelineLayout(&LinePipeline, LinePipelineLayout);
	vkuPipeline_SetRenderPass(&LinePipeline, RenderPass);

	LinePipeline.Topology=VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	LinePipeline.DepthTest=VK_TRUE;
	LinePipeline.CullMode=VK_CULL_MODE_BACK_BIT;

	if(!vkuPipeline_AddStage(&LinePipeline, "./shaders/line.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&LinePipeline, "./shaders/line.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	if(!vkuAssemblePipeline(&LinePipeline))
		return false;

	return true;
}


float RandFloat(void)
{
	return (float)rand()/RAND_MAX;
}

bool SphereSphereIntersect(vec3 PositionA, float RadiusA, vec3 PositionB, float RadiusB)
{
	const float distance=sqrtf(
		(PositionA[0]-PositionB[0])*(PositionA[0]-PositionB[0])+
		(PositionA[1]-PositionB[1])*(PositionA[1]-PositionB[1])+
		(PositionA[2]-PositionB[2])*(PositionA[2]-PositionB[2])
	);

	return distance<RadiusA+RadiusB;
}

void GenerateSkyParams(void)
{
	vkDeviceWaitIdle(Context.Device);

	Vec4_Set(Skybox_UBO->uOffset, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, 0.0f);
	Vec3_Normalize(Skybox_UBO->uOffset);

	Vec3_Set(Skybox_UBO->uNebulaAColor, RandFloat(), RandFloat(), RandFloat());
	Skybox_UBO->uNebulaADensity=RandFloat()*2.0f;

	Vec3_Set(Skybox_UBO->uNebulaBColor, RandFloat(), RandFloat(), RandFloat());
	Skybox_UBO->uNebulaBDensity=RandFloat()*2.0f;

	Vec4_Set(Skybox_UBO->uSunPosition, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, 0.0f);
	Vec3_Normalize(Skybox_UBO->uSunPosition);

	Vec4_Set(Skybox_UBO->uSunColor, min(1.0f, RandFloat()+0.5f), min(1.0f, RandFloat()+0.5f), min(1.0f, RandFloat()+0.5f), 0.0f);
	Skybox_UBO->uSunSize=1.0f/(RandFloat()*1000.0f);
	Skybox_UBO->uSunFalloff=RandFloat()*16.0f+8.0f;

	Skybox_UBO->uStarsScale=200.0f;
	Skybox_UBO->uStarDensity=8.0f;

	float *Data=NULL;

	if(!Asteroid_Instance.Buffer)
		vkuCreateHostBuffer(&Context, &Asteroid_Instance, sizeof(matrix)*NUM_ASTEROIDS, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	vkMapMemory(Context.Device, Asteroid_Instance.DeviceMemory, 0, VK_WHOLE_SIZE, 0, &Data);

	uint32_t i=0, tries=0;

	memset(Asteroids, 0, sizeof(Asteroid_t)*NUM_ASTEROIDS);

	while(i<NUM_ASTEROIDS)
	{
		vec3 RandomVec;
		Vec3_Set(RandomVec, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f);
		Vec3_Normalize(RandomVec);

		Asteroid_t Asteroid;
		Vec3_Set(Asteroid.Position, RandomVec[0]*(RandFloat()*10000.0f+500.0f), RandomVec[1]*(RandFloat()*10000.0f+500.0f), RandomVec[2]*(RandFloat()*10000.0f+500.0f));
		Vec3_Set(Asteroid.Rotate, RandFloat()*PI*2.0f, RandFloat()*PI*2.0f, RandFloat()*PI*2.0f);
		Asteroid.Radius=(RandFloat()*1000.0f+10.0f)*2.0f;

		bool overlapping=false;

		for(uint32_t j=0;j<i;j++)
		{
			if(SphereSphereIntersect(Asteroid.Position, Asteroid.Radius, Asteroids[j].Position, Asteroids[j].Radius))
				overlapping=true;
		}

		if(!overlapping)
			Asteroids[i++]=Asteroid;

		tries++;

		if(tries>NUM_ASTEROIDS*NUM_ASTEROIDS)
			break;
	}

	for(uint32_t i=0;i<NUM_ASTEROIDS;i++)
	{
		MatrixIdentity(&Data[16*i]);
		MatrixTranslatev(Asteroids[i].Position, &Data[16*i]);
		MatrixRotate(Asteroids[i].Rotate[0], 1.0f, 0.0f, 0.0f, &Data[16*i]);
		MatrixRotate(Asteroids[i].Rotate[1], 0.0f, 1.0f, 0.0f, &Data[16*i]);
		MatrixRotate(Asteroids[i].Rotate[2], 0.0f, 0.0f, 1.0f, &Data[16*i]);
		MatrixScale(Asteroids[i].Radius/2.0f, Asteroids[i].Radius/2.0f, Asteroids[i].Radius/2.0f, &Data[16*i]);
	}

	vkUnmapMemory(Context.Device, Asteroid_Instance.DeviceMemory);
}

void Render(void)
{
	static uint32_t OldIndex=0;
	uint32_t Index=OldIndex;

	// Generate the projection matrix
	MatrixIdentity(ubo->projection);
	MatrixInfPerspective(90.0f, (float)Width/Height, 0.01f, true, ubo->projection);

	// Set up the modelview matrix
	MatrixIdentity(ubo->modelview);
	CameraUpdate(&Camera, fTimeStep, ubo->modelview);

	Vec3_Setv(ubo->light_color, Skybox_UBO->uSunColor);
	Vec3_Setv(ubo->light_direction, Skybox_UBO->uSunPosition);

	MatrixMult(ubo->modelview, ubo->projection, Skybox_UBO->mvp);
	memcpy(ubo->light_mvp, shadow_ubo.mvp, sizeof(matrix));

	//float *Data=NULL;
	//memcpy(Data, &ubo, sizeof(ubo));
	//vkUnmapMemory(Context.Device, uboBuffer.DeviceMemory);

	VkResult Result=vkAcquireNextImageKHR(Context.Device, Swapchain, UINT64_MAX, PresentCompleteSemaphores[Index], VK_NULL_HANDLE, &OldIndex);

	if(Result==VK_ERROR_OUT_OF_DATE_KHR)
	{
		DBGPRINTF("Swapchain out of date... Rebuilding.\n");
		RecreateSwapchain();
		return;
	}

	vkWaitForFences(Context.Device, 1, &FrameFences[Index], VK_TRUE, UINT64_MAX);
	vkResetFences(Context.Device, 1, &FrameFences[Index]);

	vkResetDescriptorPool(Context.Device, DescriptorPool[Index], 0);

	// Start recording the commands
	vkBeginCommandBuffer(CommandBuffers[Index], &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	});

	ShadowUpdateMap(CommandBuffers[Index], Index);

	// Start a render pass and clear the frame/depth buffer
	vkCmdBeginRenderPass(CommandBuffers[Index], &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=RenderPass,
		.framebuffer=FrameBuffers[OldIndex],
		.clearValueCount=2,
		.pClearValues=(VkClearValue[]) { { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0 } },
		.renderArea.offset={ 0, 0 },
		.renderArea.extent=SwapchainExtent,
	}, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(CommandBuffers[Index], 0, 1, &(VkViewport) { 0.0f, 0, (float)SwapchainExtent.width, (float)SwapchainExtent.height, 0.0f, 1.0f });
	vkCmdSetScissor(CommandBuffers[Index], 0, 1, &(VkRect2D) { { 0, 0 }, SwapchainExtent});

	// Bind the pipeline descriptor, this sets the pipeline states (blend, depth/stencil tests, etc)
	vkCmdBindPipeline(CommandBuffers[Index], VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline.Pipeline);

	// Draw the models
	vkCmdBindVertexBuffers(CommandBuffers[Index], 1, 1, &Asteroid_Instance.Buffer, &(VkDeviceSize) { 0 });
	
	for(uint32_t i=0;i<NUM_MODELS;i++)
	{
		vkuDescriptorSet_UpdateBindingImageInfo(&DescriptorSet[MAX_FRAME_COUNT*i+Index], 0, &Textures[2*i+0]);
		vkuDescriptorSet_UpdateBindingImageInfo(&DescriptorSet[MAX_FRAME_COUNT*i+Index], 1, &Textures[2*i+1]);
		vkuDescriptorSet_UpdateBindingImageInfo(&DescriptorSet[MAX_FRAME_COUNT*i+Index], 2, &ShadowDepth);
		vkuDescriptorSet_UpdateBindingBufferInfo(&DescriptorSet[MAX_FRAME_COUNT*i+Index], 3, uboBuffer.Buffer, 0, VK_WHOLE_SIZE);
		vkuAllocateUpdateDescriptorSet(&DescriptorSet[MAX_FRAME_COUNT*i+Index], DescriptorPool[Index]);

		vkCmdBindDescriptorSets(CommandBuffers[Index], VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout, 0, 1, &DescriptorSet[MAX_FRAME_COUNT*i+Index].DescriptorSet, 0, VK_NULL_HANDLE);

		matrix local;
		MatrixIdentity(local);
		MatrixRotate(fTime, 1.0f, 0.0f, 0.0f, local);
		MatrixRotate(fTime, 0.0f, 1.0f, 0.0f, local);

		vkCmdPushConstants(CommandBuffers[Index], PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(matrix), &local);

		// Bind model data buffers and draw the triangles
		vkCmdBindVertexBuffers(CommandBuffers[Index], 0, 1, &Model[i].VertexBuffer.Buffer, &(VkDeviceSize) { 0 });

		for(uint32_t j=0;j<Model[i].NumMesh;j++)
		{
			vkCmdBindIndexBuffer(CommandBuffers[Index], Model[i].Mesh[j].IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(CommandBuffers[Index], Model[i].Mesh[j].NumFace*3, NUM_ASTEROIDS/NUM_MODELS, 0, 0, (NUM_ASTEROIDS/NUM_MODELS)*i);
		}
	}
	// ---

	// Skybox
	vkCmdBindPipeline(CommandBuffers[Index], VK_PIPELINE_BIND_POINT_GRAPHICS, SkyboxPipeline.Pipeline);

	vkuDescriptorSet_UpdateBindingBufferInfo(&SkyboxDescriptorSet[Index], 0, Skybox_UBO_Buffer.Buffer, 0, VK_WHOLE_SIZE);
	vkuAllocateUpdateDescriptorSet(&SkyboxDescriptorSet[Index], DescriptorPool[Index]);

	vkCmdBindDescriptorSets(CommandBuffers[Index], VK_PIPELINE_BIND_POINT_GRAPHICS, SkyboxPipelineLayout, 0, 1, &SkyboxDescriptorSet[Index].DescriptorSet, 0, VK_NULL_HANDLE);

	vkCmdBindVertexBuffers(CommandBuffers[Index], 0, 1, &SkyboxVertex.Buffer, &(VkDeviceSize) { 0 });
	vkCmdBindIndexBuffer(CommandBuffers[Index], SkyboxIndex.Buffer, 0, VK_INDEX_TYPE_UINT16);
	vkCmdDrawIndexed(CommandBuffers[Index], 20*3, 1, 0, 0, 0);
	//////

	////// DEBUG LINE FROM ORIGIN TO LIGHT DIRECTION
	struct
	{
		matrix mvp;
		vec4 start, end;
	} line_ubo;

	MatrixMult(ubo->modelview, ubo->projection, line_ubo.mvp);
	Vec4_Set(line_ubo.start, 0.0f, 0.0f, 0.0f, 1.0f);
	Vec3_Setv(line_ubo.end, Skybox_UBO->uSunPosition);
	Vec3_Muls(line_ubo.end, 10000.0f+(500.0f*2.0f));
	line_ubo.end[3]=1.0f;

	vkCmdBindPipeline(CommandBuffers[Index], VK_PIPELINE_BIND_POINT_GRAPHICS, LinePipeline.Pipeline);
	vkCmdPushConstants(CommandBuffers[Index], LinePipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(line_ubo), &line_ubo);
	vkCmdDraw(CommandBuffers[Index], 2, 1, 0, 0);
	//////

	// Should UI overlay stuff have it's own render pass?
	// Maybe even separate thread?
	Font_Print(CommandBuffers[Index], 0.0f, 16.0f, "FPS: %0.1f\t%d", fps, Camera.shift);

	vkCmdEndRenderPass(CommandBuffers[Index]);

	vkEndCommandBuffer(CommandBuffers[Index]);

	// Sumit command queue
	vkQueueSubmit(Context.Queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pWaitDstStageMask=&(VkPipelineStageFlags) { VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT },
		.waitSemaphoreCount=1,
		.pWaitSemaphores=&PresentCompleteSemaphores[Index],
		.signalSemaphoreCount=1,
		.pSignalSemaphores=&RenderCompleteSemaphores[Index],
		.commandBufferCount=1,
		.pCommandBuffers=&CommandBuffers[Index],
	}, FrameFences[Index]);

	// And present it to the screen
	vkQueuePresentKHR(Context.Queue, &(VkPresentInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount=1,
		.pWaitSemaphores=&RenderCompleteSemaphores[Index],
		.swapchainCount=1,
		.pSwapchains=&Swapchain,
		.pImageIndices=&OldIndex,
	});
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
	if(messageSeverity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		DBGPRINTF(DEBUG_ERROR, "\n%s\n", pCallbackData->pMessage);
	else if(messageSeverity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		DBGPRINTF(DEBUG_WARNING, "\n%s\n", pCallbackData->pMessage);
	else if(messageSeverity&VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
		DBGPRINTF(DEBUG_INFO, "\n%s\n", pCallbackData->pMessage);
	else
		DBGPRINTF(DEBUG_WARNING, "\n%s\n", pCallbackData->pMessage);

	return VK_FALSE;
}

bool Init(void)
{
#ifdef _DEBUG
	if(vkCreateDebugUtilsMessengerEXT(Instance, &(VkDebugUtilsMessengerCreateInfoEXT)
	{
		.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback=debugCallback
	}, VK_NULL_HANDLE, &debugMessenger)!=VK_SUCCESS)
		return false;
#endif

	VkZone=VkuMem_Init(&Context, Context.DeviceProperties2.maxMemoryAllocationSize);

	CameraInit(&Camera, (float[]) { 0.0f, 0.0f, 200.0f }, (float[]) { -1.0f, 0.0f, 0.0f }, (float[3]) { 0.0f, 1.0f, 0.0f });

	// Load models
	if(LoadBModel(&Model[MODEL_ASTEROID1], "./assets/asteroid1.bmodel"))
		BuildMemoryBuffersBModel(&Context, &Model[MODEL_ASTEROID1]);
	else
		return false;

	if(LoadBModel(&Model[MODEL_ASTEROID2], "./assets/asteroid2.bmodel"))
		BuildMemoryBuffersBModel(&Context, &Model[MODEL_ASTEROID2]);
	else
		return false;

	if(LoadBModel(&Model[MODEL_ASTEROID3], "./assets/asteroid3.bmodel"))
		BuildMemoryBuffersBModel(&Context, &Model[MODEL_ASTEROID3]);
	else
		return false;

	if(LoadBModel(&Model[MODEL_ASTEROID4], "./assets/asteroid4.bmodel"))
		BuildMemoryBuffersBModel(&Context, &Model[MODEL_ASTEROID4]);
	else
		return false;

	BuildSkybox(&Context, &SkyboxVertex, &SkyboxIndex);

	// Load textures
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID1], "./assets/asteroid1.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID1_NORMAL], "./assets/asteroid1_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID2], "./assets/asteroid2.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID2_NORMAL], "./assets/asteroid2_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID3], "./assets/asteroid3.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID3_NORMAL], "./assets/asteroid3_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID4], "./assets/asteroid4.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_ASTEROID4_NORMAL], "./assets/asteroid4_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);

	// Create a large descriptor pool, so I don't have to worry about readjusting for exactly what I have
	for(uint32_t i=0;i<MAX_FRAME_COUNT;i++)
	{
		vkCreateDescriptorPool(Context.Device, &(VkDescriptorPoolCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.maxSets=1024, // Max number of descriptor sets that can be allocated from this pool
			.poolSizeCount=4,
			.pPoolSizes=(VkDescriptorPoolSize[])
			{
				{
					.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
					.descriptorCount=1024, // Max number of this descriptor type that can be in each descriptor set?
				},
				{
					.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
					.descriptorCount=1024,
				},
				{
					.type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
					.descriptorCount=1024,
				},
				{
					.type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
					.descriptorCount=1024,
				},
			},
		}, VK_NULL_HANDLE, &DescriptorPool[i]);
	}

	// Create main render pipeline and renderpass
	CreatePipeline();

	// Create skybox pipeline (uses renderpass from main pipeline)
	CreateSkyboxPipeline();
	GenerateSkyParams();

	// Create debug line pipeline
	CreateLinePipeline();

	InitShadowPipeline();
	InitShadowMap();

	// Create primary frame buffers, depth image
	CreateFramebuffers();

	return true;
}

void vkuCreateSwapchain(VkuContext_t *Context, uint32_t Width, uint32_t Height, int VSync)
{
	uint32_t FormatCount, PresentModeCount;
	VkSurfaceCapabilitiesKHR SurfCaps;
	VkPresentModeKHR SwapchainPresentMode=VK_PRESENT_MODE_FIFO_KHR;
	VkSurfaceTransformFlagsKHR Pretransform;
	VkCompositeAlphaFlagBitsKHR CompositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	VkCompositeAlphaFlagBitsKHR CompositeAlphaFlags[]={ VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR };
	VkImageUsageFlags ImageUsage=0;

	vkGetPhysicalDeviceSurfaceFormatsKHR(Context->PhysicalDevice, Context->Surface, &FormatCount, VK_NULL_HANDLE);

	VkSurfaceFormatKHR *SurfaceFormats=(VkSurfaceFormatKHR *)Zone_Malloc(Zone, sizeof(VkSurfaceFormatKHR)*FormatCount);

	if(SurfaceFormats==NULL)
		return;

	vkGetPhysicalDeviceSurfaceFormatsKHR(Context->PhysicalDevice, Context->Surface, &FormatCount, SurfaceFormats);

	// If no format is specified, find a 32bit RGBA format
	if(SurfaceFormats[0].format==VK_FORMAT_UNDEFINED)
		SurfaceFormat.format=VK_FORMAT_R8G8B8A8_SNORM;
	// Otherwise the first format is the current surface format
	else
		SurfaceFormat=SurfaceFormats[0];

	Zone_Free(Zone, SurfaceFormats);

	// Get physical device surface properties and formats
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Context->PhysicalDevice, Context->Surface, &SurfCaps);

	// Get available present modes
	vkGetPhysicalDeviceSurfacePresentModesKHR(Context->PhysicalDevice, Context->Surface, &PresentModeCount, NULL);

	VkPresentModeKHR *PresentModes=(VkPresentModeKHR *)Zone_Malloc(Zone, sizeof(VkPresentModeKHR)*PresentModeCount);

	if(PresentModes==NULL)
		return;

	vkGetPhysicalDeviceSurfacePresentModesKHR(Context->PhysicalDevice, Context->Surface, &PresentModeCount, PresentModes);

	// Set swapchain extents to the surface width/height
	SwapchainExtent.width=Width;
	SwapchainExtent.height=Height;

	// Select a present mode for the swapchain

	// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
	// This mode waits for the vertical blank ("v-sync")

	// If v-sync is not requested, try to find a mailbox mode
	// It's the lowest latency non-tearing present mode available
	if(!VSync)
	{
		for(uint32_t i=0;i<PresentModeCount;i++)
		{
			if(PresentModes[i]==VK_PRESENT_MODE_MAILBOX_KHR)
			{
				SwapchainPresentMode=VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}

			if((SwapchainPresentMode!=VK_PRESENT_MODE_MAILBOX_KHR)&&(PresentModes[i]==VK_PRESENT_MODE_IMMEDIATE_KHR))
				SwapchainPresentMode=VK_PRESENT_MODE_IMMEDIATE_KHR;
		}
	}

	Zone_Free(Zone, PresentModes);

	// Find the transformation of the surface
	if(SurfCaps.supportedTransforms&VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
		// We prefer a non-rotated transform
		Pretransform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	else
		Pretransform=SurfCaps.currentTransform;

	// Find a supported composite alpha format (not all devices support alpha opaque)
	// Simply select the first composite alpha format available
	for(uint32_t i=0;i<4;i++)
	{
		if(SurfCaps.supportedCompositeAlpha&CompositeAlphaFlags[i])
		{
			CompositeAlpha=CompositeAlphaFlags[i];
			break;
		}
	}

	// Enable transfer source on swap chain images if supported
	if(SurfCaps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		ImageUsage|=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	// Enable transfer destination on swap chain images if supported
	if(SurfCaps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		ImageUsage|=VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	vkCreateSwapchainKHR(Context->Device, &(VkSwapchainCreateInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext=VK_NULL_HANDLE,
		.surface=Context->Surface,
		.minImageCount=MAX_FRAME_COUNT,
		.imageFormat=SurfaceFormat.format,
		.imageColorSpace=SurfaceFormat.colorSpace,
		.imageExtent={ SwapchainExtent.width, SwapchainExtent.height },
		.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|ImageUsage,
		.preTransform=Pretransform,
		.imageArrayLayers=1,
		.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount=0,
		.pQueueFamilyIndices=VK_NULL_HANDLE,
		.presentMode=SwapchainPresentMode,
		// Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
		.clipped=VK_TRUE,
		.compositeAlpha=CompositeAlpha,
	}, VK_NULL_HANDLE, &Swapchain);

	// Get swap chain image count
	vkGetSwapchainImagesKHR(Context->Device, Swapchain, &SwapchainImageCount, VK_NULL_HANDLE);

	// TODO: Allocate swapchain frame related stuff here (image, imageview, fences, semaphores, etc)

	// Get the swap chain images
	vkGetSwapchainImagesKHR(Context->Device, Swapchain, &SwapchainImageCount, SwapchainImage);

	// Get the swap chain buffers containing the image and imageview
	for(uint32_t i=0;i<SwapchainImageCount;i++)
	{
		vkCreateImageView(Context->Device, &(VkImageViewCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext=VK_NULL_HANDLE,
			.image=SwapchainImage[i],
			.format=SurfaceFormat.format,
			.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
			.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.subresourceRange.baseMipLevel=0,
			.subresourceRange.levelCount=1,
			.subresourceRange.baseArrayLayer=0,
			.subresourceRange.layerCount=1,
			.viewType=VK_IMAGE_VIEW_TYPE_2D,
			.flags=0,
		}, VK_NULL_HANDLE, &SwapchainImageView[i]);

		// Wait fence for command queue, to signal when we can submit commands again
		vkCreateFence(Context->Device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=VK_FENCE_CREATE_SIGNALED_BIT }, VK_NULL_HANDLE, &FrameFences[i]);

		// Semaphore for image presentation, to signal when we can present again
		vkCreateSemaphore(Context->Device, &(VkSemaphoreCreateInfo) {.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &PresentCompleteSemaphores[i]);

		// Semaphore for render complete, to signal when we can render again
		vkCreateSemaphore(Context->Device, &(VkSemaphoreCreateInfo) {.sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &RenderCompleteSemaphores[i]);
	}

	// Allocate the command buffers we will be rendering into
	vkAllocateCommandBuffers(Context->Device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=Context->CommandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=SwapchainImageCount,
	}, CommandBuffers);
}

void RecreateSwapchain(void)
{
	if(Context.Device!=VK_NULL_HANDLE) // Windows quirk, WM_SIZE is signaled on window creation, *before* Vulkan get initalized
	{
		// Wait for the device to complete any pending work
		vkDeviceWaitIdle(Context.Device);

		// To resize a surface, we need to destroy and recreate anything that's tied to the surface.
		// This is basically just the swapchain and frame buffers

		vkDestroyImageView(Context.Device, DepthImage.View, VK_NULL_HANDLE);
		VkuMem_Free(VkZone, DepthImage.DeviceMemory);
		vkDestroyImage(Context.Device, DepthImage.Image, VK_NULL_HANDLE);

		for(uint32_t i=0;i<SwapchainImageCount;i++)
		{
			vkDestroyFramebuffer(Context.Device, FrameBuffers[i], VK_NULL_HANDLE);

			vkDestroyImageView(Context.Device, SwapchainImageView[i], VK_NULL_HANDLE);

			vkDestroyFence(Context.Device, FrameFences[i], VK_NULL_HANDLE);

			vkDestroySemaphore(Context.Device, PresentCompleteSemaphores[i], VK_NULL_HANDLE);
			vkDestroySemaphore(Context.Device, RenderCompleteSemaphores[i], VK_NULL_HANDLE);
		}

		vkDestroySwapchainKHR(Context.Device, Swapchain, VK_NULL_HANDLE);

		// Recreate the swapchain and frame buffers
		vkuCreateSwapchain(&Context, Width, Height, VK_TRUE);
		CreateFramebuffers();

		// Does the render pass need to be recreated as well?
		// Validation doesn't complain about it...?
	}
}

void Destroy(void)
{
	vkDeviceWaitIdle(Context.Device);

	if(Context.PipelineCache)
	{
		DBGPRINTF(DEBUG_INFO, "\nWriting pipeline cache to disk...\n");

		size_t PipelineCacheSize=0;
		vkGetPipelineCacheData(Context.Device, Context.PipelineCache, &PipelineCacheSize, VK_NULL_HANDLE);

		uint8_t *PipelineCacheData=(uint8_t *)Zone_Malloc(Zone, PipelineCacheSize);

		if(PipelineCacheData)
		{
			FILE *Stream=fopen("pipelinecache.bin", "wb");

			if(Stream)
			{
				vkGetPipelineCacheData(Context.Device, Context.PipelineCache, &PipelineCacheSize, PipelineCacheData);
				fwrite(PipelineCacheData, 1, PipelineCacheSize, Stream);
				fclose(Stream);
				Zone_Free(Zone, PipelineCacheData);
			}
			else
				DBGPRINTF(DEBUG_ERROR, "Failed to open file handle to write pipeline cache data.\n");
		}
		else
			DBGPRINTF(DEBUG_ERROR, "Failed to allocate memory for pipeline cache data.\n");
	}

#ifdef _DEBUG
	vkDestroyDebugUtilsMessengerEXT(Instance, debugMessenger, VK_NULL_HANDLE);
#endif

	vkDestroyBuffer(Context.Device, Asteroid_Instance.Buffer, VK_NULL_HANDLE);
	vkFreeMemory(Context.Device, Asteroid_Instance.DeviceMemory, VK_NULL_HANDLE);

	vkDestroyPipeline(Context.Device, SkyboxPipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, SkyboxPipelineLayout, VK_NULL_HANDLE);

//	Font_Destroy();

	for(uint32_t i=0;i<NUM_TEXTURES;i++)
	{
		vkDestroySampler(Context.Device, Textures[i].Sampler, VK_NULL_HANDLE);
		vkDestroyImageView(Context.Device, Textures[i].View, VK_NULL_HANDLE);
		vkDestroyImage(Context.Device, Textures[i].Image, VK_NULL_HANDLE);
		VkuMem_Free(VkZone, Textures[i].DeviceMemory);
	}

	for(uint32_t i=0;i<NUM_MODELS;i++)
	{
		vkDestroyBuffer(Context.Device, Model[i].VertexBuffer.Buffer, VK_NULL_HANDLE);
		VkuMem_Free(VkZone, Model[i].VertexBuffer.Memory);

		for(uint32_t j=0;j<(uint32_t)Model[i].NumMesh;j++)
		{
			vkDestroyBuffer(Context.Device, Model[i].Mesh[j].IndexBuffer.Buffer, VK_NULL_HANDLE);
			VkuMem_Free(VkZone, Model[i].Mesh[j].IndexBuffer.Memory);
		}

		FreeBModel(&Model[i]);
	}

	vkDestroyBuffer(Context.Device, SkyboxVertex.Buffer, VK_NULL_HANDLE);
	VkuMem_Free(VkZone, SkyboxVertex.Memory);

	vkDestroyBuffer(Context.Device, SkyboxIndex.Buffer, VK_NULL_HANDLE);
	VkuMem_Free(VkZone, SkyboxIndex.Memory);

	vkDestroyPipeline(Context.Device, Pipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, PipelineLayout, VK_NULL_HANDLE);

	for(uint32_t i=0;i<MAX_FRAME_COUNT*NUM_MODELS;i++)
		vkDestroyDescriptorSetLayout(Context.Device, DescriptorSet[i].DescriptorSetLayout, VK_NULL_HANDLE);

	vkDestroyRenderPass(Context.Device, RenderPass, VK_NULL_HANDLE);

	for(uint32_t i=0;i<MAX_FRAME_COUNT;i++)
		vkDestroyDescriptorPool(Context.Device, DescriptorPool[i], VK_NULL_HANDLE);

	vkDestroyImageView(Context.Device, DepthImage.View, VK_NULL_HANDLE);
	vkDestroyImage(Context.Device, DepthImage.Image, VK_NULL_HANDLE);
	VkuMem_Free(VkZone, DepthImage.DeviceMemory);

	for(uint32_t i=0;i<SwapchainImageCount;i++)
	{
		vkDestroyFramebuffer(Context.Device, FrameBuffers[i], VK_NULL_HANDLE);

		vkDestroyImageView(Context.Device, SwapchainImageView[i], VK_NULL_HANDLE);

		vkDestroyFence(Context.Device, FrameFences[i], VK_NULL_HANDLE);

		vkDestroySemaphore(Context.Device, PresentCompleteSemaphores[i], VK_NULL_HANDLE);
		vkDestroySemaphore(Context.Device, RenderCompleteSemaphores[i], VK_NULL_HANDLE);
	}

	vkDestroySwapchainKHR(Context.Device, Swapchain, VK_NULL_HANDLE);

	DBGPRINTF(DEBUG_INFO"Remaining Vulkan memory blocks:\n");
	VkuMem_Print(VkZone);
	VkuMem_Destroy(&Context, VkZone);
	DBGPRINTF(DEBUG_NONE);
}
