#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../perframe.h"
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "../vr/vr.h"
#include "ui.h"

// external Vulkan data and font
extern VkuContext_t vkContext;
extern VkSampleCountFlags MSAA;
extern VkuSwapchain_t swapchain;
extern VkRenderPass compositeRenderPass;

extern Font_t Fnt;

extern bool isVR;
extern VkuSwapchain_t swapchain;
extern XruContext_t xrContext;
extern matrix modelView, projection[2], headPose;
// ---

typedef struct
{
	vec4 positionSize;
	vec4 colorValue;
	uint32_t type, pad[3];
} UI_Instance_t;

static bool UI_VulkanVertex(UI_t *UI)
{
	VkuBuffer_t stagingBuffer;
	void *data=NULL;

	// Create a dummy blank image for binding to descriptor sets when no texture is needed
	if(!vkuCreateImageBuffer(&vkContext, &UI->blankImage,
	   VK_IMAGE_TYPE_2D, VK_FORMAT_B8G8R8A8_UNORM, 1, 1, 1, 1, 1,
	   VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
	   VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
	   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0))
		return VK_FALSE;

	VkCommandBuffer commandBuffer=vkuOneShotCommandBufferBegin(&vkContext);
	vkuTransitionLayout(commandBuffer, UI->blankImage.image, 1, 0, 1, 0, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuOneShotCommandBufferEnd(&vkContext, commandBuffer);

	vkCreateSampler(vkContext.device, &(VkSamplerCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
		.magFilter=VK_FILTER_NEAREST,
		.minFilter=VK_FILTER_NEAREST,
		.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST,
		.addressModeU=VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeV=VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.addressModeW=VK_SAMPLER_ADDRESS_MODE_REPEAT,
		.mipLodBias=0.0f,
		.compareOp=VK_COMPARE_OP_NEVER,
		.minLod=0.0f,
		.maxLod=VK_LOD_CLAMP_NONE,
		.maxAnisotropy=1.0f,
		.anisotropyEnable=VK_FALSE,
		.borderColor=VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
	}, VK_NULL_HANDLE, &UI->blankImage.sampler);

	vkCreateImageView(vkContext.device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
		.image=UI->blankImage.image,
		.viewType=VK_IMAGE_VIEW_TYPE_2D,
		.format=VK_FORMAT_B8G8R8A8_UNORM,
		.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
		.subresourceRange={ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
	}, VK_NULL_HANDLE, &UI->blankImage.imageView);
	// ---

	// Create static vertex data buffer
	if(!vkuCreateGPUBuffer(&vkContext, &UI->vertexBuffer, sizeof(vec4)*4, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT))
		return false;

	// Create staging buffer, map it, and copy vertex data to it
	if(!vkuCreateHostBuffer(&vkContext, &stagingBuffer, sizeof(vec4)*4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT))
		return false;

	// Map it
	if(vkMapMemory(vkContext.device, stagingBuffer.deviceMemory, 0, VK_WHOLE_SIZE, 0, &data)!=VK_SUCCESS)
		return false;

	if(!data)
		return false;

	vec4 *vecPtr=data;

	*vecPtr++=Vec4(-0.5f, 0.5f, -1.0f, 1.0f);	// XYUV
	*vecPtr++=Vec4(-0.5f, -0.5f, -1.0f, -1.0f);
	*vecPtr++=Vec4(0.5f, 0.5f, 1.0f, 1.0f);
	*vecPtr++=Vec4(0.5f, -0.5f, 1.0f, -1.0f);

	vkUnmapMemory(vkContext.device, stagingBuffer.deviceMemory);

	VkCommandBuffer CopyCommand=vkuOneShotCommandBufferBegin(&vkContext);
	vkCmdCopyBuffer(CopyCommand, stagingBuffer.buffer, UI->vertexBuffer.buffer, 1, &(VkBufferCopy) {.srcOffset=0, .dstOffset=0, .size=sizeof(vec4)*4 });
	vkuOneShotCommandBufferEnd(&vkContext, CopyCommand);

	// Delete staging data
	vkuDestroyBuffer(&vkContext, &stagingBuffer);
	// ---

	// Create instance buffer and map it
	vkuCreateHostBuffer(&vkContext, &UI->instanceBuffer, sizeof(UI_Instance_t)*UI_HASHTABLE_MAX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	vkMapMemory(vkContext.device, UI->instanceBuffer.deviceMemory, 0, VK_WHOLE_SIZE, 0, (void *)&UI->instanceBufferPtr);
	// ---

	return true;
}

static bool UI_VulkanPipeline(UI_t *UI)
{
	vkuInitDescriptorSet(&UI->descriptorSet, &vkContext);
	vkuDescriptorSet_AddBinding(&UI->descriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuAssembleDescriptorSetLayout(&UI->descriptorSet);

	vkCreatePipelineLayout(vkContext.device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&UI->descriptorSet.descriptorSetLayout,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=(sizeof(vec2)*2)+sizeof(matrix),
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},		
	}, 0, &UI->pipelineLayout);

	vkuInitPipeline(&UI->pipeline, &vkContext);

	vkuPipeline_SetPipelineLayout(&UI->pipeline, UI->pipelineLayout);
	vkuPipeline_SetRenderPass(&UI->pipeline, compositeRenderPass);

	UI->pipeline.topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	UI->pipeline.cullMode=VK_CULL_MODE_BACK_BIT;
	UI->pipeline.rasterizationSamples=VK_SAMPLE_COUNT_1_BIT;

	UI->pipeline.blend=VK_TRUE;
	UI->pipeline.srcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
	UI->pipeline.dstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	UI->pipeline.colorBlendOp=VK_BLEND_OP_ADD;
	UI->pipeline.srcAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
	UI->pipeline.dstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	UI->pipeline.alphaBlendOp=VK_BLEND_OP_ADD;

	if(!vkuPipeline_AddStage(&UI->pipeline, "shaders/ui_sdf.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&UI->pipeline, "shaders/ui_sdf.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	vkuPipeline_AddVertexBinding(&UI->pipeline, 0, sizeof(vec4), VK_VERTEX_INPUT_RATE_VERTEX);
	vkuPipeline_AddVertexAttribute(&UI->pipeline, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*0);

	vkuPipeline_AddVertexBinding(&UI->pipeline, 1, sizeof(UI_Instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);
	vkuPipeline_AddVertexAttribute(&UI->pipeline, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*0);
	vkuPipeline_AddVertexAttribute(&UI->pipeline, 2, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*1);
	vkuPipeline_AddVertexAttribute(&UI->pipeline, 3, 1, VK_FORMAT_R32G32B32A32_UINT, sizeof(vec4)*2);

	VkPipelineRenderingCreateInfo PipelineRenderingCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount=1,
		.pColorAttachmentFormats=&swapchain.surfaceFormat.format,
	};

	if(!vkuAssemblePipeline(&UI->pipeline, VK_NULL_HANDLE/*&PipelineRenderingCreateInfo*/))
		return false;

	UI_VulkanVertex(UI);

	return true;
}

// Initialize UI system.
bool UI_Init(UI_t *UI, vec2 position, vec2 size)
{
	if(UI==NULL)
		return false;

	UI->baseID=0;

	// Set screen width/height
	UI->position=position;
	UI->size=size;

	// Initial 10 pre-allocated list of buttons, uninitialized
	List_Init(&UI->controls, sizeof(UI_Control_t), 10, NULL);

	memset(UI->controlsHashtable, 0, sizeof(UI_Control_t *)*UI_HASHTABLE_MAX);

	// Vulkan stuff
	if(!UI_VulkanPipeline(UI))
		return false;

	return true;
}

void UI_Destroy(UI_t *UI)
{
	List_Destroy(&UI->controls);

	if(UI->instanceBuffer.deviceMemory)
		vkUnmapMemory(vkContext.device, UI->instanceBuffer.deviceMemory);

	vkuDestroyBuffer(&vkContext, &UI->instanceBuffer);

	vkuDestroyBuffer(&vkContext, &UI->vertexBuffer);

	vkuDestroyImageBuffer(&vkContext, &UI->blankImage);

	vkDestroyPipeline(vkContext.device, UI->pipeline.pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(vkContext.device, UI->pipelineLayout, VK_NULL_HANDLE);

	vkDestroyDescriptorSetLayout(vkContext.device, UI->descriptorSet.descriptorSetLayout, VK_NULL_HANDLE);
}

UI_Control_t *UI_FindControlByID(UI_t *UI, uint32_t ID)
{
	//for(uint32_t i=0;i<List_GetCount(&UI->controls);i++)
	//{
	//	UI_Control_t *control=(UI_Control_t *)List_GetPointer(&UI->controls, i);

	//	// Check for matching ID and type
	//	if(control->ID==ID)
	//		return control;
	//}
	if(UI==NULL||ID>=UI_HASHTABLE_MAX||ID==UINT32_MAX)
		return NULL;

	UI_Control_t *Control=UI->controlsHashtable[ID];

	if(Control->ID==ID)
		return Control;

	return NULL;
}

// Checks hit on UI controls, also processes certain controls, intended to be used on mouse button down events
// Returns ID of hit, otherwise returns UINT32_MAX
// Position is the cursor position to test against UI controls
uint32_t UI_TestHit(UI_t *UI, vec2 position)
{
	if(UI==NULL)
		return UINT32_MAX;

	// offset by UI position
	position=Vec2_Addv(position, UI->position);

	// Loop through all controls in the UI
	for(uint32_t i=0;i<List_GetCount(&UI->controls);i++)
	{
		UI_Control_t *Control=List_GetPointer(&UI->controls, i);

		switch(Control->type)
		{
			case UI_CONTROL_BUTTON:
			{
				if(position.x>=Control->position.x&&position.x<=Control->position.x+Control->button.size.x&&
				   position.y>=Control->position.y&&position.y<=Control->position.y+Control->button.size.y)
				{
					// TODO: This could potentionally be an issue if the callback blocks
					if(Control->button.callback)
						Control->button.callback(NULL);

					return Control->ID;
				}
				break;
			}

			case UI_CONTROL_CHECKBOX:
			{
				vec2 Normal=Vec2_Subv(Control->position, position);

				if(Vec2_Dot(Normal, Normal)<=Control->checkBox.radius*Control->checkBox.radius)
				{
					Control->checkBox.value=!Control->checkBox.value;
					return Control->ID;
				}
				break;
			}

			// Only return the ID of this control
			case UI_CONTROL_BARGRAPH:
			{
				if(!Control->barGraph.Readonly)
				{
					// If hit inside control area, map hit position to point on bargraph and set the value scaled to the set min and max
					if(position.x>=Control->position.x&&position.x<=Control->position.x+Control->barGraph.size.x&&
					   position.y>=Control->position.y&&position.y<=Control->position.y+Control->barGraph.size.y)
						return Control->ID;
				}
				break;
			}

			case UI_CONTROL_SPRITE:
				break;

			case UI_CONTROL_CURSOR:
				break;
		}
	}

	// Nothing found
	return UINT32_MAX;
}

// Processes hit on certain UI controls by ID (returned by UI_TestHit), intended to be used by "mouse move" events.
// Returns false on error
// Position is the cursor position to modify UI controls
bool UI_ProcessControl(UI_t *UI, uint32_t ID, vec2 position)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// offset by UI position
	position=Vec2_Addv(position, UI->position);

	// Get the control from the ID
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control==NULL)
		return false;

	switch(Control->type)
	{
		case UI_CONTROL_BUTTON:
			break;

		case UI_CONTROL_CHECKBOX:
			break;

		case UI_CONTROL_BARGRAPH:
			if(!Control->barGraph.Readonly)
			{
				// If hit inside control area, map hit position to point on bargraph and set the value scaled to the set min and max
				if(position.x>=Control->position.x&&position.x<=Control->position.x+Control->barGraph.size.x&&
					position.y>=Control->position.y&&position.y<=Control->position.y+Control->barGraph.size.y)
					Control->barGraph.value=((position.x-Control->position.x)/Control->barGraph.size.x)*(Control->barGraph.Max-Control->barGraph.Min)+Control->barGraph.Min;
			}
			break;

		case UI_CONTROL_SPRITE:
			break;

		case UI_CONTROL_CURSOR:
			break;
	}

	return true;
}

bool UI_Draw(UI_t *UI, uint32_t index, uint32_t eye)
{
	if(UI==NULL)
		return false;

	UI_Instance_t *instance=(UI_Instance_t *)UI->instanceBufferPtr;
	uint32_t instanceCount=0;

	const size_t controlCount=List_GetCount(&UI->controls);

	// Build a list of instanceable UI controls
	for(uint32_t i=0;i<controlCount;i++)
	{
		UI_Control_t *control=List_GetPointer(&UI->controls, i);

		switch(control->type)
		{
			case UI_CONTROL_BUTTON:
			{
				// Get base length of title text
				float textLength=Font_StringBaseWidth(control->button.titleText);

				// Scale text size based on the button size and length of text, but no bigger than 80% of button height
				float textSize=min(control->button.size.x/textLength*0.8f, control->button.size.y*0.8f);

				// Print the text centered
				Font_Print(&Fnt,
						   textSize,
						   control->position.x-(textLength*textSize)*0.5f+control->barGraph.size.x*0.5f,
						   control->position.y-(textSize*0.5f)+(control->barGraph.size.y*0.5f),
						   "%s", control->barGraph.titleText);

				// Left justified
				//Font_Print(&Font,
				//	textSize,
				//	control->position.x,
				//	control->position.y-(textSize*0.5f)+control->barGraph.size.y*0.5f,
				//	"%s", control->barGraph.titleText
				//);

				// right justified
				//Font_Print(&Font,
				//	textSize,
				//	control->position.x-(textLength*textSize)+control->barGraph.size.x,
				//	control->position.y-(textSize*0.5f)+control->barGraph.size.y*0.5f,
				//	"%s", control->barGraph.titleText
				//);

				instance->positionSize.x=control->position.x+control->button.size.x*0.5f;
				instance->positionSize.y=control->position.y+control->button.size.y*0.5f;
				instance->positionSize.z=control->button.size.x;
				instance->positionSize.w=control->button.size.y;

				instance->colorValue.x=control->color.x;
				instance->colorValue.y=control->color.y;
				instance->colorValue.z=control->color.z;
				instance->colorValue.w=0.0f;

				instance->type=UI_CONTROL_BUTTON;
				instance++;
				instanceCount++;
				break;
			}

			case UI_CONTROL_CHECKBOX:
			{
				// Text size is the radius of the checkbox, placed radius length away horizontally, centered vertically
				Font_Print(&Fnt,
						   control->checkBox.radius,
						   control->position.x+control->checkBox.radius,
						   control->position.y-(control->checkBox.radius/2.0f),
						   "%s", control->checkBox.titleText);

				instance->positionSize.x=control->position.x;
				instance->positionSize.y=control->position.y;
				instance->positionSize.z=control->checkBox.radius*2;
				instance->positionSize.w=control->checkBox.radius*2;

				instance->colorValue.x=control->color.x;
				instance->colorValue.y=control->color.y;
				instance->colorValue.z=control->color.z;

				if(control->checkBox.value)
					instance->colorValue.w=1.0f;
				else
					instance->colorValue.w=0.0f;

				instance->type=UI_CONTROL_CHECKBOX;
				instance++;
				instanceCount++;
				break;
			}

			case UI_CONTROL_BARGRAPH:
			{
				// Get base length of title text
				float textLength=Font_StringBaseWidth(control->barGraph.titleText);

				// Scale text size based on the button size and length of text, but no bigger than 80% of button height
				float textSize=min(control->barGraph.size.x/textLength*0.8f, control->barGraph.size.y*0.8f);

				// Print the text centered
				Font_Print(&Fnt,
						   textSize,
						   control->position.x-(textLength*textSize)*0.5f+control->barGraph.size.x*0.5f,
						   control->position.y-(textSize*0.5f)+(control->barGraph.size.y*0.5f),
						   "%s", control->barGraph.titleText);

				// Left justified
				//Font_Print(
				//	textSize,
				//	control->position.x,
				//	control->position.y-(textSize*0.5f)+control->barGraph.size.y*0.5f,
				//	"%s", control->barGraph.titleText
				//);

				// right justified
				//Font_Print(
				//	textSize,
				//	control->position.x-(textLength*textSize)+control->barGraph.size.x,
				//	control->position.y-(textSize*0.5f)+control->barGraph.size.y*0.5f,
				//	"%s", control->barGraph.titleText
				//);

				float normalize_value=(control->barGraph.value-control->barGraph.Min)/(control->barGraph.Max-control->barGraph.Min);

				instance->positionSize.x=control->position.x+control->barGraph.size.x*0.5f;
				instance->positionSize.y=control->position.y+control->barGraph.size.y*0.5f;
				instance->positionSize.z=control->barGraph.size.x;
				instance->positionSize.w=control->barGraph.size.y;

				instance->colorValue.x=control->color.x;
				instance->colorValue.y=control->color.y;
				instance->colorValue.z=control->color.z;
				instance->colorValue.w=normalize_value;

				instance->type=UI_CONTROL_BARGRAPH;
				instance++;
				instanceCount++;
				break;
			}

			case UI_CONTROL_SPRITE:
				break;

			case UI_CONTROL_CURSOR:
			{
				instance->positionSize.x=control->position.x+control->cursor.radius;
				instance->positionSize.y=control->position.y-control->cursor.radius;
				instance->positionSize.z=control->cursor.radius*2;
				instance->positionSize.w=control->cursor.radius*2;

				instance->colorValue.x=control->color.x;
				instance->colorValue.y=control->color.y;
				instance->colorValue.z=control->color.z;
				instance->colorValue.w=0.0f;

				instance->type=UI_CONTROL_CURSOR;
				instance++;
				instanceCount++;
				break;
			}
		}
	}

	// At the end of the instance buffer, build a list of non-instanced UI controls
	for(uint32_t i=0;i<controlCount;i++)
	{
		UI_Control_t *control=List_GetPointer(&UI->controls, i);

		if(control->type==UI_CONTROL_SPRITE)
		{
			instance->positionSize.x=control->position.x;
			instance->positionSize.y=control->position.y;
			instance->positionSize.z=control->sprite.size.x;
			instance->positionSize.w=control->sprite.size.y;

			instance->colorValue.x=control->color.x;
			instance->colorValue.y=control->color.y;
			instance->colorValue.z=control->color.z;
			instance->colorValue.w=control->sprite.rotation;

			instance->type=UI_CONTROL_SPRITE;
			instance++;
		}
	}

	// Flush instance buffer caches, mostly needed for Android and maybe some iGPUs
	vkFlushMappedMemoryRanges(vkContext.device, 1, &(VkMappedMemoryRange)
	{
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		VK_NULL_HANDLE,
		UI->instanceBuffer.deviceMemory,
		0, VK_WHOLE_SIZE
	});

	vkCmdBindPipeline(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI->pipeline.pipeline);

	// Bind vertex data buffer
	vkCmdBindVertexBuffers(perFrame[index].commandBuffer, 0, 1, &UI->vertexBuffer.buffer, &(VkDeviceSize) { 0 });
	// Bind object instance buffer
	vkCmdBindVertexBuffers(perFrame[index].commandBuffer, 1, 1, &UI->instanceBuffer.buffer, &(VkDeviceSize) { 0 });

	struct
	{
		vec2 viewport;
		vec2 pad;
		matrix mvp;
	} UIPC;

	float z=-1.0f;

	if(isVR)
		z=-1.5f;

	UIPC.viewport=UI->size;
	UIPC.mvp=MatrixMult(MatrixMult(MatrixMult(MatrixScale(UIPC.viewport.x/UIPC.viewport.y, 1.0f, 1.0f), MatrixTranslate(0.0f, 0.0f, z)), headPose), projection[eye]);

	vkCmdPushConstants(perFrame[index].commandBuffer, UI->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UIPC), &UIPC);

	// Draw sprites, they need descriptor set changes and aren't easy to draw instanced...
	uint32_t spriteCount=instanceCount;
	for(uint32_t i=0;i<controlCount;i++)
	{
		UI_Control_t *Control=List_GetPointer(&UI->controls, i);

		if(Control->type==UI_CONTROL_SPRITE)
		{
			vkuDescriptorSet_UpdateBindingImageInfo(&UI->descriptorSet, 0, Control->sprite.image->sampler, Control->sprite.image->imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			vkuAllocateUpdateDescriptorSet(&UI->descriptorSet, perFrame[index].descriptorPool);
			vkCmdBindDescriptorSets(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI->pipelineLayout, 0, 1, &UI->descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

			// Use the last unused instanced data slot for drawing
			vkCmdDraw(perFrame[index].commandBuffer, 4, 1, 0, spriteCount++);
		}
	}

	vkuDescriptorSet_UpdateBindingImageInfo(&UI->descriptorSet, 0, UI->blankImage.sampler, UI->blankImage.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuAllocateUpdateDescriptorSet(&UI->descriptorSet, perFrame[index].descriptorPool);
	vkCmdBindDescriptorSets(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI->pipelineLayout, 0, 1, &UI->descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	// Draw instanced UI elements
	vkCmdDraw(perFrame[index].commandBuffer, 4, instanceCount, 0, 0);

	return true;
}
