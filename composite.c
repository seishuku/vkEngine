#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "system/system.h"
#include "vulkan/vulkan.h"
#include "math/math.h"
#include "font/font.h"
#include "camera/camera.h"
#include "ui/ui.h"
#include "vr/vr.h"
#include "perframe.h"

extern VkuContext_t Context;
extern VkuSwapchain_t Swapchain;
extern XruContext_t xrContext;
extern bool isVR;

extern float fps, fTimeStep;
extern double physicsTime;

extern VkFormat colorFormat;

VkuImage_t colorResolve[2];		// left and right eye MSAA resolve color buffer
VkuImage_t colorBlur[2];		// left and right eye blur color buffer
VkuImage_t colorTemp[2];		// left and right eye blur color buffer

//extern VkuImage_t ShadowDepth;
extern UI_t UI;
extern Font_t Fnt;

VkuDescriptorSet_t compositeDescriptorSet;
VkPipelineLayout compositePipelineLayout;
VkuPipeline_t compositePipeline;
VkRenderPass compositeRenderPass;

VkuDescriptorSet_t thresholdDescriptorSet;
VkPipelineLayout thresholdPipelineLayout;
VkuPipeline_t thresholdPipeline;
VkRenderPass thresholdRenderPass;
VkFramebuffer thresholdFramebuffer[2];

VkuDescriptorSet_t gaussianDescriptorSet;
VkPipelineLayout gaussianPipelineLayout;
VkuPipeline_t gaussianPipeline;
VkRenderPass gaussianRenderPass;
VkFramebuffer gaussianFramebufferTemp[2];
VkFramebuffer gaussianFramebufferBlur[2];

