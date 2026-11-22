#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <float.h>
#include <math.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../model/bmodel.h"
#include "../utils/id.h"
#include "../utils/list.h"
#include "../utils/animation.h"
#include "../utils/entitylist.h"
#include "../utils/pipeline.h"
#include "../assetmanager.h"
#include "../perframe.h"
#include "lights.h"

extern VkuContext_t vkContext;
extern AnimationSystem_t animationSystem;

static const VkFormat shadowDepthFormat=VK_FORMAT_D32_SFLOAT;

#define LIGHTS_HEADER_SIZE 16

static const float faceAxes[6][6]=
{
	{  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,  0.0f },	// +X
	{ -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,  0.0f },	// -X
	{  0.0f,  1.0f,  0.0f,  0.0f, 0.0f, -1.0f },	// +Y
	{  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,  1.0f },	// -Y
	{  0.0f,  0.0f,  1.0f,  0.0f, 1.0f,  0.0f },	// +Z
	{  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,  0.0f },	// -Z
};

static float InverseRadius(const float radius)
{
	return 1.0f/fmaxf(radius, FLT_EPSILON);
}

static size_t FindLightIndex(Lights_t *lights, const uint32_t ID)
{
	if(lights!=NULL&&ID!=UINT32_MAX)
	{
		for(size_t i=0;i<List_GetCount(&lights->lights);i++)
		{
			if(((Light_t *)List_GetPointer(&lights->lights, i))->ID==ID)
				return i;
		}
	}

	return SIZE_MAX;
}

static Light_t *FindLight(Lights_t *lights, const uint32_t ID)
{
	const size_t index=FindLightIndex(lights, ID);

	return (index==SIZE_MAX)?NULL:(Light_t *)List_GetPointer(&lights->lights, index);
}

uint32_t Lights_Add(Lights_t *lights, vec3 position, float radius, vec4 Kd)
{
	if(lights==NULL)
		return UINT32_MAX;

	const uint32_t ID=ID_Generate(lights->baseID);

	if(ID==UINT32_MAX)
		return UINT32_MAX;

	Light_t light;

	memset(&light, 0, sizeof(Light_t));

	light.ID=ID;
	light.shadowSlot=-1;
	light.position=position;
	light.radius=InverseRadius(radius);
	light.Kd=Kd;

	if(!List_Add(&lights->lights, &light))
	{
		ID_Remove(lights->baseID, ID);
		return UINT32_MAX;
	}

	return ID;
}

void Lights_Del(Lights_t *lights, uint32_t ID)
{
	const size_t index=FindLightIndex(lights, ID);

	if(index==SIZE_MAX)
		return;

	const Light_t *light=List_GetPointer(&lights->lights, index);

	if(light->shadowSlot>=0)
		lights->shadowOwner[light->shadowSlot]=UINT32_MAX;

	List_Del(&lights->lights, index);
	ID_Remove(lights->baseID, ID);
}

void Lights_Update(Lights_t *lights, uint32_t ID, vec3 position, float radius, vec4 Kd)
{
	Light_t *light=FindLight(lights, ID);

	if(light)
	{
		light->position=position;
		light->radius=InverseRadius(radius);
		light->Kd=Kd;
	}
}

void Lights_UpdatePosition(Lights_t *lights, uint32_t ID, vec3 position)
{
	Light_t *light=FindLight(lights, ID);

	if(light)
		light->position=position;
}

void Lights_UpdateRadius(Lights_t *lights, uint32_t ID, float radius)
{
	Light_t *light=FindLight(lights, ID);

	if(light)
		light->radius=InverseRadius(radius);
}

void Lights_UpdateKd(Lights_t *lights, uint32_t ID, vec4 Kd)
{
	Light_t *light=FindLight(lights, ID);

	if(light)
		light->Kd=Kd;
}

void Lights_UpdateSpotlight(Lights_t *lights, uint32_t ID, vec3 direction, float outerCone, float innerCone, float exponent)
{
	Light_t *light=FindLight(lights, ID);

	if(light)
	{
		light->spotDirection=Vec4_Vec3(direction, 0.0f);
		light->spotOuterCone=outerCone;
		light->spotInnerCone=innerCone;
		light->spotExponent=exponent;
	}
}

