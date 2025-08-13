#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../utils/pipeline.h"
#include "../font/font.h"
#include "../ui/ui.h"
#include "../vr/vr.h"
#include "../perframe.h"
#include "linegraph.h"

extern VkuContext_t vkContext;
extern VkuSwapchain_t swapchain;
extern XruContext_t xrContext;

extern float fps, fTimeStep, audioTime, physicsTime;

VkuImage_t colorResolve[2];		// left and right eye MSAA resolve color buffer
VkuImage_t colorBlur[2];		// left and right eye blur color buffer
VkuImage_t colorTemp[2];		// left and right eye blur color buffer

extern VkuImage_t depthImage[2];
extern VkuImage_t shadowDepth;

extern UI_t UI;
extern Font_t font;

extern LineGraph_t frameTimes, audioTimes, physicsTimes;

Pipeline_t compositePipeline;
VkRenderPass compositeRenderPass;

VkFramebuffer compositeFramebuffer[VKU_MAX_FRAME_COUNT][2];

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
	}, 0, &gaussianRenderPass);

	if(!CreatePipeline(&vkContext, &gaussianPipeline, gaussianRenderPass, "pipelines/gaussian.pipeline"))
		return false;

	return true;
}

void CreateCompositeFramebuffers(uint32_t eye)
{
	vkuCreateTexture2D(&vkContext, &colorTemp[eye], config.renderWidth>>2, config.renderHeight>>2, config.colorFormat, VK_SAMPLE_COUNT_1_BIT);
	vkuCreateTexture2D(&vkContext, &colorBlur[eye], config.renderWidth>>2, config.renderHeight>>2, config.colorFormat, VK_SAMPLE_COUNT_1_BIT);

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
		.width=config.renderWidth>>2,
		.height=config.renderHeight>>2,
		.layers=1,
	}, 0, &thresholdFramebuffer[eye]);

	// Gaussian temp frame buffer takes output of thresholding pipeline and does the first gaussian blur pass, outputs to ColorTemp
	vkCreateFramebuffer(vkContext.device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=gaussianRenderPass,
		.attachmentCount=1,
		.pAttachments=(VkImageView[]){ colorTemp[eye].imageView },
		.width=config.renderWidth>>2,
		.height=config.renderHeight>>2,
		.layers=1,
	}, 0, &gaussianFramebufferTemp[eye]);

	// Gaussian blur frame buffer takes output of first pass gaussian blur and does the second gaussian blur pass, outputs to ColorBlur
	vkCreateFramebuffer(vkContext.device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=gaussianRenderPass,
		.attachmentCount=1,
		.pAttachments=(VkImageView[]){ colorBlur[eye].imageView },
		.width=config.renderWidth>>2,
		.height=config.renderHeight>>2,
		.layers=1,
	}, 0, &gaussianFramebufferBlur[eye]);

	if(!config.isVR)
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
				.width=config.renderWidth,
				.height=config.renderHeight,
				.layers=1,
			}, 0, &compositeFramebuffer[i][0]);
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
				.width=config.renderWidth,
				.height=config.renderHeight,
				.layers=1,
			}, 0, &compositeFramebuffer[i][0]);

			vkCreateFramebuffer(vkContext.device, &(VkFramebufferCreateInfo)
			{
				.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.renderPass=compositeRenderPass,
				.attachmentCount=1,
				.pAttachments=(VkImageView[]){ xrContext.swapchain[1].imageView[i] },
				.width=config.renderWidth,
				.height=config.renderHeight,
				.layers=1,
			}, 0, &compositeFramebuffer[i][1]);
		}
	}
}

