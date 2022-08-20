#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "system/system.h"
#include "vulkan/vulkan.h"
#include "math/math.h"
#include "model/3ds.h"
#include "image/image.h"
#include "font/font.h"
#include "utils/list.h"
#include "lights/lights.h"

uint32_t Width=1280, Height=720;

VkInstance Instance;
VkuContext_t Context;
 
float RotateX=0.0f, RotateY=0.0f, PanX=0.0f, PanY=0.0f, Zoom=-100.0f;

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

Image_t Textures[NUM_TEXTURES];

matrix ModelView, Projection;
vec4 QuatX, QuatY, Quat;

Lights_t Lights;

struct
{
	matrix mvp;
	vec4 eye;

	uint32_t NumLights;
} ubo;

VkBuffer uniformBuffer;
VkDeviceMemory uniformBufferMemory;
void *uniformBufferPtr;

VkDebugUtilsMessengerEXT debugMessenger;

// Swapchain
VkSwapchainKHR Swapchain;

VkExtent2D SwapchainExtent;
VkSurfaceFormatKHR SurfaceFormat;
VkFormat DepthFormat=VK_FORMAT_D24_UNORM_S8_UINT;

#define MAX_FRAME_COUNT 2

uint32_t SwapchainImageCount=0;

VkImage SwapchainImage[MAX_FRAME_COUNT];
VkImageView SwapchainImageView[MAX_FRAME_COUNT];
VkFramebuffer FrameBuffers[MAX_FRAME_COUNT];

// Depth buffer handles
Image_t DepthImage;

VkRenderPass RenderPass;
VkPipelineLayout PipelineLayout;
VkuPipeline_t Pipeline;

VkDescriptorPool DescriptorPool;
VkDescriptorSet DescriptorSet;
VkuDescriptorSetLayout_t DescriptorSetLayout;


VkCommandBuffer CommandBuffers[MAX_FRAME_COUNT];

VkFence FrameFences[MAX_FRAME_COUNT];
VkSemaphore PresentCompleteSemaphores[MAX_FRAME_COUNT];
VkSemaphore RenderCompleteSemaphores[MAX_FRAME_COUNT];

// Shadow cubemap stuff
//
int32_t ShadowCubeSize=1024;

VkFramebuffer ShadowFrameBuffer;

// Shadow Frame buffer
Image_t ShadowColor;
Image_t ShadowDepth;
VkFormat ShadowColorFormat=VK_FORMAT_R32_SFLOAT;
VkFormat ShadowDepthFormat=VK_FORMAT_D32_SFLOAT;

VkuPipeline_t ShadowPipeline;
VkPipelineLayout ShadowPipelineLayout;
VkRenderPass ShadowRenderPass;

// Shadow depth cubemap texture
Image_t ShadowBuf[3];

struct
{
	matrix mvp;
	vec4 Light_Pos;
} shadow_ubo;

void InitShadowCubeMap(Image_t *Image)
{
	VkCommandBuffer CommandBuffer;
	VkFence Fence;

	vkuCreateImageBuffer(&Context, Image,
		VK_IMAGE_TYPE_2D, ShadowColorFormat, 1, 6, ShadowCubeSize, ShadowCubeSize, 1,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

	vkAllocateCommandBuffers(Context.Device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=Context.CommandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=1,
	}, &CommandBuffer);

	vkBeginCommandBuffer(CommandBuffer, &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	});

	vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=Image->Image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.levelCount=1,
		.subresourceRange.layerCount=6,
		.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	});

	vkEndCommandBuffer(CommandBuffer);
		
	// Create a fence for the queue submit
	vkCreateFence(Context.Device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=0 }, VK_NULL_HANDLE, &Fence);

	// Submit commands to the queue
	vkQueueSubmit(Context.Queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&CommandBuffer,
	}, Fence);

	// Wait for the queue submit to finish
	vkWaitForFences(Context.Device, 1, &Fence, VK_TRUE, UINT64_MAX);

	// Destroy fence
	vkDestroyFence(Context.Device, Fence, VK_NULL_HANDLE);

	vkFreeCommandBuffers(Context.Device, Context.CommandPool, 1, &CommandBuffer);

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
		.compareOp=VK_COMPARE_OP_NEVER,
		.minLod=0.0f,
		.maxLod=1.0f,
		.maxAnisotropy=1.0,
		.anisotropyEnable=VK_FALSE,
		.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	}, VK_NULL_HANDLE, &Image->Sampler);

	vkCreateImageView(Context.Device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType=VK_IMAGE_VIEW_TYPE_CUBE,
		.format=ShadowColorFormat,
		.components.r={ VK_COMPONENT_SWIZZLE_R },
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=6,
		.subresourceRange.levelCount=1,
		.image=Image->Image,
	}, VK_NULL_HANDLE, &Image->View);
}

