#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "system/system.h"
#include "vulkan/vulkan.h"
#include "math/math.h"
#include "utils/pipeline.h"
#include "font/font.h"
#include "camera/camera.h"
#include "ui/ui.h"
#include "vr/vr.h"
#include "perframe.h"

extern VkuContext_t vkContext;
extern VkuSwapchain_t swapchain;
extern XruContext_t xrContext;
extern bool isVR;

extern float fps, fTimeStep;

extern VkFormat colorFormat;

VkuImage_t colorResolve[2];		// left and right eye MSAA resolve color buffer
VkuImage_t colorBlur[2];		// left and right eye blur color buffer
VkuImage_t colorTemp[2];		// left and right eye blur color buffer

extern VkuImage_t depthImage[2];
extern VkuImage_t shadowDepth;

extern UI_t UI;
extern Font_t font;

Pipeline_t compositePipeline;
VkRenderPass compositeRenderPass;

Pipeline_t thresholdPipeline;
VkRenderPass thresholdRenderPass;
VkFramebuffer thresholdFramebuffer[2];

Pipeline_t gaussianPipeline;
VkRenderPass gaussianRenderPass;
VkFramebuffer gaussianFramebufferTemp[2];
VkFramebuffer gaussianFramebufferBlur[2];

bool CreateThresholdPipeline(void)
{
	vkCreateRenderPass(vkContext.device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=1,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=colorFormat,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
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
		.dependencyCount=3,
		.pDependencies=(VkSubpassDependency[])
		{
			{
				.srcSubpass=VK_SUBPASS_EXTERNAL,
				.dstSubpass=0,
				.srcStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT
			},
			{
				.srcSubpass=0,
				.dstSubpass=VK_SUBPASS_EXTERNAL,
				.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT
			},
			{
				.srcSubpass=VK_SUBPASS_EXTERNAL,
				.dstSubpass=0,
				.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT
			},
		},
	}, 0, &thresholdRenderPass);

	if(!CreatePipeline(&vkContext, &thresholdPipeline, thresholdRenderPass, "pipelines/threshold.pipeline"))
		return false;

	return true;
}

bool CreateGaussianPipeline(void)
{
	vkCreateRenderPass(vkContext.device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=1,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=colorFormat,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
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
				.srcStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT
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
	}, 0, &gaussianRenderPass);

	if(!CreatePipeline(&vkContext, &gaussianPipeline, gaussianRenderPass, "pipelines/gaussian.pipeline"))
		return false;

	return true;
}