bool Lights_SetShadow(Lights_t *lights, uint32_t ID, bool enable)
{
	Light_t *light=FindLight(lights, ID);

	if(light==NULL)
		return false;

	if(enable)
	{
		if(light->shadowSlot>=0)
			return true;

		for(uint32_t i=0;i<lights->numShadows;i++)
		{
			if(lights->shadowOwner[i]==UINT32_MAX)
			{
				lights->shadowOwner[i]=ID;
				light->shadowSlot=(int32_t)i;

				return true;
			}
		}

		// Every slot is taken
		return false;
	}

	if(light->shadowSlot>=0)
	{
		lights->shadowOwner[light->shadowSlot]=UINT32_MAX;
		light->shadowSlot=-1;
	}

	return true;
}

static matrix ShadowProjection(const float zNear, const float zFar)
{
	matrix projection=MatrixIdentity();

	projection.y.y=-1.0f;
	projection.z.z=-zFar/(zFar-zNear);
	projection.z.w=-1.0f;
	projection.w.z=-(zFar*zNear)/(zFar-zNear);
	projection.w.w=0.0f;

	return projection;
}

static bool EnsureBuffer(VkuBuffer_t *buffer, uint32_t *currentSize, uint32_t size)
{
	if(size<=*currentSize)
		return true;

	if(size<*currentSize*2)
		size=*currentSize*2;

	vkuDestroyBuffer(&vkContext, buffer);
	*currentSize=0;

	if(!vkuCreateHostBuffer(&vkContext, buffer, size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT))
	{
		// Don't keep half of one
		vkuDestroyBuffer(&vkContext, buffer);
		return false;
	}

	*currentSize=size;

	return true;
}

void Lights_UpdateSSBO(Lights_t *lights, uint32_t frameIndex)
{
	if(lights==NULL||frameIndex>=FRAMES_IN_FLIGHT)
		return;

	const uint32_t count=(uint32_t)List_GetCount(&lights->lights);

	if(!EnsureBuffer(&lights->buffer[frameIndex], &lights->bufferSize[frameIndex], LIGHTS_HEADER_SIZE+count*(uint32_t)sizeof(Light_t)))
		return;

	uint8_t *dst=(uint8_t *)lights->buffer[frameIndex].memory->mappedPointer;

	if(dst!=NULL)
	{
		memset(dst, 0, LIGHTS_HEADER_SIZE);
		memcpy(dst, &count, sizeof(count));

		memcpy(dst+LIGHTS_HEADER_SIZE, List_GetBufferPointer(&lights->lights), count*sizeof(Light_t));
	}

	if(lights->numShadows)
	{
		for(size_t i=0;i<List_GetCount(&lights->lights);i++)
		{
			const Light_t *light=List_GetPointer(&lights->lights, i);

			if(light->shadowSlot<0)
				continue;

			const float zFar=1.0f/light->radius;
			const float zNear=fminf(fmaxf(zFar*0.01f, 0.01f), zFar*0.5f);
			const matrix projection=ShadowProjection(zNear, zFar);

			for(uint32_t face=0;face<6;face++)
			{
				const float *axes=faceAxes[face];
				const matrix view=MatrixLookAt(light->position, Vec3_Addv(light->position, Vec3(axes[0], axes[1], axes[2])), Vec3(axes[3], axes[4], axes[5]));

				lights->shadowMatrix[light->shadowSlot*6+face]=MatrixMult(view, projection);
			}
		}

		if(lights->shadowBuffer[frameIndex].memory&&lights->shadowBuffer[frameIndex].memory->mappedPointer)
			memcpy(lights->shadowBuffer[frameIndex].memory->mappedPointer, lights->shadowMatrix, sizeof(matrix)*lights->numShadows*6);
	}
}