void InitShadowFramebuffer(void)
{
	VkCommandBuffer CommandBuffer;
	VkFence Fence;

	// Color
	vkuCreateImageBuffer(&Context, &ShadowColor,
		VK_IMAGE_TYPE_2D, ShadowColorFormat, 1, 1, ShadowCubeSize, ShadowCubeSize, 1,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0);

	// Depth
	vkuCreateImageBuffer(&Context, &ShadowDepth,
		VK_IMAGE_TYPE_2D, ShadowDepthFormat, 1, 1, ShadowCubeSize, ShadowCubeSize, 1,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0);

	vkAllocateCommandBuffers(Context.Device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=Context.CommandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=1,
	}, &CommandBuffer);

	vkBeginCommandBuffer(CommandBuffer, &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	});

	vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=ShadowColor.Image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.levelCount=1,
		.subresourceRange.layerCount=1,
		.srcAccessMask=0,
		.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	});

	vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=ShadowDepth.Image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.levelCount=1,
		.subresourceRange.layerCount=1,
		.srcAccessMask=0,
		.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	});

	vkEndCommandBuffer(CommandBuffer);
		
	// Create a fence for the queue submit
	vkCreateFence(Context.Device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=0 }, VK_NULL_HANDLE, &Fence);

	// Submit commands to the queue
	vkQueueSubmit(Context.Queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&CommandBuffer,
	}, Fence);

	// Wait for the queue submit to finish
	vkWaitForFences(Context.Device, 1, &Fence, VK_TRUE, UINT64_MAX);

	// Destroy fence
	vkDestroyFence(Context.Device, Fence, VK_NULL_HANDLE);

	vkFreeCommandBuffers(Context.Device, Context.CommandPool, 1, &CommandBuffer);

	vkCreateImageView(Context.Device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType=VK_IMAGE_VIEW_TYPE_2D,
		.format=ShadowColorFormat,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=1,
		.subresourceRange.levelCount=1,
		.image=ShadowColor.Image,
	}, VK_NULL_HANDLE, &ShadowColor.View);

	vkCreateImageView(Context.Device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType=VK_IMAGE_VIEW_TYPE_2D,
		.format=ShadowDepthFormat,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=1,
		.subresourceRange.levelCount=1,
		.image=ShadowDepth.Image,
	}, VK_NULL_HANDLE, &ShadowDepth.View);

	vkCreateRenderPass(Context.Device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=2,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=ShadowColorFormat,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			},
			{
				.format=ShadowDepthFormat,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
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
	}, 0, &ShadowRenderPass);

	vkCreateFramebuffer(Context.Device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=ShadowRenderPass,
		.attachmentCount=2,
		.pAttachments=(VkImageView[]) { ShadowColor.View, ShadowDepth.View },
		.width=ShadowCubeSize,
		.height=ShadowCubeSize,
		.layers=1,
	}, 0, &ShadowFrameBuffer);
}

