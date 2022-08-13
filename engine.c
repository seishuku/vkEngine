#include <Windows.h>
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

#ifndef FREE
#define FREE(p) { if(p) { free(p); p=NULL; } }
#endif

HWND hWnd=NULL;

char szAppName[]="Vulkan";

int Width=1280, Height=720;
 int Done=0, Key[256];

float RotateX=0.0f, RotateY=0.0f, PanX=0.0f, PanY=0.0f, Zoom=-100.0f;

unsigned __int64 Frequency, StartTime, EndTime;
float avgfps=0.0f, fps=0.0f, fTimeStep, fTime=0.0f;
int Frames=0;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

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

enum
{
	MAX_FRAME_COUNT=3
};

float ModelView[16], Projection[16];
float QuatX[4], QuatY[4], Quat[4];

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

VkInstance instance=VK_NULL_HANDLE;
VkDebugUtilsMessengerEXT debugMessenger;

VkDevice device=VK_NULL_HANDLE;
VkPhysicalDeviceMemoryProperties deviceMemProperties;

VkPhysicalDevice physicalDevice=VK_NULL_HANDLE;

VkSurfaceKHR surface=VK_NULL_HANDLE;

uint32_t queueFamilyIndex;
VkQueue queue=VK_NULL_HANDLE;

// Swapchain
VkSwapchainKHR swapchain=VK_NULL_HANDLE;

VkExtent2D swapchainExtent;
VkSurfaceFormatKHR surfaceFormat;
VkFormat depthFormat=VK_FORMAT_D32_SFLOAT;

uint32_t imageCount;

VkImage swapchainImage[MAX_FRAME_COUNT]={ VK_NULL_HANDLE, };
VkImageView swapchainImageView[MAX_FRAME_COUNT]={ VK_NULL_HANDLE, };

VkFramebuffer frameBuffers[MAX_FRAME_COUNT]={ VK_NULL_HANDLE, };

// Depth buffer handles
Image_t Depth;

VkRenderPass renderPass=VK_NULL_HANDLE;
VkPipelineLayout pipelineLayout=VK_NULL_HANDLE;
VkuPipeline_t pipeline;

VkDescriptorPool descriptorPool=VK_NULL_HANDLE;
VkDescriptorSet descriptorSet[4]={ VK_NULL_HANDLE, };
VkDescriptorSetLayout descriptorSetLayout=VK_NULL_HANDLE;

uint32_t frameIndex=0;

VkCommandPool commandPool=VK_NULL_HANDLE;
VkCommandBuffer commandBuffers[MAX_FRAME_COUNT]={ VK_NULL_HANDLE, };

VkFence frameFences[MAX_FRAME_COUNT]={ VK_NULL_HANDLE, };
VkSemaphore presentCompleteSemaphores[MAX_FRAME_COUNT]={ VK_NULL_HANDLE, };
VkSemaphore renderCompleteSemaphores[MAX_FRAME_COUNT]={ VK_NULL_HANDLE, };

// Shadow cubemap stuff
//
int32_t shadowCubeSize=1024;

VkFramebuffer shadowFrameBuffer;

// Frame buffer depth
Image_t shadowDepth;

VkuPipeline_t shadowPipeline;
VkPipelineLayout shadowPipelineLayout;
VkRenderPass shadowRenderPass;

// Shadow depth cubemap texture
Image_t shadowbuf[3];

struct
{
	float mvp[16];
	float Light_Pos[4];
} shadow_ubo;

void initShadowCubeMap(VkSampler *sampler, VkImage *image, VkDeviceMemory *memory, VkImageView *imageView)
{
	VkCommandBuffer layoutCmd;

	vkuCreateImageBuffer(device, &queueFamilyIndex, deviceMemProperties,
		VK_IMAGE_TYPE_2D, depthFormat, 1, 6, shadowCubeSize, shadowCubeSize, 1,
		image, memory,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);

	vkAllocateCommandBuffers(device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=commandPool,
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
		.image=*image,
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
		
	vkQueueSubmit(queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&layoutCmd,
	}, VK_NULL_HANDLE);

	vkQueueWaitIdle(queue);

	vkFreeCommandBuffers(device, commandPool, 1, &layoutCmd);

	vkCreateSampler(device, &(VkSamplerCreateInfo)
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
	}, VK_NULL_HANDLE, sampler);

	vkCreateImageView(device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType=VK_IMAGE_VIEW_TYPE_CUBE,
		.format=depthFormat,
		.components.r={ VK_COMPONENT_SWIZZLE_R },
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=6,
		.subresourceRange.levelCount=1,
		.image=*image,
	}, VK_NULL_HANDLE, imageView);
}