static bool CreateShadowMaps(Lights_t *lights, uint32_t numShadows, uint32_t shadowSize)
{
	const uint32_t numLayers=numShadows*6;

	lights->numShadows=numShadows;
	lights->shadowSize=shadowSize;

	lights->shadowOwner=(uint32_t *)Zone_Malloc(zone, sizeof(uint32_t)*numShadows);
	lights->shadowMatrix=(matrix *)Zone_Malloc(zone, sizeof(matrix)*numLayers);
	lights->shadowView=(VkImageView *)Zone_Malloc(zone, sizeof(VkImageView)*numLayers);
	lights->shadowFrameBuffer=(VkFramebuffer *)Zone_Malloc(zone, sizeof(VkFramebuffer)*numLayers);

	if(!lights->shadowOwner||!lights->shadowMatrix||!lights->shadowView||!lights->shadowFrameBuffer)
		return false;

	for(uint32_t i=0;i<numShadows;i++)
		lights->shadowOwner[i]=UINT32_MAX;

	for(uint32_t i=0;i<numLayers;i++)
		lights->shadowMatrix[i]=MatrixIdentity();

	memset(lights->shadowView, 0, sizeof(VkImageView)*numLayers);
	memset(lights->shadowFrameBuffer, 0, sizeof(VkFramebuffer)*numLayers);

	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
	{
		uint32_t size=0;

		if(!EnsureBuffer(&lights->shadowBuffer[i], &size, sizeof(matrix)*numLayers))
			return false;
	}

	if(!vkuCreateImageBuffer(&vkContext, &lights->shadowDepth, VK_IMAGE_TYPE_2D, shadowDepthFormat, 1, numLayers, shadowSize, shadowSize, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0))
		return false;

	vkCreateImageView(vkContext.device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image=lights->shadowDepth.image,
		.viewType=VK_IMAGE_VIEW_TYPE_2D_ARRAY,
		.format=shadowDepthFormat,
		.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.levelCount=1,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=numLayers,
	}, NULL, &lights->shadowDepth.imageView);

	VkCommandBuffer commandBuffer=vkuOneShotCommandBufferBegin(&vkContext);
	for(uint32_t i=0;i<numLayers;i++)
		vkuTransitionLayout(commandBuffer, lights->shadowDepth.image, 1, 0, 1, i, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuOneShotCommandBufferEnd(&vkContext, commandBuffer);

	// Need compare ops
	vkCreateSampler(vkContext.device, &(VkSamplerCreateInfo)
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
	}, VK_NULL_HANDLE, &lights->shadowDepth.sampler);

	// Same depth only render pass the cascaded shadows use, a face is cleared, rendered into, then left readable
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
	}, 0, &lights->shadowRenderPass);

	// A view and framebuffer for every face, to render into one at a time
	for(uint32_t i=0;i<numLayers;i++)
	{
		vkCreateImageView(vkContext.device, &(VkImageViewCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image=lights->shadowDepth.image,
			.viewType=VK_IMAGE_VIEW_TYPE_2D,
			.format=shadowDepthFormat,
			.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
			.subresourceRange.aspectMask=VK_IMAGE_ASPECT_DEPTH_BIT,
			.subresourceRange.baseMipLevel=0,
			.subresourceRange.levelCount=1,
			.subresourceRange.baseArrayLayer=i,
			.subresourceRange.layerCount=1,
		}, NULL, &lights->shadowView[i]);

		vkCreateFramebuffer(vkContext.device, &(VkFramebufferCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass=lights->shadowRenderPass,
			.attachmentCount=1,
			.pAttachments=(VkImageView[]){ lights->shadowView[i] },
			.width=shadowSize,
			.height=shadowSize,
			.layers=1,
		}, 0, &lights->shadowFrameBuffer[i]);
	}

	return CreatePipeline(&vkContext, &lights->shadowPipeline, lights->shadowRenderPass, "assets/pipelines/shadow.pipeline");
}

bool Lights_Init(Lights_t *lights, uint32_t numShadows, uint32_t shadowSize)
{
	if(lights==NULL)
		return false;

	memset(lights, 0, sizeof(Lights_t));

	ID_Init(lights->baseID);

	if(!List_Init(&lights->lights, sizeof(Light_t), 16, NULL))
		return false;

	// Room for a few lights to start, the buffers grow when they're needed
	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
	{
		if(!EnsureBuffer(&lights->buffer[i], &lights->bufferSize[i], LIGHTS_HEADER_SIZE+16*sizeof(Light_t)))
		{
			Lights_Destroy(lights);
			return false;
		}
	}

	if(numShadows&&!CreateShadowMaps(lights, numShadows, shadowSize))
	{
		Lights_Destroy(lights);
		return false;
	}

	return true;
}