bool InitShadowPipeline(void)
{
	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
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

void ShadowUpdateCubemap(VkCommandBuffer CommandBuffer, Image_t Shadow, vec4 Pos)
{
	MatrixIdentity(Projection);
	MatrixInfPerspective(90.0f, 1.0f, 0.01f, false, Projection);

	for(uint32_t face=0;face<6;face++)
	{
		switch(face)
		{
			case 0:
				MatrixIdentity(ModelView);
				MatrixLookAt(Pos, (vec3) { Pos[0]+1.0f, Pos[1]+0.0f, Pos[2]+0.0f }, (vec3) { 0.0f, -1.0f, 0.0f }, ModelView);
				break;
			case 1:
				MatrixIdentity(ModelView);
				MatrixLookAt(Pos, (vec3) { Pos[0]-1.0f, Pos[1]+0.0f, Pos[2]+0.0f }, (vec3) { 0.0f, -1.0f, 0.0f }, ModelView);
				break;
			case 2:
				MatrixIdentity(ModelView);
				MatrixLookAt(Pos, (vec3) { Pos[0]+0.0f, Pos[1]+1.0f, Pos[2]+0.0f }, (vec3) { 0.0f, 0.0f, 1.0f }, ModelView);
				break;
			case 3:
				MatrixIdentity(ModelView);
				MatrixLookAt(Pos, (vec3) { Pos[0]+0.0f, Pos[1]-1.0f, Pos[2]+0.0f }, (vec3) { 0.0f, 0.0f, -1.0f }, ModelView);
				break;
			case 4:
				MatrixIdentity(ModelView);
				MatrixLookAt(Pos, (vec3) { Pos[0]+0.0f, Pos[1]+0.0f, Pos[2]+1.0f }, (vec3) { 0.0f, -1.0f, 0.0f }, ModelView);
				break;
			case 5:
				MatrixIdentity(ModelView);
				MatrixLookAt(Pos, (vec3) { Pos[0]+0.0f, Pos[1]+0.0f, Pos[2]-1.0f }, (vec3) { 0.0f, -1.0f, 0.0f }, ModelView);
				break;
		}

		MatrixMult(ModelView, Projection, shadow_ubo.mvp);

		Vec4_Setv(shadow_ubo.Light_Pos, Pos);

		vkCmdBeginRenderPass(CommandBuffer, &(VkRenderPassBeginInfo)
		{
			.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass=ShadowRenderPass,
			.framebuffer=ShadowFrameBuffer,
			.clearValueCount=2,
			.pClearValues=(VkClearValue[]) { { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0 } },
			.renderArea.offset=(VkOffset2D) { 0, 0 },
			.renderArea.extent=(VkExtent2D)	{ ShadowCubeSize, ShadowCubeSize },
		}, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdPushConstants(CommandBuffer, ShadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(shadow_ubo), &shadow_ubo);

		// Bind the pipeline descriptor, this sets the pipeline states (blend, depth/stencil tests, etc)
		vkCmdBindPipeline(CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ShadowPipeline.Pipeline);

		vkCmdSetViewport(CommandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)ShadowCubeSize, (float)ShadowCubeSize, 0.0f, 1.0f });
		vkCmdSetScissor(CommandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { ShadowCubeSize, ShadowCubeSize } });

		// Draw the models
		for(uint32_t i=0;i<NUM_MODELS;i++)
		{
			// Bind model data buffers and draw the triangles
			for(int32_t j=0;j<Model[i].NumMesh;j++)
			{
				vkCmdBindVertexBuffers(CommandBuffer, 0, 1, &Model[i].Mesh[j].Buffer, &(VkDeviceSize) { 0 });
				vkCmdBindIndexBuffer(CommandBuffer, Model[i].Mesh[j].IndexBuffer, 0, VK_INDEX_TYPE_UINT16);
				vkCmdDrawIndexed(CommandBuffer, Model[i].Mesh[j].NumFace*3, 1, 0, 0, 0);
			}
		}

		vkCmdEndRenderPass(CommandBuffer);

		// Change frame buffer image layout to source transfer
		vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.image=ShadowColor.Image,
			.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.subresourceRange.baseMipLevel=0,
			.subresourceRange.baseArrayLayer=0,
			.subresourceRange.levelCount=1,
			.subresourceRange.layerCount=1,
			.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT,
			.oldLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		});

		// Change cubemap texture image face to transfer destination
		vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.image=Shadow.Image,
			.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.subresourceRange.baseMipLevel=0,
			.subresourceRange.baseArrayLayer=face,
			.subresourceRange.levelCount=1,
			.subresourceRange.layerCount=1,
			.srcAccessMask=VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
			.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		});

		// Copy image from framebuffer to cube face
		vkCmdCopyImage(CommandBuffer, ShadowColor.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Shadow.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkImageCopy)
		{
			.srcSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.srcSubresource.baseArrayLayer=0,
			.srcSubresource.mipLevel=0,
			.srcSubresource.layerCount=1,
			.srcOffset={ 0, 0, 0 },
			.dstSubresource.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.dstSubresource.baseArrayLayer=face,
			.dstSubresource.mipLevel=0,
			.dstSubresource.layerCount=1,
			.dstOffset={ 0, 0, 0 },
			.extent.width=ShadowCubeSize,
			.extent.height=ShadowCubeSize,
			.extent.depth=1,
		});

		// Change frame buffer image layout back to color arrachment
		vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.image=ShadowColor.Image,
			.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.subresourceRange.baseMipLevel=0,
			.subresourceRange.baseArrayLayer=0,
			.subresourceRange.levelCount=1,
			.subresourceRange.layerCount=1,
			.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT,
			.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
			.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.newLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		});

		// Change cubemap texture image face back to shader read-only (for use in the main render shader)
		vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.image=Shadow.Image,
			.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.subresourceRange.baseMipLevel=0,
			.subresourceRange.baseArrayLayer=face,
			.subresourceRange.levelCount=1,
			.subresourceRange.layerCount=1,
			.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
			.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		});
	}
}
//
// ---