void initShadowFramebuffer(void)
{
	VkCommandBuffer copyCmd;
	VkFormat Format=VK_FORMAT_R32_SFLOAT;

	// Depth
	vkuCreateImageBuffer(device, &queueFamilyIndex, deviceMemProperties,
		VK_IMAGE_TYPE_2D, depthFormat, 1, 1, shadowCubeSize, shadowCubeSize, 1,
		&shadowDepth.Image, &shadowDepth.DeviceMemory,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0);

	vkAllocateCommandBuffers(device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=commandPool,
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
		.image=shadowDepth.Image,
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
		
	vkQueueSubmit(queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&copyCmd,
	}, VK_NULL_HANDLE);

	vkQueueWaitIdle(queue);

	vkFreeCommandBuffers(device, commandPool, 1, &copyCmd);

	vkCreateImageView(device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.viewType=VK_IMAGE_VIEW_TYPE_2D,
		.format=depthFormat,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=1,
		.subresourceRange.levelCount=1,
		.image=shadowDepth.Image,
	}, VK_NULL_HANDLE, &shadowDepth.View);

	vkCreateRenderPass(device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=1,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=depthFormat,
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
	}, 0, &shadowRenderPass);

	vkCreateFramebuffer(device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=shadowRenderPass,
		.attachmentCount=1,
		.pAttachments=(VkImageView[]) { shadowDepth.View },
		.width=shadowCubeSize,
		.height=shadowCubeSize,
		.layers=1,
	}, 0, &shadowFrameBuffer);
}

bool initShadowPipeline(void)
{
	vkCreatePipelineLayout(device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=sizeof(float)*(16+4),
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},		
	}, 0, &shadowPipelineLayout);

	vkuInitPipeline(&shadowPipeline, device, shadowPipelineLayout, shadowRenderPass);

	// Add in vertex shader
	if(!vkuPipeline_AddStage(&shadowPipeline, "distance_v.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	// Add in fragment shader
	if(!vkuPipeline_AddStage(&shadowPipeline, "distance_f.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	// Set states that are different than defaults
	shadowPipeline.CullMode=VK_CULL_MODE_BACK_BIT;
	shadowPipeline.DepthTest=VK_TRUE;

	// Add vertex binding and attrib parameters
	vkuPipeline_AddVertexBinding(&shadowPipeline, 0, sizeof(float)*14, VK_VERTEX_INPUT_RATE_VERTEX);
	vkuPipeline_AddVertexAttribute(&shadowPipeline, 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);

	// Assemble the pipeline
	if(!vkuAssemblePipeline(&shadowPipeline))
		return false;

	return true;
}

void shadowUpdateCubemap(VkCommandBuffer commandBuffer, Image_t shadow, float Light_Pos[4])
{
	int i, j, face;

	MatrixIdentity(Projection);
	InfPerspective(90.0f, 1.0f, 0.01f, 1, Projection);

	for(face=0;face<6;face++)
	{
		MatrixIdentity(ModelView);

		QuatX[0]=0.0f; QuatX[1]=0.0f; QuatX[2]=0.0f; QuatX[3]=1.0f;
		QuatY[0]=0.0f; QuatY[1]=0.0f; QuatY[2]=0.0f; QuatY[3]=1.0f;

		switch(face)
		{
			case 0:
				QuatAngle(90.0f, 0.0f, 1.0f, 0.0f, QuatX);
				QuatAngle(180.0f, 1.0f, 0.0f, 0.0f, QuatY);
				break;

			case 1:
				QuatAngle(-90.0f, 0.0f, 1.0f, 0.0f, QuatX);
				QuatAngle(180.0f, 1.0f, 0.0f, 0.0f, QuatY);
				break;

			case 2:
				QuatAngle(90.0f, 1.0f, 0.0f, 0.0f, QuatX);
				break;

			case 3:
				QuatAngle(-90.0f, 1.0f, 0.0f, 0.0f, QuatX); 
				break;

			case 4:
				QuatAngle(0.0f, 1.0f, 0.0f, 0.0f, QuatX);
				break;

			case 5:
				QuatAngle(180.0f, 0.0f, 1.0f, 0.0f, QuatX);
				break;
		}

		QuatMultiply(QuatX, QuatY, Quat);
		QuatMatrix(Quat, ModelView);

		MatrixTranslate(-Light_Pos[0], -Light_Pos[1], -Light_Pos[2], ModelView);

		MatrixMult(ModelView, Projection, shadow_ubo.mvp);

		shadow_ubo.Light_Pos[0]=Light_Pos[0];
		shadow_ubo.Light_Pos[1]=Light_Pos[1];
		shadow_ubo.Light_Pos[2]=Light_Pos[2];
		shadow_ubo.Light_Pos[3]=Light_Pos[3];

		vkCmdBeginRenderPass(commandBuffer, &(VkRenderPassBeginInfo)
		{
			.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.renderPass=shadowRenderPass,
			.framebuffer=shadowFrameBuffer,
			.clearValueCount=1,
			.pClearValues=(VkClearValue[]) { { .depthStencil.depth=1.0f, .depthStencil.stencil=0 } },
			.renderArea.offset=(VkOffset2D) { 0, 0 },
			.renderArea.extent=(VkExtent2D)	{ shadowCubeSize, shadowCubeSize },
		}, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdPushConstants(commandBuffer, shadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float)*(16+4), &shadow_ubo);

		// Bind the pipeline descriptor, this sets the pipeline states (blend, depth/stencil tests, etc)
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, shadowPipeline.Pipeline);

		vkCmdSetViewport(commandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)shadowCubeSize, (float)shadowCubeSize, 0.0f, 1.0f });
		vkCmdSetScissor(commandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { shadowCubeSize, shadowCubeSize } });

		// Draw the models
		for(i=0;i<NUM_MODELS;i++)
		{
			// Bind model data buffers and draw the triangles
			for(j=0;j<Model[i].NumMesh;j++)
			{
				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &Model[i].Mesh[j].Buffer, &(VkDeviceSize) { 0 });
				vkCmdBindIndexBuffer(commandBuffer, Model[i].Mesh[j].IndexBuffer, 0, VK_INDEX_TYPE_UINT16);
				vkCmdDrawIndexed(commandBuffer, Model[i].Mesh[j].NumFace*3, 1, 0, 0, 0);
			}
		}

		vkCmdEndRenderPass(commandBuffer);

		// Change frame buffer image layout to source transfer
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.image=shadowDepth.Image,
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
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.image=shadow.Image,
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
		vkCmdCopyImage(commandBuffer, shadowDepth.Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, shadow.Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &(VkImageCopy)
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
			.extent.width=shadowCubeSize,
			.extent.height=shadowCubeSize,
			.extent.depth=1,
		});

		// Change frame buffer image layout back to color arrachment
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.image=shadowDepth.Image,
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
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
			.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
			.image=shadow.Image,
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

