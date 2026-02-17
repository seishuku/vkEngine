#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../system/system.h"
#include "../utils/id.h"
#include "../utils/list.h"
#include "lights.h"

extern VkuContext_t vkContext;

uint32_t Lights_Add(Lights_t *lights, vec3 position, float radius, vec4 Kd)
{
	if(lights==NULL)
		return UINT32_MAX;

	// Pull the next ID from the global ID count
	uint32_t ID=ID_Generate(lights->baseID);

	Light_t light;

	light.ID=ID;
	light.position=position;
	light.radius=1.0f/radius;
	light.Kd=Kd;

	light.spotDirection=Vec4b(0.0f);
	light.spotOuterCone=0.0f;
	light.spotInnerCone=0.0f;
	light.spotExponent=0.0f;

	List_Add(&lights->lights, (void *)&light);

	return ID;
}

void Lights_Del(Lights_t *lights, uint32_t ID)
{
	if(lights==NULL&&ID!=UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&lights->lights);i++)
	{
		Light_t *light=List_GetPointer(&lights->lights, i);

		if(light->ID==ID)
		{
			List_Del(&lights->lights, i);
			break;
		}
	}
}

void Lights_Update(Lights_t *lights, uint32_t ID, vec3 position, float radius, vec4 Kd)
{
	if(lights==NULL&&ID!=UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&lights->lights);i++)
	{
		Light_t *light=List_GetPointer(&lights->lights, i);

		if(light->ID==ID)
		{
			light->position=position;
			light->radius=1.0f/radius;
			light->Kd=Kd;

			return;
		}
	}
}

void Lights_UpdatePosition(Lights_t *lights, uint32_t ID, vec3 position)
{
	if(lights==NULL&&ID!=UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&lights->lights);i++)
	{
		Light_t *light=List_GetPointer(&lights->lights, i);

		if(light->ID==ID)
		{
			light->position=position;
			return;
		}
	}
}

void Lights_UpdateRadius(Lights_t *lights, uint32_t ID, float radius)
{
	if(lights==NULL&&ID!=UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&lights->lights);i++)
	{
		Light_t *light=List_GetPointer(&lights->lights, i);

		if(light->ID==ID)
		{
			light->radius=1.0f/radius;
			return;
		}
	}
}

void Lights_UpdateKd(Lights_t *lights, uint32_t ID, vec4 Kd)
{
	if(lights==NULL&&ID!=UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&lights->lights);i++)
	{
		Light_t *light=List_GetPointer(&lights->lights, i);

		if(light->ID==ID)
		{
			light->Kd=Kd;
			return;
		}
	}
}

void Lights_UpdateSpotlight(Lights_t *lights, uint32_t ID, vec3 direction, float outerCone, float innerCone, float exponent)
{
	if(lights==NULL&&ID!=UINT32_MAX)
		return;

	for(uint32_t i=0;i<List_GetCount(&lights->lights);i++)
	{
		Light_t *light=List_GetPointer(&lights->lights, i);

		if(light->ID==ID)
		{
			light->spotDirection=Vec4_Vec3(direction, 0.0f);
			light->spotOuterCone=outerCone;
			light->spotInnerCone=innerCone;
			light->spotExponent=exponent;
			return;
		}
	}
}

void Lights_UpdateSSBO(Lights_t *lights)
{
	static size_t oldSize=0;

	// If over allocated buffer size changed from last time, delete and recreate the buffer.
	if(oldSize!=lights->lights.bufSize)
	{
		oldSize=lights->lights.bufSize;

		if(lights->buffer.buffer&&lights->buffer.memory)
		{
			vkDeviceWaitIdle(vkContext.device);

			vkDestroyFramebuffer(vkContext.device, lights->shadowFrameBuffer, VK_NULL_HANDLE);
			vkuDestroyImageBuffer(&vkContext, &lights->shadowDepth);

			vkuDestroyBuffer(&vkContext, &lights->buffer);
			vkuCreateHostBuffer(&vkContext, &lights->buffer, (uint32_t)lights->lights.bufSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

//			InitShadowCubeMap((uint32_t)(lights->lights.bufSize/lights->lights.stride));
		}
	}

	// Update buffer
	if(lights->buffer.memory)
	{
		if(lights->buffer.memory->mappedPointer!=NULL)
			memcpy(lights->buffer.memory->mappedPointer, lights->lights.buffer, lights->lights.bufSize);
	}
}

bool Lights_Init(Lights_t *lights)
{
	ID_Init(lights->baseID);

	List_Init(&lights->lights, sizeof(Light_t), 10, NULL);

	vkuCreateHostBuffer(&vkContext, &lights->buffer, (uint32_t)lights->lights.bufSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

	return true;
}

void Lights_Destroy(Lights_t *lights)
{
	// Delete storage buffer and free memory
	vkuDestroyBuffer(&vkContext, &lights->buffer);

	List_Destroy(&lights->lights);
}

#if 0
// TODO: Old code for shadow map generation, need to update this still.
void InitShadowCubeMap(Image_t *Image, uint32_t NumMaps)
{
	VkCommandBuffer CommandBuffer;
	VkFence Fence;

	vkuCreateImageBuffer(&Context, Image,
		VK_IMAGE_TYPE_2D, ShadowColorFormat, 1, 6*NumMaps, ShadowCubeSize, ShadowCubeSize, 1,
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
		.subresourceRange.layerCount=6*NumMaps,
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
		.viewType=VK_IMAGE_VIEW_TYPE_CUBE_ARRAY,
		.format=ShadowColorFormat,
		.components.r={ VK_COMPONENT_SWIZZLE_R },
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=6*NumMaps,
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

void ShadowUpdateCubemap(VkCommandBuffer CommandBuffer, Image_t Shadow, uint32_t i, vec4 Pos)
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
			.subresourceRange.baseArrayLayer=6*i+face,
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
			.dstSubresource.baseArrayLayer=6*i+face,
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
			.subresourceRange.baseArrayLayer=6*i+face,
			.subresourceRange.levelCount=1,
			.subresourceRange.layerCount=1,
			.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT,
			.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
			.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		});
	}
}
#endif
