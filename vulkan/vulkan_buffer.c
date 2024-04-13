// Vulkan helper functions
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "vulkan.h"
#include "../image/image.h"
#include "../math/math.h"

uint32_t vkuMemoryTypeFromProperties(VkPhysicalDeviceMemoryProperties memoryProperties, uint32_t typeBits, VkFlags requirementsMask)
{
	// Search memtypes to find first index with those properties
	for(uint32_t i=0;i<memoryProperties.memoryTypeCount;i++)
	{
		if((typeBits&1)==1)
		{
			// Type is available, does it match user properties?
			if((memoryProperties.memoryTypes[i].propertyFlags&requirementsMask)==requirementsMask)
				return i;
		}

		typeBits>>=1;
	}

	// No memory types matched, return failure
	return 0;
}

VkBool32 vkuCreateImageBuffer(VkuContext_t *context, VkuImage_t *image,
	VkImageType imageType, VkFormat format, uint32_t mipLevels, uint32_t layers, uint32_t width, uint32_t height, uint32_t depth, 
	VkSampleCountFlagBits samples, VkImageTiling tiling, VkBufferUsageFlags flags, VkFlags requirementsMask, VkImageCreateFlags createFlags)
{
	if(context==NULL||image==NULL)
		return false;

	VkImageCreateInfo imageInfo=
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType=imageType,
		.format=format,
		.mipLevels=mipLevels,
		.arrayLayers=layers,
		.samples=samples,
		.tiling=tiling,
		.usage=flags,
		.sharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
		.extent.width=width,
		.extent.height=height,
		.extent.depth=depth,
		.flags=createFlags,
	};

	if(vkCreateImage(context->device, &imageInfo, NULL, &image->image)!=VK_SUCCESS)
		return VK_FALSE;

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(context->device, image->image, &memoryRequirements);

	memoryRequirements.memoryTypeBits=requirementsMask;

	image->memory=vkuMemAllocator_Malloc(memoryRequirements);

	if(image->memory==NULL)
		return VK_FALSE;

	if(vkBindImageMemory(context->device, image->image, image->memory->deviceMemory, image->memory->offset)!=VK_SUCCESS)
		return VK_FALSE;

	return VK_TRUE;
}