int createFramebuffers(void)
{
	uint32_t i;

	vkCreateRenderPass(device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=2,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=surfaceFormat.format,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			},
			{
				.format=depthFormat,
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
	}, 0, &renderPass);

	vkuCreateImageBuffer(device, &queueFamilyIndex, deviceMemProperties,
		VK_IMAGE_TYPE_2D, depthFormat, 1, 1, Width, Height, 1,
		&Depth.Image, &Depth.DeviceMemory,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
		0);

	vkCreateImageView(device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext=NULL,
		.image=Depth.Image,
		.format=depthFormat,
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

	for(i=0;i<imageCount;i++)
	{
		vkCreateFramebuffer(device, &(VkFramebufferCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass=renderPass,
			.attachmentCount=2,
			.pAttachments=(VkImageView[]) { swapchainImageView[i], Depth.View },
			.width=swapchainExtent.width,
			.height=swapchainExtent.height,
			.layers=1,
		}, 0, &frameBuffers[i]);
	}

	return 1;
}

bool createPipeline(void)
{
	vkCreateDescriptorPool(device, &(VkDescriptorPoolCreateInfo)
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
	}, NULL, &descriptorPool);

	vkCreateDescriptorSetLayout(device, &(VkDescriptorSetLayoutCreateInfo)
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
	}, NULL, &descriptorSetLayout);

	vkCreatePipelineLayout(device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&descriptorSetLayout,
	}, 0, &pipelineLayout);

	vkuInitPipeline(&pipeline, device, pipelineLayout, renderPass);

	pipeline.DepthTest=VK_TRUE;
	pipeline.CullMode=VK_CULL_MODE_BACK_BIT;

	if(!vkuPipeline_AddStage(&pipeline, "lighting_v.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&pipeline, "lighting_f.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	vkuPipeline_AddVertexBinding(&pipeline, 0, sizeof(float)*14, VK_VERTEX_INPUT_RATE_VERTEX);

	vkuPipeline_AddVertexAttribute(&pipeline, 0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);
	vkuPipeline_AddVertexAttribute(&pipeline, 1, 0, VK_FORMAT_R32G32_SFLOAT, sizeof(float)*3);
	vkuPipeline_AddVertexAttribute(&pipeline, 2, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float)*5);
	vkuPipeline_AddVertexAttribute(&pipeline, 3, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float)*8);
	vkuPipeline_AddVertexAttribute(&pipeline, 4, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float)*11);

	if(!vkuAssemblePipeline(&pipeline))
		return false;

	return true;
}

