#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "../math/math.h"
#include "vulkan.h"

VkBool32 vkuCreateSwapchain(VkuContext_t *context, VkuSwapchain_t *swapchain, VkBool32 vSync)
{
	VkResult result=VK_SUCCESS;
	const VkFormat preferredFormats[]=
	{
		VK_FORMAT_B8G8R8A8_SRGB,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_FORMAT_R8G8B8A8_SRGB,
		VK_FORMAT_R8G8B8A8_UNORM
	};
	bool foundFormat=false;

	if(!swapchain)
		return VK_FALSE;

	uint32_t formatCount=0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(context->physicalDevice, context->surface, &formatCount, VK_NULL_HANDLE);

	VkSurfaceFormatKHR *surfaceFormats=(VkSurfaceFormatKHR *)Zone_Malloc(zone, sizeof(VkSurfaceFormatKHR)*formatCount);

	if(surfaceFormats==NULL)
		return VK_FALSE;

	vkGetPhysicalDeviceSurfaceFormatsKHR(context->physicalDevice, context->surface, &formatCount, surfaceFormats);

	// Find a suitable format, best match to top preferred format
	for(uint32_t i=0;i<sizeof(preferredFormats);i++)
	{
		for(uint32_t j=0;j<formatCount;j++)
		{
			if(surfaceFormats[j].format==preferredFormats[i])
			{
				swapchain->surfaceFormat=surfaceFormats[j];
				foundFormat=true;
				break;
			}
		}

		if(foundFormat)
			break;
	}

	Zone_Free(zone, surfaceFormats);

	// Get physical device surface properties and formats
	VkSurfaceCapabilitiesKHR surfaceCaps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context->physicalDevice, context->surface, &surfaceCaps);

	if(surfaceCaps.currentTransform&VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR||surfaceCaps.currentTransform&VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)
	{
		// Swap to get identity width and height
		uint32_t width=surfaceCaps.currentExtent.width;
		uint32_t height=surfaceCaps.currentExtent.height;
		surfaceCaps.currentExtent.height=width;
		surfaceCaps.currentExtent.width=height;
	}

	// Set swapchain extents to the surface width/height,
	// otherwise if extent is already non-zero, use that for surface width/height.
	// This allows setting width/height externally.
	if(!(swapchain->extent.width&&swapchain->extent.height))
		swapchain->extent=surfaceCaps.currentExtent;

	// Get available present modes
	uint32_t presentModeCount=0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(context->physicalDevice, context->surface, &presentModeCount, NULL);

	VkPresentModeKHR *presentModes=(VkPresentModeKHR *)Zone_Malloc(zone, sizeof(VkPresentModeKHR)*presentModeCount);

	if(presentModes==NULL)
		return VK_FALSE;

	vkGetPhysicalDeviceSurfacePresentModesKHR(context->physicalDevice, context->surface, &presentModeCount, presentModes);

	// Select a present mode for the swapchain

	// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec,
	//     this mode waits for the vertical blank ("v-sync").
	VkPresentModeKHR swapchainPresentMode=VK_PRESENT_MODE_FIFO_KHR;

	// If v-sync is not requested, try to find a preferred mode
	if(!vSync)
	{
		const VkPresentModeKHR preferredPresentMode[]=
		{
			VK_PRESENT_MODE_MAILBOX_KHR,
			VK_PRESENT_MODE_IMMEDIATE_KHR,
			VK_PRESENT_MODE_FIFO_RELAXED_KHR,
			VK_PRESENT_MODE_FIFO_KHR,
		};
		bool foundPresentMode=false;

		for(uint32_t i=0;i<sizeof(preferredPresentMode);i++)
		{
			for(uint32_t j=0;j<presentModeCount;j++)
			{
				if(presentModes[j]==preferredPresentMode[i])
				{
					swapchainPresentMode=presentModes[j];
					foundPresentMode=true;
					break;
				}
			}

			if(foundPresentMode)
				break;
		}
	}

	Zone_Free(zone, presentModes);

	// Find a supported composite alpha format (not all devices support alpha opaque)
	// Simply select the first composite alpha format available
	VkCompositeAlphaFlagBitsKHR compositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	VkCompositeAlphaFlagBitsKHR compositeAlphaFlags[]=
	{
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
	};

	for(uint32_t i=0;i<sizeof(compositeAlphaFlags);i++)
	{
		if(surfaceCaps.supportedCompositeAlpha&compositeAlphaFlags[i])
		{
			compositeAlpha=compositeAlphaFlags[i];
			break;
		}
	}

	VkImageUsageFlags imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	// Enable transfer source on swap chain images if supported
	if(surfaceCaps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
		imageUsage|=VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	// Enable transfer destination on swap chain images if supported
	if(surfaceCaps.supportedUsageFlags&VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		imageUsage|=VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	VkSwapchainKHR newSwapchain;
	VkImageView newImageView[VKU_MAX_FRAME_COUNT];
	VkImage newImage[VKU_MAX_FRAME_COUNT];

	result=vkCreateSwapchainKHR(context->device, &(VkSwapchainCreateInfoKHR)
	{
		.sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		.pNext=VK_NULL_HANDLE,
		.flags=0,
		.surface=context->surface,
		.minImageCount=max(3, surfaceCaps.minImageCount),
		.imageFormat=swapchain->surfaceFormat.format,
		.imageColorSpace=swapchain->surfaceFormat.colorSpace,
		.imageExtent=swapchain->extent,
		.imageArrayLayers=1,
		.imageUsage=imageUsage,
		.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount=0,
		.pQueueFamilyIndices=VK_NULL_HANDLE,
		.preTransform=surfaceCaps.currentTransform,
		.compositeAlpha=compositeAlpha,
		.presentMode=swapchainPresentMode,
		.clipped=VK_TRUE,
		.oldSwapchain=swapchain->swapchain,
	}, VK_NULL_HANDLE, &newSwapchain);

	if(result!=VK_SUCCESS)
	{
		DBGPRINTF(DEBUG_ERROR, "vkCreateSwapchainKHR failed. (result: %d)", result);
		return VK_FALSE;
	}

	// Get swap chain image count
	swapchain->numImages=0;
	vkGetSwapchainImagesKHR(context->device, newSwapchain, &swapchain->numImages, VK_NULL_HANDLE);

	// Check to make sure the driver doesn't want more than what we can support
	if(swapchain->numImages==0||swapchain->numImages>VKU_MAX_FRAME_COUNT)
	{
		DBGPRINTF(DEBUG_ERROR, "Swapchain minimum image count is greater than supported number of images. (requested: %d, minimum: %d)", VKU_MAX_FRAME_COUNT, surfaceCaps.minImageCount);
		return VK_FALSE;
	}

	// Get the swap chain images
	vkGetSwapchainImagesKHR(context->device, newSwapchain, &swapchain->numImages, newImage);

	// Create imageviews and transition to correct image layout
	VkCommandBuffer commandBuffer=vkuOneShotCommandBufferBegin(context);

	for(uint32_t i=0;i<swapchain->numImages;i++)
	{
		vkuTransitionLayout(commandBuffer, newImage[i], 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		vkCreateImageView(context->device, &(VkImageViewCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext=VK_NULL_HANDLE,
			.image=newImage[i],
			.format=swapchain->surfaceFormat.format,
			.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
			.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.subresourceRange.baseMipLevel=0,
			.subresourceRange.levelCount=1,
			.subresourceRange.baseArrayLayer=0,
			.subresourceRange.layerCount=1,
			.viewType=VK_IMAGE_VIEW_TYPE_2D,
			.flags=0,
		}, VK_NULL_HANDLE, &newImageView[i]);
	}

	vkuOneShotCommandBufferEnd(context, commandBuffer);

	// If there was already a swapchain
	if(swapchain->swapchain)
		vkuDestroySwapchain(context, swapchain);

	swapchain->swapchain=newSwapchain;

	for(uint32_t i=0;i<swapchain->numImages;i++)
	{
		swapchain->image[i]=newImage[i];
		swapchain->imageView[i]=newImageView[i];
	}

	return VK_TRUE;
}

void vkuDestroySwapchain(VkuContext_t *context, VkuSwapchain_t *swapchain)
{
	if(!context||!swapchain)
		return;

	for(uint32_t i=0;i<swapchain->numImages;i++)
		vkDestroyImageView(context->device, swapchain->imageView[i], VK_NULL_HANDLE);

	vkDestroySwapchainKHR(context->device, swapchain->swapchain, VK_NULL_HANDLE);
	swapchain->swapchain=VK_NULL_HANDLE;
}
