#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "system/system.h"
#include "vulkan/vulkan.h"
#include "math/math.h"
#include "camera/camera.h"
#include "model/3ds.h"
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
	MODEL_HELLKNIGHT,
	MODEL_PINKY,
	MODEL_FATTY,
	MODEL_LEVEL,
	NUM_MODELS
};

Model3DS_t Model[NUM_MODELS];
Model3DS_t Skybox;

enum
{
	TEXTURE_HELLKNIGHT,
	TEXTURE_HELLKNIGHT_NORMAL,
	TEXTURE_PINKY,
	TEXTURE_PINKY_NORMAL,
	TEXTURE_FATTY,
	TEXTURE_FATTY_NORMAL,
	TEXTURE_LEVEL,
	TEXTURE_LEVEL_NORMAL,
	NUM_TEXTURES
};

VkuImage_t Textures[NUM_TEXTURES];

matrix ModelView, Projection;

Lights_t Lights;

struct
{
	matrix mvp;
	vec4 eye;

	uint32_t NumLights;
} ubo;

VkDebugUtilsMessengerEXT debugMessenger;

// Swapchain
VkSwapchainKHR Swapchain;

VkExtent2D SwapchainExtent;
VkSurfaceFormatKHR SurfaceFormat;
VkFormat DepthFormat=VK_FORMAT_D32_SFLOAT_S8_UINT;

#define MAX_FRAME_COUNT 3

uint32_t SwapchainImageCount=0;

VkImage SwapchainImage[MAX_FRAME_COUNT];
VkImageView SwapchainImageView[MAX_FRAME_COUNT];
VkFramebuffer FrameBuffers[MAX_FRAME_COUNT];

// Depth buffer handles
VkuImage_t DepthImage;

VkRenderPass RenderPass;
VkPipelineLayout PipelineLayout;
VkuPipeline_t Pipeline;

struct
{
	matrix mvp;
	vec4 uOffset;

	vec4 uNebulaAColor;
	vec4 uNebulaBColor;

	float uStarsScale;
	float uStarDensity;
	float pad0[2];

	vec4 uSunPosition;
	float uSunSize;
	float uSunFalloff;
	float pad1[2];
	vec4 uSunColor;
} skybox_ubo;

VkPipelineLayout SkyboxPipelineLayout;
VkuPipeline_t SkyboxPipeline;

VkDescriptorPool DescriptorPool[MAX_FRAME_COUNT];
VkuDescriptorSet_t DescriptorSet[MAX_FRAME_COUNT*NUM_MODELS];

VkCommandBuffer CommandBuffers[MAX_FRAME_COUNT];

VkFence FrameFences[MAX_FRAME_COUNT];
VkSemaphore PresentCompleteSemaphores[MAX_FRAME_COUNT];
VkSemaphore RenderCompleteSemaphores[MAX_FRAME_COUNT];

// Shadow cubemap stuff
int32_t ShadowCubeSize=1024;

VkFramebuffer ShadowFrameBuffer;
VkuImage_t ShadowDepth;
VkFormat ShadowDepthFormat=VK_FORMAT_D32_SFLOAT;

VkuPipeline_t ShadowPipeline;
VkuDescriptorSet_t ShadowDescriptorSet[MAX_FRAME_COUNT];
VkPipelineLayout ShadowPipelineLayout;
VkRenderPass ShadowRenderPass;

struct
{
	matrix mv[6];
	matrix proj;
	vec4 Light_Pos;
	int32_t index;
	int32_t pad[11]; // Need to pad out to the nearest 256 byte
} shadow_ubo;

VkuBuffer_t shadow_ubo_buffer;
void *shadow_ubo_ptr;

void RecreateSwapchain(void);