void BuildMemoryBuffers(Model3DS_t *Model)
{
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	void *Data=NULL;
	int i, j;

	for(i=0;i<Model->NumMesh;i++)
	{
		// Vertex data on device memory
		vkuCreateBuffer(device, &queueFamilyIndex, deviceMemProperties,
			&Model->Mesh[i].Buffer, &Model->Mesh[i].BufferMemory,
			sizeof(float)*14*Model->Mesh[i].NumVertex,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		// Create staging buffer to transfer from host memory to device memory
		vkuCreateBuffer(device, &queueFamilyIndex, deviceMemProperties,
			&stagingBuffer, &stagingBufferMemory,
			sizeof(float)*14*Model->Mesh[i].NumVertex,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		vkMapMemory(device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &Data);

		for(j=0;j<Model->Mesh[i].NumVertex;j++)
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

		vkUnmapMemory(device, stagingBufferMemory);

		// Copy to device memory
		vkuCopyBuffer(device, queue, commandPool, stagingBuffer, Model->Mesh[i].Buffer, sizeof(float)*14*Model->Mesh[i].NumVertex);

		// Delete staging data
		vkFreeMemory(device, stagingBufferMemory, VK_NULL_HANDLE);
		vkDestroyBuffer(device, stagingBuffer, VK_NULL_HANDLE);

		// Index data
		vkuCreateBuffer(device, &queueFamilyIndex, deviceMemProperties,
			&Model->Mesh[i].IndexBuffer, &Model->Mesh[i].IndexBufferMemory,
			sizeof(uint16_t)*Model->Mesh[i].NumFace*3,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

		// Staging buffer
		vkuCreateBuffer(device, &queueFamilyIndex, deviceMemProperties,
			&stagingBuffer, &stagingBufferMemory,
			sizeof(uint16_t)*Model->Mesh[i].NumFace*3,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		vkMapMemory(device, stagingBufferMemory, 0, VK_WHOLE_SIZE, 0, &Data);

		for(j=0;j<Model->Mesh[i].NumFace;j++)
		{
			*((uint16_t *)Data)++=Model->Mesh[i].Face[3*j+0];
			*((uint16_t *)Data)++=Model->Mesh[i].Face[3*j+1];
			*((uint16_t *)Data)++=Model->Mesh[i].Face[3*j+2];
		}

		vkUnmapMemory(device, stagingBufferMemory);

		vkuCopyBuffer(device, queue, commandPool, stagingBuffer, Model->Mesh[i].IndexBuffer, sizeof(uint16_t)*Model->Mesh[i].NumFace*3);

		// Delete staging data
		vkFreeMemory(device, stagingBufferMemory, VK_NULL_HANDLE);
		vkDestroyBuffer(device, stagingBuffer, VK_NULL_HANDLE);
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

bool Init(void)
{
	uint32_t i;

	// Create primary frame buffers, depth image, and renderpass
	createFramebuffers();

	// Create main render pipeline
	createPipeline();

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
	Image_Upload(&Textures[TEXTURE_HELLKNIGHT], "hellknight.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Textures[TEXTURE_HELLKNIGHT_NORMAL], "hellknight_n.tga", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Textures[TEXTURE_PINKY], "pinky.tga", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Textures[TEXTURE_PINKY_NORMAL], "pinky_n.tga", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Textures[TEXTURE_FATTY], "fatty.tga", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Textures[TEXTURE_FATTY_NORMAL], "fatty_n.tga", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALIZE);
	Image_Upload(&Textures[TEXTURE_LEVEL], "tile.qoi", IMAGE_MIPMAP|IMAGE_BILINEAR);
	Image_Upload(&Textures[TEXTURE_LEVEL_NORMAL], "tile_b.tga", IMAGE_MIPMAP|IMAGE_BILINEAR|IMAGE_NORMALMAP);

	// Uniform data buffer and pointer mapping
	vkuCreateBuffer(device, &queueFamilyIndex, deviceMemProperties,
		&uniformBuffer, &uniformBufferMemory,
		sizeof(ubo),
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkMapMemory(device, uniformBufferMemory, 0, VK_WHOLE_SIZE, 0, &uniformBufferPtr);

	initShadowCubeMap(&shadowbuf[0].Sampler, &shadowbuf[0].Image, &shadowbuf[0].DeviceMemory, &shadowbuf[0].View);
	initShadowCubeMap(&shadowbuf[1].Sampler, &shadowbuf[1].Image, &shadowbuf[1].DeviceMemory, &shadowbuf[1].View);
	initShadowCubeMap(&shadowbuf[2].Sampler, &shadowbuf[2].Image, &shadowbuf[2].DeviceMemory, &shadowbuf[2].View);

	initShadowFramebuffer();
	initShadowPipeline();

	// Allocate and update descriptor sets for each model, each model has a different texture, so different sampler bindings are needed.
	for(i=0;i<NUM_MODELS;i++)
	{
		vkAllocateDescriptorSets(device, &(VkDescriptorSetAllocateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext=NULL,
			.descriptorPool=descriptorPool,
			.descriptorSetCount=1,
			.pSetLayouts=&descriptorSetLayout
		}, &descriptorSet[i]);

		vkUpdateDescriptorSets(device, 6, (VkWriteDescriptorSet[])
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
				.dstSet=descriptorSet[i],
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
				.dstSet=descriptorSet[i],
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
				.dstSet=descriptorSet[i],
			},
			{
				.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.descriptorCount=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.dstBinding=3,
				.pImageInfo=&(VkDescriptorImageInfo)
				{
					.imageView=shadowbuf[0].View,
					.sampler=shadowbuf[0].Sampler,
					.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				},
				.dstSet=descriptorSet[i],
			},
			{
				.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.descriptorCount=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.dstBinding=4,
				.pImageInfo=&(VkDescriptorImageInfo)
				{
					.imageView=shadowbuf[1].View,
					.sampler=shadowbuf[1].Sampler,
					.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				},
				.dstSet=descriptorSet[i],
			},
			{
				.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.descriptorCount=1,
				.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.dstBinding=5,
				.pImageInfo=&(VkDescriptorImageInfo)
				{
					.imageView=shadowbuf[2].View,
					.sampler=shadowbuf[2].Sampler,
					.imageLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				},
				.dstSet=descriptorSet[i],
			},
		}, 0, VK_NULL_HANDLE);
	}
 
	return true;
}

void Render(void)
{
	uint32_t index=frameIndex%imageCount;
	uint32_t imageIndex;
	int i, j;

	// Generate the projection matrix
	MatrixIdentity(Projection);
	InfPerspective(90.0f, (float)Width/Height, 0.01f, 1, Projection);

	// Set up the modelview matrix
	MatrixIdentity(ModelView);
	MatrixTranslate(PanX, PanY, Zoom, ModelView);

	QuatAngle(RotateX, 0.0f, 1.0f, 0.0f, QuatX);
	QuatAngle(RotateY, 1.0f, 0.0f, 0.0f, QuatY);
	QuatMultiply(QuatX, QuatY, Quat);
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

	vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, presentCompleteSemaphores[index], VK_NULL_HANDLE, &imageIndex);

	vkWaitForFences(device, 1, &frameFences[index], VK_TRUE, UINT64_MAX);
	vkResetFences(device, 1, &frameFences[index]);

	// Start recording the commands
	vkBeginCommandBuffer(commandBuffers[index], &(VkCommandBufferBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
		.flags=VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
	});

	shadowUpdateCubemap(commandBuffers[index], shadowbuf[0], ubo.Light0_Pos);
	shadowUpdateCubemap(commandBuffers[index], shadowbuf[1], ubo.Light1_Pos);
	shadowUpdateCubemap(commandBuffers[index], shadowbuf[2], ubo.Light2_Pos);

	// Start a render pass and clear the frame/depth buffer
	vkCmdBeginRenderPass(commandBuffers[index], &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=renderPass,
		.framebuffer=frameBuffers[imageIndex],
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
		.renderArea.extent=swapchainExtent,
	}, VK_SUBPASS_CONTENTS_INLINE);

	// Bind the pipeline descriptor, this sets the pipeline states (blend, depth/stencil tests, etc)
	vkCmdBindPipeline(commandBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.Pipeline);

	vkCmdSetViewport(commandBuffers[index], 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)swapchainExtent.width, (float)swapchainExtent.height, 0.0f, 1.0f });
	vkCmdSetScissor(commandBuffers[index], 0, 1, &(VkRect2D) { { 0, 0 }, swapchainExtent});

	// Draw the models
//	i=0;
	for(i=0;i<NUM_MODELS;i++)
	{
		// Bind per-model destriptor set, this changes texture binding
		vkCmdBindDescriptorSets(commandBuffers[index], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet[i], 0, NULL);

		// Bind model data buffers and draw the triangles
		for(j=0;j<Model[i].NumMesh;j++)
		{
			vkCmdBindVertexBuffers(commandBuffers[index], 0, 1, &Model[i].Mesh[j].Buffer, &(VkDeviceSize) { 0 });
			vkCmdBindIndexBuffer(commandBuffers[index], Model[i].Mesh[j].IndexBuffer, 0, VK_INDEX_TYPE_UINT16);
			vkCmdDrawIndexed(commandBuffers[index], Model[i].Mesh[j].NumFace*3, 1, 0, 0, 0);
		}
	}
	// ---

	// Should UI overlay stuff have it's own render pass?
	// Maybe even separate thread?
	Font_Print(commandBuffers[index], 0.0f, 0.0f, "FPS: %0.1f", fps);

	vkCmdEndRenderPass(commandBuffers[index]);

	vkEndCommandBuffer(commandBuffers[index]);

	// Sumit command queue
	vkQueueSubmit(queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.pWaitDstStageMask=&(VkPipelineStageFlags) { VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT },
		.waitSemaphoreCount=1,
		.pWaitSemaphores=&presentCompleteSemaphores[index],
		.signalSemaphoreCount=1,
		.pSignalSemaphores=&renderCompleteSemaphores[index],
		.commandBufferCount=1,
		.pCommandBuffers=&commandBuffers[index],
	}, frameFences[index]);

	// And present it to the screen
	vkQueuePresentKHR(queue, &(VkPresentInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.waitSemaphoreCount=1,
		.pWaitSemaphores=&renderCompleteSemaphores[index],
		.swapchainCount=1,
		.pSwapchains=&swapchain,
		.pImageIndices=&imageIndex,
	});

	frameIndex++;
}

void createSwapchain(uint32_t width, uint32_t height, int vsync)
{
	uint32_t i, formatCount, presentModeCount;
	uint32_t desiredNumberOfSwapchainImages;
	VkSurfaceFormatKHR *surfaceFormats=NULL;
	VkSurfaceCapabilitiesKHR surfCaps;
	VkPresentModeKHR swapchainPresentMode=VK_PRESENT_MODE_FIFO_KHR;
	VkPresentModeKHR *presentModes=NULL;
	VkSurfaceTransformFlagsKHR preTransform;
	VkCompositeAlphaFlagBitsKHR compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	VkCompositeAlphaFlagBitsKHR compositeAlphaFlags[]=
	{
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
	};
	VkImageUsageFlags imageUsage=0;

	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, VK_NULL_HANDLE);

	surfaceFormats=(VkSurfaceFormatKHR *)malloc(sizeof(VkSurfaceFormatKHR)*formatCount);

	if(surfaceFormats==NULL)
		return;

	vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, surfaceFormats);

	// If no format is specified, find a 32bit RGBA format
	if(surfaceFormats[0].format==VK_FORMAT_UNDEFINED)
		surfaceFormat.format=VK_FORMAT_R8G8B8A8_SNORM;
	// Otherwise the first format is the current surface format
	else
		surfaceFormat=surfaceFormats[0];

	FREE(surfaceFormats);

	// Get physical device surface properties and formats
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &surfCaps);

	// Get available present modes
	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, NULL);

	presentModes=(VkPresentModeKHR *)malloc(sizeof(VkPresentModeKHR)*presentModeCount);

	if(presentModes==NULL)
		return;

	vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes);

	// Set swapchain extents to the surface width/height
	swapchainExtent.width=width;
	swapchainExtent.height=height;

	// Select a present mode for the swapchain

	// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
	// This mode waits for the vertical blank ("v-sync")

	// If v-sync is not requested, try to find a mailbox mode
	// It's the lowest latency non-tearing present mode available
	if(!vsync)
	{
		for(i=0;i<presentModeCount;i++)
		{
			if(presentModes[i]==VK_PRESENT_MODE_MAILBOX_KHR)
			{
				swapchainPresentMode=VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}

			if((swapchainPresentMode!=VK_PRESENT_MODE_MAILBOX_KHR)&&(presentModes[i]==VK_PRESENT_MODE_IMMEDIATE_KHR))
				swapchainPresentMode=VK_PRESENT_MODE_IMMEDIATE_KHR;
		}
	}

	FREE(presentModes);

	// Determine the number of images
	desiredNumberOfSwapchainImages=surfCaps.minImageCount+1;

	if((surfCaps.maxImageCount>0)&&(desiredNumberOfSwapchainImages>surfCaps.maxImageCount))
		desiredNumberOfSwapchainImages=surfCaps.maxImageCount;

	// Find the transformation of the surface
	if(surfCaps.supportedTransforms&VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
		// We prefer a non-rotated transform
		preTransform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	else
		preTransform=surfCaps.currentTransform;

	// Find a supported composite alpha format (not all devices support alpha opaque)
	// Simply select the first composite alpha format available
	for(i=0;i<4;i++)
	{
		if(surfCaps.supportedCompositeAlpha&compositeAlphaFlags[i])
		{
			compositeAlpha=compositeAlphaFlags[i];
			break;
		}
	}

	// Enable transfer source on swap chain images if supported
	if(surfCaps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		imageUsage|=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	// Enable transfer destination on swap chain images if supported
	if(surfCaps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		imageUsage|=VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	vkCreateSwapchainKHR(device, &(VkSwapchainCreateInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext=VK_NULL_HANDLE,
		.surface=surface,
		.minImageCount=desiredNumberOfSwapchainImages,
		.imageFormat=surfaceFormat.format,
		.imageColorSpace=surfaceFormat.colorSpace,
		.imageExtent={ swapchainExtent.width, swapchainExtent.height },
		.imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT|imageUsage,
		.preTransform=preTransform,
		.imageArrayLayers=1,
		.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount=0,
		.pQueueFamilyIndices=VK_NULL_HANDLE,
		.presentMode=swapchainPresentMode,
		// Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
		.clipped=VK_TRUE,
		.compositeAlpha=compositeAlpha,
	}, VK_NULL_HANDLE, &swapchain);

	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, VK_NULL_HANDLE);

	// Get the swap chain images
	vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImage);

	// Get the swap chain buffers containing the image and imageview
	for(i=0;i<imageCount;i++)
	{
		vkCreateImageView(device, &(VkImageViewCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext=VK_NULL_HANDLE,
			.image=swapchainImage[i],
			.format=surfaceFormat.format,
			.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
			.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.subresourceRange.baseMipLevel=0,
			.subresourceRange.levelCount=1,
			.subresourceRange.baseArrayLayer=0,
			.subresourceRange.layerCount=1,
			.viewType=VK_IMAGE_VIEW_TYPE_2D,
			.flags=0,
		}, VK_NULL_HANDLE, &swapchainImageView[i]);
	}
}

