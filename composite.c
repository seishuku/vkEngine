#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "system/system.h"
#include "vulkan/vulkan.h"
#include "math/math.h"
#include "font/font.h"
#include "camera/camera.h"
#include "ui/ui.h"
#include "perframe.h"

extern VkuContext_t Context;
extern VkuSwapchain_t Swapchain;

extern uint32_t rtWidth, rtHeight;
extern uint32_t Width, Height;

extern bool IsVR;

extern float fps, fTimeStep;
extern double physicsTime;

extern VkFormat ColorFormat;

VkuImage_t ColorResolve[2];		// left and right eye MSAA resolve color buffer
VkuImage_t ColorBlur[2];		// left and right eye blur color buffer
VkuImage_t ColorTemp[2];		// left and right eye blur color buffer

//extern VkuImage_t ShadowDepth;
extern UI_t UI;
extern Font_t Font;

VkuDescriptorSet_t CompositeDescriptorSet;
VkPipelineLayout CompositePipelineLayout;
VkuPipeline_t CompositePipeline;

VkuDescriptorSet_t ThresholdDescriptorSet;
VkPipelineLayout ThresholdPipelineLayout;
VkuPipeline_t ThresholdPipeline;

VkuDescriptorSet_t GaussianDescriptorSet;
VkPipelineLayout GaussianPipelineLayout;
VkuPipeline_t GaussianPipeline;

bool CreateThresholdPipeline(void)
{
	vkuInitDescriptorSet(&ThresholdDescriptorSet, &Context);
	vkuDescriptorSet_AddBinding(&ThresholdDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuAssembleDescriptorSetLayout(&ThresholdDescriptorSet);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&ThresholdDescriptorSet.DescriptorSetLayout,
	}, 0, &ThresholdPipelineLayout);

	vkuInitPipeline(&ThresholdPipeline, &Context);

	vkuPipeline_SetPipelineLayout(&ThresholdPipeline, ThresholdPipelineLayout);

	ThresholdPipeline.Topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	ThresholdPipeline.CullMode=VK_CULL_MODE_FRONT_BIT;

	if(!vkuPipeline_AddStage(&ThresholdPipeline, "./shaders/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&ThresholdPipeline, "./shaders/threshold.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	VkPipelineRenderingCreateInfo PipelineRenderingCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount=1,
		.pColorAttachmentFormats=&ColorFormat,
	};

	if(!vkuAssemblePipeline(&ThresholdPipeline, &PipelineRenderingCreateInfo))
		return false;

	return true;
}

bool CreateGaussianPipeline(void)
{
	vkuInitDescriptorSet(&GaussianDescriptorSet, &Context);
	vkuDescriptorSet_AddBinding(&GaussianDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuAssembleDescriptorSetLayout(&GaussianDescriptorSet);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&GaussianDescriptorSet.DescriptorSetLayout,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT,
			.offset=0, .size=sizeof(float)*2,
		},
	}, 0, &GaussianPipelineLayout);

	vkuInitPipeline(&GaussianPipeline, &Context);

	vkuPipeline_SetPipelineLayout(&GaussianPipeline, GaussianPipelineLayout);

	GaussianPipeline.Topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	GaussianPipeline.CullMode=VK_CULL_MODE_FRONT_BIT;

	if(!vkuPipeline_AddStage(&GaussianPipeline, "./shaders/fullscreen.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&GaussianPipeline, "./shaders/gaussian.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	VkPipelineRenderingCreateInfo PipelineRenderingCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount=1,
		.pColorAttachmentFormats=&ColorFormat,
	};

	if(!vkuAssemblePipeline(&GaussianPipeline, &PipelineRenderingCreateInfo))
		return false;

	return true;
}

