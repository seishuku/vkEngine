#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../utils/pipeline.h"
#include "../ui/ui.h"
#include "../assetmanager.h"
#include "../perframe.h"
#include "shadow.h"

extern VkuContext_t vkContext;
extern VkuImage_t depthImage[2];
extern VkRenderPass renderPass;

extern UI_t UI;
extern uint32_t colorShiftID;

extern float fTime;

// Volume rendering vulkan stuff
Pipeline_t volumePipeline;
//////

// Nebula volume texture generation
static Pipeline_t computePipeline;

static VkCommandPool computeCommandPool;
static VkCommandBuffer computeCommand;
static VkDescriptorPool computeDescriptorPool;

static VkFence fence=VK_NULL_HANDLE;

VkBool32 InitNebulaVolume(VkuImage_t *image)
{
	image->width=512;
	image->height=512;
	image->depth=512; // Slight abuse of image struct, depth is supposed to be color depth, not image depth.

	if(!vkuCreateImageBuffer(&vkContext, image,
							 VK_IMAGE_TYPE_3D, VK_FORMAT_R8_UNORM, 1, 1, image->width, image->height, image->depth,
							 VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
							 VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_STORAGE_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
							 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0))
		return VK_FALSE;

	// Create texture sampler object
	vkCreateSampler(vkContext.device, &(VkSamplerCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter=VK_FILTER_LINEAR,
		.minFilter=VK_FILTER_LINEAR,
		.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeV=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		.mipLodBias=0.0f,
		.compareOp=VK_COMPARE_OP_NEVER,
		.minLod=0.0f,
		.maxLod=VK_LOD_CLAMP_NONE,
		.maxAnisotropy=1.0f,
		.anisotropyEnable=VK_FALSE,
		.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	}, VK_NULL_HANDLE, &image->sampler);

	// Create texture image view object
	vkCreateImageView(vkContext.device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image=image->image,
		.viewType=VK_IMAGE_VIEW_TYPE_3D,
		.format=VK_FORMAT_R8_UNORM,
		.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
		.subresourceRange={ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
	}, VK_NULL_HANDLE, &image->imageView);

	if(!CreatePipeline(&vkContext, &computePipeline, VK_NULL_HANDLE, "pipelines/volume_gen.pipeline"))
		return VK_FALSE;

	vkCreateDescriptorPool(vkContext.device, &(VkDescriptorPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets=1024, // Max number of descriptor sets that can be allocated from this pool
		.poolSizeCount=1,
		.pPoolSizes=(VkDescriptorPoolSize[]){ {.type=VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount=1, }, },
	}, VK_NULL_HANDLE, &computeDescriptorPool);

	if(vkCreateCommandPool(vkContext.device, &(VkCommandPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		.flags=0,
		.queueFamilyIndex=vkContext.computeQueueIndex,
	}, VK_NULL_HANDLE, &computeCommandPool)!=VK_SUCCESS)
		return VK_FALSE;

	if(vkAllocateCommandBuffers(vkContext.device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=computeCommandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=1,
	}, &computeCommand)!=VK_SUCCESS)
		return VK_FALSE;

	if(vkCreateFence(vkContext.device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=0 }, VK_NULL_HANDLE, &fence)!=VK_SUCCESS)
		return VK_FALSE;

	return VK_TRUE;
}

