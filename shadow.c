#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "system/system.h"
#include "vulkan/vulkan.h"
#include "math/math.h"
#include "camera/camera.h"
#include "model/bmodel.h"
#include "models.h"
#include "skybox.h"
#include "shadow.h"
#include "perframe.h"

#define NUM_ASTEROIDS 1000

extern VkuContext_t Context;
extern VkuBuffer_t Asteroid_Instance;
extern float fTime;
extern Camera_t Camera;

Shadow_UBO_t Shadow_UBO;

VkuPipeline_t ShadowPipeline;
VkPipelineLayout ShadowPipelineLayout;
VkRenderPass ShadowRenderPass;

const uint32_t ShadowSize=4096;

VkuImage_t ShadowDepth;
VkFramebuffer ShadowFrameBuffer;
VkFormat ShadowDepthFormat=VK_FORMAT_D32_SFLOAT;

void CreateShadowMap(void)
{
	// Depth render target
	vkuCreateTexture2D(&Context, &ShadowDepth, ShadowSize, ShadowSize, ShadowDepthFormat, VK_SAMPLE_COUNT_1_BIT);

	// Need compare ops, so recreate the sampler
	vkDestroySampler(Context.Device, ShadowDepth.Sampler, VK_NULL_HANDLE);
	vkCreateSampler(Context.Device, &(VkSamplerCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
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
		.maxAnisotropy=1.0f,
		.anisotropyEnable=VK_FALSE,
		.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	}, VK_NULL_HANDLE, &ShadowDepth.Sampler);

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

bool CreateShadowPipeline(void)
{
	vkCreateRenderPass(Context.Device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
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
			.size=sizeof(Shadow_UBO_t),
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},
	}, 0, &ShadowPipelineLayout);

	vkuInitPipeline(&ShadowPipeline, &Context);

	vkuPipeline_SetPipelineLayout(&ShadowPipeline, ShadowPipelineLayout);
	vkuPipeline_SetRenderPass(&ShadowPipeline, ShadowRenderPass);

	// Add in vertex shader
	if(!vkuPipeline_AddStage(&ShadowPipeline, "shaders/shadow.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
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

	VkPipelineRenderingCreateInfo PipelineRenderingCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.depthAttachmentFormat=ShadowDepthFormat,
	};

	// Assemble the pipeline
	if(!vkuAssemblePipeline(&ShadowPipeline, VK_NULL_HANDLE/*&PipelineRenderingCreateInfo*/))
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
		.pClearValues=(VkClearValue[]){ {{{ 1.0f, 0 }}} },
		.renderArea.offset=(VkOffset2D){ 0, 0 },
		.renderArea.extent=(VkExtent2D){ ShadowSize, ShadowSize },
	}, VK_SUBPASS_CONTENTS_INLINE);

	// Bind the pipeline descriptor, this sets the pipeline states (blend, depth/stencil tests, etc)
	vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ShadowPipeline.Pipeline);

	vkCmdSetViewport(CommandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)ShadowSize, (float)ShadowSize, 0.0f, 1.0f });
	vkCmdSetScissor(CommandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { ShadowSize, ShadowSize } });

	matrix Projection=MatrixOrtho(-1200.0f, 1200.0f, -1200.0f, 1200.0f, 0.1f, 4000.0f);

	// Looking at the asteroid field from the sun's position "a number" away
	// This should technically be an infinite distance away, but that's not possible, so "a number" is whatever best compromise.
	// (uSubPosition is a vec4, so need to recreate that in a vec3)
	vec3 Position=Vec3_Muls(Vec3(
		PerFrame[FrameIndex].Skybox_UBO[0]->uSunPosition.x,
		PerFrame[FrameIndex].Skybox_UBO[0]->uSunPosition.y,
		PerFrame[FrameIndex].Skybox_UBO[0]->uSunPosition.z
	), 3000.0f);

	// Following the camera's position, so we don't have to composite multiple shadow maps or have super large maps.
	matrix ModelView=MatrixLookAt(Vec3_Addv(Position, Camera.Position), Camera.Position, Vec3(0.0f, 1.0f, 0.0f));

	// Multiply matrices together, so we can just send one matrix as a push constant.
	Shadow_UBO.mvp=MatrixMult(ModelView, Projection);

	vkCmdBindVertexBuffers(CommandBuffer, 1, 1, &Asteroid_Instance.Buffer, &(VkDeviceSize) { 0 });

	// Draw the models
	for(uint32_t j=0;j<NUM_MODELS;j++)
	{
		Shadow_UBO.local=MatrixIdentity();
		Shadow_UBO.local=MatrixMult(Shadow_UBO.local, MatrixRotate(fTime, 1.0f, 0.0f, 0.0f));
		Shadow_UBO.local=MatrixMult(Shadow_UBO.local, MatrixRotate(fTime, 0.0f, 1.0f, 0.0f));

		vkCmdPushConstants(CommandBuffer, ShadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(Shadow_UBO_t), &Shadow_UBO);

		// Bind model data buffers and draw the triangles
		vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Models[j].VertexBuffer.Buffer, &(VkDeviceSize) { 0 });

		for(uint32_t k=0;k<Models[j].NumMesh;k++)
		{
			vkCmdBindIndexBuffer(CommandBuffer, Models[j].Mesh[k].IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(CommandBuffer, Models[j].Mesh[k].NumFace*3, NUM_ASTEROIDS/NUM_MODELS, 0, 0, (NUM_ASTEROIDS/NUM_MODELS)*j);
		}
	}

	vkCmdEndRenderPass(CommandBuffer);
}

void DestroyShadow(void)
{
	vkuDestroyImageBuffer(&Context, &ShadowDepth);

	vkDestroyPipeline(Context.Device, ShadowPipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, ShadowPipelineLayout, VK_NULL_HANDLE);
	vkDestroyRenderPass(Context.Device, ShadowRenderPass, VK_NULL_HANDLE);

	vkDestroyFramebuffer(Context.Device, ShadowFrameBuffer, VK_NULL_HANDLE);
}