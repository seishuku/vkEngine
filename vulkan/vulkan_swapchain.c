#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../system/system.h"
#include "vulkan.h"

VkBool32 vkuCreateSwapchain(VkuContext_t *Context, VkuSwapchain_t *Swapchain, uint32_t Width, uint32_t Height, VkBool32 VSync)
{
	if(!Swapchain)
		return VK_FALSE;

	uint32_t FormatCount=0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(Context->PhysicalDevice, Context->Surface, &FormatCount, VK_NULL_HANDLE);

	VkSurfaceFormatKHR *SurfaceFormats=(VkSurfaceFormatKHR *)Zone_Malloc(Zone, sizeof(VkSurfaceFormatKHR)*FormatCount);

	if(SurfaceFormats==NULL)
		return VK_FALSE;

	vkGetPhysicalDeviceSurfaceFormatsKHR(Context->PhysicalDevice, Context->Surface, &FormatCount, SurfaceFormats);

	// Find the format we want
	for(uint32_t i=0;i<FormatCount;i++)
	{
		if(SurfaceFormats[i].format==VK_FORMAT_B8G8R8A8_SRGB)
		{
			Swapchain->SurfaceFormat=SurfaceFormats[i];
			break;
		}
	}

	Zone_Free(Zone, SurfaceFormats);

	// Get physical device surface properties and formats
	VkSurfaceCapabilitiesKHR SurfCaps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Context->PhysicalDevice, Context->Surface, &SurfCaps);

	// Get available present modes
	uint32_t PresentModeCount=0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(Context->PhysicalDevice, Context->Surface, &PresentModeCount, NULL);

	VkPresentModeKHR *PresentModes=(VkPresentModeKHR *)Zone_Malloc(Zone, sizeof(VkPresentModeKHR)*PresentModeCount);

	if(PresentModes==NULL)
		return VK_FALSE;

	vkGetPhysicalDeviceSurfacePresentModesKHR(Context->PhysicalDevice, Context->Surface, &PresentModeCount, PresentModes);

	// Set swapchain extents to the surface width/height
	Swapchain->Extent.width=Width;
	Swapchain->Extent.height=Height;

	// Select a present mode for the swapchain

	// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
	// This mode waits for the vertical blank ("v-sync")

	// If v-sync is not requested, try to find a mailbox mode
	// It's the lowest latency non-tearing present mode available
	VkPresentModeKHR SwapchainPresentMode=VK_PRESENT_MODE_FIFO_KHR;

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

	Zone_Free(Zone, PresentModes);

	// Find a supported composite alpha format (not all devices support alpha opaque)
	// Simply select the first composite alpha format available
	VkCompositeAlphaFlagBitsKHR CompositeAlpha=VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	VkCompositeAlphaFlagBitsKHR CompositeAlphaFlags[]=
	{
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
	};

	for(uint32_t i=0;i<4;i++)
	{
		if(SurfCaps.supportedCompositeAlpha&CompositeAlphaFlags[i])
		{
			CompositeAlpha=CompositeAlphaFlags[i];
			break;
		}
	}

	VkImageUsageFlags ImageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

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
		.flags=0,
		.surface=Context->Surface,
		.minImageCount=SurfCaps.minImageCount,
		.imageFormat=Swapchain->SurfaceFormat.format,
		.imageColorSpace=Swapchain->SurfaceFormat.colorSpace,
		.imageExtent=Swapchain->Extent,
		.imageArrayLayers=1,
		.imageUsage=ImageUsage,
		.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE,
		.queueFamilyIndexCount=0,
		.pQueueFamilyIndices=VK_NULL_HANDLE,
		.preTransform=VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
		.compositeAlpha=CompositeAlpha,
		.presentMode=SwapchainPresentMode,
		// Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
		.clipped=VK_TRUE,
		.oldSwapchain=VK_NULL_HANDLE,
	}, VK_NULL_HANDLE, &Swapchain->Swapchain);

	// Get swap chain image count
	Swapchain->NumImages=0;
	vkGetSwapchainImagesKHR(Context->Device, Swapchain->Swapchain, &Swapchain->NumImages, VK_NULL_HANDLE);

	// Check to make sure the driver doesn't want more than what we can support
	if(Swapchain->NumImages==0||Swapchain->NumImages>VKU_MAX_FRAME_COUNT)
	{
		DBGPRINTF(DEBUG_ERROR, "Swapchain minimum image count is greater than supported number of images. (requested: %d, minimum: %d", VKU_MAX_FRAME_COUNT, SurfCaps.minImageCount);
		return VK_FALSE;
	}

	// Get the swap chain images
	vkGetSwapchainImagesKHR(Context->Device, Swapchain->Swapchain, &Swapchain->NumImages, Swapchain->Image);

	// Create imageviews and transition to correct image layout
	VkCommandBuffer CommandBuffer=vkuOneShotCommandBufferBegin(Context);

	for(uint32_t i=0;i<Swapchain->NumImages;i++)
	{
		vkuTransitionLayout(CommandBuffer, Swapchain->Image[i], 1, 0, 1, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

		vkCreateImageView(Context->Device, &(VkImageViewCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.pNext=VK_NULL_HANDLE,
			.image=Swapchain->Image[i],
			.format=Swapchain->SurfaceFormat.format,
			.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
			.subresourceRange.aspectMask=VK_IMAGE_ASPECT_COLOR_BIT,
			.subresourceRange.baseMipLevel=0,
			.subresourceRange.levelCount=1,
			.subresourceRange.baseArrayLayer=0,
			.subresourceRange.layerCount=1,
			.viewType=VK_IMAGE_VIEW_TYPE_2D,
			.flags=0,
		}, VK_NULL_HANDLE, &Swapchain->ImageView[i]);
	}

	vkuOneShotCommandBufferEnd(Context, CommandBuffer);

	return VK_TRUE;
}

void vkuDestroySwapchain(VkuContext_t *Context, VkuSwapchain_t *Swapchain)
{
	if(!Context||!Swapchain)
		return;

	for(uint32_t i=0;i<Swapchain->NumImages;i++)
		vkDestroyImageView(Context->Device, Swapchain->ImageView[i], VK_NULL_HANDLE);

	vkDestroySwapchainKHR(Context->Device, Swapchain->Swapchain, VK_NULL_HANDLE);
}