void CreateCompositeFramebuffers(uint32_t eye)
{
	uint32_t targetWidth=0;
	uint32_t targetHeight=0;

	if(isVR)
	{
		targetWidth=xrContext.swapchainExtent.width;
		targetHeight=xrContext.swapchainExtent.height;
	}
	else
	{
		targetWidth=swapchain.extent.width;
		targetHeight=swapchain.extent.height;
	}

	vkuCreateTexture2D(&vkContext, &colorTemp[eye], targetWidth>>2, targetHeight>>2, colorFormat, VK_SAMPLE_COUNT_1_BIT);
	vkuCreateTexture2D(&vkContext, &colorBlur[eye], targetWidth>>2, targetHeight>>2, colorFormat, VK_SAMPLE_COUNT_1_BIT);

	VkCommandBuffer commandBuffer=vkuOneShotCommandBufferBegin(&vkContext);
	vkuTransitionLayout(commandBuffer, colorTemp[eye].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuTransitionLayout(commandBuffer, colorBlur[eye].image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuOneShotCommandBufferEnd(&vkContext, commandBuffer);

	// Threshold framebuffer contains 1/4 sized final main render frame, outputs to ColorBlur image
	vkCreateFramebuffer(vkContext.device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=thresholdRenderPass,
		.attachmentCount=1,
		.pAttachments=(VkImageView[]){ colorBlur[eye].imageView },
		.width=targetWidth>>2,
		.height=targetHeight>>2,
		.layers=1,
	}, 0, &thresholdFramebuffer[eye]);

	// Gaussian temp frame buffer takes output of thresholding pipeline and does the first gaussian blur pass, outputs to ColorTemp
	vkCreateFramebuffer(vkContext.device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=gaussianRenderPass,
		.attachmentCount=1,
		.pAttachments=(VkImageView[]){ colorTemp[eye].imageView },
		.width=targetWidth>>2,
		.height=targetHeight>>2,
		.layers=1,
	}, 0, &gaussianFramebufferTemp[eye]);

	// Gaussian blur frame buffer takes output of first pass gaussian blur and does the second gaussian blur pass, outputs to ColorBlur
	vkCreateFramebuffer(vkContext.device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=gaussianRenderPass,
		.attachmentCount=1,
		.pAttachments=(VkImageView[]){ colorBlur[eye].imageView },
		.width=targetWidth>>2,
		.height=targetHeight>>2,
		.layers=1,
	}, 0, &gaussianFramebufferBlur[eye]);

	if(!isVR)
	{
		// Compositing pipeline images, these are the actual swapchain framebuffers that will get presented
		for(uint32_t i=0;i<swapchain.numImages;i++)
		{
			vkCreateFramebuffer(vkContext.device, &(VkFramebufferCreateInfo)
			{
				.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.renderPass=compositeRenderPass,
				.attachmentCount=1,
				.pAttachments=(VkImageView[]){ swapchain.imageView[i] },
				.width=targetWidth,
				.height=targetHeight,
				.layers=1,
			}, 0, &perFrame[i].compositeFramebuffer[0]);
		}
	}
	else
	{
		// Compositing pipeline images, these are the actual swapchain framebuffers that will get presented
		for(uint32_t i=0;i<xrContext.swapchain[0].numImages;i++)
		{
			vkCreateFramebuffer(vkContext.device, &(VkFramebufferCreateInfo)
			{
				.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.renderPass=compositeRenderPass,
				.attachmentCount=1,
				.pAttachments=(VkImageView[]){ xrContext.swapchain[0].imageView[i] },
				.width=targetWidth,
				.height=targetHeight,
				.layers=1,
			}, 0, &perFrame[i].compositeFramebuffer[0]);

			vkCreateFramebuffer(vkContext.device, &(VkFramebufferCreateInfo)
			{
				.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.renderPass=compositeRenderPass,
				.attachmentCount=1,
				.pAttachments=(VkImageView[]){ xrContext.swapchain[1].imageView[i] },
				.width=targetWidth,
				.height=targetHeight,
				.layers=1,
			}, 0, &perFrame[i].compositeFramebuffer[1]);
		}
	}
}