bool CreateFramebuffers(void)
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
	vkCreateDescriptorPool(Context.Device, &(VkDescriptorPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext=VK_NULL_HANDLE,
		.maxSets=1,
		.poolSizeCount=2,
		.pPoolSizes=(VkDescriptorPoolSize[])
		{
			{
				.type=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.descriptorCount=1,
			},
			{
				.type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount=6,
			},
		},
	}, NULL, &DescriptorPool);

	vkuInitDescriptorSetLayout(&DescriptorSetLayout, &Context);

	vkuDescriptorSetLayout_AddBinding(&DescriptorSetLayout, 0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE);
	vkuDescriptorSetLayout_AddBinding(&DescriptorSetLayout, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE);
	vkuDescriptorSetLayout_AddBinding(&DescriptorSetLayout, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE);
	vkuDescriptorSetLayout_AddBinding(&DescriptorSetLayout, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE);
	vkuDescriptorSetLayout_AddBinding(&DescriptorSetLayout, 4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE);
	vkuDescriptorSetLayout_AddBinding(&DescriptorSetLayout, 5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, VK_NULL_HANDLE);

	vkuAssembleDescriptorSetLayout(&DescriptorSetLayout);

	vkAllocateDescriptorSets(Context.Device, &(VkDescriptorSetAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext=NULL,
		.descriptorPool=DescriptorPool,
		.descriptorSetCount=1,
		.pSetLayouts=&DescriptorSetLayout.DescriptorSetLayout
	}, &DescriptorSet);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&DescriptorSetLayout.DescriptorSetLayout,
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

