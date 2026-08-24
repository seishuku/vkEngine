#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../system/system.h"
#include "../../vulkan/vulkan.h"
#include "../../math/math.h"
#include "../../utils/postprocess.h"
#include "../../utils/pipeline.h"
#include "../../perframe.h"
#include "bloom.h"

extern VkuContext_t vkContext;

typedef struct
{
	VkRenderPass renderPass;

	Pipeline_t thresholdPipeline;
	VkFramebuffer thresholdFramebuffer[2];

	Pipeline_t gaussianPipeline;
	VkFramebuffer gaussianFramebufferTemp[2];
	VkFramebuffer gaussianFramebufferBlur[2];

	VkuImage_t colorTemp[2];
	VkuImage_t colorBlur[2];

	uint32_t width, height;
} BloomData_t;

static BloomData_t bloomData;

static bool Bloom_Create(PostProcessEffect_t *self)
{
	BloomData_t *bloom=(BloomData_t *)self->data;

	if(vkCreateRenderPass(vkContext.device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=1,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=config.colorFormat,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			}
		},
		.subpassCount=1,
		.pSubpasses=&(VkSubpassDescription)
		{
			.pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS,
			.colorAttachmentCount=1,
			.pColorAttachments=&(VkAttachmentReference)
			{
				.attachment=0,
				.layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			}
		},
		.dependencyCount=2,
		.pDependencies=(VkSubpassDependency[])
		{
			{
				.srcSubpass=VK_SUBPASS_EXTERNAL,
				.dstSubpass=0,
				.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dependencyFlags=0
			},
			{
				.srcSubpass=0,
				.dstSubpass=VK_SUBPASS_EXTERNAL,
				.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT
			}
		},
	}, 0, &bloom->renderPass)!=VK_SUCCESS)
		return false;

	if(!CreatePipeline(&vkContext, &bloom->thresholdPipeline, bloom->renderPass, "pipelines/threshold.pipeline"))
		return false;

	if(!CreatePipeline(&vkContext, &bloom->gaussianPipeline, bloom->renderPass, "pipelines/gaussian.pipeline"))
		return false;

	return true;
}

static void Bloom_Destroy(PostProcessEffect_t *self)
{
	BloomData_t *bloom=(BloomData_t *)self->data;

	vkDestroyRenderPass(vkContext.device, bloom->renderPass, VK_NULL_HANDLE);

	DestroyPipeline(&vkContext, &bloom->gaussianPipeline);
	DestroyPipeline(&vkContext, &bloom->thresholdPipeline);
}

static bool Bloom_CreateFramebuffers(PostProcessEffect_t *self, uint32_t width, uint32_t height)
{
	BloomData_t *bloom=(BloomData_t *)self->data;

	bloom->width=width>>2;
	bloom->height=height>>2;

	const uint32_t numEyes=config.isVR?2:1;

	for(uint32_t eye=0;eye<numEyes;eye++)
	{
		vkuCreateTexture2D(&vkContext, &bloom->colorTemp[eye], bloom->width, bloom->height, config.colorFormat, VK_SAMPLE_COUNT_1_BIT);
		vkuCreateTexture2D(&vkContext, &bloom->colorBlur[eye], bloom->width, bloom->height, config.colorFormat, VK_SAMPLE_COUNT_1_BIT);

		VkCommandBuffer commandBuffer=vkuOneShotCommandBufferBegin(&vkContext);
		vkuTransitionLayout(commandBuffer, bloom->colorTemp[eye].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vkuTransitionLayout(commandBuffer, bloom->colorBlur[eye].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vkuOneShotCommandBufferEnd(&vkContext, commandBuffer);

		// Threshold framebuffer: downsampled main render -> colorBlur
		if(vkCreateFramebuffer(vkContext.device, &(VkFramebufferCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass=bloom->renderPass,
			.attachmentCount=1,
			.pAttachments=(VkImageView[]){ bloom->colorBlur[eye].imageView },
			.width=bloom->width,
			.height=bloom->height,
			.layers=1,
		}, 0, &bloom->thresholdFramebuffer[eye])!=VK_SUCCESS)
			return false;

		// Gaussian pass 1 (vertical): colorBlur -> colorTemp
		if(vkCreateFramebuffer(vkContext.device, &(VkFramebufferCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass=bloom->renderPass,
			.attachmentCount=1,
			.pAttachments=(VkImageView[]){ bloom->colorTemp[eye].imageView },
			.width=bloom->width,
			.height=bloom->height,
			.layers=1,
		}, 0, &bloom->gaussianFramebufferTemp[eye])!=VK_SUCCESS)
			return false;

		// Gaussian pass 2 (horizontal): colorTemp -> colorBlur
		if(vkCreateFramebuffer(vkContext.device, &(VkFramebufferCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass=bloom->renderPass,
			.attachmentCount=1,
			.pAttachments=(VkImageView[]){ bloom->colorBlur[eye].imageView },
			.width=bloom->width,
			.height=bloom->height,
			.layers=1,
		}, 0, &bloom->gaussianFramebufferBlur[eye])!=VK_SUCCESS)
			return false;
	}

	return true;
}

static void Bloom_DestroyFramebuffers(PostProcessEffect_t *self)
{
	BloomData_t *bloom=(BloomData_t *)self->data;

	const uint32_t numEyes=config.isVR?2:1;

	for(uint32_t eye=0;eye<numEyes;eye++)
	{
		vkDestroyFramebuffer(vkContext.device, bloom->thresholdFramebuffer[eye], VK_NULL_HANDLE);
		vkDestroyFramebuffer(vkContext.device, bloom->gaussianFramebufferTemp[eye], VK_NULL_HANDLE);
		vkDestroyFramebuffer(vkContext.device, bloom->gaussianFramebufferBlur[eye], VK_NULL_HANDLE);

		vkuDestroyImageBuffer(&vkContext, &bloom->colorTemp[eye]);
		vkuDestroyImageBuffer(&vkContext, &bloom->colorBlur[eye]);
	}
}

static void Bloom_Draw(PostProcessEffect_t *self, VkCommandBuffer commandBuffer, uint32_t frameIndex, uint32_t eye, VkImageView inputView, VkSampler inputSampler)
{
	BloomData_t *bloom=(BloomData_t *)self->data;

	// Threshold and downsample
	// Input = inputView (whatever feeds this effect - colorResolve, or a prior effect's output)
	// Output = colorBlur
	vkCmdBeginRenderPass(commandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=bloom->renderPass,
		.framebuffer=bloom->thresholdFramebuffer[eye],
		.renderArea={ { 0, 0 }, { bloom->width, bloom->height } },
	}, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(commandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)bloom->width, (float)bloom->height, 0.0f, 1.0f });
	vkCmdSetScissor(commandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { bloom->width, bloom->height } });

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bloom->thresholdPipeline.pipeline.pipeline);

	vkuDescriptorSet_UpdateBindingImageInfo(&bloom->thresholdPipeline.descriptorSet, 0, inputSampler, inputView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuAllocateUpdateDescriptorSet(&bloom->thresholdPipeline.descriptorSet, perFrame[frameIndex].descriptorPool);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bloom->thresholdPipeline.pipelineLayout, 0, 1, &bloom->thresholdPipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(commandBuffer);
	//////

	// Gaussian blur (vertical)
	// Input = colorBlur
	// Output = colorTemp
	vkCmdBeginRenderPass(commandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=bloom->renderPass,
		.framebuffer=bloom->gaussianFramebufferTemp[eye],
		.renderArea={ { 0, 0 }, { bloom->width, bloom->height } },
	}, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(commandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)bloom->width, (float)bloom->height, 0.0f, 1.0f });
	vkCmdSetScissor(commandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { bloom->width, bloom->height } });

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bloom->gaussianPipeline.pipeline.pipeline);

