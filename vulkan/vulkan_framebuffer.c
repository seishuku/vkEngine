#include "vulkan.h"

VkBool32 vkuFramebufferAddAttachment(VkuFramebuffer_t *Framebuffer, VkImageView Attachment)
{
	if(!Framebuffer)
		return VK_FALSE;

	if(Framebuffer->NumAttachments>=VKU_MAX_FRAMEBUFFER_ATTACHMENTS)
		return VK_FALSE;

	Framebuffer->Attachments[Framebuffer->NumAttachments++]=Attachment;

	return VK_TRUE;
}

VkBool32 vkuInitFramebuffer(VkuFramebuffer_t *Framebuffer, VkuContext_t *Context, VkRenderPass RenderPass)
{
	if(!Framebuffer)
		return VK_FALSE;

	if(!Context)
		return VK_FALSE;

	Framebuffer->Device=Context->Device;
	Framebuffer->RenderPass=RenderPass;

	Framebuffer->NumAttachments=0;
	memset(Framebuffer->Attachments, 0, sizeof(VkImageView)*VKU_MAX_FRAMEBUFFER_ATTACHMENTS);

	return VK_TRUE;
}

VkBool32 vkuFramebufferCreate(VkuFramebuffer_t *Framebuffer, VkExtent2D Size)
{
	VkFramebufferCreateInfo FramebufferCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.pNext=VK_NULL_HANDLE,
		.flags=0,
		.renderPass=Framebuffer->RenderPass,
		.attachmentCount=Framebuffer->NumAttachments,
		.pAttachments=Framebuffer->Attachments,
		.width=Size.width,
		.height=Size.height,
		.layers=1,
	};

	if(vkCreateFramebuffer(Framebuffer->Device, &FramebufferCreateInfo, 0, &Framebuffer->Framebuffer)!=VK_SUCCESS)
		return VK_FALSE;

	return VK_TRUE;
}