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

extern VkuMemZone_t *VkZone;

uint32_t vkuMemoryTypeFromProperties(VkPhysicalDeviceMemoryProperties memory_properties, uint32_t typeBits, VkFlags requirements_mask)
{
	// Search memtypes to find first index with those properties
	for(uint32_t i=0;i<memory_properties.memoryTypeCount;i++)
	{
		if((typeBits&1)==1)
		{
			// Type is available, does it match user properties?
			if((memory_properties.memoryTypes[i].propertyFlags&requirements_mask)==requirements_mask)
				return i;
		}

		typeBits>>=1;
	}

	// No memory types matched, return failure
	return 0;
}

VkBool32 vkuCreateImageBuffer(VkuContext_t *Context, VkuImage_t *Image,
	VkImageType ImageType, VkFormat Format, uint32_t MipLevels, uint32_t Layers, uint32_t Width, uint32_t Height, uint32_t Depth, 
	VkSampleCountFlagBits Samples, VkImageTiling Tiling, VkBufferUsageFlags Flags, VkFlags RequirementsMask, VkImageCreateFlags CreateFlags)
{
	if(Context==NULL||Image==NULL)
		return false;

	VkImageCreateInfo ImageInfo=
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
		.imageType=ImageType,
		.format=Format,
		.mipLevels=MipLevels,
		.arrayLayers=Layers,
		.samples=Samples,
		.tiling=Tiling,
		.usage=Flags,
		.sharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
		.extent.width=Width,
		.extent.height=Height,
		.extent.depth=Depth,
		.flags=CreateFlags,
	};

	if(vkCreateImage(Context->Device, &ImageInfo, NULL, &Image->Image)!=VK_SUCCESS)
		return VK_FALSE;

	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(Context->Device, Image->Image, &memoryRequirements);

	// Quick hack: getting it to use the vulkan memory allocator
	Image->DeviceMemory=vkuMem_Malloc(VkZone, memoryRequirements);

	if(Image->DeviceMemory==NULL)
		return VK_FALSE;

	if(vkBindImageMemory(Context->Device, Image->Image, VkZone->DeviceMemory, Image->DeviceMemory->Offset)!=VK_SUCCESS)
		return VK_FALSE;

	return VK_TRUE;
}

void vkuCreateTexture2D(VkuContext_t *Context, VkuImage_t *Image, uint32_t Width, uint32_t Height, VkFormat Format, VkSampleCountFlagBits Samples)
{
	// Set up some typical usage flags
	VkBufferUsageFlags UsageFlags=VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	VkImageAspectFlags AspectFlags=VK_IMAGE_ASPECT_COLOR_BIT;

	// If the format is in a range of depth formats, select depth/stencil attachment.
	// Otherwise it's a color format and thus color attachment.
	if(Format>=VK_FORMAT_D16_UNORM&&Format<=VK_FORMAT_D32_SFLOAT_S8_UINT)
	{
		UsageFlags|=VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		AspectFlags=VK_IMAGE_ASPECT_DEPTH_BIT;

		if(Format>=VK_FORMAT_S8_UINT)
			AspectFlags|=VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	else
		UsageFlags|=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	// Create the image buffer and memory
	vkuCreateImageBuffer(Context, Image,
						 VK_IMAGE_TYPE_2D, Format, 1, 1, Width, Height, 1,
						 Samples, VK_IMAGE_TILING_OPTIMAL, UsageFlags,
						 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0);

	// Create image view for the image buffer
	vkCreateImageView(Context->Device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext=NULL,
			.image=Image->Image,
			.format=Format,
			.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
			.subresourceRange.aspectMask=AspectFlags,
			.subresourceRange.baseMipLevel=0,
			.subresourceRange.levelCount=1,
			.subresourceRange.baseArrayLayer=0,
			.subresourceRange.layerCount=1,
			.viewType=VK_IMAGE_VIEW_TYPE_2D,
			.flags=0,
	}, NULL, &Image->View);

	vkCreateSampler(Context->Device, &(VkSamplerCreateInfo)
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
	}, VK_NULL_HANDLE, &Image->Sampler);
}