void Lights_Destroy(Lights_t *lights)
{
	if(lights==NULL)
		return;

	vkDeviceWaitIdle(vkContext.device);

	for(uint32_t i=0;i<FRAMES_IN_FLIGHT;i++)
	{
		vkuDestroyBuffer(&vkContext, &lights->buffer[i]);
		vkuDestroyBuffer(&vkContext, &lights->shadowBuffer[i]);
	}

	if(lights->numShadows)
	{
		for(uint32_t i=0;i<lights->numShadows*6;i++)
		{
			if(lights->shadowView)
				vkDestroyImageView(vkContext.device, lights->shadowView[i], VK_NULL_HANDLE);

			if(lights->shadowFrameBuffer)
				vkDestroyFramebuffer(vkContext.device, lights->shadowFrameBuffer[i], VK_NULL_HANDLE);
		}

		vkuDestroyImageBuffer(&vkContext, &lights->shadowDepth);

		DestroyPipeline(&vkContext, &lights->shadowPipeline);
		vkDestroyRenderPass(vkContext.device, lights->shadowRenderPass, VK_NULL_HANDLE);

		Zone_Free(zone, lights->shadowOwner);
		Zone_Free(zone, lights->shadowMatrix);
		Zone_Free(zone, lights->shadowView);
		Zone_Free(zone, lights->shadowFrameBuffer);
	}

	if(lights->lights.buffer)
		List_Destroy(&lights->lights);

	memset(lights, 0, sizeof(Lights_t));
}

void Lights_UpdateShadowMaps(Lights_t *lights, VkCommandBuffer commandBuffer, const EntityList_t *entityList, uint32_t frameIndex, VkDescriptorPool descriptorPool)
{
	if(lights==NULL||lights->numShadows==0||frameIndex>=FRAMES_IN_FLIGHT)
		return;

	bool descriptorReady=false;

	for(size_t i=0;i<List_GetCount(&lights->lights);i++)
	{
		const Light_t *light=List_GetPointer(&lights->lights, i);

		if(light->shadowSlot<0)
			continue;

		// Skinned models need the bone palette, it's the same for every face so it's set up once, and only if something casts a shadow
		if(!descriptorReady)
		{
			vkuDescriptorSet_UpdateBindingBufferInfo(&lights->shadowPipeline.descriptorSet, 0, animationSystem.perFrame[frameIndex].paletteBuffer.buffer, 0, VK_WHOLE_SIZE);
			vkuAllocateUpdateDescriptorSet(&lights->shadowPipeline.descriptorSet, descriptorPool);

			descriptorReady=true;
		}

		for(uint32_t face=0;face<6;face++)
		{
			const uint32_t layer=light->shadowSlot*6+face;

			vkCmdBeginRenderPass(commandBuffer, &(VkRenderPassBeginInfo)
			{
				.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				.renderPass=lights->shadowRenderPass,
				.framebuffer=lights->shadowFrameBuffer[layer],
				.clearValueCount=1,
				.pClearValues=(VkClearValue[]){ {{{ 1.0f, 0 }}} },
				.renderArea.offset=(VkOffset2D){ 0, 0 },
				.renderArea.extent=(VkExtent2D){ lights->shadowSize, lights->shadowSize },
			}, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lights->shadowPipeline.pipeline.pipeline);

			vkCmdSetViewport(commandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)lights->shadowSize, (float)lights->shadowSize, 0.0f, 1.0f });
			vkCmdSetScissor(commandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { lights->shadowSize, lights->shadowSize } });

			vkCmdPushConstants(commandBuffer, lights->shadowPipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(matrix), &lights->shadowMatrix[layer]);

			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lights->shadowPipeline.pipelineLayout, 0, 1, &lights->shadowPipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

			// Draw entity batches
			for(uint32_t b=0;b<entityList->batchCount;b++)
			{
				const EntityBatch_t *batch=&entityList->batches[b];

				if(batch->noRender)
					continue;

				const BModel_t *model=&AssetManager_GetAsset(assets, batch->modelID)->model;

				vkCmdBindVertexBuffers(commandBuffer, 0, 1, &model->vertexBuffer.buffer, &(VkDeviceSize){0});

				// Offset the instance buffer binding to the start of this batch
				VkDeviceSize instanceOffset=sizeof(EntityInstance_t)*batch->instanceOffset;
				vkCmdBindVertexBuffers(commandBuffer, 1, 1, &entityList->perFrame[frameIndex].instanceBuffer.buffer, &instanceOffset);

				for(uint32_t m=0;m<model->numMesh;m++)
				{
					vkCmdBindIndexBuffer(commandBuffer, model->mesh[m].indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
					vkCmdDrawIndexed(commandBuffer, model->mesh[m].numFace*3, batch->instanceCount, 0, 0, 0);
				}
			}

			vkCmdEndRenderPass(commandBuffer);
		}
	}
}
