#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "system/system.h"
#include "vulkan/vulkan.h"
#include "math/math.h"
#include "font/font.h"
#include "perframe.h"

extern VkuContext_t Context;
extern VkuSwapchain_t Swapchain;

extern uint32_t rtWidth, rtHeight;
extern uint32_t Width, Height;

extern bool IsVR;

extern float fps, fTimeStep;

extern VkFormat ColorFormat;

extern VkuImage_t ColorImage[2];

extern VkuImage_t ColorResolve[2];
extern VkuImage_t ColorBlur[2];
extern VkuImage_t ColorTemp[2];

VkPipelineLayout CompositePipelineLayout;
VkuPipeline_t CompositePipeline;
VkRenderPass CompositeRenderPass;

VkPipelineLayout ThresholdPipelineLayout;
VkuPipeline_t ThresholdPipeline;
VkRenderPass ThresholdRenderPass;
VkFramebuffer ThresholdFramebuffer;

VkPipelineLayout GaussianPipelineLayout;
VkuPipeline_t GaussianPipeline;
VkRenderPass GaussianRenderPass;
VkFramebuffer GaussianFramebufferTemp;
VkFramebuffer GaussianFramebufferBlur;

bool CreateThresholdPipeline(void)
{
	vkCreateRenderPass(Context.Device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=1,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=ColorFormat,
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
	}, 0, &ThresholdRenderPass);

	vkCreateFramebuffer(Context.Device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=ThresholdRenderPass,
		.attachmentCount=1,
		.pAttachments=(VkImageView[]){ ColorBlur[0].View },
		.width=rtWidth>>2,
		.height=rtHeight>>2,
		.layers=1,
	}, 0, &ThresholdFramebuffer);

	for(uint32_t i=0;i<Swapchain.NumImages;i++)
	{
		vkuInitDescriptorSet(&PerFrame[i].ThresholdDescriptorSet, &Context);
		vkuDescriptorSet_AddBinding(&PerFrame[i].ThresholdDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		vkuAssembleDescriptorSetLayout(&PerFrame[i].ThresholdDescriptorSet);
	}

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&PerFrame[0].ThresholdDescriptorSet.DescriptorSetLayout,
	}, 0, &ThresholdPipelineLayout);

	vkuInitPipeline(&ThresholdPipeline, &Context);

	vkuPipeline_SetPipelineLayout(&ThresholdPipeline, ThresholdPipelineLayout);
	vkuPipeline_SetRenderPass(&ThresholdPipeline, ThresholdRenderPass);

	ThresholdPipeline.Topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	ThresholdPipeline.CullMode=VK_CULL_MODE_FRONT_BIT;

	if(!vkuPipeline_AddStage(&ThresholdPipeline, "./shaders/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&ThresholdPipeline, "./shaders/threshold.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	if(!vkuAssemblePipeline(&ThresholdPipeline))
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
				.format=ColorFormat,
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
	}, 0, &GaussianRenderPass);

	vkCreateFramebuffer(Context.Device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=GaussianRenderPass,
		.attachmentCount=1,
		.pAttachments=(VkImageView[]){ ColorTemp[0].View },
		.width=rtWidth>>2,
		.height=rtHeight>>2,
		.layers=1,
	}, 0, &GaussianFramebufferTemp);

	vkCreateFramebuffer(Context.Device, &(VkFramebufferCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
		.renderPass=GaussianRenderPass,
		.attachmentCount=1,
		.pAttachments=(VkImageView[]){ ColorBlur[0].View },
		.width=rtWidth>>2,
		.height=rtHeight>>2,
		.layers=1,
	}, 0, &GaussianFramebufferBlur);

	for(uint32_t i=0;i<Swapchain.NumImages;i++)
	{
		vkuInitDescriptorSet(&PerFrame[i].GaussianVDescriptorSet, &Context);
		vkuDescriptorSet_AddBinding(&PerFrame[i].GaussianVDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		vkuAssembleDescriptorSetLayout(&PerFrame[i].GaussianVDescriptorSet);

		vkuInitDescriptorSet(&PerFrame[i].GaussianHDescriptorSet, &Context);
		vkuDescriptorSet_AddBinding(&PerFrame[i].GaussianHDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		vkuAssembleDescriptorSetLayout(&PerFrame[i].GaussianHDescriptorSet);
	}

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&PerFrame[0].GaussianVDescriptorSet.DescriptorSetLayout,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset=0, .size=sizeof(vec2),
		},
	}, 0, &GaussianPipelineLayout);

	vkuInitPipeline(&GaussianPipeline, &Context);

	vkuPipeline_SetPipelineLayout(&GaussianPipeline, GaussianPipelineLayout);
	vkuPipeline_SetRenderPass(&GaussianPipeline, GaussianRenderPass);

	GaussianPipeline.Topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	GaussianPipeline.CullMode=VK_CULL_MODE_FRONT_BIT;

	if(!vkuPipeline_AddStage(&GaussianPipeline, "./shaders/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&GaussianPipeline, "./shaders/gaussian.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	if(!vkuAssemblePipeline(&GaussianPipeline))
		return false;

	return true;
}

bool CreateCompositePipeline(void)
{
	vkCreateRenderPass(Context.Device, &(VkRenderPassCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		.attachmentCount=1,
		.pAttachments=(VkAttachmentDescription[])
		{
			{
				.format=Swapchain.SurfaceFormat.format,
				.samples=VK_SAMPLE_COUNT_1_BIT,
				.loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
				.stencilLoadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.stencilStoreOp=VK_ATTACHMENT_STORE_OP_DONT_CARE,
				.initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
				.finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
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
	}, 0, &CompositeRenderPass);

	for(uint32_t i=0;i<Swapchain.NumImages;i++)
	{
		vkCreateFramebuffer(Context.Device, &(VkFramebufferCreateInfo)
		{
			.sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			.renderPass=CompositeRenderPass,
			.attachmentCount=1,
			.pAttachments=(VkImageView[]){ Swapchain.ImageView[i] },
			.width=Width,
			.height=Height,
			.layers=1,
		}, 0, &PerFrame[i].CompositeFramebuffer);

		vkuInitDescriptorSet(&PerFrame[i].CompositeDescriptorSet, &Context);

		vkuDescriptorSet_AddBinding(&PerFrame[i].CompositeDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		vkuDescriptorSet_AddBinding(&PerFrame[i].CompositeDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

		vkuAssembleDescriptorSetLayout(&PerFrame[i].CompositeDescriptorSet);
	}

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&PerFrame[0].CompositeDescriptorSet.DescriptorSetLayout,
	}, 0, &CompositePipelineLayout);

	vkuInitPipeline(&CompositePipeline, &Context);

	vkuPipeline_SetPipelineLayout(&CompositePipeline, CompositePipelineLayout);
	vkuPipeline_SetRenderPass(&CompositePipeline, CompositeRenderPass);

	CompositePipeline.Topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	CompositePipeline.CullMode=VK_CULL_MODE_FRONT_BIT;

	if(!vkuPipeline_AddStage(&CompositePipeline, "./shaders/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(IsVR)
	{
		if(!vkuPipeline_AddStage(&CompositePipeline, "./shaders/compositeVR.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
			return false;
	}
	else
	{
		if(!vkuPipeline_AddStage(&CompositePipeline, "./shaders/composite.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
			return false;
	}

	if(!vkuAssemblePipeline(&CompositePipeline))
		return false;

	CreateThresholdPipeline();
	CreateGaussianPipeline();

	return true;
}

void CompositeDraw(uint32_t Index)
{
	// Threshold and down sample to 1/4 original image size
	// Input = ColorResolve
	// Output = ColorBlur
	vkCmdBeginRenderPass(PerFrame[Index].CommandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=ThresholdRenderPass,
		.framebuffer=ThresholdFramebuffer,
		.clearValueCount=1,
		.pClearValues=(VkClearValue[]){ {{{ 1.0f, 0.0f, 0.0f, 1.0f }}} },
		.renderArea={ { 0, 0 }, { rtWidth>>2, rtHeight>>2 } },
	}, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(PerFrame[Index].CommandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)(rtWidth>>2), (float)(rtHeight>>2), 0.0f, 1.0f });
	vkCmdSetScissor(PerFrame[Index].CommandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { rtWidth>>2, rtHeight>>2 } });

	vkCmdBindPipeline(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ThresholdPipeline.Pipeline);

	vkuDescriptorSet_UpdateBindingImageInfo(&PerFrame[Index].ThresholdDescriptorSet, 0, &ColorResolve[0]);

	vkuAllocateUpdateDescriptorSet(&PerFrame[Index].ThresholdDescriptorSet, PerFrame[Index].DescriptorPool);
	vkCmdBindDescriptorSets(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ThresholdPipelineLayout, 0, 1, &PerFrame[Index].ThresholdDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(PerFrame[Index].CommandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(PerFrame[Index].CommandBuffer);
	//////

	// Gaussian blur (vertical)
	// Input = ColorBlur
	// Output = ColorTemp
	vkCmdBeginRenderPass(PerFrame[Index].CommandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=GaussianRenderPass,
		.framebuffer=GaussianFramebufferTemp,
		.clearValueCount=1,
		.pClearValues=(VkClearValue[]){ {{{ 1.0f, 0.0f, 0.0f, 1.0f }}} },
		.renderArea={ { 0, 0 }, { rtWidth>>2, rtHeight>>2 } },
	}, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(PerFrame[Index].CommandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)(rtWidth>>2), (float)(rtHeight>>2), 0.0f, 1.0f });
	vkCmdSetScissor(PerFrame[Index].CommandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { rtWidth>>2, rtHeight>>2 } });

	vkCmdBindPipeline(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GaussianPipeline.Pipeline);

	vkuDescriptorSet_UpdateBindingImageInfo(&PerFrame[Index].GaussianVDescriptorSet, 0, &ColorBlur[0]);

	vkCmdPushConstants(PerFrame[Index].CommandBuffer, GaussianPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(vec2), &(vec2){ 1.0f, 0.0 });

	vkuAllocateUpdateDescriptorSet(&PerFrame[Index].GaussianVDescriptorSet, PerFrame[Index].DescriptorPool);
	vkCmdBindDescriptorSets(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GaussianPipelineLayout, 0, 1, &PerFrame[Index].GaussianVDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(PerFrame[Index].CommandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(PerFrame[Index].CommandBuffer);
	//////

	// Gaussian blur (horizontal)
	// Input = ColorBlur
	// Output = ColorTemp
	vkCmdBeginRenderPass(PerFrame[Index].CommandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=GaussianRenderPass,
		.framebuffer=GaussianFramebufferBlur,
		.clearValueCount=1,
		.pClearValues=(VkClearValue[]){ {{{ 1.0f, 0.0f, 0.0f, 1.0f }}} },
		.renderArea={ { 0, 0 }, { rtWidth>>2, rtHeight>>2 } },
	}, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(PerFrame[Index].CommandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)(rtWidth>>2), (float)(rtHeight>>2), 0.0f, 1.0f });
	vkCmdSetScissor(PerFrame[Index].CommandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { rtWidth>>2, rtHeight>>2 } });

	vkCmdBindPipeline(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GaussianPipeline.Pipeline);

	vkuDescriptorSet_UpdateBindingImageInfo(&PerFrame[Index].GaussianHDescriptorSet, 0, &ColorTemp[0]);

	vkCmdPushConstants(PerFrame[Index].CommandBuffer, GaussianPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(vec2), &(vec2){ 0.0f, 1.0 });

	vkuAllocateUpdateDescriptorSet(&PerFrame[Index].GaussianHDescriptorSet, PerFrame[Index].DescriptorPool);
	vkCmdBindDescriptorSets(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GaussianPipelineLayout, 0, 1, &PerFrame[Index].GaussianHDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(PerFrame[Index].CommandBuffer, 3, 1, 0, 0);

	vkCmdEndRenderPass(PerFrame[Index].CommandBuffer);
	//////

	// Draw final composited image
	// Input = ColorResolve, ColorBlur
	// Output = Swapchain
	vkCmdBeginRenderPass(PerFrame[Index].CommandBuffer, &(VkRenderPassBeginInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass=CompositeRenderPass,
		.framebuffer=PerFrame[Index].CompositeFramebuffer,
		.clearValueCount=1,
		.pClearValues=(VkClearValue[]){ {{{ 0.0f, 0.0f, 0.0f, 1.0f }}} },
		.renderArea={ { 0, 0 }, { Width, Height } },
	}, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdSetViewport(PerFrame[Index].CommandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)Width, (float)Height, 0.0f, 1.0f });
	vkCmdSetScissor(PerFrame[Index].CommandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { Width, Height } });

	vkCmdBindPipeline(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CompositePipeline.Pipeline);

	vkuDescriptorSet_UpdateBindingImageInfo(&PerFrame[Index].CompositeDescriptorSet, 0, &ColorResolve[0]);
	vkuDescriptorSet_UpdateBindingImageInfo(&PerFrame[Index].CompositeDescriptorSet, 1, &ColorBlur[0]);

	vkuAllocateUpdateDescriptorSet(&PerFrame[Index].CompositeDescriptorSet, PerFrame[Index].DescriptorPool);
	vkCmdBindDescriptorSets(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CompositePipelineLayout, 0, 1, &PerFrame[Index].CompositeDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(PerFrame[Index].CommandBuffer, 3, 1, 0, 0);

	// Draw text in the compositing renderpass
	Font_Print(PerFrame[Index].CommandBuffer, 0, 0.0f, 16.0f, "FPS: %0.1f\n\x1B[91mFrame time: %0.5fms", fps, fTimeStep);

	vkCmdEndRenderPass(PerFrame[Index].CommandBuffer);
}