bool CreateCompositePipeline(void)
{
	VkImageLayout attachementFinalLayout=VK_IMAGE_LAYOUT_UNDEFINED;
	VkFormat surfaceFormat=VK_FORMAT_UNDEFINED;

	// VR gets rendered directly to HMD, desktop needs to be presented
	if(!config.isVR)
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
				.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
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
		.dependencyCount=1,
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

	if(config.isVR)
	{
		vkuDestroyImageBuffer(&vkContext, &colorBlur[1]);
		vkuDestroyImageBuffer(&vkContext, &colorTemp[1]);
	}

	// Thresholding
	vkDestroyFramebuffer(vkContext.device, thresholdFramebuffer[0], VK_NULL_HANDLE);

	if(config.isVR)
		vkDestroyFramebuffer(vkContext.device, thresholdFramebuffer[1], VK_NULL_HANDLE);
	//////

	// Gaussian blur
	vkDestroyFramebuffer(vkContext.device, gaussianFramebufferTemp[0], VK_NULL_HANDLE);
	vkDestroyFramebuffer(vkContext.device, gaussianFramebufferBlur[0], VK_NULL_HANDLE);

	if(config.isVR)
	{
		vkDestroyFramebuffer(vkContext.device, gaussianFramebufferTemp[1], VK_NULL_HANDLE);
		vkDestroyFramebuffer(vkContext.device, gaussianFramebufferBlur[1], VK_NULL_HANDLE);
	}
	//////

	// Compositing
	for(uint32_t i=0;i<swapchain.numImages;i++)
	{
		vkDestroyFramebuffer(vkContext.device, compositeFramebuffer[i][0], VK_NULL_HANDLE);

		if(config.isVR)
			vkDestroyFramebuffer(vkContext.device, compositeFramebuffer[i][1], VK_NULL_HANDLE);
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

void CompositeDraw(uint32_t imageIndex, uint32_t frameIndex, uint32_t eye)
{
	static uint32_t uFrame=0;

	// Threshold and down sample to 1/4 original image size
	// Input = colorResolve
	// Output = colorBlur
	vkCmdBeginRenderPass(perFrame[frameIndex].commandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=thresholdRenderPass,
		.framebuffer=thresholdFramebuffer[eye],
		.renderArea={ { 0, 0 }, { config.renderWidth>>2, config.renderHeight>>2 } },
	}, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(perFrame[frameIndex].commandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)(config.renderWidth>>2), (float)(config.renderHeight>>2), 0.0f, 1.0f });
	vkCmdSetScissor(perFrame[frameIndex].commandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { config.renderWidth>>2, config.renderHeight>>2 } });

	vkCmdBindPipeline(perFrame[frameIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, thresholdPipeline.pipeline.pipeline);

	vkuDescriptorSet_UpdateBindingImageInfo(&thresholdPipeline.descriptorSet, 0, colorResolve[eye].sampler, colorResolve[eye].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkuAllocateUpdateDescriptorSet(&thresholdPipeline.descriptorSet, perFrame[frameIndex].descriptorPool);
	vkCmdBindDescriptorSets(perFrame[frameIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, thresholdPipeline.pipelineLayout, 0, 1, &thresholdPipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(perFrame[frameIndex].commandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(perFrame[frameIndex].commandBuffer);
	//////

	// Gaussian blur (vertical)
	// Input = colorBlur
	// Output = colorTemp
	vkCmdBeginRenderPass(perFrame[frameIndex].commandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=gaussianRenderPass,
		.framebuffer=gaussianFramebufferTemp[eye],
		.renderArea={ { 0, 0 }, { config.renderWidth>>2, config.renderHeight>>2 } },
	}, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(perFrame[frameIndex].commandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)(config.renderWidth>>2), (float)(config.renderHeight>>2), 0.0f, 1.0f });
	vkCmdSetScissor(perFrame[frameIndex].commandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { config.renderWidth>>2, config.renderHeight>>2 } });

	vkCmdBindPipeline(perFrame[frameIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gaussianPipeline.pipeline.pipeline);

	vkCmdPushConstants(perFrame[frameIndex].commandBuffer, gaussianPipeline.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(vec2), &(vec2){ 1.0f, 0.0 });

	vkuDescriptorSet_UpdateBindingImageInfo(&gaussianPipeline.descriptorSet, 0, colorBlur[eye].sampler, colorBlur[eye].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuAllocateUpdateDescriptorSet(&gaussianPipeline.descriptorSet, perFrame[frameIndex].descriptorPool);

	vkCmdBindDescriptorSets(perFrame[frameIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gaussianPipeline.pipelineLayout, 0, 1, &gaussianPipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(perFrame[frameIndex].commandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(perFrame[frameIndex].commandBuffer);
	//////

	// Gaussian blur (horizontal)
	// Input = colorTemp
	// Output = colorBlur
	vkCmdBeginRenderPass(perFrame[frameIndex].commandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=gaussianRenderPass,
		.framebuffer=gaussianFramebufferBlur[eye],
		.renderArea={ { 0, 0 }, { config.renderWidth>>2, config.renderHeight>>2 } },
	}, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdPushConstants(perFrame[frameIndex].commandBuffer, gaussianPipeline.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(vec2), &(vec2){ 0.0f, 1.0f });

	vkuDescriptorSet_UpdateBindingImageInfo(&gaussianPipeline.descriptorSet, 0, colorTemp[eye].sampler, colorTemp[eye].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuAllocateUpdateDescriptorSet(&gaussianPipeline.descriptorSet, perFrame[frameIndex].descriptorPool);

	vkCmdBindDescriptorSets(perFrame[frameIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gaussianPipeline.pipelineLayout, 0, 1, &gaussianPipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(perFrame[frameIndex].commandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(perFrame[frameIndex].commandBuffer);
	//////

	// Draw final composited image
	// Input = colorResolve, colorBlur
	// Output = swapchain
	// NOTE: ColorResolve should already be in shader read-only
	vkCmdBeginRenderPass(perFrame[frameIndex].commandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=compositeRenderPass,
		.framebuffer=compositeFramebuffer[imageIndex][eye],
		.clearValueCount=1,
		.pClearValues=(VkClearValue[]){ {{{ 0.0f, 0.0f, 0.0f, 1.0f }}} },
		.renderArea={ { 0, 0 }, { config.renderWidth, config.renderHeight } },
	}, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(perFrame[frameIndex].commandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)config.renderWidth, (float)config.renderHeight, 0.0f, 1.0f });
	vkCmdSetScissor(perFrame[frameIndex].commandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { config.renderWidth, config.renderHeight } });

	vkCmdBindPipeline(perFrame[frameIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline.pipeline.pipeline);

	vkuDescriptorSet_UpdateBindingImageInfo(&compositePipeline.descriptorSet, 0, colorResolve[eye].sampler, colorResolve[eye].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&compositePipeline.descriptorSet, 1, colorBlur[eye].sampler, colorBlur[eye].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&compositePipeline.descriptorSet, 2, depthImage[eye].sampler, depthImage[eye].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&compositePipeline.descriptorSet, 3, shadowDepth.sampler, shadowDepth.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingBufferInfo(&compositePipeline.descriptorSet, 4, perFrame[frameIndex].mainUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);

	vkuAllocateUpdateDescriptorSet(&compositePipeline.descriptorSet, perFrame[frameIndex].descriptorPool);
	vkCmdBindDescriptorSets(perFrame[frameIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline.pipelineLayout, 0, 1, &compositePipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	struct
	{
		uint32_t uFrame;
		uint32_t uSize[2];
		uint32_t uSamples;
	} PC;

	PC.uFrame=uFrame++;
	PC.uSize[0]=config.renderWidth;
	PC.uSize[1]=config.renderHeight;
	PC.uSamples=config.msaaSamples;

	matrix mvp=MatrixScale(1.0f, -1.0f, 1.0f);

	if(config.isVR)
		mvp=MatrixMult(MatrixMult(MatrixScale(((float)config.renderWidth/config.renderHeight)*1.0f, 1.0f, 1.0f), MatrixTranslate(0.0f, 0.0f, -1.0f)), MatrixMult(perFrame[frameIndex].mainUBO[eye]->HMD, perFrame[frameIndex].mainUBO[eye]->projection));

	vkCmdPushConstants(perFrame[frameIndex].commandBuffer, compositePipeline.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PC), &PC);

	vkCmdDraw(perFrame[frameIndex].commandBuffer, 3, 1, 0, 0);

	// Draw UI controls
	UI_Draw(&UI, perFrame[frameIndex].commandBuffer, perFrame[frameIndex].descriptorPool, mvp, fTimeStep);

	// Draw text in the compositing renderpass
	Font_Print(&font, 16.0f, 0.0f, (float)config.renderHeight-16.0f, "FPS: %0.1f\n\x1B[33mFrame time: %0.3fms\nAudio time: %0.3fms\nPhysics time: %0.3fms", fps, fTimeStep*1000.0f, audioTime*1000.0f, physicsTime*1000.0f);

	Font_Draw(&font, perFrame[frameIndex].commandBuffer, mvp);

	DrawLineGraph(perFrame[frameIndex].commandBuffer, &frameTimes, mvp);
	DrawLineGraph(perFrame[frameIndex].commandBuffer, &audioTimes, mvp);
	DrawLineGraph(perFrame[frameIndex].commandBuffer, &physicsTimes, mvp);

	vkCmdEndRenderPass(perFrame[frameIndex].commandBuffer);
}
