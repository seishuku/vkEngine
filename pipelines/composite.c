#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../utils/postprocess.h"
#include "../utils/pipeline.h"
#include "../font/font.h"
#include "../ui/ui.h"
#include "../vr/vr.h"
#include "../perframe.h"
#include "linegraph.h"
#include "postprocess/bloom.h"

extern VkuContext_t vkContext;
extern VkuSwapchain_t swapchain;
extern XruContext_t xrContext;

extern float fps, fTimeStep, audioTime, physicsTime;

VkuImage_t colorResolve[2];		// left and right eye MSAA resolve color buffer

extern VkuImage_t depthImage[2];
extern VkuImage_t shadowDepth;

extern UI_t UI;
extern Font_t font;

extern LineGraph_t frameTimes, audioTimes, physicsTimes;

Pipeline_t compositePipeline;
VkRenderPass compositeRenderPass;

VkFramebuffer compositeFramebuffer[VKU_MAX_FRAME_COUNT][2];

void CreateCompositeFramebuffers(void)
{
	PostProcess_CreateFramebuffers(config.renderWidth, config.renderHeight);

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
	VkImageLayout initialLayout=VK_IMAGE_LAYOUT_UNDEFINED;
	VkImageLayout attachementFinalLayout=VK_IMAGE_LAYOUT_UNDEFINED;
	VkFormat surfaceFormat=VK_FORMAT_UNDEFINED;

	// VR gets rendered directly to HMD, desktop needs to be presented
	if(!config.isVR)
	{
		initialLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		attachementFinalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		surfaceFormat=swapchain.surfaceFormat.format;
	}
	else
	{
		initialLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachementFinalLayout=VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
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
				.initialLayout=initialLayout,
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

	// Post-process chain runs in the order they are added.
	PostProcess_Register(&bloomEffect);

	if(!PostProcess_CreateAll())
		return false;

	bloomEffect.SetInput(&bloomEffect, &colorResolve[0], 0);
	bloomEffect.SetInput(&bloomEffect, &colorResolve[1], 1);

	return true;
}

void DestroyCompositeFramebuffers(void)
{
	PostProcess_DestroyFramebuffers();

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

	PostProcess_DestroyAll();

	// Compositing pipeline
	DestroyPipeline(&vkContext, &compositePipeline);
	vkDestroyRenderPass(vkContext.device, compositeRenderPass, VK_NULL_HANDLE);
	//////
}

void CompositeDraw(uint32_t imageIndex, uint32_t frameIndex, uint32_t eye)
{
	static uint32_t uFrame=0;

	// Run the post-process chain
	PostProcess_Draw(perFrame[frameIndex].commandBuffer, frameIndex, eye);

	// Draw final composited image
	// Input = colorResolve, post-process chain output
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
	vkuDescriptorSet_UpdateBindingImageInfo(&compositePipeline.descriptorSet, 1, bloomEffect.GetOutput(&bloomEffect, eye)->sampler, bloomEffect.GetOutput(&bloomEffect, eye)->imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&compositePipeline.descriptorSet, 2, depthImage[eye].sampler, depthImage[eye].imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingImageInfo(&compositePipeline.descriptorSet, 3, shadowDepth.sampler, shadowDepth.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuDescriptorSet_UpdateBindingBufferInfo(&compositePipeline.descriptorSet, 4, perFrame[frameIndex].mainUBOBuffer[eye].buffer, 0, VK_WHOLE_SIZE);

	vkuAllocateUpdateDescriptorSet(&compositePipeline.descriptorSet, perFrame[frameIndex].descriptorPool);
	vkCmdBindDescriptorSets(perFrame[frameIndex].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline.pipelineLayout, 0, 1, &compositePipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	struct
	{
		uint32_t uSize[2];
		uint32_t uSamples, uFrame;
	} PC;

	PC.uSize[0]=config.renderWidth;
	PC.uSize[1]=config.renderHeight;
	PC.uSamples=config.msaaSamples;
	PC.uFrame=uFrame++;

#ifdef ANDROID
	matrix mvp=MatrixMult(MatrixScale(1.0f, -1.0f, 1.0f), MatrixRotate(PI/2.0f, 0.0f, 0.0f, 1.0f));
#else
	matrix mvp=MatrixScale(1.0f, -1.0f, 1.0f);
#endif

	if(config.isVR)
		mvp=MatrixMult(MatrixMult(MatrixScale(((float)config.renderWidth/config.renderHeight)*1.0f, 1.0f, 1.0f), MatrixTranslate(0.0f, 0.0f, -1.0f)), MatrixMult(perFrame[frameIndex].mainUBO[eye]->HMD, perFrame[frameIndex].mainUBO[eye]->projection));

	vkCmdPushConstants(perFrame[frameIndex].commandBuffer, compositePipeline.pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PC), &PC);

	vkCmdDraw(perFrame[frameIndex].commandBuffer, 3, 1, 0, 0);

	if(!config.isVR)
	{
		// Draw UI controls
		UI_Draw(&UI, perFrame[frameIndex].commandBuffer, perFrame[frameIndex].descriptorPool, mvp, fTimeStep);

		// Draw text in the compositing renderpass
		Font_Print(&font, 16.0f, 0.0f, (float)config.renderHeight-16.0f, "FPS: %0.1f\n\x1B[33mFrame time: %0.3fms\nAudio time: %0.3fms\nPhysics time: %0.3fms", fps, fTimeStep*1000.0f, audioTime*1000.0f, physicsTime*1000.0f);

		Font_Draw(&font, perFrame[frameIndex].commandBuffer, mvp);

		DrawLineGraph(perFrame[frameIndex].commandBuffer, &frameTimes, mvp);
		DrawLineGraph(perFrame[frameIndex].commandBuffer, &audioTimes, mvp);
		DrawLineGraph(perFrame[frameIndex].commandBuffer, &physicsTimes, mvp);
	}

	vkCmdEndRenderPass(perFrame[frameIndex].commandBuffer);
}