int CreateVulkan(void)
{
	uint32_t physicalDeviceCount;
	VkPhysicalDevice *deviceHandles=NULL;
	VkQueueFamilyProperties *queueFamilyProperties=NULL;
	uint32_t i, j;
	VkPresentModeKHR presentMode=VK_PRESENT_MODE_FIFO_KHR;

	if(vkCreateInstance(&(VkInstanceCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
		.pApplicationInfo=&(VkApplicationInfo)
		{
			.sType=VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName=szAppName,
			.applicationVersion=VK_MAKE_VERSION(1, 0, 0),
			.pEngineName=szAppName,
			.engineVersion=VK_MAKE_VERSION(1, 0, 0),
			.apiVersion=VK_API_VERSION_1_0,
		},
		.enabledExtensionCount=3,
		.ppEnabledExtensionNames=(const char *const []) { VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME },
	}, 0, &instance)!=VK_SUCCESS)
		return 1;

	VkDebugUtilsMessengerCreateInfoEXT createInfo=
	{
		.sType=VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
		.messageSeverity=VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
		.messageType=VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT|VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
		.pfnUserCallback=debugCallback
	};

	if(CreateDebugUtilsMessengerEXT(instance, &createInfo, VK_NULL_HANDLE, &debugMessenger)!=VK_SUCCESS)
		return false;
			
	if(vkCreateWin32SurfaceKHR(instance, &(VkWin32SurfaceCreateInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
		.hinstance=GetModuleHandle(0),
		.hwnd=hWnd,
	}, VK_NULL_HANDLE, &surface)!=VK_SUCCESS)
		return 1;

	// Get the number of physical devices in the system
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, VK_NULL_HANDLE);

	// Allocate an array of handles
	deviceHandles=(VkPhysicalDevice *)malloc(sizeof(VkPhysicalDevice)*physicalDeviceCount);

	if(deviceHandles==NULL)
		return 1;

	// Get the handles to the devices
	vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, deviceHandles);

	for(i=0;i<physicalDeviceCount;i++)
	{
		uint32_t queueFamilyCount;

		// Get the number of queue families for this device
		vkGetPhysicalDeviceQueueFamilyProperties(deviceHandles[i], &queueFamilyCount, VK_NULL_HANDLE);

		// Allocate the memory for the structs 
		queueFamilyProperties=(VkQueueFamilyProperties *)malloc(sizeof(VkQueueFamilyProperties)*queueFamilyCount);

		if(queueFamilyProperties==NULL)
			return 1;

		// Get the queue family properties
		vkGetPhysicalDeviceQueueFamilyProperties(deviceHandles[i], &queueFamilyCount, queueFamilyProperties);

		// Find a queue index on a device that supports both graphics rendering and present support
		for(j=0;j<queueFamilyCount;j++)
		{
			VkBool32 supportsPresent=VK_FALSE;

			vkGetPhysicalDeviceSurfaceSupportKHR(deviceHandles[i], j, surface, &supportsPresent);

			if(supportsPresent&&(queueFamilyProperties[j].queueFlags&VK_QUEUE_GRAPHICS_BIT))
			{
				queueFamilyIndex=j;
				physicalDevice=deviceHandles[i];

				break;
			}
		}

		// Found device?
		if(physicalDevice)
			break;
	}

	// Free allocated handles
	FREE(queueFamilyProperties);
	FREE(deviceHandles);

	// Create the logical device from the physical device and queue index from above
	if(vkCreateDevice(physicalDevice, &(VkDeviceCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		.enabledExtensionCount=1,
		.ppEnabledExtensionNames=(const char *const []) { VK_KHR_SWAPCHAIN_EXTENSION_NAME },
		.queueCreateInfoCount=1,
		.pQueueCreateInfos=(VkDeviceQueueCreateInfo[])
		{
			{
				.sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex=queueFamilyIndex,
				.queueCount=1,
				.pQueuePriorities=(const float[]) { 1.0f }
			}
		}
	}, VK_NULL_HANDLE, &device)!=VK_SUCCESS)
		return 1;

	// Get device physical memory properties
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemProperties);

	// Get device queue
	vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);

	// Create swapchain
	createSwapchain(Width, Height, VK_TRUE);

	// Create command pool
	vkCreateCommandPool(device, &(VkCommandPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		.queueFamilyIndex=queueFamilyIndex,
	}, 0, &commandPool);

	// Allocate the command buffers we will be rendering into
	vkAllocateCommandBuffers(device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=commandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=imageCount,
	}, commandBuffers);

	for(i=0;i<imageCount;i++)
	{
		// Wait fence for command queue, to signal when we can submit commands again
		vkCreateFence(device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=VK_FENCE_CREATE_SIGNALED_BIT }, VK_NULL_HANDLE, &frameFences[i]);

		// Semaphore for image presentation, to signal when we can present again
		vkCreateSemaphore(device, &(VkSemaphoreCreateInfo) { .sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &presentCompleteSemaphores[i]);

		// Semaphore for render complete, to signal when we can render again
		vkCreateSemaphore(device, &(VkSemaphoreCreateInfo) { .sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO, .pNext=VK_NULL_HANDLE }, VK_NULL_HANDLE, &renderCompleteSemaphores[i]);
	}

	return 0;
}

