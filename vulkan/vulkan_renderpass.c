#include "vulkan.h"

VkBool32 vkuRenderPass_AddAttachment(VkuRenderPass_t *renderPass, VkuRenderPassAttachmentType type, VkFormat format, VkSampleCountFlagBits samples, VkAttachmentLoadOp loadOp, VkAttachmentStoreOp storeOp, VkImageLayout subpassLayout, VkImageLayout finalLayout)
{
	if(!renderPass)
		return VK_FALSE;

	if(renderPass->numAttachments>=VKU_MAX_RENDERPASS_ATTACHMENTS)
		return VK_FALSE;

	renderPass->attachments[renderPass->numAttachments].type=type;
	renderPass->attachments[renderPass->numAttachments].format=format;
	renderPass->attachments[renderPass->numAttachments].samples=samples;
	renderPass->attachments[renderPass->numAttachments].loadOp=loadOp;
	renderPass->attachments[renderPass->numAttachments].storeOp=storeOp;
	renderPass->attachments[renderPass->numAttachments].subpassLayout=subpassLayout;
	renderPass->attachments[renderPass->numAttachments].finalLayout=finalLayout;
	renderPass->numAttachments++;

	return VK_TRUE;
}

VkBool32 vkuRenderPass_AddSubpassDependency(VkuRenderPass_t *renderPass, uint32_t sourceSubpass, uint32_t destinationSubpass, VkPipelineStageFlags sourceStageMask, VkPipelineStageFlags destinationStageMask, VkAccessFlags sourceAccessMask, VkAccessFlags destinationAccessMask, VkDependencyFlags dependencyFlags)
{
	if(!renderPass)
		return VK_FALSE;

	if(renderPass->numSubpassDependencies>=VKU_MAX_RENDERPASS_SUBPASS_DEPENDENCIES)
		return VK_FALSE;

	VkSubpassDependency Dependency=
	{
		.srcSubpass=sourceSubpass,
		.dstSubpass=destinationSubpass,
		.srcStageMask=sourceStageMask,
		.dstStageMask=destinationStageMask,
		.srcAccessMask=sourceAccessMask,
		.dstAccessMask=destinationAccessMask,
		.dependencyFlags=dependencyFlags,
	};

	renderPass->subpassDependencies[renderPass->numSubpassDependencies]=Dependency;
	renderPass->numSubpassDependencies++;

	return VK_TRUE;
}

VkBool32 vkuInitRenderPass(VkuContext_t *context, VkuRenderPass_t *renderPass)
{
	if(!context)
		return VK_FALSE;

	if(!renderPass)
		return VK_FALSE;

	renderPass->device=context->device;

	renderPass->renderPass=VK_NULL_HANDLE;

	renderPass->numAttachments=0;
	memset(renderPass->attachments, 0, sizeof(VkuRenderPassAttachments_t)*VKU_MAX_RENDERPASS_ATTACHMENTS);

	renderPass->numSubpassDependencies=0;
	memset(renderPass->subpassDependencies, 0, sizeof(VkSubpassDependency)*VKU_MAX_RENDERPASS_SUBPASS_DEPENDENCIES);

	return VK_TRUE;
}

VkBool32 vkuCreateRenderPass(VkuRenderPass_t *renderPass)
{
	VkAttachmentDescription descriptions[VKU_MAX_RENDERPASS_ATTACHMENTS];

	uint32_t inputRefCount=0;
	VkAttachmentReference inputReferences[VKU_MAX_RENDERPASS_ATTACHMENTS];
	uint32_t colorRefCount=0;
	VkAttachmentReference colorReferences[VKU_MAX_RENDERPASS_ATTACHMENTS];
	uint32_t depthRefCount=0;
	VkAttachmentReference depthReference;
	uint32_t resolveRefCount=0;
	VkAttachmentReference resolveReference;

	uint32_t index=0;

	for(uint32_t i=0;i<renderPass->numAttachments;i++)
	{
		switch(renderPass->attachments[i].type)
		{
			case VKU_RENDERPASS_ATTACHMENT_INPUT:
				inputReferences[index].attachment=index;
				inputReferences[index].layout=renderPass->attachments[i].subpassLayout;
				inputRefCount++;
				break;

			case VKU_RENDERPASS_ATTACHMENT_COLOR:
				colorReferences[index].attachment=index;
				colorReferences[index].layout=renderPass->attachments[i].subpassLayout;
				colorRefCount++;
				break;

			case VKU_RENDERPASS_ATTACHMENT_DEPTH:
				depthReference.attachment=index;
				depthReference.layout=renderPass->attachments[i].subpassLayout;
				depthRefCount++;
				break;

			case VKU_RENDERPASS_ATTACHMENT_RESOLVE:
				resolveReference.attachment=index;
				resolveReference.layout=renderPass->attachments[i].subpassLayout;
				resolveRefCount++;
				break;

			default:
				break;
		};

		descriptions[index].flags=0;
		descriptions[index].format=renderPass->attachments[i].format;
		descriptions[index].samples=renderPass->attachments[i].samples;
		descriptions[index].loadOp=renderPass->attachments[i].loadOp;
		descriptions[index].storeOp=renderPass->attachments[i].storeOp;
		descriptions[index].stencilLoadOp=renderPass->attachments[i].loadOp;
		descriptions[index].stencilStoreOp=renderPass->attachments[i].storeOp;
		descriptions[index].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
		descriptions[index].finalLayout=renderPass->attachments[i].finalLayout;
		index++;
	}

	if(depthRefCount>1)
		return VK_FALSE; // Too many depth attachments

	if(resolveRefCount>1)
		return VK_FALSE; // Too many resolve attachments

	VkSubpassDescription subpasses=
	{
		.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,
		.inputAttachmentCount=inputRefCount,
		.pInputAttachments=inputReferences,
		.colorAttachmentCount=colorRefCount,
		.pColorAttachments=colorReferences,
		.pDepthStencilAttachment=depthRefCount?&depthReference:VK_NULL_HANDLE,
		.pResolveAttachments=resolveRefCount?&resolveReference:VK_NULL_HANDLE,
	};

	VkRenderPassCreateInfo renderPassCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=renderPass->numAttachments,
		.pAttachments=descriptions,
		.subpassCount=1,
		.pSubpasses=&subpasses,
		.dependencyCount=renderPass->numSubpassDependencies,
		.pDependencies=renderPass->subpassDependencies,
	};

	if(vkCreateRenderPass(renderPass->device, &renderPassCreateInfo, 0, &renderPass->renderPass)!=VK_SUCCESS)
		return VK_FALSE;

	return VK_TRUE;
}