void CreateCompositeFramebuffers(uint32_t Eye, uint32_t targetWidth, uint32_t targetHeight)
{
	vkuCreateTexture2D(&Context, &ColorTemp[Eye], targetWidth>>2, targetHeight>>2, ColorFormat, VK_SAMPLE_COUNT_1_BIT);
	vkuCreateTexture2D(&Context, &ColorBlur[Eye], targetWidth>>2, targetHeight>>2, ColorFormat, VK_SAMPLE_COUNT_1_BIT);

	VkCommandBuffer CommandBuffer=vkuOneShotCommandBufferBegin(&Context);
	vkuTransitionLayout(CommandBuffer, ColorTemp[Eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuTransitionLayout(CommandBuffer, ColorBlur[Eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuOneShotCommandBufferEnd(&Context, CommandBuffer);
}

bool CreateCompositePipeline(void)
{
	vkuInitDescriptorSet(&CompositeDescriptorSet, &Context);
	vkuDescriptorSet_AddBinding(&CompositeDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuDescriptorSet_AddBinding(&CompositeDescriptorSet, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);

	if(IsVR)
	{
		vkuDescriptorSet_AddBinding(&CompositeDescriptorSet, 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
		vkuDescriptorSet_AddBinding(&CompositeDescriptorSet, 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	}

	vkuAssembleDescriptorSetLayout(&CompositeDescriptorSet);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&CompositeDescriptorSet.DescriptorSetLayout,
	}, 0, &CompositePipelineLayout);

	vkuInitPipeline(&CompositePipeline, &Context);

	vkuPipeline_SetPipelineLayout(&CompositePipeline, CompositePipelineLayout);

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

	VkPipelineRenderingCreateInfo PipelineRenderingCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount=1,
		.pColorAttachmentFormats=&Swapchain.SurfaceFormat.format,
	};

	if(!vkuAssemblePipeline(&CompositePipeline, &PipelineRenderingCreateInfo))
		return false;

	CreateThresholdPipeline();
	CreateGaussianPipeline();

	return true;
}

void DestroyComposite(void)
{
	// Thresholding pipeline
	vkDestroyDescriptorSetLayout(Context.Device, ThresholdDescriptorSet.DescriptorSetLayout, VK_NULL_HANDLE);

	vkDestroyPipeline(Context.Device, ThresholdPipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, ThresholdPipelineLayout, VK_NULL_HANDLE);
	//////

	// Gaussian blur pipeline
	vkDestroyDescriptorSetLayout(Context.Device, GaussianDescriptorSet.DescriptorSetLayout, VK_NULL_HANDLE);

	vkDestroyPipeline(Context.Device, GaussianPipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, GaussianPipelineLayout, VK_NULL_HANDLE);
	//////

	// Compositing pipeline
	vkDestroyDescriptorSetLayout(Context.Device, CompositeDescriptorSet.DescriptorSetLayout, VK_NULL_HANDLE);

	vkDestroyPipeline(Context.Device, CompositePipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, CompositePipelineLayout, VK_NULL_HANDLE);
	//////
}

void CompositeDraw(uint32_t Index, uint32_t Eye)
{
	// Threshold and down sample to 1/4 original image size
	// Input = ColorResolve
	// Output = ColorBlur
	vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorResolve[Eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorBlur[Eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	vkCmdBeginRendering(PerFrame[Index].CommandBuffer, &(VkRenderingInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea=(VkRect2D){ { 0, 0 }, { rtWidth>>2, rtHeight>>2 } },
		.layerCount=1,
		.colorAttachmentCount=1,
		.pColorAttachments=&(VkRenderingAttachmentInfo)
		{
			.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView=ColorBlur[Eye].View,
			.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
		},
	});

	vkCmdSetViewport(PerFrame[Index].CommandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)(rtWidth>>2), (float)(rtHeight>>2), 0.0f, 1.0f });
	vkCmdSetScissor(PerFrame[Index].CommandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { rtWidth>>2, rtHeight>>2 } });

	vkCmdBindPipeline(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ThresholdPipeline.Pipeline);

	vkuDescriptorSet_UpdateBindingImageInfo(&ThresholdDescriptorSet, 0, &ColorResolve[Eye]);

	vkuAllocateUpdateDescriptorSet(&ThresholdDescriptorSet, PerFrame[Index].DescriptorPool);
	vkCmdBindDescriptorSets(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, ThresholdPipelineLayout, 0, 1, &ThresholdDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(PerFrame[Index].CommandBuffer, 3, 1, 0, 0);

	vkCmdEndRendering(PerFrame[Index].CommandBuffer);
	//////

	// Gaussian blur (vertical)
	// Input = ColorBlur
	// Output = ColorTemp
	vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorBlur[Eye] .Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorTemp[Eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	vkCmdBeginRendering(PerFrame[Index].CommandBuffer, &(VkRenderingInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea=(VkRect2D){ { 0, 0 }, { rtWidth>>2, rtHeight>>2 } },
		.layerCount=1,
		.colorAttachmentCount=1,
		.pColorAttachments=&(VkRenderingAttachmentInfo)
		{
			.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView=ColorTemp[Eye].View,
			.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
		},
	});

	vkCmdSetViewport(PerFrame[Index].CommandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)(rtWidth>>2), (float)(rtHeight>>2), 0.0f, 1.0f });
	vkCmdSetScissor(PerFrame[Index].CommandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { rtWidth>>2, rtHeight>>2 } });

	vkCmdBindPipeline(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GaussianPipeline.Pipeline);

	vkCmdPushConstants(PerFrame[Index].CommandBuffer, GaussianPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float)*2, &(float[]){ 1.0f, 0.0 });

	vkuDescriptorSet_UpdateBindingImageInfo(&GaussianDescriptorSet, 0, &ColorBlur[Eye]);
	vkuAllocateUpdateDescriptorSet(&GaussianDescriptorSet, PerFrame[Index].DescriptorPool);

	vkCmdBindDescriptorSets(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GaussianPipelineLayout, 0, 1, &GaussianDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(PerFrame[Index].CommandBuffer, 3, 1, 0, 0);

	vkCmdEndRendering(PerFrame[Index].CommandBuffer);
	//////

	// Gaussian blur (horizontal)
	// Input = ColorTemp
	// Output = ColorBlur
	vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorTemp[Eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorBlur[Eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	vkCmdBeginRendering(PerFrame[Index].CommandBuffer, &(VkRenderingInfo)
	{
		.sType=VK_STRUCTURE_TYPE_RENDERING_INFO,
		.renderArea=(VkRect2D){ { 0, 0 }, { rtWidth>>2, rtHeight>>2 } },
		.layerCount=1,
		.viewMask=0,
		.colorAttachmentCount=1,
		.pColorAttachments=&(VkRenderingAttachmentInfo)
		{
			.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView=ColorBlur[Eye].View,
			.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
		},
	});

	vkCmdSetViewport(PerFrame[Index].CommandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)(rtWidth>>2), (float)(rtHeight>>2), 0.0f, 1.0f });
	vkCmdSetScissor(PerFrame[Index].CommandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { rtWidth>>2, rtHeight>>2 } });

	vkCmdBindPipeline(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GaussianPipeline.Pipeline);

	vkCmdPushConstants(PerFrame[Index].CommandBuffer, GaussianPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float)*2, &(float[]){ 0.0f, 1.0 });

	vkuDescriptorSet_UpdateBindingImageInfo(&GaussianDescriptorSet, 0, &ColorTemp[Eye]);
	vkuAllocateUpdateDescriptorSet(&GaussianDescriptorSet, PerFrame[Index].DescriptorPool);

	vkCmdBindDescriptorSets(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GaussianPipelineLayout, 0, 1, &GaussianDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	vkCmdDraw(PerFrame[Index].CommandBuffer, 3, 1, 0, 0);

	vkCmdEndRendering(PerFrame[Index].CommandBuffer);
	//////

	// Only draw final pass to swapchain on left eye pass,
	// need to fix this so it can also be composited out to the VR display and not just the swapchain.
	if(!Eye)
	{
		// Draw final composited image
		// Input = ColorResolve, ColorBlur
		// Output = Swapchain
		// NOTE: ColorResolve should already be in shader read-only
		vkuTransitionLayout(PerFrame[Index].CommandBuffer, ColorBlur[Eye].Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		vkuTransitionLayout(PerFrame[Index].CommandBuffer, Swapchain.Image[Index], 1, 0, 1, 0, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

		vkCmdBeginRendering(PerFrame[Index].CommandBuffer, &(VkRenderingInfo)
		{
			.sType=VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea=(VkRect2D){ { 0, 0 }, { Width, Height } },
			.layerCount=1,
			.viewMask=0,
			.colorAttachmentCount=1,
			.pColorAttachments=&(VkRenderingAttachmentInfo)
			{
				.sType=VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
				.imageView=Swapchain.ImageView[Index],
				.imageLayout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
				.loadOp=VK_ATTACHMENT_LOAD_OP_DONT_CARE,
				.storeOp=VK_ATTACHMENT_STORE_OP_STORE,
			},
		});

		vkCmdSetViewport(PerFrame[Index].CommandBuffer, 0, 1, &(VkViewport) { 0.0f, 0.0f, (float)Width, (float)Height, 0.0f, 1.0f });
		vkCmdSetScissor(PerFrame[Index].CommandBuffer, 0, 1, &(VkRect2D) { { 0, 0 }, { Width, Height } });

		vkCmdBindPipeline(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CompositePipeline.Pipeline);

		vkuDescriptorSet_UpdateBindingImageInfo(&CompositeDescriptorSet, 0, &ColorResolve[0]);
//		vkuDescriptorSet_UpdateBindingImageInfo(&CompositeDescriptorSet, 0, &ShadowDepth);
		vkuDescriptorSet_UpdateBindingImageInfo(&CompositeDescriptorSet, 1, &ColorBlur[0]);

		if(IsVR)
		{
			vkuDescriptorSet_UpdateBindingImageInfo(&CompositeDescriptorSet, 2, &ColorResolve[1]);
			vkuDescriptorSet_UpdateBindingImageInfo(&CompositeDescriptorSet, 3, &ColorBlur[1]);
		}

		vkuAllocateUpdateDescriptorSet(&CompositeDescriptorSet, PerFrame[Index].DescriptorPool);
		vkCmdBindDescriptorSets(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, CompositePipelineLayout, 0, 1, &CompositeDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

		vkCmdDraw(PerFrame[Index].CommandBuffer, 3, 1, 0, 0);

		// Draw UI controls
		UI_Draw(&UI, Index);

		// Draw text in the compositing renderpass
		Font_Print(&Font, 16.0f, 0.0f, 0.0f, "FPS: %0.1f\n\x1B[91mFrame time: %0.5fms", fps, fTimeStep*1000.0f);
		Font_Draw(&Font, Index);

		vkCmdEndRendering(PerFrame[Index].CommandBuffer);
	}
}