void vkuDestroyImageBuffer(VkuContext_t *Context, VkuImage_t *Image)
{
	if(Context==NULL||Image==NULL)
		return;

	if(Image->Sampler)
		vkDestroySampler(Context->Device, Image->Sampler, VK_NULL_HANDLE);

	if(Image->View)
		vkDestroyImageView(Context->Device, Image->View, VK_NULL_HANDLE);

	if(Image->Image)
		vkDestroyImage(Context->Device, Image->Image, VK_NULL_HANDLE);

	if(Image->DeviceMemory)
		vkuMem_Free(VkZone, Image->DeviceMemory);
}

VkBool32 vkuCreateHostBuffer(VkuContext_t *Context, VkuBuffer_t *Buffer, uint32_t Size, VkBufferUsageFlags Flags)
{
	if(Context==NULL||Buffer==NULL)
		return false;

	memset(Buffer, 0, sizeof(VkuBuffer_t));

	VkMemoryRequirements memoryRequirements;

	VkBufferCreateInfo BufferInfo=
	{
		.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size=Size,
		.usage=Flags,
		.sharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount=1,
		.pQueueFamilyIndices=&Context->QueueFamilyIndex,
	};

	if(vkCreateBuffer(Context->Device, &BufferInfo, NULL, &Buffer->Buffer)!=VK_SUCCESS)
		return VK_FALSE;

	vkGetBufferMemoryRequirements(Context->Device, Buffer->Buffer, &memoryRequirements);

	VkMemoryAllocateInfo AllocateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
		.allocationSize=memoryRequirements.size,
		.memoryTypeIndex=vkuMemoryTypeFromProperties(Context->DeviceMemProperties, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT),
	};

	if(vkAllocateMemory(Context->Device, &AllocateInfo, NULL, &Buffer->DeviceMemory)!=VK_SUCCESS)
		return VK_FALSE;

	if(vkBindBufferMemory(Context->Device, Buffer->Buffer, Buffer->DeviceMemory, 0)!=VK_SUCCESS)
			return VK_FALSE;

	return VK_TRUE;
}

VkBool32 vkuCreateGPUBuffer(VkuContext_t *Context, VkuBuffer_t *Buffer, uint32_t Size, VkBufferUsageFlags Flags)
{
	if(Context==NULL||Buffer==NULL)
		return false;

	memset(Buffer, 0, sizeof(VkuBuffer_t));

	VkMemoryRequirements memoryRequirements;

	VkBufferCreateInfo BufferInfo=
	{
		.sType=VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size=Size,
		.usage=Flags,
		.sharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount=1,
		.pQueueFamilyIndices=&Context->QueueFamilyIndex,
	};

	if(vkCreateBuffer(Context->Device, &BufferInfo, NULL, &Buffer->Buffer)!=VK_SUCCESS)
		return VK_FALSE;

	vkGetBufferMemoryRequirements(Context->Device, Buffer->Buffer, &memoryRequirements);

	Buffer->Memory=vkuMem_Malloc(VkZone, memoryRequirements);

	if(Buffer->Memory==NULL)
		return VK_FALSE;

	if(vkBindBufferMemory(Context->Device, Buffer->Buffer, VkZone->DeviceMemory, Buffer->Memory->Offset)!=VK_SUCCESS)
		return VK_FALSE;

	return VK_TRUE;
}

void vkuDestroyBuffer(VkuContext_t *Context, VkuBuffer_t *Buffer)
{
	if(Context==NULL||Buffer==NULL)
		return;

	if(Buffer->Buffer)
		vkDestroyBuffer(Context->Device, Buffer->Buffer, VK_NULL_HANDLE);

	if(Buffer->DeviceMemory)
		vkFreeMemory(Context->Device, Buffer->DeviceMemory, VK_NULL_HANDLE);

	if(Buffer->Memory)
		vkuMem_Free(VkZone, Buffer->Memory);
}