void DestroyVulkan(void)
{
	uint32_t i, j;

	vkDeviceWaitIdle(device);

	// Shadow stuff
	vkDestroyPipeline(device, shadowPipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(device, shadowPipelineLayout, VK_NULL_HANDLE);

	vkDestroyFramebuffer(device, shadowFrameBuffer, VK_NULL_HANDLE);

	vkDestroyRenderPass(device, shadowRenderPass, VK_NULL_HANDLE);

	// Frame buffer depth
	vkDestroyImageView(device, shadowDepth.View, VK_NULL_HANDLE);
	vkFreeMemory(device, shadowDepth.DeviceMemory, VK_NULL_HANDLE);
	vkDestroyImage(device, shadowDepth.Image, VK_NULL_HANDLE);

	// Shadow depth cubemap texture
	vkDestroySampler(device, shadowbuf[0].Sampler, VK_NULL_HANDLE);
	vkDestroyImageView(device, shadowbuf[0].View, VK_NULL_HANDLE);
	vkFreeMemory(device, shadowbuf[0].DeviceMemory, VK_NULL_HANDLE);
	vkDestroyImage(device, shadowbuf[0].Image, VK_NULL_HANDLE);

	vkDestroySampler(device, shadowbuf[1].Sampler, VK_NULL_HANDLE);
	vkDestroyImageView(device, shadowbuf[1].View, VK_NULL_HANDLE);
	vkFreeMemory(device, shadowbuf[1].DeviceMemory, VK_NULL_HANDLE);
	vkDestroyImage(device, shadowbuf[1].Image, VK_NULL_HANDLE);

	vkDestroySampler(device, shadowbuf[2].Sampler, VK_NULL_HANDLE);
	vkDestroyImageView(device, shadowbuf[2].View, VK_NULL_HANDLE);
	vkFreeMemory(device, shadowbuf[2].DeviceMemory, VK_NULL_HANDLE);
	vkDestroyImage(device, shadowbuf[2].Image, VK_NULL_HANDLE);
	// ---

	Font_Destroy();

	for(i=0;i<NUM_TEXTURES;i++)
	{
		vkDestroySampler(device, Textures[i].Sampler, VK_NULL_HANDLE);
		vkDestroyImageView(device, Textures[i].View, VK_NULL_HANDLE);
		vkFreeMemory(device, Textures[i].DeviceMemory, VK_NULL_HANDLE);
		vkDestroyImage(device, Textures[i].Image, VK_NULL_HANDLE);
	}

	for(i=0;i<NUM_MODELS;i++)
	{
		for(j=0;j<(uint32_t)Model[i].NumMesh;j++)
		{
			vkFreeMemory(device, Model[i].Mesh[j].BufferMemory, VK_NULL_HANDLE);
			vkDestroyBuffer(device, Model[i].Mesh[j].Buffer, VK_NULL_HANDLE);

			vkFreeMemory(device, Model[i].Mesh[j].IndexBufferMemory, VK_NULL_HANDLE);
			vkDestroyBuffer(device, Model[i].Mesh[j].IndexBuffer, VK_NULL_HANDLE);
		}
	}

	vkFreeMemory(device, uniformBufferMemory, VK_NULL_HANDLE);
	vkDestroyBuffer(device, uniformBuffer, VK_NULL_HANDLE);

	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, VK_NULL_HANDLE);
	vkDestroyDescriptorPool(device, descriptorPool, VK_NULL_HANDLE);

	vkDestroyPipeline(device, pipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(device, pipelineLayout, VK_NULL_HANDLE);

	vkDestroyImageView(device, Depth.View, VK_NULL_HANDLE);
	vkFreeMemory(device, Depth.DeviceMemory, VK_NULL_HANDLE);
	vkDestroyImage(device, Depth.Image, VK_NULL_HANDLE);

	for(i=0;i<imageCount;i++)
	{
		vkDestroyFramebuffer(device, frameBuffers[i], VK_NULL_HANDLE);

		vkDestroyImageView(device, swapchainImageView[i], VK_NULL_HANDLE);

		vkDestroyFence(device, frameFences[i], VK_NULL_HANDLE);

		vkDestroySemaphore(device, presentCompleteSemaphores[i], VK_NULL_HANDLE);
		vkDestroySemaphore(device, renderCompleteSemaphores[i], VK_NULL_HANDLE);
	}

	vkDestroyRenderPass(device, renderPass, VK_NULL_HANDLE);

	vkDestroyCommandPool(device, commandPool, VK_NULL_HANDLE);

	vkDestroySwapchainKHR(device, swapchain, VK_NULL_HANDLE);

	vkDestroyDevice(device, VK_NULL_HANDLE);
	vkDestroySurfaceKHR(instance, surface, VK_NULL_HANDLE);

	DestroyDebugUtilsMessengerEXT(instance, debugMessenger, VK_NULL_HANDLE);

	vkDestroyInstance(instance, VK_NULL_HANDLE);
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

	hWnd=CreateWindow(szAppName, szAppName, WS_OVERLAPPEDWINDOW|WS_CLIPSIBLINGS, CW_USEDEFAULT, CW_USEDEFAULT, Rect.right-Rect.left, Rect.bottom-Rect.top, NULL, NULL, hInstance, NULL);

	ShowWindow(hWnd, SW_SHOW);
	SetForegroundWindow(hWnd);

	Frequency=GetFrequency();

	if(CreateVulkan())
	{
		MessageBox(hWnd, "Vulkan init failed.", "Error", MB_OK);
		return -1;
	}

	Init();

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

	DestroyVulkan();

	DestroyWindow(hWnd);

	return (int)msg.wParam;
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

			if(device!=VK_NULL_HANDLE) // Windows quirk, WM_SIZE is signaled on window creation, *before* Vulkan get initalized
			{
				uint32_t i;

				// Wait for the device to complete any pending work
				vkDeviceWaitIdle(device);

				// To resize a surface, we need to destroy and recreate anything that's tied to the surface.
				// This is basically just the swapchain and frame buffers

				for(i=0;i<imageCount;i++)
				{
					// Frame buffers
					vkDestroyFramebuffer(device, frameBuffers[i], VK_NULL_HANDLE);

					// Swapchain image views
					vkDestroyImageView(device, swapchainImageView[i], VK_NULL_HANDLE);
				}

				// Depth buffer objects
				vkDestroyImageView(device, Depth.View, VK_NULL_HANDLE);
				vkFreeMemory(device, Depth.DeviceMemory, VK_NULL_HANDLE);
				vkDestroyImage(device, Depth.Image, VK_NULL_HANDLE);

				// And finally the swapchain
				vkDestroySwapchainKHR(device, swapchain, VK_NULL_HANDLE);

				// Recreate the swapchain and frame buffers
				createSwapchain(Width, Height, VK_TRUE);
				createFramebuffers();

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
					RotateX+=delta.x;
					RotateY-=delta.y;
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
