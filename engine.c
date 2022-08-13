#include <windows.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "system.h"
#include "vulkan.h"
#include "math.h"
#include "3ds.h"
#include "image.h"
#include "font.h"

char szAppName[]="Vulkan";

int Width=1280, Height=720;
 int Done=0, Key[256];

 VkInstance Instance;
 VkContext_t Context;
 
 float RotateX=0.0f, RotateY=0.0f, PanX=0.0f, PanY=0.0f, Zoom=-100.0f;

unsigned __int64 Frequency, StartTime, EndTime;
float avgfps=0.0f, fps=0.0f, fTimeStep, fTime=0.0f;
int Frames=0;

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

struct
{
	float mvp[16];
	float eye[4];

	float Light0_Pos[4];
	float Light0_Kd[4];
	float Light1_Pos[4];
	float Light1_Kd[4];
	float Light2_Pos[4];
	float Light2_Kd[4];
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

#define MAX_FRAME_COUNT 3

uint32_t SwapchainImageCount=0;

VkImage SwapchainImage[MAX_FRAME_COUNT];
VkImageView SwapchainImageView[MAX_FRAME_COUNT];
VkFramebuffer FrameBuffers[MAX_FRAME_COUNT];

// Depth buffer handles
Image_t Depth;

VkRenderPass RenderPass;
VkPipelineLayout PipelineLayout;
VkuPipeline_t Pipeline;

VkDescriptorPool DescriptorPool;
VkDescriptorSet DescriptorSet[4];
VkDescriptorSetLayout DescriptorSetLayout;

uint32_t FrameIndex=0;

VkCommandBuffer CommandBuffers[MAX_FRAME_COUNT];

VkFence FrameFences[MAX_FRAME_COUNT];
VkSemaphore PresentCompleteSemaphores[MAX_FRAME_COUNT];
VkSemaphore RenderCompleteSemaphores[MAX_FRAME_COUNT];

// Shadow cubemap stuff
//
int32_t ShadowCubeSize=1024;

VkFramebuffer ShadowFrameBuffer;

// Frame buffer depth
Image_t ShadowDepth;

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
	VkCommandBuffer layoutCmd;

	vkuCreateImageBuffer(&Context, Image,
		VK_IMAGE_TYPE_2D, DepthFormat, 1, 6, ShadowCubeSize, ShadowCubeSize, 1,
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
	}, &layoutCmd);

	vkBeginCommandBuffer(layoutCmd, &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	});

	vkCmdPipelineBarrier(layoutCmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=Image->Image,
		.subresourceRange=(VkImageSubresourceRange)
		{
			.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel=0,
			.levelCount=1,
			.layerCount=6,
		},
		.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT,
		.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	});

	vkEndCommandBuffer(layoutCmd);
		
	vkQueueSubmit(Context.Queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&layoutCmd,
	}, VK_NULL_HANDLE);

	vkQueueWaitIdle(Context.Queue);

	vkFreeCommandBuffers(Context.Device, Context.CommandPool, 1, &layoutCmd);

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
		.format=DepthFormat,
		.components.r={ VK_COMPONENT_SWIZZLE_R },
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=6,
		.subresourceRange.levelCount=1,
		.image=Image->Image,
	}, VK_NULL_HANDLE, &Image->View);
}