void vkuTransitionLayout(VkCommandBuffer CommandBuffer, VkImage Image, uint32_t levelCount, uint32_t baseLevel, uint32_t layerCount, uint32_t baseLayer, VkImageLayout oldLayout, VkImageLayout newLayout)
{
	VkPipelineStageFlags SrcStageFlag=VK_PIPELINE_STAGE_NONE;
	VkPipelineStageFlags DstStageFlag=VK_PIPELINE_STAGE_NONE;
	VkAccessFlags SrcAccessMask=VK_ACCESS_NONE;
	VkAccessFlags DstAccessMask=VK_ACCESS_NONE;
	VkImageAspectFlags AspectMask=VK_IMAGE_ASPECT_COLOR_BIT;

	switch(oldLayout)
	{
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			SrcAccessMask=VK_ACCESS_HOST_WRITE_BIT;
			SrcStageFlag=VK_PIPELINE_STAGE_HOST_BIT;
			break;

		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			SrcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			SrcStageFlag=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			SrcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			AspectMask=VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT;
			SrcStageFlag=VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			SrcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
			SrcStageFlag=VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			SrcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
			SrcStageFlag=VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			SrcAccessMask=VK_ACCESS_SHADER_READ_BIT;
			SrcStageFlag=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;

		case VK_IMAGE_LAYOUT_UNDEFINED:
		default:
			SrcStageFlag=VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			break;
	}

	switch(newLayout)
	{
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			DstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;
			DstStageFlag=VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			DstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;
			DstStageFlag=VK_PIPELINE_STAGE_TRANSFER_BIT;
			break;

		case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
			DstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			DstStageFlag=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;

		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			DstAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
			DstStageFlag=VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
			AspectMask=VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT;
			break;

		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
//			if(SrcAccessMask==0)
//				SrcAccessMask=VK_ACCESS_HOST_WRITE_BIT|VK_ACCESS_TRANSFER_WRITE_BIT;

			DstAccessMask=VK_ACCESS_SHADER_READ_BIT;
			DstStageFlag=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			break;

		case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
			DstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			DstStageFlag=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			break;

		default:
			break;
	}

	vkCmdPipelineBarrier(CommandBuffer, SrcStageFlag, DstStageFlag, 0, 0, VK_NULL_HANDLE, 0, VK_NULL_HANDLE, 1, &(VkImageMemoryBarrier)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
		.srcQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED,
		.image=Image,
		.subresourceRange.aspectMask=AspectMask,
		.subresourceRange.baseMipLevel=baseLevel,
		.subresourceRange.levelCount=levelCount,
		.subresourceRange.baseArrayLayer=baseLayer,
		.subresourceRange.layerCount=layerCount,
		.srcAccessMask=SrcAccessMask,
		.dstAccessMask=DstAccessMask,
		.oldLayout=oldLayout,
		.newLayout=newLayout,
	});
}

VkCommandBuffer vkuOneShotCommandBufferBegin(VkuContext_t *Context)
{
	VkCommandBuffer CommandBuffer;

	// Create a command buffer to submit a copy command from the staging buffer into the static vertex buffer
	VkResult Result=vkAllocateCommandBuffers(Context->Device, &(VkCommandBufferAllocateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		.commandPool=Context->CommandPool,
		.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		.commandBufferCount=1,
	}, &CommandBuffer);

	if(Result!=VK_SUCCESS)
		return VK_FALSE;

	Result=vkBeginCommandBuffer(CommandBuffer, &(VkCommandBufferBeginInfo) { .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO });

	if(Result!=VK_SUCCESS)
	{
		vkFreeCommandBuffers(Context->Device, Context->CommandPool, 1, &CommandBuffer);
		return VK_NULL_HANDLE;
	}

	return CommandBuffer;
}

VkBool32 vkuOneShotCommandBufferEnd(VkuContext_t *Context, VkCommandBuffer CommandBuffer)
{
	VkFence Fence=VK_NULL_HANDLE;

	// End command buffer and submit
	VkResult Result=vkEndCommandBuffer(CommandBuffer);

	if(Result!=VK_SUCCESS)
		return VK_FALSE;

	Result=vkCreateFence(Context->Device, &(VkFenceCreateInfo) {.sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=0 }, VK_NULL_HANDLE, &Fence);

	if(Result!=VK_SUCCESS)
		return VK_FALSE;

	Result=vkQueueSubmit(Context->Queue, 1, &(VkSubmitInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
		.commandBufferCount=1,
		.pCommandBuffers=&CommandBuffer,
	}, Fence);

	if(Result!=VK_SUCCESS)
		return VK_FALSE;

	// Wait for it to finish
	Result=vkWaitForFences(Context->Device, 1, &Fence, VK_TRUE, UINT64_MAX);

	if(Result!=VK_SUCCESS)
		return VK_FALSE;

	vkDestroyFence(Context->Device, Fence, VK_NULL_HANDLE);

	vkFreeCommandBuffers(Context->Device, Context->CommandPool, 1, &CommandBuffer);

	return VK_TRUE;
}