#ifdef ANDROID
	// TODO: Need a better fix for this, Android/Vulkan pre-transform screws this up
	vkCmdPushConstants(commandBuffer, bloom->gaussianPipeline.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(vec2), &(vec2){ (float)config.renderWidth/config.renderHeight, 0.0f });
#else
	vkCmdPushConstants(commandBuffer, bloom->gaussianPipeline.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(vec2), &(vec2){ 1.0f, 0.0f });
#endif

	vkuDescriptorSet_UpdateBindingImageInfo(&bloom->gaussianPipeline.descriptorSet, 0, bloom->colorBlur[eye].sampler, bloom->colorBlur[eye].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuAllocateUpdateDescriptorSet(&bloom->gaussianPipeline.descriptorSet, perFrame[frameIndex].descriptorPool);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bloom->gaussianPipeline.pipelineLayout, 0, 1, &bloom->gaussianPipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(commandBuffer);
	//////

	// Gaussian blur (horizontal)
	// Input = colorTemp
	// Output = colorBlur
	vkCmdBeginRenderPass(commandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=bloom->renderPass,
		.framebuffer=bloom->gaussianFramebufferBlur[eye],
		.renderArea={ { 0, 0 }, { bloom->width, bloom->height } },
	}, VK_SUBPASS_CONTENTS_INLINE);

#ifdef ANDROID
	// TODO: Need a better fix for this, Android/Vulkan pre-transform screws this up
	vkCmdPushConstants(commandBuffer, bloom->gaussianPipeline.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(vec2), &(vec2){ 0.0f, (float)config.renderHeight/config.renderWidth });
#else
	vkCmdPushConstants(commandBuffer, bloom->gaussianPipeline.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(vec2), &(vec2){ 0.0f, 1.0f });
#endif

	vkuDescriptorSet_UpdateBindingImageInfo(&bloom->gaussianPipeline.descriptorSet, 0, bloom->colorTemp[eye].sampler, bloom->colorTemp[eye].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuAllocateUpdateDescriptorSet(&bloom->gaussianPipeline.descriptorSet, perFrame[frameIndex].descriptorPool);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bloom->gaussianPipeline.pipelineLayout, 0, 1, &bloom->gaussianPipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(commandBuffer);
	//////
}

static VkImageView Bloom_GetOutputView(PostProcessEffect_t *self, uint32_t eye)
{
	BloomData_t *bloom=(BloomData_t *)self->data;

	return bloom->colorBlur[eye].imageView;
}

static VkSampler Bloom_GetOutputSampler(PostProcessEffect_t *self, uint32_t eye)
{
	BloomData_t *bloom=(BloomData_t *)self->data;

	return bloom->colorBlur[eye].sampler;
}

PostProcessEffect_t bloomEffect=
{
	.name="Bloom",
	.Create=Bloom_Create,
	.Destroy=Bloom_Destroy,
	.CreateFramebuffers=Bloom_CreateFramebuffers,
	.DestroyFramebuffers=Bloom_DestroyFramebuffers,
	.Draw=Bloom_Draw,
	.GetOutputView=Bloom_GetOutputView,
	.GetOutputSampler=Bloom_GetOutputSampler,
	.enabled=true,
	.data=&bloomData,
};