VkBool32 GenNebulaVolume(VkuImage_t *image)
{
	vkResetDescriptorPool(vkContext.device, computeDescriptorPool, 0);
	vkResetCommandPool(vkContext.device, computeCommandPool, 0);
	vkResetFences(vkContext.device, 1, (VkFence []){ fence });

	vkBeginCommandBuffer(computeCommand, &(VkCommandBufferBeginInfo) { .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO });

	vkCmdPipelineBarrier(computeCommand, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_NONE,
		.dstAccessMask=VK_ACCESS_SHADER_WRITE_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED,
		.newLayout=VK_IMAGE_LAYOUT_GENERAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=image->image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.levelCount=1,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=1,
	});

	vkCmdBindPipeline(computeCommand, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.pipeline.pipeline);

	vkuDescriptorSet_UpdateBindingImageInfo(&computePipeline.descriptorSet, 0, VK_NULL_HANDLE, image->imageView, VK_IMAGE_LAYOUT_GENERAL);
	vkuAllocateUpdateDescriptorSet(&computePipeline.descriptorSet, computeDescriptorPool);

	vkCmdBindDescriptorSets(computeCommand, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline.pipelineLayout, 0, 1, &computePipeline.descriptorSet.descriptorSet, 0, 0);

	static vec4 vRandom={ 0.0f, 0.0f, 0.0f, 0.0f };
	vRandom=Vec4(fTime/16.0f, 0.0f, 0.0f, 0.0f);
	vkCmdPushConstants(computeCommand, computePipeline.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(vec4), &vRandom);

	vkCmdDispatch(computeCommand, image->width/8, image->height/8, image->depth/8);

	vkCmdPipelineBarrier(computeCommand, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, NULL, 0, NULL, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT,
		.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
		.oldLayout=VK_IMAGE_LAYOUT_GENERAL,
		.newLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=image->image,
		.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.levelCount=1,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=1,
	});

	// End command buffer and submit
	if(vkEndCommandBuffer(computeCommand)!=VK_SUCCESS)
		return VK_FALSE;

	VkSubmitInfo submitInfo=
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&computeCommand,
	};

	vkQueueSubmit(vkContext.computeQueue, 1, &submitInfo, fence);

	if(vkWaitForFences(vkContext.device, 1, &fence, VK_TRUE, UINT64_MAX)!=VK_SUCCESS)
		return VK_FALSE;

	return VK_TRUE;
}
//////

// Create functions for volume rendering
bool CreateVolumePipeline(void)
{
	PipelineOverrideRasterizationSamples(config.MSAA);

	if(!CreatePipeline(&vkContext, &volumePipeline, renderPass, "pipelines/volume.pipeline"))
		return false;

	PipelineOverrideRasterizationSamples(VK_SAMPLE_COUNT_FLAG_BITS_MAX_ENUM);

	return true;
}

void DestroyVolume(void)
{
	vkDestroyFence(vkContext.device, fence, VK_NULL_HANDLE);

	vkDestroyCommandPool(vkContext.device, computeCommandPool, VK_NULL_HANDLE);
	vkDestroyDescriptorPool(vkContext.device, computeDescriptorPool, VK_NULL_HANDLE);
	DestroyPipeline(&vkContext, &computePipeline);

	DestroyPipeline(&vkContext, &volumePipeline);
}

void DrawVolume(VkCommandBuffer commandBuffer, uint32_t index, uint32_t eye, VkDescriptorPool descriptorPool)
{
	// Volumetric rendering is broken on Android when rendering at half resolution for some reason.
	static uint32_t uFrame=0;

	struct
	{
		uint32_t uFrame;
		uint32_t uWidth;
		uint32_t uHeight;
		float fShift;
		uint32_t uSamples;
		uint32_t pad[3];
	} PC;

	PC.uFrame=uFrame++;
	PC.uWidth=config.renderWidth;
	PC.uHeight=config.renderHeight;
	PC.fShift=UI_GetBarGraphValue(&UI, colorShiftID);
	PC.uSamples=config.msaaSamples;

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, volumePipeline.pipeline.pipeline);

	vkCmdPushConstants(commandBuffer, volumePipeline.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PC), &PC);

	vkuDescriptorSet_UpdateBindingImageInfo(&volumePipeline.descriptorSet, 0, AssetManager_GetAsset(assets, TEXTURE_VOLUME)->image.sampler, AssetManager_GetAsset(assets, TEXTURE_VOLUME)->image.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&volumePipeline.descriptorSet, 1, depthImage[eye].sampler, depthImage[eye].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&volumePipeline.descriptorSet, 2, shadowDepth.sampler, shadowDepth.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingBufferInfo(&volumePipeline.descriptorSet, 3, perFrame[index].mainUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
	vkuDescriptorSet_UpdateBindingBufferInfo(&volumePipeline.descriptorSet, 4, perFrame[index].skyboxUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);
	vkuAllocateUpdateDescriptorSet(&volumePipeline.descriptorSet, perFrame[index].descriptorPool);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, volumePipeline.pipelineLayout, 0, 1, &volumePipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	// No vertex data, it's baked into the vertex shader
	vkCmdDraw(commandBuffer, 36, 1, 0, 0);
}