void InitShadowCubeMap(uint32_t NumMaps)
{
	// Cubemap depth render target
	vkuCreateImageBuffer(&Context, &ShadowDepth,
		VK_IMAGE_TYPE_2D, ShadowDepthFormat, 1, 6*NumMaps, ShadowCubeSize, ShadowCubeSize, 1,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

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
		.viewType=VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,
		.format=ShadowDepthFormat,
		.components.r={ VK_COMPONENT_SWIZZLE_R },
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=6*NumMaps,
		.subresourceRange.levelCount=1,
		.image=ShadowDepth.Image,
	}, VK_NULL_HANDLE, &ShadowDepth.View);

	vkCreateFramebuffer(Context.Device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=ShadowRenderPass,
		.attachmentCount=1,
		.pAttachments=(VkImageView[]){ ShadowDepth.View },
		.width=ShadowCubeSize,
		.height=ShadowCubeSize,
		.layers=6*NumMaps,
	}, 0, &ShadowFrameBuffer);

	vkuCreateHostBuffer(&Context, &shadow_ubo_buffer, sizeof(shadow_ubo)*NumMaps, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
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

	for(uint32_t i=0;i<MAX_FRAME_COUNT;i++)
	{
		vkuInitDescriptorSet(&ShadowDescriptorSet[i], &Context);
		vkuDescriptorSet_AddBinding(&ShadowDescriptorSet[i], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_GEOMETRY_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);
		vkuAssembleDescriptorSetLayout(&ShadowDescriptorSet[i]);
	}

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&ShadowDescriptorSet[0].DescriptorSetLayout,	// Just use the first one, they're all the same
	}, 0, &ShadowPipelineLayout);

	vkuInitPipeline(&ShadowPipeline, &Context);

	vkuPipeline_SetPipelineLayout(&ShadowPipeline, ShadowPipelineLayout);
	vkuPipeline_SetRenderPass(&ShadowPipeline, ShadowRenderPass);

	// Add in vertex shader
	if(!vkuPipeline_AddStage(&ShadowPipeline, "./shaders/distance.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	// Add in geometry shader
	if(!vkuPipeline_AddStage(&ShadowPipeline, "./shaders/distance.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT))
		return false;

	// Add in fragment shader
	if(!vkuPipeline_AddStage(&ShadowPipeline, "./shaders/distance.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	// Set states that are different than defaults
	ShadowPipeline.CullMode=VK_CULL_MODE_FRONT_BIT;
	ShadowPipeline.DepthTest=VK_TRUE;

	// Add vertex binding and attrib parameters
	vkuPipeline_AddVertexBinding(&ShadowPipeline, 0, sizeof(float)*20, VK_VERTEX_INPUT_RATE_VERTEX);
	vkuPipeline_AddVertexAttribute(&ShadowPipeline, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);

	// Assemble the pipeline
	if(!vkuAssemblePipeline(&ShadowPipeline))
		return false;

	return true;
}

void ShadowUpdateCubemap(VkCommandBuffer CommandBuffer, uint32_t FrameIndex)
{
	vkCmdBeginRenderPass(CommandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=ShadowRenderPass,
		.framebuffer=ShadowFrameBuffer,
		.clearValueCount=1,
		.pClearValues=(VkClearValue[]) { { 1.0f, 0 } },
		.renderArea.offset=(VkOffset2D) { 0, 0 },
		.renderArea.extent=(VkExtent2D)	{ ShadowCubeSize, ShadowCubeSize },
	}, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(CommandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)ShadowCubeSize, (float)ShadowCubeSize, 0.0f, 1.0f });
	vkCmdSetScissor(CommandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { ShadowCubeSize, ShadowCubeSize } });

	// Bind the pipeline descriptor, this sets the pipeline states (blend, depth/stencil tests, etc)
	vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ShadowPipeline.Pipeline);

	for(uint32_t i=0;i<List_GetCount(&Lights.Lights);i++)
	{
		Light_t *Light=List_GetPointer(&Lights.Lights, i);

		vec4 LightPositionRadius;
		Vec3_Setv(LightPositionRadius, Light->Position);
		LightPositionRadius[3]=Light->Radius;

		MatrixIdentity(shadow_ubo.proj);
		MatrixInfPerspective(90.0f, 1.0f, 0.01f, false, shadow_ubo.proj);

		MatrixIdentity(shadow_ubo.mv[0]);
		MatrixLookAt(LightPositionRadius, (vec3) { LightPositionRadius[0]+1.0f, LightPositionRadius[1]+0.0f, LightPositionRadius[2]+0.0f }, (vec3) { 0.0f, -1.0f, 0.0f }, shadow_ubo.mv[0]);
		MatrixIdentity(shadow_ubo.mv[1]);
		MatrixLookAt(LightPositionRadius, (vec3) { LightPositionRadius[0]-1.0f, LightPositionRadius[1]+0.0f, LightPositionRadius[2]+0.0f }, (vec3) { 0.0f, -1.0f, 0.0f }, shadow_ubo.mv[1]);
		MatrixIdentity(shadow_ubo.mv[2]);
		MatrixLookAt(LightPositionRadius, (vec3) { LightPositionRadius[0]+0.0f, LightPositionRadius[1]+1.0f, LightPositionRadius[2]+0.0f }, (vec3) { 0.0f, 0.0f, 1.0f }, shadow_ubo.mv[2]);
		MatrixIdentity(shadow_ubo.mv[3]);
		MatrixLookAt(LightPositionRadius, (vec3) { LightPositionRadius[0]+0.0f, LightPositionRadius[1]-1.0f, LightPositionRadius[2]+0.0f }, (vec3) { 0.0f, 0.0f, -1.0f }, shadow_ubo.mv[3]);
		MatrixIdentity(shadow_ubo.mv[4]);
		MatrixLookAt(LightPositionRadius, (vec3) { LightPositionRadius[0]+0.0f, LightPositionRadius[1]+0.0f, LightPositionRadius[2]+1.0f }, (vec3) { 0.0f, -1.0f, 0.0f }, shadow_ubo.mv[4]);
		MatrixIdentity(shadow_ubo.mv[5]);
		MatrixLookAt(LightPositionRadius, (vec3) { LightPositionRadius[0]+0.0f, LightPositionRadius[1]+0.0f, LightPositionRadius[2]-1.0f }, (vec3) { 0.0f, -1.0f, 0.0f }, shadow_ubo.mv[5]);

		Vec4_Setv(shadow_ubo.Light_Pos, LightPositionRadius);

		shadow_ubo.index=i;

		vkMapMemory(Context.Device, shadow_ubo_buffer.DeviceMemory, sizeof(shadow_ubo)*i, sizeof(shadow_ubo), 0, &shadow_ubo_ptr);
		memcpy(shadow_ubo_ptr, &shadow_ubo, sizeof(shadow_ubo));
		vkUnmapMemory(Context.Device, shadow_ubo_buffer.DeviceMemory);

		vkuDescriptorSet_UpdateBindingBufferInfo(&ShadowDescriptorSet[FrameIndex], 0, shadow_ubo_buffer.Buffer, 0, sizeof(shadow_ubo));
		vkuAllocateUpdateDescriptorSet(&ShadowDescriptorSet[FrameIndex], DescriptorPool[FrameIndex]);

		uint32_t DynamicOffset=sizeof(shadow_ubo)*i;
		vkCmdBindDescriptorSets(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ShadowPipelineLayout, 0, 1, &ShadowDescriptorSet[FrameIndex].DescriptorSet, 1, &DynamicOffset);

		// Draw the models
		for(uint32_t j=0;j<NUM_MODELS;j++)
		{
			// Bind model data buffers and draw the triangles
			for(int32_t k=0;k<Model[j].NumMesh;k++)
			{
				vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Model[j].Mesh[k].VertexBuffer.Buffer, &(VkDeviceSize) { 0 });
				vkCmdBindIndexBuffer(CommandBuffer, Model[j].Mesh[k].IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT16);
				vkCmdDrawIndexed(CommandBuffer, Model[j].Mesh[k].NumFace*3, 1, 0, 0, 0);
			}
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

		vkuDescriptorSet_AddBinding(&DescriptorSet[i], 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT);
		vkuDescriptorSet_AddBinding(&DescriptorSet[i], 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		vkuDescriptorSet_AddBinding(&DescriptorSet[i], 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		vkuDescriptorSet_AddBinding(&DescriptorSet[i], 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

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
			.size=sizeof(ubo),
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

	vkuPipeline_AddVertexBinding(&Pipeline, 0, sizeof(float)*20, VK_VERTEX_INPUT_RATE_VERTEX);

	vkuPipeline_AddVertexAttribute(&Pipeline, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0);
	vkuPipeline_AddVertexAttribute(&Pipeline, 1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(float)*4);
	vkuPipeline_AddVertexAttribute(&Pipeline, 2, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(float)*8);
	vkuPipeline_AddVertexAttribute(&Pipeline, 3, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(float)*12);
	vkuPipeline_AddVertexAttribute(&Pipeline, 4, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(float)*16);

	if(!vkuAssemblePipeline(&Pipeline))
		return false;

	return true;
}

bool CreateSkyboxPipeline(void)
{
	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=sizeof(skybox_ubo),
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},
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

	return true;
}

void BuildMemoryBuffers(Model3DS_t *Model)
{
	VkuBuffer_t stagingBuffer;
	void *Data=NULL;

	for(int32_t i=0;i<Model->NumMesh;i++)
	{
		// Vertex data on device memory
		vkuCreateGPUBuffer(&Context, &Model->Mesh[i].VertexBuffer, sizeof(float)*20*Model->Mesh[i].NumVertex, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT);

		// Create staging buffer to transfer from host memory to device memory
		vkuCreateHostBuffer(&Context, &stagingBuffer, sizeof(float)*20*Model->Mesh[i].NumVertex, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

		vkMapMemory(Context.Device, stagingBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, &Data);

		if(!Data)
			return;

		float *fPtr=Data;

		for(int32_t j=0;j<Model->Mesh[i].NumVertex;j++)
		{
			*fPtr++=Model->Mesh[i].Vertex[3*j+0];
			*fPtr++=Model->Mesh[i].Vertex[3*j+1];
			*fPtr++=Model->Mesh[i].Vertex[3*j+2];
			*fPtr++=1.0f;

			*fPtr++=Model->Mesh[i].UV[2*j+0];
			*fPtr++=1.0f-Model->Mesh[i].UV[2*j+1];
			*fPtr++=0.0f;
			*fPtr++=0.0f;

			*fPtr++=Model->Mesh[i].Tangent[3*j+0];
			*fPtr++=Model->Mesh[i].Tangent[3*j+1];
			*fPtr++=Model->Mesh[i].Tangent[3*j+2];
			*fPtr++=0.0f;

			*fPtr++=Model->Mesh[i].Binormal[3*j+0];
			*fPtr++=Model->Mesh[i].Binormal[3*j+1];
			*fPtr++=Model->Mesh[i].Binormal[3*j+2];
			*fPtr++=0.0f;

			*fPtr++=Model->Mesh[i].Normal[3*j+0];
			*fPtr++=Model->Mesh[i].Normal[3*j+1];
			*fPtr++=Model->Mesh[i].Normal[3*j+2];
			*fPtr++=0.0f;
		}

		vkUnmapMemory(Context.Device, stagingBuffer.DeviceMemory);

		// Copy to device memory
		vkuCopyBuffer(&Context, stagingBuffer.Buffer, Model->Mesh[i].VertexBuffer.Buffer, sizeof(float)*20*Model->Mesh[i].NumVertex);

		// Delete staging data
		vkFreeMemory(Context.Device, stagingBuffer.DeviceMemory, VK_NULL_HANDLE);
		vkDestroyBuffer(Context.Device, stagingBuffer.Buffer, VK_NULL_HANDLE);

		// Index data
		vkuCreateGPUBuffer(&Context, &Model->Mesh[i].IndexBuffer, sizeof(uint16_t)*Model->Mesh[i].NumFace*3, VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT);

		// Staging buffer
		vkuCreateHostBuffer(&Context, &stagingBuffer, sizeof(uint16_t)*Model->Mesh[i].NumFace*3, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);

		vkMapMemory(Context.Device, stagingBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, &Data);

		if(!Data)
			return;

		uint16_t *sPtr=Data;

		for(int32_t j=0;j<Model->Mesh[i].NumFace;j++)
		{
			*sPtr++=Model->Mesh[i].Face[3*j+0];
			*sPtr++=Model->Mesh[i].Face[3*j+1];
			*sPtr++=Model->Mesh[i].Face[3*j+2];
		}

		vkUnmapMemory(Context.Device, stagingBuffer.DeviceMemory);

		vkuCopyBuffer(&Context, stagingBuffer.Buffer, Model->Mesh[i].IndexBuffer.Buffer, sizeof(uint16_t)*Model->Mesh[i].NumFace*3);

		// Delete staging data
		vkFreeMemory(Context.Device, stagingBuffer.DeviceMemory, VK_NULL_HANDLE);
		vkDestroyBuffer(Context.Device, stagingBuffer.Buffer, VK_NULL_HANDLE);
	}
}

float RandFloat(void)
{
	return (float)rand()/RAND_MAX;
}

void GenerateSkyParams(void)
{
	Vec4_Set(skybox_ubo.uOffset, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, 0.0f);
	Vec3_Normalize(skybox_ubo.uOffset);

	Vec4_Set(skybox_ubo.uNebulaAColor, RandFloat(), RandFloat(), RandFloat(), 0.0f);
	Vec4_Set(skybox_ubo.uNebulaBColor, RandFloat(), RandFloat(), RandFloat(), 0.0f);

	Vec4_Set(skybox_ubo.uSunPosition, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, RandFloat()*2.0f-1.0f, 0.0f);
	Vec3_Normalize(skybox_ubo.uSunPosition);

	Vec4_Set(skybox_ubo.uSunColor, RandFloat(), RandFloat(), RandFloat(), 0.0f);
	skybox_ubo.uSunSize=1.0f/(RandFloat()*1000.0f);
	skybox_ubo.uSunFalloff=RandFloat()*16.0f+8.0f;

	skybox_ubo.uStarsScale=200.0f;
	skybox_ubo.uStarDensity=8.0f;
}

void Render(void)
{
	static uint32_t OldIndex=0;
	uint32_t Index=OldIndex;

	//Lights_UpdatePosition(&Lights, 0, (vec3) { sinf(fTime)*150.0f, -25.0f, cosf(fTime)*150.0f });
	//Lights_UpdatePosition(&Lights, 1, (vec3) { cosf(fTime)*100.0f, 50.0f, sinf(fTime)*100.0f });
	//Lights_UpdatePosition(&Lights, 2, (vec3) { cosf(fTime)*100.0f, -80.0f, -15.0f });
	//Lights_UpdatePosition(&Lights, 10, (vec3) { cosf(fTime)*300.0f, 100.0f, sinf(fTime)*300.0f });
	//Lights_UpdateRadius(&Lights, 10, 300.0f);

	// Generate the projection matrix
	MatrixIdentity(Projection);
	MatrixInfPerspective(90.0f, (float)Width/Height, 0.01f, true, Projection);

	// Set up the modelview matrix
	MatrixIdentity(ModelView);
	//MatrixTranslate(PanX, PanY, Zoom, ModelView);

	//QuatAngle(RotateX, 0.0f, 1.0f, 0.0f, QuatX);
	//QuatAngle(RotateY, 1.0f, 0.0f, 0.0f, QuatY);
	//QuatMultiply(QuatY, QuatX, Quat);
	//QuatMatrix(Quat, ModelView);
	CameraUpdate(&Camera, fTimeStep, ModelView);

	// Generate an inverse modelview matrix (only really need the last 3 from the calculation)
	ubo.eye[0]=-(ModelView[12]*ModelView[ 0])-(ModelView[13]*ModelView[ 1])-(ModelView[14]*ModelView[ 2]);
	ubo.eye[1]=-(ModelView[12]*ModelView[ 4])-(ModelView[13]*ModelView[ 5])-(ModelView[14]*ModelView[ 6]);
	ubo.eye[2]=-(ModelView[12]*ModelView[ 8])-(ModelView[13]*ModelView[ 9])-(ModelView[14]*ModelView[10]);

	// Generate a modelview+projection matrix
	MatrixMult(ModelView, Projection, ubo.mvp);

	// Set number of lights to the shader
	ubo.NumLights=(uint32_t)List_GetCount(&Lights.Lights);

	//Lights_UpdateSSBO(&Lights);

	memcpy(skybox_ubo.mvp, ubo.mvp, sizeof(matrix));

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

	//ShadowUpdateCubemap(CommandBuffers[Index], Index);

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
	//vkCmdBindPipeline(CommandBuffers[Index], VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline.Pipeline);

	//vkCmdPushConstants(CommandBuffers[Index], PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ubo), &ubo);

	//// Draw the models
	//for(uint32_t i=0;i<NUM_MODELS;i++)
	//{

	//	vkuDescriptorSet_UpdateBindingBufferInfo(&DescriptorSet[MAX_FRAME_COUNT*i+Index], 0, Lights.StorageBuffer.Buffer, 0, VK_WHOLE_SIZE);
	//	vkuDescriptorSet_UpdateBindingImageInfo(&DescriptorSet[MAX_FRAME_COUNT*i+Index], 1, &Textures[2*i+0]);
	//	vkuDescriptorSet_UpdateBindingImageInfo(&DescriptorSet[MAX_FRAME_COUNT*i+Index], 2, &Textures[2*i+1]);
	//	vkuDescriptorSet_UpdateBindingImageInfo(&DescriptorSet[MAX_FRAME_COUNT*i+Index], 3, &ShadowDepth);
	//	vkuAllocateUpdateDescriptorSet(&DescriptorSet[MAX_FRAME_COUNT*i+Index], DescriptorPool[Index]);

	//	vkCmdBindDescriptorSets(CommandBuffers[Index], VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout, 0, 1, &DescriptorSet[MAX_FRAME_COUNT*i+Index].DescriptorSet, 0, VK_NULL_HANDLE);

	//	// Bind model data buffers and draw the triangles
	//	for(int32_t j=0;j<Model[i].NumMesh;j++)
	//	{
	//		vkCmdBindVertexBuffers(CommandBuffers[Index], 0, 1, &Model[i].Mesh[j].VertexBuffer.Buffer, &(VkDeviceSize) { 0 });
	//		vkCmdBindIndexBuffer(CommandBuffers[Index], Model[i].Mesh[j].IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT16);
	//		vkCmdDrawIndexed(CommandBuffers[Index], Model[i].Mesh[j].NumFace*3, 1, 0, 0, 0);
	//	}
	//}
	// ---

	vkCmdBindPipeline(CommandBuffers[Index], VK_PIPELINE_BIND_POINT_GRAPHICS, SkyboxPipeline.Pipeline);

	vkCmdPushConstants(CommandBuffers[Index], SkyboxPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(skybox_ubo), &skybox_ubo);

	vkCmdBindVertexBuffers(CommandBuffers[Index], 0, 1, &Skybox.Mesh->VertexBuffer.Buffer, &(VkDeviceSize) { 0 });
	vkCmdBindIndexBuffer(CommandBuffers[Index], Skybox.Mesh->IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT16);
	vkCmdDrawIndexed(CommandBuffers[Index], Skybox.Mesh->NumFace*3, 1, 0, 0, 0);

	// Should UI overlay stuff have it's own render pass?
	// Maybe even separate thread?
//	Font_Print(CommandBuffers[Index], 0.0f, 16.0f, "FPS: %0.1f\n\n\n\nNumber of lights: %d", fps, ubo.NumLights);

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

	CameraInit(&Camera, (float[]) { 0.0f, 0.0f, 100.0f }, (float[]) { -1.0f, 0.0f, 0.0f }, (float[3]) { 0.0f, 1.0f, 0.0f });

	Lights_Init(&Lights);
	Lights_Add(&Lights, (vec3) { 0.0f, 0.0f, 0.0f }, 256.0f, (vec4) { 1.0f, 0.0f, 0.0f, 1.0f });
	Lights_Add(&Lights, (vec3) { -100.0f, 0.0f, 0.0f }, 256.0f, (vec4) { 0.0f, 1.0f, 0.0f, 1.0f });
	Lights_Add(&Lights, (vec3) { 100.0f, 0.0f, 0.0f }, 256.0f, (vec4) { 0.0f, 0.0f, 1.0f, 1.0f });

	// Load models
	if(Load3DS(&Model[MODEL_HELLKNIGHT], "./assets/hellknight.3ds"))
		BuildMemoryBuffers(&Model[MODEL_HELLKNIGHT]);

	if(Load3DS(&Model[MODEL_PINKY], "./assets/pinky.3ds"))
		BuildMemoryBuffers(&Model[MODEL_PINKY]);

	if(Load3DS(&Model[MODEL_FATTY], "./assets/fatty.3ds"))
		BuildMemoryBuffers(&Model[MODEL_FATTY]);

	if(Load3DS(&Model[MODEL_LEVEL], "./assets/level.3ds"))
		BuildMemoryBuffers(&Model[MODEL_LEVEL]);

	BuildSkybox(&Skybox);

	// Load textures
	Image_Upload(&Context, &Textures[TEXTURE_HELLKNIGHT], "./assets/hellknight.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_HELLKNIGHT_NORMAL], "./assets/hellknight_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_PINKY], "./assets/pinky.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_PINKY_NORMAL], "./assets/pinky_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_FATTY], "./assets/fatty.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_FATTY_NORMAL], "./assets/fatty_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_LEVEL], "./assets/tile.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_LEVEL_NORMAL], "./assets/tile_b.tga", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALMAP);

	GenerateSkyParams();

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

	CreateSkyboxPipeline();

//	InitShadowPipeline();
//	InitShadowCubeMap(13);

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

	Lights_Destroy(&Lights);

	vkDestroyPipeline(Context.Device, SkyboxPipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, SkyboxPipelineLayout, VK_NULL_HANDLE);

	// Shadow stuff
	vkDestroyPipeline(Context.Device, ShadowPipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, ShadowPipelineLayout, VK_NULL_HANDLE);

	for(uint32_t i=0;i<MAX_FRAME_COUNT;i++)
		vkDestroyDescriptorSetLayout(Context.Device, ShadowDescriptorSet[i].DescriptorSetLayout, VK_NULL_HANDLE);

	vkDestroyRenderPass(Context.Device, ShadowRenderPass, VK_NULL_HANDLE);

	vkDestroyFramebuffer(Context.Device, ShadowFrameBuffer, VK_NULL_HANDLE);
	vkDestroySampler(Context.Device, ShadowDepth.Sampler, VK_NULL_HANDLE);
	vkDestroyImageView(Context.Device, ShadowDepth.View, VK_NULL_HANDLE);
	vkDestroyImage(Context.Device, ShadowDepth.Image, VK_NULL_HANDLE);
	VkuMem_Free(VkZone, ShadowDepth.DeviceMemory);

	vkDestroyBuffer(Context.Device, shadow_ubo_buffer.Buffer, VK_NULL_HANDLE);
	vkFreeMemory(Context.Device, shadow_ubo_buffer.DeviceMemory, VK_NULL_HANDLE);
	// ---

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
		for(uint32_t j=0;j<(uint32_t)Model[i].NumMesh;j++)
		{
			vkDestroyBuffer(Context.Device, Model[i].Mesh[j].VertexBuffer.Buffer, VK_NULL_HANDLE);
			VkuMem_Free(VkZone, Model[i].Mesh[j].VertexBuffer.Memory);

			vkDestroyBuffer(Context.Device, Model[i].Mesh[j].IndexBuffer.Buffer, VK_NULL_HANDLE);
			VkuMem_Free(VkZone, Model[i].Mesh[j].IndexBuffer.Memory);
		}

		Free3DS(&Model[i]);
	}

	vkDestroyBuffer(Context.Device, Skybox.Mesh->VertexBuffer.Buffer, VK_NULL_HANDLE);
	VkuMem_Free(VkZone, Skybox.Mesh->VertexBuffer.Memory);

	vkDestroyBuffer(Context.Device, Skybox.Mesh->IndexBuffer.Buffer, VK_NULL_HANDLE);
	VkuMem_Free(VkZone, Skybox.Mesh->IndexBuffer.Memory);

	Zone_Free(Zone, Skybox.Mesh);

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