bool CreateThresholdPipeline(void)
{
	vkCreateRenderPass(Context.Device, &(VkRenderPassCreateInfo)
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
				.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask=0,
				.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
				.dependencyFlags=0,
			},
			{
				.srcSubpass=0,
				.dstSubpass=VK_SUBPASS_EXTERNAL,
				.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
			},
		},
	}, 0, &thresholdRenderPass);

	vkuInitDescriptorSet(&thresholdDescriptorSet, &Context);
	vkuDescriptorSet_AddBinding(&thresholdDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuAssembleDescriptorSetLayout(&thresholdDescriptorSet);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&thresholdDescriptorSet.DescriptorSetLayout,
	}, 0, &thresholdPipelineLayout);

	vkuInitPipeline(&thresholdPipeline, &Context);

	vkuPipeline_SetPipelineLayout(&thresholdPipeline, thresholdPipelineLayout);
	vkuPipeline_SetRenderPass(&thresholdPipeline, thresholdRenderPass);

	thresholdPipeline.Topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	thresholdPipeline.CullMode=VK_CULL_MODE_FRONT_BIT;

	if(!vkuPipeline_AddStage(&thresholdPipeline, "shaders/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&thresholdPipeline, "shaders/threshold.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	//VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo=
	//{
	//	.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
	//	.colorAttachmentCount=1,
	//	.pColorAttachmentFormats=&ColorFormat,
	//};

	if(!vkuAssemblePipeline(&thresholdPipeline, VK_NULL_HANDLE/*&pipelineRenderingCreateInfo*/))
		return false;

	return true;
}

bool CreateGaussianPipeline(void)
{
	vkCreateRenderPass(Context.Device, &(VkRenderPassCreateInfo)
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
				.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask=0,
				.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT|VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
				.dependencyFlags=0,
			},
			{
				.srcSubpass=0,
				.dstSubpass=VK_SUBPASS_EXTERNAL,
				.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
				.srcAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dstAccessMask=VK_ACCESS_SHADER_READ_BIT,
				.dependencyFlags=VK_DEPENDENCY_BY_REGION_BIT,
			},
		},
	}, 0, &gaussianRenderPass);

	vkuInitDescriptorSet(&gaussianDescriptorSet, &Context);
	vkuDescriptorSet_AddBinding(&gaussianDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuAssembleDescriptorSetLayout(&gaussianDescriptorSet);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&gaussianDescriptorSet.DescriptorSetLayout,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset=0, .size=sizeof(float)*2,
		},
	}, 0, &gaussianPipelineLayout);

	vkuInitPipeline(&gaussianPipeline, &Context);

	vkuPipeline_SetPipelineLayout(&gaussianPipeline, gaussianPipelineLayout);
	vkuPipeline_SetRenderPass(&gaussianPipeline, gaussianRenderPass);

	gaussianPipeline.Topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	gaussianPipeline.CullMode=VK_CULL_MODE_FRONT_BIT;

	if(!vkuPipeline_AddStage(&gaussianPipeline, "shaders/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&gaussianPipeline, "shaders/gaussian.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	//VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo=
	//{
	//	.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
	//	.colorAttachmentCount=1,
	//	.pColorAttachmentFormats=&ColorFormat,
	//};

	if(!vkuAssemblePipeline(&gaussianPipeline, VK_NULL_HANDLE/*&pipelineRenderingCreateInfo*/))
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
		targetWidth=Swapchain.Extent.width;
		targetHeight=Swapchain.Extent.height;
	}

	vkuCreateTexture2D(&Context, &colorTemp[eye], targetWidth>>2, targetHeight>>2, colorFormat, VK_SAMPLE_COUNT_1_BIT);
	vkuCreateTexture2D(&Context, &colorBlur[eye], targetWidth>>2, targetHeight>>2, colorFormat, VK_SAMPLE_COUNT_1_BIT);

	VkCommandBuffer commandBuffer=vkuOneShotCommandBufferBegin(&Context);
	vkuTransitionLayout(commandBuffer, colorTemp[eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuTransitionLayout(commandBuffer, colorBlur[eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuOneShotCommandBufferEnd(&Context, commandBuffer);

	// Threshold framebuffer contains 1/4 sized final main render frame, outputs to ColorBlur image
	vkCreateFramebuffer(Context.Device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=thresholdRenderPass,
		.attachmentCount=1,
		.pAttachments=(VkImageView[]){ colorBlur[eye].View },
		.width=targetWidth>>2,
		.height=targetHeight>>2,
		.layers=1,
	}, 0, &thresholdFramebuffer[eye]);

	// Gussian temp frame buffer takes output of thresholding pipeline and does the first gaussian blur pass, outputs to ColorTemp
	vkCreateFramebuffer(Context.Device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=gaussianRenderPass,
		.attachmentCount=1,
		.pAttachments=(VkImageView[]){ colorTemp[eye].View },
		.width=targetWidth>>2,
		.height=targetHeight>>2,
		.layers=1,
	}, 0, &gaussianFramebufferTemp[eye]);

	// Gussian blur frame buffer takes output of first pass gaussian blur and does the second gaussian blur pass, outputs to ColorBlur
	vkCreateFramebuffer(Context.Device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=gaussianRenderPass,
		.attachmentCount=1,
		.pAttachments=(VkImageView[]){ colorBlur[eye].View },
		.width=targetWidth>>2,
		.height=targetHeight>>2,
		.layers=1,
	}, 0, &gaussianFramebufferBlur[eye]);

	if(!isVR)
	{
		// Compositing pipeline images, these are the actual swapchain framebuffers that will get presented
		for(uint32_t i=0;i<Swapchain.NumImages;i++)
		{
			vkCreateFramebuffer(Context.Device, &(VkFramebufferCreateInfo)
			{
				.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.renderPass=compositeRenderPass,
				.attachmentCount=1,
				.pAttachments=(VkImageView[]){ Swapchain.ImageView[i] },
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
			vkCreateFramebuffer(Context.Device, &(VkFramebufferCreateInfo)
			{
				.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.renderPass=compositeRenderPass,
				.attachmentCount=1,
				.pAttachments=(VkImageView[]){ xrContext.swapchain[0].imageView[i] },
				.width=targetWidth,
				.height=targetHeight,
				.layers=1,
			}, 0, &perFrame[i].compositeFramebuffer[0]);

			vkCreateFramebuffer(Context.Device, &(VkFramebufferCreateInfo)
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
		surfaceFormat=Swapchain.SurfaceFormat.format;
	}
	else
	{
		attachementFinalLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		surfaceFormat=xrContext.swapchainFormat;
	}

	vkCreateRenderPass(Context.Device, &(VkRenderPassCreateInfo)
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
		.dependencyCount=1,
		.pDependencies=(VkSubpassDependency[])
		{
			{
				.srcSubpass=VK_SUBPASS_EXTERNAL,
				.dstSubpass=0,
				.srcStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.dstStageMask=VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
				.srcAccessMask=0,
				.dstAccessMask=VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
				.dependencyFlags=0,
			},
		}
		}, 0, &compositeRenderPass);

	vkuInitDescriptorSet(&compositeDescriptorSet, &Context);
	vkuDescriptorSet_AddBinding(&compositeDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&compositeDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	vkuAssembleDescriptorSetLayout(&compositeDescriptorSet);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&compositeDescriptorSet.DescriptorSetLayout,
	}, 0, &compositePipelineLayout);

	vkuInitPipeline(&compositePipeline, &Context);

	vkuPipeline_SetPipelineLayout(&compositePipeline, compositePipelineLayout);
	vkuPipeline_SetRenderPass(&compositePipeline, compositeRenderPass);

	compositePipeline.Topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	compositePipeline.CullMode=VK_CULL_MODE_FRONT_BIT;

	if(!vkuPipeline_AddStage(&compositePipeline, "shaders/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&compositePipeline, "shaders/composite.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	//VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo=
	//{
	//	.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
	//	.colorAttachmentCount=1,
	//	.pColorAttachmentFormats=&Swapchain.SurfaceFormat.format,
	//};

	if(!vkuAssemblePipeline(&compositePipeline, VK_NULL_HANDLE/*&pipelineRenderingCreateInfo*/))
		return false;

	CreateThresholdPipeline();
	CreateGaussianPipeline();

	return true;
}

void DestroyComposite(void)
{
	// Thresholding pipeline
	vkDestroyDescriptorSetLayout(Context.Device, thresholdDescriptorSet.DescriptorSetLayout, VK_NULL_HANDLE);

	vkDestroyFramebuffer(Context.Device, thresholdFramebuffer[0], VK_NULL_HANDLE);

	if(isVR)
		vkDestroyFramebuffer(Context.Device, thresholdFramebuffer[1], VK_NULL_HANDLE);

	vkDestroyPipeline(Context.Device, thresholdPipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, thresholdPipelineLayout, VK_NULL_HANDLE);
	vkDestroyRenderPass(Context.Device, thresholdRenderPass, VK_NULL_HANDLE);
	//////

	// Gaussian blur pipeline
	vkDestroyDescriptorSetLayout(Context.Device, gaussianDescriptorSet.DescriptorSetLayout, VK_NULL_HANDLE);

	vkDestroyFramebuffer(Context.Device, gaussianFramebufferTemp[0], VK_NULL_HANDLE);
	vkDestroyFramebuffer(Context.Device, gaussianFramebufferBlur[0], VK_NULL_HANDLE);

	if(isVR)
	{
		vkDestroyFramebuffer(Context.Device, gaussianFramebufferTemp[1], VK_NULL_HANDLE);
		vkDestroyFramebuffer(Context.Device, gaussianFramebufferBlur[1], VK_NULL_HANDLE);
	}

	vkDestroyPipeline(Context.Device, gaussianPipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, gaussianPipelineLayout, VK_NULL_HANDLE);
	vkDestroyRenderPass(Context.Device, gaussianRenderPass, VK_NULL_HANDLE);
	//////

	// Compositing pipeline
	vkDestroyDescriptorSetLayout(Context.Device, compositeDescriptorSet.DescriptorSetLayout, VK_NULL_HANDLE);

	for(uint32_t i=0;i<Swapchain.NumImages;i++)
	{
		vkDestroyFramebuffer(Context.Device, perFrame[i].compositeFramebuffer[0], VK_NULL_HANDLE);

		if(isVR)
			vkDestroyFramebuffer(Context.Device, perFrame[i].compositeFramebuffer[1], VK_NULL_HANDLE);
	}

	vkDestroyPipeline(Context.Device, compositePipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, compositePipelineLayout, VK_NULL_HANDLE);
	vkDestroyRenderPass(Context.Device, compositeRenderPass, VK_NULL_HANDLE);
	//////
}

void CompositeDraw(uint32_t index, uint32_t eye)
{
	uint32_t width=0, height=0;

	if(isVR)
	{
		width=xrContext.swapchainExtent.width;
		height=xrContext.swapchainExtent.height;
	}
	else
	{
		width=Swapchain.Extent.width;
		height=Swapchain.Extent.height;
	}

	// Threshold and down sample to 1/4 original image size
	// Input = colorResolve
	// Output = colorBlur
	//vkuTransitionLayout(perFrame[index].commandBuffer, colorResolve[eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	//vkuTransitionLayout(perFrame[index].commandBuffer, colorBlur[eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	//vkCmdBeginRendering(perFrame[index].commandBuffer, &(VkRenderingInfo)
	//{
	//	.sType=VK_STRUCTURE_TYPE_RENDERING_INFO,
	//	.renderArea=(VkRect2D){ { 0, 0 }, { renderWidth>>2, renderHeight>>2 } },
	//	.layerCount=1,
	//	.colorAttachmentCount=1,
	//	.pColorAttachments=&(VkRenderingAttachmentInfo)
	//	{
	//		.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
	//		.imageView=colorBlur[Eye].View,
	//		.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	//		.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	//		.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
	//	},
	//});
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

	vkCmdBindPipeline(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, thresholdPipeline.Pipeline);

	vkuDescriptorSet_UpdateBindingImageInfo(&thresholdDescriptorSet, 0, &colorResolve[eye]);

	vkuAllocateUpdateDescriptorSet(&thresholdDescriptorSet, perFrame[index].descriptorPool);
	vkCmdBindDescriptorSets(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, thresholdPipelineLayout, 0, 1, &thresholdDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(perFrame[index].commandBuffer, 3, 1, 0, 0);

	//vkCmdEndRendering(perFrame[index].commandBuffer);
	vkCmdEndRenderPass(perFrame[index].commandBuffer);
	//////

	// Gaussian blur (vertical)
	// Input = colorBlur
	// Output = colorTemp
	//vkuTransitionLayout(perFrame[index].commandBuffer, colorBlur[eye] .Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	//vkuTransitionLayout(perFrame[index].commandBuffer, colorTemp[eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	//vkCmdBeginRendering(perFrame[index].commandBuffer, &(VkRenderingInfo)
	//{
	//	.sType=VK_STRUCTURE_TYPE_RENDERING_INFO,
	//	.renderArea=(VkRect2D){ { 0, 0 }, { renderWidth>>2, renderHeight>>2 } },
	//	.layerCount=1,
	//	.colorAttachmentCount=1,
	//	.pColorAttachments=&(VkRenderingAttachmentInfo)
	//	{
	//		.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
	//		.imageView=colorTemp[Eye].View,
	//		.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	//		.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	//		.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
	//	},
	//});
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

	vkCmdBindPipeline(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gaussianPipeline.Pipeline);

	vkCmdPushConstants(perFrame[index].commandBuffer, gaussianPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float)*2, &(float[]){ 1.0f, 0.0 });

	vkuDescriptorSet_UpdateBindingImageInfo(&gaussianDescriptorSet, 0, &colorBlur[eye]);
	vkuAllocateUpdateDescriptorSet(&gaussianDescriptorSet, perFrame[index].descriptorPool);

	vkCmdBindDescriptorSets(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gaussianPipelineLayout, 0, 1, &gaussianDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(perFrame[index].commandBuffer, 3, 1, 0, 0);

	//vkCmdEndRendering(perFrame[index].commandBuffer);
	vkCmdEndRenderPass(perFrame[index].commandBuffer);
	//////

	// Gaussian blur (horizontal)
	// Input = colorTemp
	// Output = colorBlur
	//vkuTransitionLayout(perFrame[index].commandBuffer, colorTemp[eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	//vkuTransitionLayout(perFrame[index].commandBuffer, colorBlur[eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	//vkCmdBeginRendering(perFrame[index].commandBuffer, &(VkRenderingInfo)
	//{
	//	.sType=VK_STRUCTURE_TYPE_RENDERING_INFO,
	//	.renderArea=(VkRect2D){ { 0, 0 }, { renderWidth>>2, renderHeight>>2 } },
	//	.layerCount=1,
	//	.viewMask=0,
	//	.colorAttachmentCount=1,
	//	.pColorAttachments=&(VkRenderingAttachmentInfo)
	//	{
	//		.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
	//		.imageView=colorBlur[Eye].View,
	//		.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	//		.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	//		.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
	//	},
	//});
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

	vkCmdBindPipeline(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gaussianPipeline.Pipeline);

	vkCmdPushConstants(perFrame[index].commandBuffer, gaussianPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float)*2, &(float[]){ 0.0f, 1.0 });

	vkuDescriptorSet_UpdateBindingImageInfo(&gaussianDescriptorSet, 0, &colorTemp[eye]);
	vkuAllocateUpdateDescriptorSet(&gaussianDescriptorSet, perFrame[index].descriptorPool);

	vkCmdBindDescriptorSets(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, gaussianPipelineLayout, 0, 1, &gaussianDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(perFrame[index].commandBuffer, 3, 1, 0, 0);

	//vkCmdEndRendering(PerFrame[index].CommandBuffer);
	vkCmdEndRenderPass(perFrame[index].commandBuffer);
	//////

	// Draw final composited image
	// Input = colorResolve, colorBlur
	// Output = Swapchain
	// NOTE: ColorResolve should already be in shader read-only
	//vkuTransitionLayout(perFrame[index].commandBuffer, colorBlur[eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	//vkuTransitionLayout(perFrame[index].commandBuffer, Swapchain.Image[index], 1, 0, 1, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	//vkCmdBeginRendering(perFrame[index].commandBuffer, &(VkRenderingInfo)
	//{
	//	.sType=VK_STRUCTURE_TYPE_RENDERING_INFO,
	//	.renderArea=(VkRect2D){ { 0, 0 }, { width, height } },
	//	.layerCount=1,
	//	.viewMask=0,
	//	.colorAttachmentCount=1,
	//	.pColorAttachments=&(VkRenderingAttachmentInfo)
	//	{
	//		.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
	//		.imageView=Swapchain.ImageView[index],
	//		.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
	//		.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
	//		.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
	//	},
	//});
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

	vkCmdBindPipeline(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipeline.Pipeline);

	vkuDescriptorSet_UpdateBindingImageInfo(&compositeDescriptorSet, 0, &colorResolve[eye]);
//	vkuDescriptorSet_UpdateBindingImageInfo(&compositeDescriptorSet, 0, &shadowDepth);
	vkuDescriptorSet_UpdateBindingImageInfo(&compositeDescriptorSet, 1, &colorBlur[eye]);

	vkuAllocateUpdateDescriptorSet(&compositeDescriptorSet, perFrame[index].descriptorPool);
	vkCmdBindDescriptorSets(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositePipelineLayout, 0, 1, &compositeDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(perFrame[index].commandBuffer, 3, 1, 0, 0);

	// Draw UI controls
	UI_Draw(&UI, index, eye);

	// Draw text in the compositing renderpass
	Font_Print(&Fnt, 16.0f, 0.0f, 0.0f, "FPS: %0.1f\n\x1B[91mFrame time: %0.5fms", fps, fTimeStep*1000.0f);

	Font_Draw(&Fnt, index, eye);

	//vkCmdEndRendering(perFrame[index].commandBuffer);
	vkCmdEndRenderPass(perFrame[index].commandBuffer);
}