void BuildMemoryBuffers(Model3DS_t *Model)
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	void *Data=NULL;

	for(int32_t i=0;i<Model->NumMesh;i++)
	{
		// Vertex data on device memory
		vkuCreateBuffer(&Context,
			&Model->Mesh[i].Buffer, &Model->Mesh[i].BufferMemory,
			sizeof(float)*20*Model->Mesh[i].NumVertex,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		// Create staging buffer to transfer from host memory to device memory
		vkuCreateBuffer(&Context,
			&stagingBuffer, &stagingBufferMemory,
			sizeof(float)*20*Model->Mesh[i].NumVertex,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		vkMapMemory(Context.Device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &Data);

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

		vkUnmapMemory(Context.Device, stagingBufferMemory);

		// Copy to device memory
		vkuCopyBuffer(&Context, stagingBuffer, Model->Mesh[i].Buffer, sizeof(float)*20*Model->Mesh[i].NumVertex);

		// Delete staging data
		vkFreeMemory(Context.Device, stagingBufferMemory, VK_NULL_HANDLE);
		vkDestroyBuffer(Context.Device, stagingBuffer, VK_NULL_HANDLE);

		// Index data
		vkuCreateBuffer(&Context, &Model->Mesh[i].IndexBuffer, &Model->Mesh[i].IndexBufferMemory, sizeof(uint16_t)*Model->Mesh[i].NumFace*3,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		// Staging buffer
		vkuCreateBuffer(&Context, &stagingBuffer, &stagingBufferMemory, sizeof(uint16_t)*Model->Mesh[i].NumFace*3,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		vkMapMemory(Context.Device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &Data);

		if(!Data)
			return;

		uint16_t *sPtr=Data;

		for(int32_t j=0;j<Model->Mesh[i].NumFace;j++)
		{
			*sPtr++=Model->Mesh[i].Face[3*j+0];
			*sPtr++=Model->Mesh[i].Face[3*j+1];
			*sPtr++=Model->Mesh[i].Face[3*j+2];
		}

		vkUnmapMemory(Context.Device, stagingBufferMemory);

		vkuCopyBuffer(&Context, stagingBuffer, Model->Mesh[i].IndexBuffer, sizeof(uint16_t)*Model->Mesh[i].NumFace*3);

		// Delete staging data
		vkFreeMemory(Context.Device, stagingBufferMemory, VK_NULL_HANDLE);
		vkDestroyBuffer(Context.Device, stagingBuffer, VK_NULL_HANDLE);
	}
}

void Render(void)
{
	static OldIndex=0;
	uint32_t Index=OldIndex;
	uint32_t ImageIndex;
	int i, j;

	// Generate the projection matrix
	MatrixIdentity(Projection);
	MatrixInfPerspective(90.0f, (float)Width/Height, 0.01f, true, Projection);

	// Set up the modelview matrix
	MatrixIdentity(ModelView);
	MatrixTranslate(PanX, PanY, Zoom, ModelView);

	QuatAngle(RotateX, 0.0f, 1.0f, 0.0f, QuatX);
	QuatAngle(RotateY, 1.0f, 0.0f, 0.0f, QuatY);
	QuatMultiply(QuatY, QuatX, Quat);
	QuatMatrix(Quat, ModelView);

	// Generate an inverse modelview matrix (only really need the last 3 from the calculation)
	ubo.eye[0]=-(ModelView[12]*ModelView[ 0])-(ModelView[13]*ModelView[ 1])-(ModelView[14]*ModelView[ 2]);
	ubo.eye[1]=-(ModelView[12]*ModelView[ 4])-(ModelView[13]*ModelView[ 5])-(ModelView[14]*ModelView[ 6]);
	ubo.eye[2]=-(ModelView[12]*ModelView[ 8])-(ModelView[13]*ModelView[ 9])-(ModelView[14]*ModelView[10]);

	// Generate a modelview+projection matrix
	MatrixMult(ModelView, Projection, ubo.mvp);

	// Set number of lights to the shader
	ubo.NumLights=(uint32_t)List_GetCount(&Lights.Lights);

	Lights_UpdateSSBO(&Lights);

	vkAcquireNextImageKHR(Context.Device, Swapchain, UINT64_MAX, PresentCompleteSemaphores[Index], VK_NULL_HANDLE, &ImageIndex);

	vkWaitForFences(Context.Device, 1, &FrameFences[Index], VK_TRUE, UINT64_MAX);
	vkResetFences(Context.Device, 1, &FrameFences[Index]);

	// Start recording the commands
	vkBeginCommandBuffer(CommandBuffers[Index], &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	});

	//ShadowUpdateCubemap(CommandBuffers[Index], ShadowBuf[0], ubo.Light0_Pos);
	//ShadowUpdateCubemap(CommandBuffers[Index], ShadowBuf[1], ubo.Light1_Pos);
	//ShadowUpdateCubemap(CommandBuffers[Index], ShadowBuf[2], ubo.Light2_Pos);

	// Start a render pass and clear the frame/depth buffer
	vkCmdBeginRenderPass(CommandBuffers[Index], &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=RenderPass,
		.framebuffer=FrameBuffers[ImageIndex],
		.clearValueCount=2,
		.pClearValues=(VkClearValue[]) { { 0.0f, 0.0f, 0.0f, 1.0f }, { 1.0f, 0 } },
		.renderArea.offset={ 0, 0 },
		.renderArea.extent=SwapchainExtent,
	}, VK_SUBPASS_CONTENTS_INLINE);

	// Bind the pipeline descriptor, this sets the pipeline states (blend, depth/stencil tests, etc)
	vkCmdBindPipeline(CommandBuffers[Index], VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline.Pipeline);

	vkCmdSetViewport(CommandBuffers[Index], 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)SwapchainExtent.width, (float)SwapchainExtent.height, 0.0f, 1.0f });
	vkCmdSetScissor(CommandBuffers[Index], 0, 1, &(VkRect2D) { { 0, 0 }, SwapchainExtent});

	vkCmdPushConstants(CommandBuffers[Index], PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(ubo), &ubo);

	// Draw the models
	for(i=0;i<NUM_MODELS;i++)
	{
		vkUpdateDescriptorSets(Context.Device, 6, (VkWriteDescriptorSet[])
		{
			{
				.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.descriptorCount=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.dstBinding=0,
				.pBufferInfo=&(VkDescriptorBufferInfo)
				{
					.buffer=Lights.StorageBuffer,
					.offset=0,
					.range=VK_WHOLE_SIZE,
				},
				.dstSet=DescriptorSet,
			},
			{
				.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.descriptorCount=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.dstBinding=1,
				.pImageInfo=&(VkDescriptorImageInfo)
				{
					.imageView=Textures[2*i+0].View,
					.sampler=Textures[2*i+0].Sampler,
					.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				},
				.dstSet=DescriptorSet,
			},
			{
				.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.descriptorCount=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.dstBinding=2,
				.pImageInfo=&(VkDescriptorImageInfo)
				{
					.imageView=Textures[2*i+1].View,
					.sampler=Textures[2*i+1].Sampler,
					.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				},
				.dstSet=DescriptorSet,
			},
			{
				.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.descriptorCount=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.dstBinding=3,
				.pImageInfo=&(VkDescriptorImageInfo)
				{
					.imageView=ShadowBuf[0].View,
					.sampler=ShadowBuf[0].Sampler,
					.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				},
				.dstSet=DescriptorSet,
			},
			{
				.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.descriptorCount=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.dstBinding=4,
				.pImageInfo=&(VkDescriptorImageInfo)
				{
					.imageView=ShadowBuf[1].View,
					.sampler=ShadowBuf[1].Sampler,
					.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				},
				.dstSet=DescriptorSet,
			},
			{
				.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.descriptorCount=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.dstBinding=5,
				.pImageInfo=&(VkDescriptorImageInfo)
				{
					.imageView=ShadowBuf[2].View,
					.sampler=ShadowBuf[2].Sampler,
					.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				},
				.dstSet=DescriptorSet,
			},
		}, 0, VK_NULL_HANDLE);

		// Bind per-model destriptor set, this changes texture binding
		vkCmdBindDescriptorSets(CommandBuffers[Index], VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout, 0, 1, &DescriptorSet, 0, NULL);

		// Bind model data buffers and draw the triangles
		for(j=0;j<Model[i].NumMesh;j++)
		{
			vkCmdBindVertexBuffers(CommandBuffers[Index], 0, 1, &Model[i].Mesh[j].Buffer, &(VkDeviceSize) { 0 });
			vkCmdBindIndexBuffer(CommandBuffers[Index], Model[i].Mesh[j].IndexBuffer, 0, VK_INDEX_TYPE_UINT16);
			vkCmdDrawIndexed(CommandBuffers[Index], Model[i].Mesh[j].NumFace*3, 1, 0, 0, 0);
		}
	}
	// ---

	// Should UI overlay stuff have it's own render pass?
	// Maybe even separate thread?
	Font_Print(CommandBuffers[Index], 0.0f, 16.0f, "FPS: %0.1f\n\n\n\nNumber of lights: %d", fps, ubo.NumLights);

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
		.pImageIndices=&ImageIndex,
	});

	OldIndex=Index;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger)
{
	PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT=(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

	if(vkCreateDebugUtilsMessengerEXT!=NULL)
		return vkCreateDebugUtilsMessengerEXT(instance, pCreateInfo, pAllocator, pDebugMessenger);
	else
		return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator)
{
	PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT=(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

	if(vkDestroyDebugUtilsMessengerEXT!=NULL)
		vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, pAllocator);
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
{
	DBGPRINTF("Validation layer: %s\n", pCallbackData->pMessage);

	return VK_FALSE;
}

bool Init(void)
{
	VkDebugUtilsMessengerCreateInfoEXT createInfo=
	{
		.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback=debugCallback
	};

	if(CreateDebugUtilsMessengerEXT(Instance, &createInfo, VK_NULL_HANDLE, &debugMessenger)!=VK_SUCCESS)
		return false;

	Lights_Init(&Lights);
	Lights_Add(&Lights, (vec3) { 0.0f, 0.0f, 0.0f }, 256.0f, (vec4) { 1.0f, 0.0f, 0.0f, 1.0f });
	Lights_Add(&Lights, (vec3) { -50.0f, 0.0f, 0.0f }, 256.0f, (vec4) { 0.0f, 1.0f, 0.0f, 1.0f });
	Lights_Add(&Lights, (vec3) { 50.0f, 0.0f, 0.0f }, 256.0f, (vec4) { 0.0f, 0.0f, 1.0f, 1.0f });

	// Create primary frame buffers, depth image, and renderpass
	CreateFramebuffers();

	// Create main render pipeline
	CreatePipeline();

	// Load models
	if(Load3DS(&Model[MODEL_HELLKNIGHT], "./assets/hellknight.3ds"))
		BuildMemoryBuffers(&Model[MODEL_HELLKNIGHT]);

	if(Load3DS(&Model[MODEL_PINKY], "./assets/pinky.3ds"))
		BuildMemoryBuffers(&Model[MODEL_PINKY]);

	if(Load3DS(&Model[MODEL_FATTY], "./assets/fatty.3ds"))
		BuildMemoryBuffers(&Model[MODEL_FATTY]);

	if(Load3DS(&Model[MODEL_LEVEL], "./assets/level.3ds"))
		BuildMemoryBuffers(&Model[MODEL_LEVEL]);

	// Load textures
	Image_Upload(&Context, &Textures[TEXTURE_HELLKNIGHT], "./assets/hellknight.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_HELLKNIGHT_NORMAL], "./assets/hellknight_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_PINKY], "./assets/pinky.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_PINKY_NORMAL], "./assets/pinky_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_FATTY], "./assets/fatty.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_FATTY_NORMAL], "./assets/fatty_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_LEVEL], "./assets/tile.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_LEVEL_NORMAL], "./assets/tile_b.tga", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALMAP);

	// Uniform data buffer and pointer mapping
	vkuCreateBuffer(&Context, &uniformBuffer, &uniformBufferMemory, sizeof(ubo),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkMapMemory(Context.Device, uniformBufferMemory, 0, VK_WHOLE_SIZE, 0, &uniformBufferPtr);

	InitShadowCubeMap(&ShadowBuf[0]);
	InitShadowCubeMap(&ShadowBuf[1]);
	InitShadowCubeMap(&ShadowBuf[2]);

	InitShadowFramebuffer();
	InitShadowPipeline();
 
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

	VkSurfaceFormatKHR *SurfaceFormats=(VkSurfaceFormatKHR *)malloc(sizeof(VkSurfaceFormatKHR)*FormatCount);

	if(SurfaceFormats==NULL)
		return;

	vkGetPhysicalDeviceSurfaceFormatsKHR(Context->PhysicalDevice, Context->Surface, &FormatCount, SurfaceFormats);

	// If no format is specified, find a 32bit RGBA format
	if(SurfaceFormats[0].format==VK_FORMAT_UNDEFINED)
		SurfaceFormat.format=VK_FORMAT_R8G8B8A8_SNORM;
	// Otherwise the first format is the current surface format
	else
		SurfaceFormat=SurfaceFormats[0];

	FREE(SurfaceFormats);

	// Get physical device surface properties and formats
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Context->PhysicalDevice, Context->Surface, &SurfCaps);

	// Get available present modes
	vkGetPhysicalDeviceSurfacePresentModesKHR(Context->PhysicalDevice, Context->Surface, &PresentModeCount, NULL);

	VkPresentModeKHR *PresentModes=(VkPresentModeKHR *)malloc(sizeof(VkPresentModeKHR)*PresentModeCount);

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

	FREE(PresentModes);

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
		vkFreeMemory(Context.Device, DepthImage.DeviceMemory, VK_NULL_HANDLE);
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

	DestroyDebugUtilsMessengerEXT(Instance, debugMessenger, VK_NULL_HANDLE);

	Lights_Destroy(&Lights);

	// Shadow stuff
	vkDestroyPipeline(Context.Device, ShadowPipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, ShadowPipelineLayout, VK_NULL_HANDLE);

	vkDestroyFramebuffer(Context.Device, ShadowFrameBuffer, VK_NULL_HANDLE);

	vkDestroyRenderPass(Context.Device, ShadowRenderPass, VK_NULL_HANDLE);

	// Shadow frame buffer color
	vkDestroyImageView(Context.Device, ShadowColor.View, VK_NULL_HANDLE);
	vkFreeMemory(Context.Device, ShadowColor.DeviceMemory, VK_NULL_HANDLE);
	vkDestroyImage(Context.Device, ShadowColor.Image, VK_NULL_HANDLE);

	// Shadow frame buffer depth
	vkDestroyImageView(Context.Device, ShadowDepth.View, VK_NULL_HANDLE);
	vkFreeMemory(Context.Device, ShadowDepth.DeviceMemory, VK_NULL_HANDLE);
	vkDestroyImage(Context.Device, ShadowDepth.Image, VK_NULL_HANDLE);

	// Shadow depth cubemap texture
	vkDestroySampler(Context.Device, ShadowBuf[0].Sampler, VK_NULL_HANDLE);
	vkDestroyImageView(Context.Device, ShadowBuf[0].View, VK_NULL_HANDLE);
	vkFreeMemory(Context.Device, ShadowBuf[0].DeviceMemory, VK_NULL_HANDLE);
	vkDestroyImage(Context.Device, ShadowBuf[0].Image, VK_NULL_HANDLE);

	vkDestroySampler(Context.Device, ShadowBuf[1].Sampler, VK_NULL_HANDLE);
	vkDestroyImageView(Context.Device, ShadowBuf[1].View, VK_NULL_HANDLE);
	vkFreeMemory(Context.Device, ShadowBuf[1].DeviceMemory, VK_NULL_HANDLE);
	vkDestroyImage(Context.Device, ShadowBuf[1].Image, VK_NULL_HANDLE);

	vkDestroySampler(Context.Device, ShadowBuf[2].Sampler, VK_NULL_HANDLE);
	vkDestroyImageView(Context.Device, ShadowBuf[2].View, VK_NULL_HANDLE);
	vkFreeMemory(Context.Device, ShadowBuf[2].DeviceMemory, VK_NULL_HANDLE);
	vkDestroyImage(Context.Device, ShadowBuf[2].Image, VK_NULL_HANDLE);
	// ---

	Font_Destroy();

	for(uint32_t i=0;i<NUM_TEXTURES;i++)
	{
		vkDestroySampler(Context.Device, Textures[i].Sampler, VK_NULL_HANDLE);
		vkDestroyImageView(Context.Device, Textures[i].View, VK_NULL_HANDLE);
		vkFreeMemory(Context.Device, Textures[i].DeviceMemory, VK_NULL_HANDLE);
		vkDestroyImage(Context.Device, Textures[i].Image, VK_NULL_HANDLE);
	}

	for(uint32_t i=0;i<NUM_MODELS;i++)
	{
		for(uint32_t j=0;j<(uint32_t)Model[i].NumMesh;j++)
		{
			vkFreeMemory(Context.Device, Model[i].Mesh[j].BufferMemory, VK_NULL_HANDLE);
			vkDestroyBuffer(Context.Device, Model[i].Mesh[j].Buffer, VK_NULL_HANDLE);

			vkFreeMemory(Context.Device, Model[i].Mesh[j].IndexBufferMemory, VK_NULL_HANDLE);
			vkDestroyBuffer(Context.Device, Model[i].Mesh[j].IndexBuffer, VK_NULL_HANDLE);
		}
	}

	vkFreeMemory(Context.Device, uniformBufferMemory, VK_NULL_HANDLE);
	vkDestroyBuffer(Context.Device, uniformBuffer, VK_NULL_HANDLE);

	vkDestroyDescriptorSetLayout(Context.Device, DescriptorSetLayout.DescriptorSetLayout, VK_NULL_HANDLE);
	vkDestroyDescriptorPool(Context.Device, DescriptorPool, VK_NULL_HANDLE);

	vkDestroyPipeline(Context.Device, Pipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, PipelineLayout, VK_NULL_HANDLE);

	vkDestroyImageView(Context.Device, DepthImage.View, VK_NULL_HANDLE);
	vkFreeMemory(Context.Device, DepthImage.DeviceMemory, VK_NULL_HANDLE);
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

	vkDestroyRenderPass(Context.Device, RenderPass, VK_NULL_HANDLE);
}