void vkuCreateTexture2D(VkuContext_t *context, VkuImage_t *image, uint32_t width, uint32_t height, VkFormat format, VkSampleCountFlagBits samples)
{
	// Set up some typical usage flags
	VkBufferUsageFlags usageFlags=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	VkImageAspectFlags aspectFlags=VK_IMAGE_ASPECT_COLOR_BIT;

	// If the format is in a range of depth formats, select depth/stencil attachment.
	// Otherwise it's a color format and thus color attachment.
	if(format>=VK_FORMAT_D16_UNORM&&format<=VK_FORMAT_D32_SFLOAT_S8_UINT)
	{
		usageFlags|=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		aspectFlags=VK_IMAGE_ASPECT_DEPTH_BIT;

		if(format>=VK_FORMAT_S8_UINT)
			aspectFlags|=VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	else
		usageFlags|=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	// Create the image buffer and memory
	vkuCreateImageBuffer(context, image,
						 VK_IMAGE_TYPE_2D, format, 1, 1, width, height, 1,
						 samples, VK_IMAGE_TILING_OPTIMAL, usageFlags,
						 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);

	// Create image view for the image buffer
	vkCreateImageView(context->device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.pNext=NULL,
		.image=image->image,
		.format=format,
		.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
		.subresourceRange.aspectMask=aspectFlags,
		.subresourceRange.baseMipLevel=0,
		.subresourceRange.levelCount=1,
		.subresourceRange.baseArrayLayer=0,
		.subresourceRange.layerCount=1,
		.viewType=VK_IMAGE_VIEW_TYPE_2D,
		.flags=0,
	}, NULL, &image->imageView);

	vkCreateSampler(context->device, &(VkSamplerCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter=VK_FILTER_LINEAR,
		.minFilter=VK_FILTER_LINEAR,
		.mipmapMode=VK_SAMPLER_MIPMAP_MODE_LINEAR,
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
}

void vkuDestroyImageBuffer(VkuContext_t *context, VkuImage_t *image)
{
	if(context==NULL||image==NULL)
		return;

	if(image->sampler)
	{
		vkDestroySampler(context->device, image->sampler, VK_NULL_HANDLE);
		image->sampler=VK_NULL_HANDLE;
	}

	if(image->imageView)
	{
		vkDestroyImageView(context->device, image->imageView, VK_NULL_HANDLE);
		image->imageView=VK_NULL_HANDLE;
	}

	if(image->image)
	{
		vkDestroyImage(context->device, image->image, VK_NULL_HANDLE);
		image->image=VK_NULL_HANDLE;
	}

	if(image->memory)
	{
		vkuMemAllocator_Free(image->memory);
		image->memory=NULL;
	}
}

void PrintMemoryTypeFlags(VkMemoryPropertyFlags propertyFlags);

VkBool32 vkuCreateHostBuffer(VkuContext_t *context, VkuBuffer_t *buffer, uint32_t size, VkBufferUsageFlags flags)
{
	if(context==NULL||buffer==NULL)
		return false;

	memset(buffer, 0, sizeof(VkuBuffer_t));

	VkMemoryRequirements memoryRequirements;

	VkBufferCreateInfo bufferInfo=
	{
		.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size=size,
		.usage=flags,
		.sharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount=1,
		.pQueueFamilyIndices=&context->graphicsQueueIndex,
	};

	if(vkCreateBuffer(context->device, &bufferInfo, NULL, &buffer->buffer)!=VK_SUCCESS)
		return VK_FALSE;

	vkGetBufferMemoryRequirements(context->device, buffer->buffer, &memoryRequirements);

	memoryRequirements.memoryTypeBits=(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT|VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

	// Clamp to a minimum size
	//if(memoryRequirements.size<VKU_MIN_DEVICE_ALLOCATION_SIZE)
	//	memoryRequirements.size=VKU_MIN_DEVICE_ALLOCATION_SIZE;

	buffer->memory=vkuMemAllocator_Malloc(memoryRequirements);

	if(buffer->memory==NULL)
		return VK_FALSE;

	if(vkBindBufferMemory(context->device, buffer->buffer, buffer->memory->deviceMemory, buffer->memory->offset)!=VK_SUCCESS)
			return VK_FALSE;

	return VK_TRUE;
}

VkBool32 vkuCreateGPUBuffer(VkuContext_t *context, VkuBuffer_t *buffer, uint32_t size, VkBufferUsageFlags flags)
{
	if(context==NULL||buffer==NULL)
		return false;

	memset(buffer, 0, sizeof(VkuBuffer_t));

	VkMemoryRequirements memoryRequirements;

	VkBufferCreateInfo bufferInfo=
	{
		.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size=size,
		.usage=flags,
		.sharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount=1,
		.pQueueFamilyIndices=&context->graphicsQueueIndex,
	};

	if(vkCreateBuffer(context->device, &bufferInfo, NULL, &buffer->buffer)!=VK_SUCCESS)
		return VK_FALSE;

	vkGetBufferMemoryRequirements(context->device, buffer->buffer, &memoryRequirements);

	memoryRequirements.memoryTypeBits=VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

	buffer->memory=vkuMemAllocator_Malloc(memoryRequirements);

	if(buffer->memory==NULL)
		return VK_FALSE;

	if(vkBindBufferMemory(context->device, buffer->buffer, buffer->memory->deviceMemory, buffer->memory->offset)!=VK_SUCCESS)
		return VK_FALSE;

	return VK_TRUE;
}

void vkuDestroyBuffer(VkuContext_t *context, VkuBuffer_t *buffer)
{
	if(context==NULL||buffer==NULL)
		return;

	if(buffer->buffer)
	{
		vkDestroyBuffer(context->device, buffer->buffer, VK_NULL_HANDLE);
		buffer->buffer=VK_NULL_HANDLE;
	}

	if(buffer->memory)
	{
		vkuMemAllocator_Free(buffer->memory);
		buffer->memory=NULL;
	}
}

void vkuTransitionLayout(VkCommandBuffer commandBuffer, VkImage image, uint32_t levelCount, uint32_t baseLevel, uint32_t layerCount, uint32_t baseLayer, VkImageAspectFlags aspectMask, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkPipelineStageFlags srcStageFlag=VK_PIPELINE_STAGE_NONE;
	VkPipelineStageFlags dstStageFlag=VK_PIPELINE_STAGE_NONE;
	VkAccessFlags srcAccessMask=VK_ACCESS_NONE;
	VkAccessFlags dstAccessMask=VK_ACCESS_NONE;

	switch(oldLayout)
	{
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;
			srcStageFlag=VK_PIPELINE_STAGE_HOST_BIT;
			break;

		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			srcStageFlag=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			srcStageFlag=VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
			srcStageFlag=VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
			srcStageFlag=VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			srcAccessMask=VK_ACCESS_SHADER_READ_BIT;
			srcStageFlag=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;

		case VK_IMAGE_LAYOUT_UNDEFINED:
		default:
			srcStageFlag=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			break;
	}

	switch(newLayout)
	{
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
			dstStageFlag=VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
			dstStageFlag=VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dstStageFlag=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			dstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			dstStageFlag=VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			break;

		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
//			if(srcAccessMask==0)
//				srcAccessMask=VK_ACCESS_HOST_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;

			dstAccessMask=VK_ACCESS_SHADER_READ_BIT;
			dstStageFlag=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;

		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			//dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			dstStageFlag=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;

		default:
			break;
	}

	vkCmdPipelineBarrier(commandBuffer, srcStageFlag, dstStageFlag, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=image,
		.subresourceRange.aspectMask=aspectMask,
		.subresourceRange.baseMipLevel=baseLevel,
		.subresourceRange.levelCount=levelCount,
		.subresourceRange.baseArrayLayer=baseLayer,
		.subresourceRange.layerCount=layerCount,
		.srcAccessMask=srcAccessMask,
		.dstAccessMask=dstAccessMask,
		.oldLayout=oldLayout,
		.newLayout=newLayout,
	});
}

VkCommandBuffer vkuOneShotCommandBufferBegin(VkuContext_t *context)
{
	VkCommandBuffer commandBuffer;

	// Create a command buffer to submit a copy command from the staging buffer into the static vertex buffer
	if(vkAllocateCommandBuffers(context->device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=context->commandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=1,
	}, &commandBuffer)!=VK_SUCCESS)
		return VK_FALSE;

	if(vkBeginCommandBuffer(commandBuffer, &(VkCommandBufferBeginInfo) {.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO })!=VK_SUCCESS)
	{
		vkFreeCommandBuffers(context->device, context->commandPool, 1, &commandBuffer);
		return VK_NULL_HANDLE;
	}

	return commandBuffer;
}

VkBool32 vkuOneShotCommandBufferFlush(VkuContext_t *context, VkCommandBuffer commandBuffer)
{
	VkFence fence=VK_NULL_HANDLE;

	// End command buffer and submit
	if(vkEndCommandBuffer(commandBuffer)!=VK_SUCCESS)
		return VK_FALSE;

	if(vkCreateFence(context->device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=0 }, VK_NULL_HANDLE, &fence)!=VK_SUCCESS)
		return VK_FALSE;

	if(vkQueueSubmit(context->graphicsQueue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&commandBuffer,
	}, fence)!=VK_SUCCESS)
		return VK_FALSE;

	// Wait for it to finish
	if(vkWaitForFences(context->device, 1, &fence, VK_TRUE, UINT64_MAX)!=VK_SUCCESS)
		return VK_FALSE;

	vkDestroyFence(context->device, fence, VK_NULL_HANDLE);

	// Reset command buffer state before restarting
	vkResetCommandBuffer(commandBuffer, 0);

	// Restart recording command buffer
	if(vkBeginCommandBuffer(commandBuffer, &(VkCommandBufferBeginInfo) {.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO })!=VK_SUCCESS)
	{
		vkFreeCommandBuffers(context->device, context->commandPool, 1, &commandBuffer);
		return VK_FALSE;
	}

	return VK_TRUE;
}

VkBool32 vkuOneShotCommandBufferEnd(VkuContext_t *context, VkCommandBuffer commandBuffer)
{
	VkFence fence=VK_NULL_HANDLE;

	// End command buffer and submit
	if(vkEndCommandBuffer(commandBuffer)!=VK_SUCCESS)
		return VK_FALSE;

	if(vkCreateFence(context->device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=0 }, VK_NULL_HANDLE, &fence)!=VK_SUCCESS)
		return VK_FALSE;

	if(vkQueueSubmit(context->graphicsQueue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&commandBuffer,
	}, fence)!=VK_SUCCESS)
		return VK_FALSE;

	// Wait for it to finish
	if(vkWaitForFences(context->device, 1, &fence, VK_TRUE, UINT64_MAX)!=VK_SUCCESS)
		return VK_FALSE;

	vkDestroyFence(context->device, fence, VK_NULL_HANDLE);

	vkFreeCommandBuffers(context->device, context->commandPool, 1, &commandBuffer);

	return VK_TRUE;
}
