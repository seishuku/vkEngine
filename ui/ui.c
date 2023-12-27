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
	vec4 PositionSize;
	vec4 ColorValue;
	uint32_t type, Pad[3];
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

	VkCommandBuffer CommandBuffer=vkuOneShotCommandBufferBegin(&vkContext);
	vkuTransitionLayout(CommandBuffer, UI->blankImage.Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuOneShotCommandBufferEnd(&vkContext, CommandBuffer);

	vkCreateSampler(vkContext.Device, &(VkSamplerCreateInfo)
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
	}, VK_NULL_HANDLE, &UI->blankImage.Sampler);
	vkCreateImageView(vkContext.Device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image=UI->blankImage.Image,
			.viewType=VK_IMAGE_VIEW_TYPE_2D,
			.format=VK_FORMAT_B8G8R8A8_UNORM,
			.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
			.subresourceRange={ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
	}, VK_NULL_HANDLE, &UI->blankImage.View);
	// ---

	// Create static vertex data buffer
	if(!vkuCreateGPUBuffer(&vkContext, &UI->vertexBuffer, sizeof(vec4)*4, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT))
		return false;

	// Create staging buffer, map it, and copy vertex data to it
	if(!vkuCreateHostBuffer(&vkContext, &stagingBuffer, sizeof(vec4)*4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT))
		return false;

	// Map it
	if(vkMapMemory(vkContext.Device, stagingBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, &data)!=VK_SUCCESS)
		return false;

	if(!data)
		return false;

	vec4 *Ptr=data;

	*Ptr++=Vec4(-0.5f, 0.5f, -1.0f, 1.0f);	// XYUV
	*Ptr++=Vec4(-0.5f, -0.5f, -1.0f, -1.0f);
	*Ptr++=Vec4(0.5f, 0.5f, 1.0f, 1.0f);
	*Ptr++=Vec4(0.5f, -0.5f, 1.0f, -1.0f);

	vkUnmapMemory(vkContext.Device, stagingBuffer.DeviceMemory);

	VkCommandBuffer CopyCommand=vkuOneShotCommandBufferBegin(&vkContext);
	vkCmdCopyBuffer(CopyCommand, stagingBuffer.Buffer, UI->vertexBuffer.Buffer, 1, &(VkBufferCopy) {.srcOffset=0, .dstOffset=0, .size=sizeof(vec4)*4 });
	vkuOneShotCommandBufferEnd(&vkContext, CopyCommand);

	// Delete staging data
	vkuDestroyBuffer(&vkContext, &stagingBuffer);
	// ---

	// Create instance buffer and map it
	vkuCreateHostBuffer(&vkContext, &UI->instanceBuffer, sizeof(UI_Instance_t)*UI_HASHTABLE_MAX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	vkMapMemory(vkContext.Device, UI->instanceBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void *)&UI->instanceBufferPtr);
	// ---

	return true;
}

