#include "vulkan.h"

VkBool32 vkuFramebufferAddAttachment(VkuFramebuffer_t *framebuffer, VkImageView attachment)
{
	if(!framebuffer)
		return VK_FALSE;

	if(framebuffer->numAttachments>=VKU_MAX_FRAMEBUFFER_ATTACHMENTS)
		return VK_FALSE;

	framebuffer->attachments[framebuffer->numAttachments++]=attachment;

	return VK_TRUE;
}

VkBool32 vkuInitFramebuffer(VkuFramebuffer_t *framebuffer, VkuContext_t *context, VkRenderPass renderPass)
{
	if(!framebuffer)
		return VK_FALSE;

	if(!context)
		return VK_FALSE;

	framebuffer->device=context->device;
	framebuffer->renderPass=renderPass;

	framebuffer->numAttachments=0;
	memset(framebuffer->attachments, 0, sizeof(VkImageView)*VKU_MAX_FRAMEBUFFER_ATTACHMENTS);

	return VK_TRUE;
}

VkBool32 vkuFramebufferCreate(VkuFramebuffer_t *framebuffer, VkExtent2D size)
{
	VkFramebufferCreateInfo FramebufferCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.pNext=VK_NULL_HANDLE,
		.flags=0,
		.renderPass=framebuffer->renderPass,
		.attachmentCount=framebuffer->numAttachments,
		.pAttachments=framebuffer->attachments,
		.width=size.width,
		.height=size.height,
		.layers=1,
	};

	if(vkCreateFramebuffer(framebuffer->device, &FramebufferCreateInfo, 0, &framebuffer->framebuffer)!=VK_SUCCESS)
		return VK_FALSE;

	return VK_TRUE;
}