void InitShadowFramebuffer(void)
{
	VkCommandBuffer copyCmd;

	// Depth
	vkuCreateImageBuffer(&Context, &ShadowDepth,
		VK_IMAGE_TYPE_2D, DepthFormat, 1, 1, ShadowCubeSize, ShadowCubeSize, 1,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0);

	vkAllocateCommandBuffers(Context.Device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=Context.CommandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=1,
	}, &copyCmd);

	vkBeginCommandBuffer(copyCmd, &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
	});

	vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=ShadowDepth.Image,
		.subresourceRange=(VkImageSubresourceRange)
		{
			.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
			.baseMipLevel=0,
			.levelCount=1,
			.layerCount=1,
		},
		.srcAccessMask=0,
		.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
	});

	vkEndCommandBuffer(copyCmd);
		
	vkQueueSubmit(Context.Queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&copyCmd,
	}, VK_NULL_HANDLE);

	vkQueueWaitIdle(Context.Queue);

	vkFreeCommandBuffers(Context.Device, Context.CommandPool, 1, &copyCmd);

	vkCreateImageView(Context.Device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType=VK_IMAGE_VIEW_TYPE_2D,
		.format=DepthFormat,
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
		.attachmentCount=1,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=DepthFormat,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			},
		},
		.subpassCount=1,
		.pSubpasses=&(VkSubpassDescription)
		{
			.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount=0,
			.pDepthStencilAttachment=&(VkAttachmentReference)
			{
				.attachment=0,
				.layout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			},
		},
	}, 0, &ShadowRenderPass);

	vkCreateFramebuffer(Context.Device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=ShadowRenderPass,
		.attachmentCount=1,
		.pAttachments=(VkImageView[]) { ShadowDepth.View },
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
			.size=sizeof(float)*(16+4),
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},		
	}, 0, &ShadowPipelineLayout);

	vkuInitPipeline(&ShadowPipeline, Context.Device, ShadowPipelineLayout, ShadowRenderPass);

	// Add in vertex shader
	if(!vkuPipeline_AddStage(&ShadowPipeline, "distance_v.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	// Add in fragment shader
	if(!vkuPipeline_AddStage(&ShadowPipeline, "distance_f.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	// Set states that are different than defaults
	ShadowPipeline.CullMode=VK_CULL_MODE_FRONT_BIT;
	ShadowPipeline.DepthTest=VK_TRUE;

	// Add vertex binding and attrib parameters
	vkuPipeline_AddVertexBinding(&ShadowPipeline, 0, sizeof(float)*14, VK_VERTEX_INPUT_RATE_VERTEX);
	vkuPipeline_AddVertexAttribute(&ShadowPipeline, 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);

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
			.clearValueCount=1,
			.pClearValues=(VkClearValue[]) { { .depthStencil.depth=1.0f, .depthStencil.stencil=0 } },
			.renderArea.offset=(VkOffset2D) { 0, 0 },
			.renderArea.extent=(VkExtent2D)	{ ShadowCubeSize, ShadowCubeSize },
		}, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdPushConstants(CommandBuffer, ShadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float)*(16+4), &shadow_ubo);

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
		vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.image=ShadowDepth.Image,
			.subresourceRange=(VkImageSubresourceRange)
			{
				.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel=0,
				.baseArrayLayer=0,
				.levelCount=1,
				.layerCount=1,
			},
			.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT,
			.oldLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
			.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		});

		// Change cubemap texture image face to transfer destination
		vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.image=Shadow.Image,
			.subresourceRange=(VkImageSubresourceRange)
			{
				.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel=0,
				.baseArrayLayer=face,
				.levelCount=1,
				.layerCount=1,
			},
			.srcAccessMask=VK_ACCESS_SHADER_READ_BIT,
			.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
			.oldLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		});

		// Copy image from framebuffer to cube face
		vkCmdCopyImage(CommandBuffer, ShadowDepth.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Shadow.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkImageCopy)
		{
			.srcSubresource.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
			.srcSubresource.baseArrayLayer=0,
			.srcSubresource.mipLevel=0,
			.srcSubresource.layerCount=1,
			.srcOffset={ 0, 0, 0 },
			.dstSubresource.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
			.dstSubresource.baseArrayLayer=face,
			.dstSubresource.mipLevel=0,
			.dstSubresource.layerCount=1,
			.dstOffset={ 0, 0, 0 },
			.extent.width=ShadowCubeSize,
			.extent.height=ShadowCubeSize,
			.extent.depth=1,
		});

		// Change frame buffer image layout back to color arrachment
		vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.image=ShadowDepth.Image,
			.subresourceRange=(VkImageSubresourceRange)
			{
				.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel=0,
				.baseArrayLayer=0,
				.levelCount=1,
				.layerCount=1,
			},
			.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT,
			.dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
			.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			.newLayout=VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
		});

		// Change cubemap texture image face back to shader read-only (for use in the main render shader)
		vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.image=Shadow.Image,
			.subresourceRange=(VkImageSubresourceRange)
			{
				.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
				.baseMipLevel=0,
				.baseArrayLayer=face,
				.levelCount=1,
				.layerCount=1,
			},
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

	vkuCreateImageBuffer(&Context, &Depth,
		VK_IMAGE_TYPE_2D, DepthFormat, 1, 1, Width, Height, 1,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0);

	vkCreateImageView(Context.Device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext=NULL,
		.image=Depth.Image,
		.format=DepthFormat,
		.components.r=VK_COMPONENT_SWIZZLE_R,
		.components.g=VK_COMPONENT_SWIZZLE_G,
		.components.b=VK_COMPONENT_SWIZZLE_B,
		.components.a=VK_COMPONENT_SWIZZLE_A,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.levelCount=1,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=1,
		.viewType=VK_IMAGE_VIEW_TYPE_2D,
		.flags=0,
	}, NULL, &Depth.View);

	for(uint32_t i=0;i<SwapchainImageCount;i++)
	{
		vkCreateFramebuffer(Context.Device, &(VkFramebufferCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass=RenderPass,
			.attachmentCount=2,
			.pAttachments=(VkImageView[]) { SwapchainImageView[i], Depth.View },
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
		.maxSets=NUM_MODELS,
		.poolSizeCount=2,
		.pPoolSizes=(VkDescriptorPoolSize[])
		{
			{
				.type=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount=NUM_MODELS,
			},
			{
				.type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount=NUM_MODELS*6,
			},
		},
	}, NULL, &DescriptorPool);

	vkCreateDescriptorSetLayout(Context.Device, &(VkDescriptorSetLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.pNext=VK_NULL_HANDLE,
		.bindingCount=6,
		.pBindings=(VkDescriptorSetLayoutBinding[])
		{
			{
				.binding=0,
				.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.descriptorCount=1,
				.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers=NULL,
			},
			{
				.binding=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount=1,
				.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers=NULL,

			},
			{
				.binding=2,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount=1,
				.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers=NULL,
			},
			{
				.binding=3,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount=1,
				.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers=NULL,
			},
			{
				.binding=4,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount=1,
				.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers=NULL,
			},
			{
				.binding=5,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount=1,
				.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT,
				.pImmutableSamplers=NULL,
			},
		},
	}, NULL, &DescriptorSetLayout);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&DescriptorSetLayout,
	}, 0, &PipelineLayout);

	vkuInitPipeline(&Pipeline, Context.Device, PipelineLayout, RenderPass);

	Pipeline.DepthTest=VK_TRUE;
	Pipeline.CullMode=VK_CULL_MODE_BACK_BIT;

	if(!vkuPipeline_AddStage(&Pipeline, "lighting_v.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&Pipeline, "lighting_f.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	vkuPipeline_AddVertexBinding(&Pipeline, 0, sizeof(float)*14, VK_VERTEX_INPUT_RATE_VERTEX);

	vkuPipeline_AddVertexAttribute(&Pipeline, 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);
	vkuPipeline_AddVertexAttribute(&Pipeline, 1, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float)*3);
	vkuPipeline_AddVertexAttribute(&Pipeline, 2, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float)*5);
	vkuPipeline_AddVertexAttribute(&Pipeline, 3, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float)*8);
	vkuPipeline_AddVertexAttribute(&Pipeline, 4, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float)*11);

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
			sizeof(float)*14*Model->Mesh[i].NumVertex,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		// Create staging buffer to transfer from host memory to device memory
		vkuCreateBuffer(&Context,
			&stagingBuffer, &stagingBufferMemory,
			sizeof(float)*14*Model->Mesh[i].NumVertex,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		vkMapMemory(Context.Device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &Data);

		for(int32_t j=0;j<Model->Mesh[i].NumVertex;j++)
		{
			*((float *)Data)++=Model->Mesh[i].Vertex[3*j+0];
			*((float *)Data)++=Model->Mesh[i].Vertex[3*j+1];
			*((float *)Data)++=Model->Mesh[i].Vertex[3*j+2];

			*((float *)Data)++=Model->Mesh[i].UV[2*j+0];
			*((float *)Data)++=1.0f-Model->Mesh[i].UV[2*j+1];

			*((float *)Data)++=Model->Mesh[i].Tangent[3*j+0];
			*((float *)Data)++=Model->Mesh[i].Tangent[3*j+1];
			*((float *)Data)++=Model->Mesh[i].Tangent[3*j+2];

			*((float *)Data)++=Model->Mesh[i].Binormal[3*j+0];
			*((float *)Data)++=Model->Mesh[i].Binormal[3*j+1];
			*((float *)Data)++=Model->Mesh[i].Binormal[3*j+2];

			*((float *)Data)++=Model->Mesh[i].Normal[3*j+0];
			*((float *)Data)++=Model->Mesh[i].Normal[3*j+1];
			*((float *)Data)++=Model->Mesh[i].Normal[3*j+2];
		}

		vkUnmapMemory(Context.Device, stagingBufferMemory);

		// Copy to device memory
		vkuCopyBuffer(&Context, stagingBuffer, Model->Mesh[i].Buffer, sizeof(float)*14*Model->Mesh[i].NumVertex);

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

		for(int32_t j=0;j<Model->Mesh[i].NumFace;j++)
		{
			*((uint16_t *)Data)++=Model->Mesh[i].Face[3*j+0];
			*((uint16_t *)Data)++=Model->Mesh[i].Face[3*j+1];
			*((uint16_t *)Data)++=Model->Mesh[i].Face[3*j+2];
		}

		vkUnmapMemory(Context.Device, stagingBufferMemory);

		vkuCopyBuffer(&Context, stagingBuffer, Model->Mesh[i].IndexBuffer, sizeof(uint16_t)*Model->Mesh[i].NumFace*3);

		// Delete staging data
		vkFreeMemory(Context.Device, stagingBufferMemory, VK_NULL_HANDLE);
		vkDestroyBuffer(Context.Device, stagingBuffer, VK_NULL_HANDLE);
	}
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
	DBGPRINTF("validation layer: %s\n", pCallbackData->pMessage);

	return VK_FALSE;
}