static bool UI_VulkanPipeline(UI_t *UI)
{
	vkuInitDescriptorSet(&UI->descriptorSet, &vkContext);
	vkuDescriptorSet_AddBinding(&UI->descriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuAssembleDescriptorSetLayout(&UI->descriptorSet);

	vkCreatePipelineLayout(vkContext.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&UI->descriptorSet.DescriptorSetLayout,
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

	UI->pipeline.Topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	UI->pipeline.CullMode=VK_CULL_MODE_BACK_BIT;
	UI->pipeline.RasterizationSamples=VK_SAMPLE_COUNT_1_BIT;

	UI->pipeline.Blend=VK_TRUE;
	UI->pipeline.SrcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
	UI->pipeline.DstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	UI->pipeline.ColorBlendOp=VK_BLEND_OP_ADD;
	UI->pipeline.SrcAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
	UI->pipeline.DstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	UI->pipeline.AlphaBlendOp=VK_BLEND_OP_ADD;

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
		.pColorAttachmentFormats=&swapchain.SurfaceFormat.format,
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

	if(UI->instanceBuffer.DeviceMemory)
		vkUnmapMemory(vkContext.Device, UI->instanceBuffer.DeviceMemory);

	vkuDestroyBuffer(&vkContext, &UI->instanceBuffer);

	vkuDestroyBuffer(&vkContext, &UI->vertexBuffer);

	vkuDestroyImageBuffer(&vkContext, &UI->blankImage);

	vkDestroyPipeline(vkContext.Device, UI->pipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(vkContext.Device, UI->pipelineLayout, VK_NULL_HANDLE);

	vkDestroyDescriptorSetLayout(vkContext.Device, UI->descriptorSet.DescriptorSetLayout, VK_NULL_HANDLE);
}

UI_Control_t *UI_FindControlByID(UI_t *UI, uint32_t ID)
{
	//for(uint32_t i=0;i<List_GetCount(&UI->controls);i++)
	//{
	//	UI_Control_t *Control=(UI_Control_t *)List_GetPointer(&UI->controls, i);

	//	// Check for matching ID and type
	//	if(Control->ID==ID)
	//		return Control;
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

	// Offset by UI position
	position=Vec2_Addv(position, UI->position);

	// Loop through all controls in the UI
	for(uint32_t i=0;i<List_GetCount(&UI->controls);i++)
	{
		UI_Control_t *Control=List_GetPointer(&UI->controls, i);

		switch(Control->type)
		{
			case UI_CONTROL_BUTTON:
			{
				if(position.x>=Control->position.x&&position.x<=Control->position.x+Control->Button.size.x&&
				   position.y>=Control->position.y&&position.y<=Control->position.y+Control->Button.size.y)
				{
					// TODO: This could potentionally be an issue if the callback blocks
					if(Control->Button.Callback)
						Control->Button.Callback(NULL);

					return Control->ID;
				}
				break;
			}

			case UI_CONTROL_CHECKBOX:
			{
				vec2 Normal=Vec2_Subv(Control->position, position);

				if(Vec2_Dot(Normal, Normal)<=Control->CheckBox.radius*Control->CheckBox.radius)
				{
					Control->CheckBox.value=!Control->CheckBox.value;
					return Control->ID;
				}
				break;
			}

			// Only return the ID of this control
			case UI_CONTROL_BARGRAPH:
			{
				if(!Control->BarGraph.Readonly)
				{
					// If hit inside control area, map hit position to point on bargraph and set the value scaled to the set min and max
					if(position.x>=Control->position.x&&position.x<=Control->position.x+Control->BarGraph.size.x&&
					   position.y>=Control->position.y&&position.y<=Control->position.y+Control->BarGraph.size.y)
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

	// Offset by UI position
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
			if(!Control->BarGraph.Readonly)
			{
				// If hit inside control area, map hit position to point on bargraph and set the value scaled to the set min and max
				if(position.x>=Control->position.x&&position.x<=Control->position.x+Control->BarGraph.size.x&&
					position.y>=Control->position.y&&position.y<=Control->position.y+Control->BarGraph.size.y)
					Control->BarGraph.value=((position.x-Control->position.x)/Control->BarGraph.size.x)*(Control->BarGraph.Max-Control->BarGraph.Min)+Control->BarGraph.Min;
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

	UI_Instance_t *Instance=(UI_Instance_t *)UI->instanceBufferPtr;
	uint32_t instanceCount=0;

	const size_t controlCount=List_GetCount(&UI->controls);

	// Build a list of instanceable UI controls
	for(uint32_t i=0;i<controlCount;i++)
	{
		UI_Control_t *Control=List_GetPointer(&UI->controls, i);

		switch(Control->type)
		{
			case UI_CONTROL_BUTTON:
			{
				// Get base length of title text
				float textlen=Font_StringBaseWidth(Control->Button.titleText);

				// Scale text size based on the button size and length of text, but no bigger than 80% of button height
				float TextSize=min(Control->Button.size.x/textlen*0.8f, Control->Button.size.y*0.8f);

				// Print the text centered
				Font_Print(&Fnt,
						   TextSize,
						   Control->position.x-(textlen*TextSize)*0.5f+Control->BarGraph.size.x*0.5f,
						   Control->position.y-(TextSize*0.5f)+(Control->BarGraph.size.y*0.5f),
						   "%s", Control->BarGraph.titleText);

				// Left justified
				//Font_Print(&Font,
				//	TextSize,
				//	Control->position.x,
				//	Control->position.y-(TextSize*0.5f)+Control->BarGraph.size.y*0.5f,
				//	"%s", Control->BarGraph.titleText
				//);

				// right justified
				//Font_Print(&Font,
				//	TextSize,
				//	Control->position.x-(textlen*TextSize)+Control->BarGraph.size.x,
				//	Control->position.y-(TextSize*0.5f)+Control->BarGraph.size.y*0.5f,
				//	"%s", Control->BarGraph.titleText
				//);

				Instance->PositionSize.x=Control->position.x+Control->Button.size.x*0.5f;
				Instance->PositionSize.y=Control->position.y+Control->Button.size.y*0.5f;
				Instance->PositionSize.z=Control->Button.size.x;
				Instance->PositionSize.w=Control->Button.size.y;

				Instance->ColorValue.x=Control->color.x;
				Instance->ColorValue.y=Control->color.y;
				Instance->ColorValue.z=Control->color.z;
				Instance->ColorValue.w=0.0f;

				Instance->type=UI_CONTROL_BUTTON;
				Instance++;
				instanceCount++;
				break;
			}

			case UI_CONTROL_CHECKBOX:
			{
				// Text size is the radius of the checkbox, placed radius length away horizontally, centered vertically
				Font_Print(&Fnt,
						   Control->CheckBox.radius,
						   Control->position.x+Control->CheckBox.radius,
						   Control->position.y-(Control->CheckBox.radius/2.0f),
						   "%s", Control->CheckBox.titleText);

				Instance->PositionSize.x=Control->position.x;
				Instance->PositionSize.y=Control->position.y;
				Instance->PositionSize.z=Control->CheckBox.radius*2;
				Instance->PositionSize.w=Control->CheckBox.radius*2;

				Instance->ColorValue.x=Control->color.x;
				Instance->ColorValue.y=Control->color.y;
				Instance->ColorValue.z=Control->color.z;

				if(Control->CheckBox.value)
					Instance->ColorValue.w=1.0f;
				else
					Instance->ColorValue.w=0.0f;

				Instance->type=UI_CONTROL_CHECKBOX;
				Instance++;
				instanceCount++;
				break;
			}

			case UI_CONTROL_BARGRAPH:
			{
				// Get base length of title text
				float textlen=Font_StringBaseWidth(Control->BarGraph.titleText);

				// Scale text size based on the button size and length of text, but no bigger than 80% of button height
				float TextSize=min(Control->BarGraph.size.x/textlen*0.8f, Control->BarGraph.size.y*0.8f);

				// Print the text centered
				Font_Print(&Fnt,
						   TextSize,
						   Control->position.x-(textlen*TextSize)*0.5f+Control->BarGraph.size.x*0.5f,
						   Control->position.y-(TextSize*0.5f)+(Control->BarGraph.size.y*0.5f),
						   "%s", Control->BarGraph.titleText);

				// Left justified
				//Font_Print(
				//	TextSize,
				//	Control->position.x,
				//	Control->position.y-(TextSize*0.5f)+Control->BarGraph.size.y*0.5f,
				//	"%s", Control->BarGraph.titleText
				//);

				// right justified
				//Font_Print(
				//	TextSize,
				//	Control->position.x-(textlen*TextSize)+Control->BarGraph.size.x,
				//	Control->position.y-(TextSize*0.5f)+Control->BarGraph.size.y*0.5f,
				//	"%s", Control->BarGraph.titleText
				//);

				float normalize_value=(Control->BarGraph.value-Control->BarGraph.Min)/(Control->BarGraph.Max-Control->BarGraph.Min);

				Instance->PositionSize.x=Control->position.x+Control->BarGraph.size.x*0.5f;
				Instance->PositionSize.y=Control->position.y+Control->BarGraph.size.y*0.5f;
				Instance->PositionSize.z=Control->BarGraph.size.x;
				Instance->PositionSize.w=Control->BarGraph.size.y;

				Instance->ColorValue.x=Control->color.x;
				Instance->ColorValue.y=Control->color.y;
				Instance->ColorValue.z=Control->color.z;
				Instance->ColorValue.w=normalize_value;

				Instance->type=UI_CONTROL_BARGRAPH;
				Instance++;
				instanceCount++;
				break;
			}

			case UI_CONTROL_SPRITE:
				break;

			case UI_CONTROL_CURSOR:
			{
				Instance->PositionSize.x=Control->position.x+Control->Cursor.radius;
				Instance->PositionSize.y=Control->position.y-Control->Cursor.radius;
				Instance->PositionSize.z=Control->Cursor.radius*2;
				Instance->PositionSize.w=Control->Cursor.radius*2;

				Instance->ColorValue.x=Control->color.x;
				Instance->ColorValue.y=Control->color.y;
				Instance->ColorValue.z=Control->color.z;
				Instance->ColorValue.w=0.0f;

				Instance->type=UI_CONTROL_CURSOR;
				Instance++;
				instanceCount++;
				break;
			}
		}
	}

	// At the end of the instance buffer, build a list of non-instanced UI controls
	for(uint32_t i=0;i<controlCount;i++)
	{
		UI_Control_t *Control=List_GetPointer(&UI->controls, i);

		if(Control->type==UI_CONTROL_SPRITE)
		{
			Instance->PositionSize.x=Control->position.x;
			Instance->PositionSize.y=Control->position.y;
			Instance->PositionSize.z=Control->Sprite.size.x;
			Instance->PositionSize.w=Control->Sprite.size.y;

			Instance->ColorValue.x=Control->color.x;
			Instance->ColorValue.y=Control->color.y;
			Instance->ColorValue.z=Control->color.z;
			Instance->ColorValue.w=Control->Sprite.rotation;

			Instance->type=UI_CONTROL_SPRITE;
			Instance++;
		}
	}

	// Flush instance buffer caches, mostly needed for Android and maybe some iGPUs
	vkFlushMappedMemoryRanges(vkContext.Device, 1, &(VkMappedMemoryRange)
	{
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		VK_NULL_HANDLE,
		UI->instanceBuffer.DeviceMemory,
		0, VK_WHOLE_SIZE
	});

	vkCmdBindPipeline(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI->pipeline.Pipeline);

	// Bind vertex data buffer
	vkCmdBindVertexBuffers(perFrame[index].commandBuffer, 0, 1, &UI->vertexBuffer.Buffer, &(VkDeviceSize) { 0 });
	// Bind object instance buffer
	vkCmdBindVertexBuffers(perFrame[index].commandBuffer, 1, 1, &UI->instanceBuffer.Buffer, &(VkDeviceSize) { 0 });

	struct
	{
		vec2 Viewport;
		vec2 pad;
		matrix mvp;
	} UIPC;

	float z=-1.0f;

	if(isVR)
	{
		z=-1.5f;
		UIPC.Viewport=Vec2((float)xrContext.swapchainExtent.width, (float)xrContext.swapchainExtent.height);
	}
	else
		UIPC.Viewport=Vec2((float)swapchain.Extent.width, (float)swapchain.Extent.height);

	UIPC.mvp=MatrixMult(MatrixMult(MatrixMult(MatrixScale(UIPC.Viewport.x/UIPC.Viewport.y, 1.0f, 1.0f), MatrixTranslate(0.0f, 0.0f, z)), headPose), projection[eye]);

	vkCmdPushConstants(perFrame[index].commandBuffer, UI->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UIPC), &UIPC);

	// Draw sprites, they need descriptor set changes and aren't easy to draw instanced...
	uint32_t spriteCount=instanceCount;
	for(uint32_t i=0;i<controlCount;i++)
	{
		UI_Control_t *Control=List_GetPointer(&UI->controls, i);

		if(Control->type==UI_CONTROL_SPRITE)
		{
			vkuDescriptorSet_UpdateBindingImageInfo(&UI->descriptorSet, 0, Control->Sprite.Image);
			vkuAllocateUpdateDescriptorSet(&UI->descriptorSet, perFrame[index].descriptorPool);
			vkCmdBindDescriptorSets(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI->pipelineLayout, 0, 1, &UI->descriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

			// Use the last unused instanced data slot for drawing
			vkCmdDraw(perFrame[index].commandBuffer, 4, 1, 0, spriteCount++);
		}
	}

	vkuDescriptorSet_UpdateBindingImageInfo(&UI->descriptorSet, 0, &UI->blankImage);
	vkuAllocateUpdateDescriptorSet(&UI->descriptorSet, perFrame[index].descriptorPool);
	vkCmdBindDescriptorSets(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI->pipelineLayout, 0, 1, &UI->descriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	// Draw instanced UI elements
	vkCmdDraw(perFrame[index].commandBuffer, 4, instanceCount, 0, 0);

	return true;
}
