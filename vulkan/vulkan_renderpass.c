#include "vulkan.h"

VkBool32 vkuRenderPass_AddAttachment(VkuRenderPass_t *RenderPass, VkuRenderPassAttachmentType Type, VkFormat Format, VkSampleCountFlagBits Samples, VkAttachmentLoadOp LoadOp, VkAttachmentStoreOp StoreOp, VkImageLayout SubpassLayout, VkImageLayout FinalLayout)
{
	if(!RenderPass)
		return VK_FALSE;

	if(RenderPass->NumAttachments>=VKU_MAX_RENDERPASS_ATTACHMENTS)
		return VK_FALSE;

	RenderPass->Attachments[RenderPass->NumAttachments].Type=Type;
	RenderPass->Attachments[RenderPass->NumAttachments].Format=Format;
	RenderPass->Attachments[RenderPass->NumAttachments].Samples=Samples;
	RenderPass->Attachments[RenderPass->NumAttachments].LoadOp=LoadOp;
	RenderPass->Attachments[RenderPass->NumAttachments].StoreOp=StoreOp;
	RenderPass->Attachments[RenderPass->NumAttachments].SubpassLayout=SubpassLayout;
	RenderPass->Attachments[RenderPass->NumAttachments].FinalLayout=FinalLayout;
	RenderPass->NumAttachments++;

	return VK_TRUE;
}

VkBool32 vkuRenderPass_AddSubpassDependency(VkuRenderPass_t *RenderPass, uint32_t SourceSubpass, uint32_t DestinationSubpass, VkPipelineStageFlags SourceStageMask, VkPipelineStageFlags DestinationStageMask, VkAccessFlags SourceAccessMask, VkAccessFlags DestinationAccessMask, VkDependencyFlags DependencyFlags)
{
	if(!RenderPass)
		return VK_FALSE;

	if(RenderPass->NumSubpassDependencies>=VKU_MAX_RENDERPASS_SUBPASS_DEPENDENCIES)
		return VK_FALSE;

	VkSubpassDependency Dependency=
	{
		.srcSubpass=SourceSubpass,
		.dstSubpass=DestinationSubpass,
		.srcStageMask=SourceStageMask,
		.dstStageMask=DestinationStageMask,
		.srcAccessMask=SourceAccessMask,
		.dstAccessMask=DestinationAccessMask,
		.dependencyFlags=DependencyFlags,
	};

	RenderPass->SubpassDependencies[RenderPass->NumSubpassDependencies]=Dependency;
	RenderPass->NumSubpassDependencies++;

	return VK_TRUE;
}

VkBool32 vkuInitRenderPass(VkuContext_t *Context, VkuRenderPass_t *RenderPass)
{
	if(!Context)
		return VK_FALSE;

	if(!RenderPass)
		return VK_FALSE;

	RenderPass->Device=Context->Device;

	RenderPass->RenderPass=VK_NULL_HANDLE;

	RenderPass->NumAttachments=0;
	memset(RenderPass->Attachments, 0, sizeof(VkuRenderPassAttachments_t)*VKU_MAX_RENDERPASS_ATTACHMENTS);

	RenderPass->NumSubpassDependencies=0;
	memset(RenderPass->SubpassDependencies, 0, sizeof(VkSubpassDependency)*VKU_MAX_RENDERPASS_SUBPASS_DEPENDENCIES);

	return VK_TRUE;
}

VkBool32 vkuCreateRenderPass(VkuRenderPass_t *RenderPass)
{
	VkAttachmentDescription Descriptions[VKU_MAX_RENDERPASS_ATTACHMENTS];

	uint32_t InputRefCount=0;
	VkAttachmentReference InputReferences[VKU_MAX_RENDERPASS_ATTACHMENTS];
	uint32_t ColorRefCount=0;
	VkAttachmentReference ColorReferences[VKU_MAX_RENDERPASS_ATTACHMENTS];
	uint32_t DepthRefCount=0;
	VkAttachmentReference DepthReference;
	uint32_t ResolveRefCount=0;
	VkAttachmentReference ResolveReference;

	uint32_t Index=0;

	for(uint32_t i=0;i<RenderPass->NumAttachments;i++)
	{
		switch(RenderPass->Attachments[i].Type)
		{
			case VKU_RENDERPASS_ATTACHMENT_INPUT:
				InputReferences[Index].attachment=Index;
				InputReferences[Index].layout=RenderPass->Attachments[i].SubpassLayout;
				InputRefCount++;
				break;

			case VKU_RENDERPASS_ATTACHMENT_COLOR:
				ColorReferences[Index].attachment=Index;
				ColorReferences[Index].layout=RenderPass->Attachments[i].SubpassLayout;
				ColorRefCount++;
				break;

			case VKU_RENDERPASS_ATTACHMENT_DEPTH:
				DepthReference.attachment=Index;
				DepthReference.layout=RenderPass->Attachments[i].SubpassLayout;
				DepthRefCount++;
				break;

			case VKU_RENDERPASS_ATTACHMENT_RESOLVE:
				ResolveReference.attachment=Index;
				ResolveReference.layout=RenderPass->Attachments[i].SubpassLayout;
				ResolveRefCount++;
				break;

			default:
				break;
		};

		Descriptions[Index].flags=0;
		Descriptions[Index].format=RenderPass->Attachments[i].Format;
		Descriptions[Index].samples=RenderPass->Attachments[i].Samples;
		Descriptions[Index].loadOp=RenderPass->Attachments[i].LoadOp;
		Descriptions[Index].storeOp=RenderPass->Attachments[i].StoreOp;
		Descriptions[Index].stencilLoadOp=RenderPass->Attachments[i].LoadOp;
		Descriptions[Index].stencilStoreOp=RenderPass->Attachments[i].StoreOp;
		Descriptions[Index].initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
		Descriptions[Index].finalLayout=RenderPass->Attachments[i].FinalLayout;
		Index++;
	}

	if(DepthRefCount>1)
		return VK_FALSE; // Too many depth attachments

	if(ResolveRefCount>1)
		return VK_FALSE; // Too many resolve attachments

	VkSubpassDescription Subpasses=
	{
		.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,
		.inputAttachmentCount=InputRefCount,
		.pInputAttachments=InputReferences,
		.colorAttachmentCount=ColorRefCount,
		.pColorAttachments=ColorReferences,
		.pDepthStencilAttachment=DepthRefCount?&DepthReference:VK_NULL_HANDLE,
		.pResolveAttachments=ResolveRefCount?&ResolveReference:VK_NULL_HANDLE,
	};

	VkRenderPassCreateInfo RenderPassCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=RenderPass->NumAttachments,
		.pAttachments=Descriptions,
		.subpassCount=1,
		.pSubpasses=&Subpasses,
		.dependencyCount=RenderPass->NumSubpassDependencies,
		.pDependencies=RenderPass->SubpassDependencies,
	};

	if(vkCreateRenderPass(RenderPass->Device, &RenderPassCreateInfo, 0, &RenderPass->RenderPass)!=VK_SUCCESS)
		return VK_FALSE;

	return VK_TRUE;
}