bool CreateCompositePipeline(void)
{
	VkImageLayout attachementFinalLayout=VK_IMAGE_LAYOUT_UNDEFINED;
	VkFormat surfaceFormat=VK_FORMAT_UNDEFINED;

	// VR gets rendered directly to HMD, desktop needs to be presented
	if(!isVR)
	{
		attachementFinalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		surfaceFormat=swapchain.surfaceFormat.format;
	}
	else
	{
		attachementFinalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		surfaceFormat=xrContext.swapchainFormat;
	}

	vkCreateRenderPass(vkContext.device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=1,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=surfaceFormat,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout=attachementFinalLayout,
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
		.dependencyCount=3,
		.pDependencies=(VkSubpassDependency[])
		{
			{
				.srcSubpass=VK_SUBPASS_EXTERNAL,
				.dstSubpass=0,
				.srcStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT
			},
			{
				.srcSubpass=VK_SUBPASS_EXTERNAL,
				.dstSubpass=0,
				.srcStageMask=VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT|VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask=VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT
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
	}, 0, &compositeRenderPass);

	if(!CreatePipeline(&vkContext, &compositePipeline, compositeRenderPass, "pipelines/composite.pipeline"))
		return false;

	CreateThresholdPipeline();
	CreateGaussianPipeline();

	return true;
}

void DestroyCompositeFramebuffers(void)
{
	vkuDestroyImageBuffer(&vkContext, &colorBlur[0]);
	vkuDestroyImageBuffer(&vkContext, &colorTemp[0]);

	if(isVR)
	{
		vkuDestroyImageBuffer(&vkContext, &colorBlur[1]);
		vkuDestroyImageBuffer(&vkContext, &colorTemp[1]);
	}

	// Thresholding
	vkDestroyFramebuffer(vkContext.device, thresholdFramebuffer[0], VK_NULL_HANDLE);

	if(isVR)
		vkDestroyFramebuffer(vkContext.device, thresholdFramebuffer[1], VK_NULL_HANDLE);
	//////

	// Gaussian blur
	vkDestroyFramebuffer(vkContext.device, gaussianFramebufferTemp[0], VK_NULL_HANDLE);
	vkDestroyFramebuffer(vkContext.device, gaussianFramebufferBlur[0], VK_NULL_HANDLE);

	if(isVR)
	{
		vkDestroyFramebuffer(vkContext.device, gaussianFramebufferTemp[1], VK_NULL_HANDLE);
		vkDestroyFramebuffer(vkContext.device, gaussianFramebufferBlur[1], VK_NULL_HANDLE);
	}
	//////

	// Compositing
	for(uint32_t i=0;i<swapchain.numImages;i++)
	{
		vkDestroyFramebuffer(vkContext.device, perFrame[i].compositeFramebuffer[0], VK_NULL_HANDLE);

		if(isVR)
			vkDestroyFramebuffer(vkContext.device, perFrame[i].compositeFramebuffer[1], VK_NULL_HANDLE);
	}
	//////
}

void DestroyComposite(void)
{
	DestroyCompositeFramebuffers();

	// Thresholding pipeline
	DestroyPipeline(&vkContext, &thresholdPipeline);
	vkDestroyRenderPass(vkContext.device, thresholdRenderPass, VK_NULL_HANDLE);
	//////

	// Gaussian blur pipeline
	DestroyPipeline(&vkContext, &gaussianPipeline);
	vkDestroyRenderPass(vkContext.device, gaussianRenderPass, VK_NULL_HANDLE);
	//////

	// Compositing pipeline
	DestroyPipeline(&vkContext, &compositePipeline);
	vkDestroyRenderPass(vkContext.device, compositeRenderPass, VK_NULL_HANDLE);
	//////
}

void CompositeDraw(uint32_t index, uint32_t eye)
{
	static uint32_t uFrame=0;
	uint32_t width=0, height=0;

	if(isVR)
	{
		width=xrContext.swapchainExtent.width;
		height=xrContext.swapchainExtent.height;
	}
	else
	{
		width=swapchain.extent.width;
		height=swapchain.extent.height;
	}

	// Threshold and down sample to 1/4 original image size
	// Input = colorResolve
	// Output = colorBlur
	vkCmdBeginRenderPass(perFrame[index].commandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=thresholdRenderPass,
		.framebuffer=thresholdFramebuffer[eye],
		.clearValueCount=1,
		.pClearValues=(VkClearValue[]){ {{{ 1.0f, 0.0f, 0.0f, 1.0f }}} },
		.renderArea={ { 0, 0 }, { width>>2, height>>2 } },
	}, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(perFrame[index].commandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)(width>>2), (float)(height>>2), 0.0f, 1.0f });
	vkCmdSetScissor(perFrame[index].commandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { width>>2, height>>2 } });

	vkCmdBindPipeline(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, thresholdPipeline.pipeline.pipeline);

	vkuDescriptorSet_UpdateBindingImageInfo(&thresholdPipeline.descriptorSet, 0, colorResolve[eye].sampler, colorResolve[eye].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkuAllocateUpdateDescriptorSet(&thresholdPipeline.descriptorSet, perFrame[index].descriptorPool);
	vkCmdBindDescriptorSets(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, thresholdPipeline.pipelineLayout, 0, 1, &thresholdPipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(perFrame[index].commandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(perFrame[index].commandBuffer);
	//////

	// Gaussian blur (vertical)
	// Input = colorBlur
	// Output = colorTemp
	vkCmdBeginRenderPass(perFrame[index].commandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=gaussianRenderPass,
		.framebuffer=gaussianFramebufferTemp[eye],
		.clearValueCount=1,
		.pClearValues=(VkClearValue[]){ {{{ 1.0f, 0.0f, 0.0f, 1.0f }}} },
		.renderArea={ { 0, 0 }, { width>>2, height>>2 } },
	}, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(perFrame[index].commandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)(width>>2), (float)(height>>2), 0.0f, 1.0f });
	vkCmdSetScissor(perFrame[index].commandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { width>>2, height>>2 } });

	vkCmdBindPipeline(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gaussianPipeline.pipeline.pipeline);

	vkCmdPushConstants(perFrame[index].commandBuffer, gaussianPipeline.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float)*2, &(float[]){ 1.0f, 0.0 });

	vkuDescriptorSet_UpdateBindingImageInfo(&gaussianPipeline.descriptorSet, 0, colorBlur[eye].sampler, colorBlur[eye].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuAllocateUpdateDescriptorSet(&gaussianPipeline.descriptorSet, perFrame[index].descriptorPool);

	vkCmdBindDescriptorSets(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gaussianPipeline.pipelineLayout, 0, 1, &gaussianPipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(perFrame[index].commandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(perFrame[index].commandBuffer);
	//////

	// Gaussian blur (horizontal)
	// Input = colorTemp
	// Output = colorBlur
	vkCmdBeginRenderPass(perFrame[index].commandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=gaussianRenderPass,
		.framebuffer=gaussianFramebufferBlur[eye],
		.clearValueCount=1,
		.pClearValues=(VkClearValue[]){ {{{ 1.0f, 0.0f, 0.0f, 1.0f }}} },
		.renderArea={ { 0, 0 }, { width>>2, height>>2 } },
	}, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(perFrame[index].commandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)(width>>2), (float)(height>>2), 0.0f, 1.0f });
	vkCmdSetScissor(perFrame[index].commandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { width>>2, height>>2 } });

	vkCmdBindPipeline(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gaussianPipeline.pipeline.pipeline);

	vkCmdPushConstants(perFrame[index].commandBuffer, gaussianPipeline.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float)*2, &(float[]){ 0.0f, 1.0 });

	vkuDescriptorSet_UpdateBindingImageInfo(&gaussianPipeline.descriptorSet, 0, colorTemp[eye].sampler, colorTemp[eye].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuAllocateUpdateDescriptorSet(&gaussianPipeline.descriptorSet, perFrame[index].descriptorPool);

	vkCmdBindDescriptorSets(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gaussianPipeline.pipelineLayout, 0, 1, &gaussianPipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(perFrame[index].commandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(perFrame[index].commandBuffer);
	//////

	// Draw final composited image
	// Input = colorResolve, colorBlur
	// Output = swapchain
	// NOTE: ColorResolve should already be in shader read-only
	vkCmdBeginRenderPass(perFrame[index].commandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=compositeRenderPass,
		.framebuffer=perFrame[index].compositeFramebuffer[eye],
		.clearValueCount=1,
		.pClearValues=(VkClearValue[]){ {{{ 0.0f, 0.0f, 0.0f, 1.0f }}} },
		.renderArea={ { 0, 0 }, { width, height } },
	}, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(perFrame[index].commandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f });
	vkCmdSetScissor(perFrame[index].commandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { width, height } });

	vkCmdBindPipeline(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline.pipeline.pipeline);

	vkuDescriptorSet_UpdateBindingImageInfo(&compositePipeline.descriptorSet, 0, colorResolve[eye].sampler, colorResolve[eye].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&compositePipeline.descriptorSet, 1, colorBlur[eye].sampler, colorBlur[eye].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&compositePipeline.descriptorSet, 2, depthImage[eye].sampler, depthImage[eye].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&compositePipeline.descriptorSet, 3, shadowDepth.sampler, shadowDepth.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingBufferInfo(&compositePipeline.descriptorSet, 4, perFrame[index].mainUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);

	vkuAllocateUpdateDescriptorSet(&compositePipeline.descriptorSet, perFrame[index].descriptorPool);
	vkCmdBindDescriptorSets(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline.pipelineLayout, 0, 1, &compositePipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	struct
	{
		uint32_t uFrame;
		uint32_t uSize[2];
		uint32_t Pad;
	} PC;

	PC.uFrame=uFrame++;
	PC.uSize[0]=width;
	PC.uSize[1]=height;

	vkCmdPushConstants(perFrame[index].commandBuffer, compositePipeline.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PC), &PC);

	vkCmdDraw(perFrame[index].commandBuffer, 3, 1, 0, 0);

	// Draw UI controls
	UI_Draw(&UI, index, eye, fTimeStep);

	// Draw text in the compositing renderpass
	Font_Print(&font, 16.0f, 0.0f, 0.0f, "FPS: %0.1f\n\x1B[91mFrame time: %0.5fms", fps, fTimeStep*1000.0f);

	Font_Draw(&font, index, eye);

	vkCmdEndRenderPass(perFrame[index].commandBuffer);
}