void Render(void)
{
	uint32_t Index=FrameIndex%SwapchainImageCount;
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
//	MatrixInverse(ModelView, ubo.mvinv);
	ubo.eye[0]=-(ModelView[12]*ModelView[ 0])-(ModelView[13]*ModelView[ 1])-(ModelView[14]*ModelView[ 2]);
	ubo.eye[1]=-(ModelView[12]*ModelView[ 4])-(ModelView[13]*ModelView[ 5])-(ModelView[14]*ModelView[ 6]);
	ubo.eye[2]=-(ModelView[12]*ModelView[ 8])-(ModelView[13]*ModelView[ 9])-(ModelView[14]*ModelView[10]);

	// Generate a modelview+projection matrix
	MatrixMult(ModelView, Projection, ubo.mvp);

	// Set light uniform positions and color
	ubo.Light0_Pos[0]=sinf(fTime)*150.0f;
	ubo.Light0_Pos[1]=-25.0f;
	ubo.Light0_Pos[2]=cosf(fTime)*150.0f;
	ubo.Light0_Pos[3]=1.0f/256.0f;
	ubo.Light0_Kd[0]=1.0f;
	ubo.Light0_Kd[1]=1.0f;
	ubo.Light0_Kd[2]=1.0f;
	ubo.Light0_Kd[3]=1.0f;

	ubo.Light1_Pos[0]=cosf(fTime)*100.0f;
	ubo.Light1_Pos[1]=50.0f;
	ubo.Light1_Pos[2]=sinf(fTime)*100.0f;
	ubo.Light1_Pos[3]=1.0f/256.0f;
	ubo.Light1_Kd[0]=1.0f;
	ubo.Light1_Kd[1]=1.0f;
	ubo.Light1_Kd[2]=1.0f;
	ubo.Light1_Kd[3]=1.0f;

	ubo.Light2_Pos[0]=cosf(fTime)*100.0f;
	ubo.Light2_Pos[1]=-80.0f;
	ubo.Light2_Pos[2]=-15.0f;
	ubo.Light2_Pos[3]=1.0f/256.0f;
	ubo.Light2_Kd[0]=1.0f;
	ubo.Light2_Kd[1]=1.0f;
	ubo.Light2_Kd[2]=1.0f;
	ubo.Light2_Kd[3]=1.0f;

	// Copy uniform data to the Vulkan UBO buffer
	memcpy(uniformBufferPtr, &ubo, sizeof(ubo));

	vkAcquireNextImageKHR(Context.Device, Swapchain, UINT64_MAX, PresentCompleteSemaphores[Index], VK_NULL_HANDLE, &ImageIndex);

	vkWaitForFences(Context.Device, 1, &FrameFences[Index], VK_TRUE, UINT64_MAX);
	vkResetFences(Context.Device, 1, &FrameFences[Index]);

	// Start recording the commands
	vkBeginCommandBuffer(CommandBuffers[Index], &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	});

	ShadowUpdateCubemap(CommandBuffers[Index], ShadowBuf[0], ubo.Light0_Pos);
	ShadowUpdateCubemap(CommandBuffers[Index], ShadowBuf[1], ubo.Light1_Pos);
	ShadowUpdateCubemap(CommandBuffers[Index], ShadowBuf[2], ubo.Light2_Pos);

	// Start a render pass and clear the frame/depth buffer
	vkCmdBeginRenderPass(CommandBuffers[Index], &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=RenderPass,
		.framebuffer=FrameBuffers[ImageIndex],
		.clearValueCount=2,
		.pClearValues=(VkClearValue[])
		{
			{
				.color.float32[0]=0.0f,
				.color.float32[1]=0.0f,
				.color.float32[2]=0.0f,
				.color.float32[3]=1.0f
			},
			{
				.depthStencil.depth=1.0f,
				.depthStencil.stencil=0
			}
		},
		.renderArea.offset=(VkOffset2D)
		{
			.x=0,
			.y=0
		},
		.renderArea.extent=SwapchainExtent,
	}, VK_SUBPASS_CONTENTS_INLINE);

	// Bind the pipeline descriptor, this sets the pipeline states (blend, depth/stencil tests, etc)
	vkCmdBindPipeline(CommandBuffers[Index], VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline.Pipeline);

	vkCmdSetViewport(CommandBuffers[Index], 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)SwapchainExtent.width, (float)SwapchainExtent.height, 0.0f, 1.0f });
	vkCmdSetScissor(CommandBuffers[Index], 0, 1, &(VkRect2D) { { 0, 0 }, SwapchainExtent});

	// Draw the models
	for(i=0;i<NUM_MODELS;i++)
	{
		// Bind per-model destriptor set, this changes texture binding
		vkCmdBindDescriptorSets(CommandBuffers[Index], VK_PIPELINE_BIND_POINT_GRAPHICS, PipelineLayout, 0, 1, &DescriptorSet[i], 0, NULL);

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
	Font_Print(CommandBuffers[Index], 0.0f, 0.0f, "FPS: %0.1f", fps);

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

	FrameIndex++;
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

	// Create primary frame buffers, depth image, and renderpass
	CreateFramebuffers();

	// Create main render pipeline
	CreatePipeline();

	// Load models
	if(Load3DS(&Model[MODEL_HELLKNIGHT], "hellknight.3ds"))
		BuildMemoryBuffers(&Model[MODEL_HELLKNIGHT]);

	if(Load3DS(&Model[MODEL_PINKY], "pinky.3ds"))
		BuildMemoryBuffers(&Model[MODEL_PINKY]);

	if(Load3DS(&Model[MODEL_FATTY], "fatty.3ds"))
		BuildMemoryBuffers(&Model[MODEL_FATTY]);

	if(Load3DS(&Model[MODEL_LEVEL], "level.3ds"))
		BuildMemoryBuffers(&Model[MODEL_LEVEL]);

	// Load textures
	Image_Upload(&Context, &Textures[TEXTURE_HELLKNIGHT], "hellknight.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_HELLKNIGHT_NORMAL], "hellknight_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_PINKY], "pinky.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_PINKY_NORMAL], "pinky_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_FATTY], "fatty.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_FATTY_NORMAL], "fatty_n.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Context, &Textures[TEXTURE_LEVEL], "tile.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Context, &Textures[TEXTURE_LEVEL_NORMAL], "tile_b.tga", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALMAP);

	// Uniform data buffer and pointer mapping
	vkuCreateBuffer(&Context, &uniformBuffer, &uniformBufferMemory, sizeof(ubo),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkMapMemory(Context.Device, uniformBufferMemory, 0, VK_WHOLE_SIZE, 0, &uniformBufferPtr);

	InitShadowCubeMap(&ShadowBuf[0]);
	InitShadowCubeMap(&ShadowBuf[1]);
	InitShadowCubeMap(&ShadowBuf[2]);

	InitShadowFramebuffer();
	InitShadowPipeline();

	// Allocate and update descriptor sets for each model, each model has a different texture, so different sampler bindings are needed.
	for(uint32_t i=0;i<NUM_MODELS;i++)
	{
		vkAllocateDescriptorSets(Context.Device, &(VkDescriptorSetAllocateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext=NULL,
			.descriptorPool=DescriptorPool,
			.descriptorSetCount=1,
			.pSetLayouts=&DescriptorSetLayout
		}, &DescriptorSet[i]);

		vkUpdateDescriptorSets(Context.Device, 6, (VkWriteDescriptorSet[])
		{
			{
				.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.descriptorCount=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.dstBinding=0,
				.pBufferInfo=&(VkDescriptorBufferInfo)
				{
					.buffer=uniformBuffer,
					.offset=0,
					.range=sizeof(ubo),
				},
				.dstSet=DescriptorSet[i],
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
					.imageLayout=Textures[2*i+0].ImageLayout,
				},
				.dstSet=DescriptorSet[i],
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
					.imageLayout=Textures[2*i+1].ImageLayout,
				},
				.dstSet=DescriptorSet[i],
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
				.dstSet=DescriptorSet[i],
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
				.dstSet=DescriptorSet[i],
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
				.dstSet=DescriptorSet[i],
			},
		}, 0, VK_NULL_HANDLE);
	}
 
	return true;
}

void vkuCreateSwapchain(VkContext_t *Context, uint32_t Width, uint32_t Height, int VSync)
{
	uint32_t FormatCount, PresentModeCount;
	uint32_t DesiredNumberOfSwapchainImages;
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

	// Determine the number of images
	DesiredNumberOfSwapchainImages=SurfCaps.minImageCount+1;

	if((SurfCaps.maxImageCount>0)&&(DesiredNumberOfSwapchainImages>SurfCaps.maxImageCount))
		DesiredNumberOfSwapchainImages=SurfCaps.maxImageCount;

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
		.minImageCount=DesiredNumberOfSwapchainImages,
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

void Destroy(void)
{
	vkDeviceWaitIdle(Context.Device);

	DestroyDebugUtilsMessengerEXT(Instance, debugMessenger, VK_NULL_HANDLE);

	// Shadow stuff
	vkDestroyPipeline(Context.Device, ShadowPipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, ShadowPipelineLayout, VK_NULL_HANDLE);

	vkDestroyFramebuffer(Context.Device, ShadowFrameBuffer, VK_NULL_HANDLE);

	vkDestroyRenderPass(Context.Device, ShadowRenderPass, VK_NULL_HANDLE);

	// Frame buffer depth
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

	vkDestroyDescriptorSetLayout(Context.Device, DescriptorSetLayout, VK_NULL_HANDLE);
	vkDestroyDescriptorPool(Context.Device, DescriptorPool, VK_NULL_HANDLE);

	vkDestroyPipeline(Context.Device, Pipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, PipelineLayout, VK_NULL_HANDLE);

	vkDestroyImageView(Context.Device, Depth.View, VK_NULL_HANDLE);
	vkFreeMemory(Context.Device, Depth.DeviceMemory, VK_NULL_HANDLE);
	vkDestroyImage(Context.Device, Depth.Image, VK_NULL_HANDLE);

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
//----------------------------------------------------------

// Windows junk beyond here:

unsigned __int64 rdtsc(void)
{
	return __rdtsc();
}

unsigned __int64 GetFrequency(void)
{
	unsigned __int64 TimeStart, TimeStop, TimeFreq;
	unsigned __int64 StartTicks, StopTicks;
	volatile unsigned __int64 i;

	QueryPerformanceFrequency((LARGE_INTEGER *)&TimeFreq);

	QueryPerformanceCounter((LARGE_INTEGER *)&TimeStart);
	StartTicks=rdtsc();

	for(i=0;i<1000000;i++);

	StopTicks=rdtsc();
	QueryPerformanceCounter((LARGE_INTEGER *)&TimeStop);

	return (StopTicks-StartTicks)*TimeFreq/(TimeStop-TimeStart);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static POINT old;
	POINT pos, delta;

	switch(uMsg)
	{
	case WM_CREATE:
		break;

	case WM_CLOSE:
		PostQuitMessage(0);
		break;

	case WM_DESTROY:
		break;

	case WM_SIZE:
		Width=max(LOWORD(lParam), 2);
		Height=max(HIWORD(lParam), 2);

		if(Context.Device!=VK_NULL_HANDLE) // Windows quirk, WM_SIZE is signaled on window creation, *before* Vulkan get initalized
		{
			// Wait for the device to complete any pending work
			vkDeviceWaitIdle(Context.Device);

			// To resize a surface, we need to destroy and recreate anything that's tied to the surface.
			// This is basically just the swapchain and frame buffers

			vkDestroyImageView(Context.Device, Depth.View, VK_NULL_HANDLE);
			vkFreeMemory(Context.Device, Depth.DeviceMemory, VK_NULL_HANDLE);
			vkDestroyImage(Context.Device, Depth.Image, VK_NULL_HANDLE);

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
		break;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		SetCapture(hWnd);
		ShowCursor(FALSE);

		GetCursorPos(&pos);
		old.x=pos.x;
		old.y=pos.y;
		break;

	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		ShowCursor(TRUE);
		ReleaseCapture();
		break;

	case WM_MOUSEMOVE:
		GetCursorPos(&pos);

		if(!wParam)
		{
			old.x=pos.x;
			old.y=pos.y;
			break;
		}

		delta.x=pos.x-old.x;
		delta.y=old.y-pos.y;

		if(!delta.x&&!delta.y)
			break;

		SetCursorPos(old.x, old.y);

		switch(wParam)
		{
		case MK_LBUTTON:
			RotateX+=(delta.x*0.01f);
			RotateY-=(delta.y*0.01f);
			break;

		case MK_MBUTTON:
			PanX+=delta.x;
			PanY+=delta.y;
			break;

		case MK_RBUTTON:
			Zoom+=delta.y;
			break;
		}
		break;

	case WM_KEYDOWN:
		Key[wParam]=1;

		switch(wParam)
		{
		case VK_ESCAPE:
			PostQuitMessage(0);
			break;

		default:
			break;
		}
		break;

	case WM_KEYUP:
		Key[wParam]=0;
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int iCmdShow)
{
	WNDCLASS wc;
	MSG msg;
	RECT Rect;

	wc.style=CS_VREDRAW|CS_HREDRAW|CS_OWNDC;
	wc.lpfnWndProc=WndProc;
	wc.cbClsExtra=0;
	wc.cbWndExtra=0;
	wc.hInstance=hInstance;
	wc.hIcon=LoadIcon(NULL, IDI_WINLOGO);
	wc.hCursor=LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground=GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName=NULL;
	wc.lpszClassName=szAppName;

	RegisterClass(&wc);

	SetRect(&Rect, 0, 0, Width, Height);
	AdjustWindowRect(&Rect, WS_OVERLAPPEDWINDOW, FALSE);

	Context.hWnd=CreateWindow(szAppName, szAppName, WS_OVERLAPPEDWINDOW|WS_CLIPSIBLINGS, CW_USEDEFAULT, CW_USEDEFAULT, Rect.right-Rect.left, Rect.bottom-Rect.top, NULL, NULL, hInstance, NULL);

	ShowWindow(Context.hWnd, SW_SHOW);
	SetForegroundWindow(Context.hWnd);

	Frequency=GetFrequency();

	if(!CreateVulkanInstance(&Instance))
	{
		MessageBox(Context.hWnd, "Failed to create Vulkan instance", "Error", MB_OK);
		return -1;
	}

	if(!CreateVulkanContext(Instance, &Context))
	{
		MessageBox(Context.hWnd, "Failed to create Vulkan context", "Error", MB_OK);
		return -1;
	}

	vkuCreateSwapchain(&Context, Width, Height, VK_TRUE);

	if(!Init())
	{
		MessageBox(Context.hWnd, "Failed to init resources", "Error", MB_OK);
		return -1;
	}

	while(!Done)
	{
		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if(msg.message==WM_QUIT)
				Done=1;
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			StartTime=rdtsc();
			Render();
			EndTime=rdtsc();

			fTimeStep=(float)(EndTime-StartTime)/Frequency;
			fTime+=fTimeStep;
			avgfps+=1.0f/fTimeStep;

			if(Frames++>100)
			{
				fps=avgfps/Frames;
				avgfps=0.0f;
				Frames=0;
			}
		}
	}

	Destroy();
	DestroyVulkan(Instance, &Context);
	vkDestroyInstance(Instance, VK_NULL_HANDLE);
	DestroyWindow(Context.hWnd);

	return (int)msg.wParam;
}
