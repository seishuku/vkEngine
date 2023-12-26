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
extern VkuContext_t Context;
extern VkSampleCountFlags MSAA;
extern VkuSwapchain_t Swapchain;
extern VkRenderPass compositeRenderPass;

extern Font_t Fnt;

extern bool isVR;
extern VkuSwapchain_t Swapchain;
extern XruContext_t xrContext;
extern matrix modelView, projection[2], headPose;
// ---

typedef struct
{
	vec4 PositionSize;
	vec4 ColorValue;
	uint32_t Type, Pad[3];
} UI_Instance_t;

static bool UI_VulkanVertex(UI_t *UI)
{
	VkuBuffer_t stagingBuffer;
	void *data=NULL;

	// Create a dummy blank image for binding to descriptor sets when no texture is needed
	if(!vkuCreateImageBuffer(&Context, &UI->BlankImage,
	   VK_IMAGE_TYPE_2D, VK_FORMAT_B8G8R8A8_UNORM, 1, 1, 1, 1, 1,
	   VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL,
	   VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,
	   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0))
		return VK_FALSE;

	VkCommandBuffer CommandBuffer=vkuOneShotCommandBufferBegin(&Context);
	vkuTransitionLayout(CommandBuffer, UI->BlankImage.Image, 1, 0, 1, 0, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuOneShotCommandBufferEnd(&Context, CommandBuffer);

	vkCreateSampler(Context.Device, &(VkSamplerCreateInfo)
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
	}, VK_NULL_HANDLE, &UI->BlankImage.Sampler);
	vkCreateImageView(Context.Device, &(VkImageViewCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
			.image=UI->BlankImage.Image,
			.viewType=VK_IMAGE_VIEW_TYPE_2D,
			.format=VK_FORMAT_B8G8R8A8_UNORM,
			.components={ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
			.subresourceRange={ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 },
	}, VK_NULL_HANDLE, &UI->BlankImage.View);
	// ---

	// Create static vertex data buffer
	if(!vkuCreateGPUBuffer(&Context, &UI->VertexBuffer, sizeof(vec4)*4, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT))
		return false;

	// Create staging buffer, map it, and copy vertex data to it
	if(!vkuCreateHostBuffer(&Context, &stagingBuffer, sizeof(vec4)*4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT))
		return false;

	// Map it
	if(vkMapMemory(Context.Device, stagingBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, &data)!=VK_SUCCESS)
		return false;

	if(!data)
		return false;

	vec4 *Ptr=data;

	*Ptr++=Vec4(-0.5f, 0.5f, -1.0f, 1.0f);	// XYUV
	*Ptr++=Vec4(-0.5f, -0.5f, -1.0f, -1.0f);
	*Ptr++=Vec4(0.5f, 0.5f, 1.0f, 1.0f);
	*Ptr++=Vec4(0.5f, -0.5f, 1.0f, -1.0f);

	vkUnmapMemory(Context.Device, stagingBuffer.DeviceMemory);

	VkCommandBuffer CopyCommand=vkuOneShotCommandBufferBegin(&Context);
	vkCmdCopyBuffer(CopyCommand, stagingBuffer.Buffer, UI->VertexBuffer.Buffer, 1, &(VkBufferCopy) {.srcOffset=0, .dstOffset=0, .size=sizeof(vec4)*4 });
	vkuOneShotCommandBufferEnd(&Context, CopyCommand);

	// Delete staging data
	vkuDestroyBuffer(&Context, &stagingBuffer);
	// ---

	// Create instance buffer and map it
	vkuCreateHostBuffer(&Context, &UI->InstanceBuffer, sizeof(UI_Instance_t)*UI_HASHTABLE_MAX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	vkMapMemory(Context.Device, UI->InstanceBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void *)&UI->InstanceBufferPtr);
	// ---

	return true;
}

static bool UI_VulkanPipeline(UI_t *UI)
{
	vkuInitDescriptorSet(&UI->DescriptorSet, &Context);
	vkuDescriptorSet_AddBinding(&UI->DescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuAssembleDescriptorSetLayout(&UI->DescriptorSet);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&UI->DescriptorSet.DescriptorSetLayout,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=(sizeof(vec2)*2)+sizeof(matrix),
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},		
	}, 0, &UI->PipelineLayout);

	vkuInitPipeline(&UI->Pipeline, &Context);

	vkuPipeline_SetPipelineLayout(&UI->Pipeline, UI->PipelineLayout);
	vkuPipeline_SetRenderPass(&UI->Pipeline, compositeRenderPass);

	UI->Pipeline.Topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	UI->Pipeline.CullMode=VK_CULL_MODE_BACK_BIT;
	UI->Pipeline.RasterizationSamples=VK_SAMPLE_COUNT_1_BIT;

	UI->Pipeline.Blend=VK_TRUE;
	UI->Pipeline.SrcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
	UI->Pipeline.DstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	UI->Pipeline.ColorBlendOp=VK_BLEND_OP_ADD;
	UI->Pipeline.SrcAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
	UI->Pipeline.DstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	UI->Pipeline.AlphaBlendOp=VK_BLEND_OP_ADD;

	if(!vkuPipeline_AddStage(&UI->Pipeline, "shaders/ui_sdf.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&UI->Pipeline, "shaders/ui_sdf.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	vkuPipeline_AddVertexBinding(&UI->Pipeline, 0, sizeof(vec4), VK_VERTEX_INPUT_RATE_VERTEX);
	vkuPipeline_AddVertexAttribute(&UI->Pipeline, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*0);

	vkuPipeline_AddVertexBinding(&UI->Pipeline, 1, sizeof(UI_Instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);
	vkuPipeline_AddVertexAttribute(&UI->Pipeline, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*0);
	vkuPipeline_AddVertexAttribute(&UI->Pipeline, 2, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*1);
	vkuPipeline_AddVertexAttribute(&UI->Pipeline, 3, 1, VK_FORMAT_R32G32B32A32_UINT, sizeof(vec4)*2);

	VkPipelineRenderingCreateInfo PipelineRenderingCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount=1,
		.pColorAttachmentFormats=&Swapchain.SurfaceFormat.format,
	};

	if(!vkuAssemblePipeline(&UI->Pipeline, VK_NULL_HANDLE/*&PipelineRenderingCreateInfo*/))
		return false;

	UI_VulkanVertex(UI);

	return true;
}

// Initialize UI system.
bool UI_Init(UI_t *UI, vec2 Position, vec2 Size)
{
	if(UI==NULL)
		return false;

	UI->IDBase=0;

	// Set screen width/height
	UI->Position=Position;
	UI->Size=Size;

	// Initial 10 pre-allocated list of buttons, uninitialized
	List_Init(&UI->Controls, sizeof(UI_Control_t), 10, NULL);

	memset(UI->Controls_Hashtable, 0, sizeof(UI_Control_t *)*UI_HASHTABLE_MAX);

	// Vulkan stuff
	if(!UI_VulkanPipeline(UI))
		return false;

	return true;
}

void UI_Destroy(UI_t *UI)
{
	List_Destroy(&UI->Controls);

	if(UI->InstanceBuffer.DeviceMemory)
		vkUnmapMemory(Context.Device, UI->InstanceBuffer.DeviceMemory);

	vkuDestroyBuffer(&Context, &UI->InstanceBuffer);

	vkuDestroyBuffer(&Context, &UI->VertexBuffer);

	vkuDestroyImageBuffer(&Context, &UI->BlankImage);

	vkDestroyPipeline(Context.Device, UI->Pipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, UI->PipelineLayout, VK_NULL_HANDLE);

	vkDestroyDescriptorSetLayout(Context.Device, UI->DescriptorSet.DescriptorSetLayout, VK_NULL_HANDLE);
}

UI_Control_t *UI_FindControlByID(UI_t *UI, uint32_t ID)
{
	//for(uint32_t i=0;i<List_GetCount(&UI->Controls);i++)
	//{
	//	UI_Control_t *Control=(UI_Control_t *)List_GetPointer(&UI->Controls, i);

	//	// Check for matching ID and type
	//	if(Control->ID==ID)
	//		return Control;
	//}
	if(UI==NULL||ID>=UI_HASHTABLE_MAX||ID==UINT32_MAX)
		return NULL;

	UI_Control_t *Control=UI->Controls_Hashtable[ID];

	if(Control->ID==ID)
		return Control;

	return NULL;
}

// Checks hit on UI controls, also processes certain controls, intended to be used on mouse button down events
// Returns ID of hit, otherwise returns UINT32_MAX
// Position is the cursor position to test against UI controls
uint32_t UI_TestHit(UI_t *UI, vec2 Position)
{
	if(UI==NULL)
		return UINT32_MAX;

	// Offset by UI position
	Position=Vec2_Addv(Position, UI->Position);

	// Loop through all controls in the UI
	for(uint32_t i=0;i<List_GetCount(&UI->Controls);i++)
	{
		UI_Control_t *Control=List_GetPointer(&UI->Controls, i);

		switch(Control->Type)
		{
			case UI_CONTROL_BUTTON:
			{
				if(Position.x>=Control->Position.x&&Position.x<=Control->Position.x+Control->Button.Size.x&&
				   Position.y>=Control->Position.y&&Position.y<=Control->Position.y+Control->Button.Size.y)
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
				vec2 Normal=Vec2_Subv(Control->Position, Position);

				if(Vec2_Dot(Normal, Normal)<=Control->CheckBox.Radius*Control->CheckBox.Radius)
				{
					Control->CheckBox.Value=!Control->CheckBox.Value;
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
					if(Position.x>=Control->Position.x&&Position.x<=Control->Position.x+Control->BarGraph.Size.x&&
					   Position.y>=Control->Position.y&&Position.y<=Control->Position.y+Control->BarGraph.Size.y)
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
bool UI_ProcessControl(UI_t *UI, uint32_t ID, vec2 Position)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Offset by UI position
	Position=Vec2_Addv(Position, UI->Position);

	// Get the control from the ID
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control==NULL)
		return false;

	switch(Control->Type)
	{
		case UI_CONTROL_BUTTON:
			break;

		case UI_CONTROL_CHECKBOX:
			break;

		case UI_CONTROL_BARGRAPH:
			if(!Control->BarGraph.Readonly)
			{
				// If hit inside control area, map hit position to point on bargraph and set the value scaled to the set min and max
				if(Position.x>=Control->Position.x&&Position.x<=Control->Position.x+Control->BarGraph.Size.x&&
					Position.y>=Control->Position.y&&Position.y<=Control->Position.y+Control->BarGraph.Size.y)
					Control->BarGraph.Value=((Position.x-Control->Position.x)/Control->BarGraph.Size.x)*(Control->BarGraph.Max-Control->BarGraph.Min)+Control->BarGraph.Min;
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

	UI_Instance_t *Instance=(UI_Instance_t *)UI->InstanceBufferPtr;
	uint32_t instanceCount=0;

	const size_t controlCount=List_GetCount(&UI->Controls);

	// Build a list of instanceable UI controls
	for(uint32_t i=0;i<controlCount;i++)
	{
		UI_Control_t *Control=List_GetPointer(&UI->Controls, i);

		switch(Control->Type)
		{
			case UI_CONTROL_BUTTON:
			{
				// Get base length of title text
				float textlen=Font_StringBaseWidth(Control->Button.TitleText);

				// Scale text size based on the button size and length of text, but no bigger than 80% of button height
				float TextSize=min(Control->Button.Size.x/textlen*0.8f, Control->Button.Size.y*0.8f);

				// Print the text centered
				Font_Print(&Fnt,
						   TextSize,
						   Control->Position.x-(textlen*TextSize)*0.5f+Control->BarGraph.Size.x*0.5f,
						   Control->Position.y-(TextSize*0.5f)+(Control->BarGraph.Size.y*0.5f),
						   "%s", Control->BarGraph.TitleText);

				// Left justified
				//Font_Print(&Font,
				//	TextSize,
				//	Control->Position.x,
				//	Control->Position.y-(TextSize*0.5f)+Control->BarGraph.Size.y*0.5f,
				//	"%s", Control->BarGraph.TitleText
				//);

				// right justified
				//Font_Print(&Font,
				//	TextSize,
				//	Control->Position.x-(textlen*TextSize)+Control->BarGraph.Size.x,
				//	Control->Position.y-(TextSize*0.5f)+Control->BarGraph.Size.y*0.5f,
				//	"%s", Control->BarGraph.TitleText
				//);

				Instance->PositionSize.x=Control->Position.x+Control->Button.Size.x*0.5f;
				Instance->PositionSize.y=Control->Position.y+Control->Button.Size.y*0.5f;
				Instance->PositionSize.z=Control->Button.Size.x;
				Instance->PositionSize.w=Control->Button.Size.y;

				Instance->ColorValue.x=Control->Color.x;
				Instance->ColorValue.y=Control->Color.y;
				Instance->ColorValue.z=Control->Color.z;
				Instance->ColorValue.w=0.0f;

				Instance->Type=UI_CONTROL_BUTTON;
				Instance++;
				instanceCount++;
				break;
			}

			case UI_CONTROL_CHECKBOX:
			{
				// Text size is the radius of the checkbox, placed radius length away horizontally, centered vertically
				Font_Print(&Fnt,
						   Control->CheckBox.Radius,
						   Control->Position.x+Control->CheckBox.Radius,
						   Control->Position.y-(Control->CheckBox.Radius/2.0f),
						   "%s", Control->CheckBox.TitleText);

				Instance->PositionSize.x=Control->Position.x;
				Instance->PositionSize.y=Control->Position.y;
				Instance->PositionSize.z=Control->CheckBox.Radius*2;
				Instance->PositionSize.w=Control->CheckBox.Radius*2;

				Instance->ColorValue.x=Control->Color.x;
				Instance->ColorValue.y=Control->Color.y;
				Instance->ColorValue.z=Control->Color.z;

				if(Control->CheckBox.Value)
					Instance->ColorValue.w=1.0f;
				else
					Instance->ColorValue.w=0.0f;

				Instance->Type=UI_CONTROL_CHECKBOX;
				Instance++;
				instanceCount++;
				break;
			}

			case UI_CONTROL_BARGRAPH:
			{
				// Get base length of title text
				float textlen=Font_StringBaseWidth(Control->BarGraph.TitleText);

				// Scale text size based on the button size and length of text, but no bigger than 80% of button height
				float TextSize=min(Control->BarGraph.Size.x/textlen*0.8f, Control->BarGraph.Size.y*0.8f);

				// Print the text centered
				Font_Print(&Fnt,
						   TextSize,
						   Control->Position.x-(textlen*TextSize)*0.5f+Control->BarGraph.Size.x*0.5f,
						   Control->Position.y-(TextSize*0.5f)+(Control->BarGraph.Size.y*0.5f),
						   "%s", Control->BarGraph.TitleText);

				// Left justified
				//Font_Print(
				//	TextSize,
				//	Control->Position.x,
				//	Control->Position.y-(TextSize*0.5f)+Control->BarGraph.Size.y*0.5f,
				//	"%s", Control->BarGraph.TitleText
				//);

				// right justified
				//Font_Print(
				//	TextSize,
				//	Control->Position.x-(textlen*TextSize)+Control->BarGraph.Size.x,
				//	Control->Position.y-(TextSize*0.5f)+Control->BarGraph.Size.y*0.5f,
				//	"%s", Control->BarGraph.TitleText
				//);

				float normalize_value=(Control->BarGraph.Value-Control->BarGraph.Min)/(Control->BarGraph.Max-Control->BarGraph.Min);

				Instance->PositionSize.x=Control->Position.x+Control->BarGraph.Size.x*0.5f;
				Instance->PositionSize.y=Control->Position.y+Control->BarGraph.Size.y*0.5f;
				Instance->PositionSize.z=Control->BarGraph.Size.x;
				Instance->PositionSize.w=Control->BarGraph.Size.y;

				Instance->ColorValue.x=Control->Color.x;
				Instance->ColorValue.y=Control->Color.y;
				Instance->ColorValue.z=Control->Color.z;
				Instance->ColorValue.w=normalize_value;

				Instance->Type=UI_CONTROL_BARGRAPH;
				Instance++;
				instanceCount++;
				break;
			}

			case UI_CONTROL_SPRITE:
				break;

			case UI_CONTROL_CURSOR:
			{
				Instance->PositionSize.x=Control->Position.x+Control->Cursor.Radius;
				Instance->PositionSize.y=Control->Position.y-Control->Cursor.Radius;
				Instance->PositionSize.z=Control->Cursor.Radius*2;
				Instance->PositionSize.w=Control->Cursor.Radius*2;

				Instance->ColorValue.x=Control->Color.x;
				Instance->ColorValue.y=Control->Color.y;
				Instance->ColorValue.z=Control->Color.z;
				Instance->ColorValue.w=0.0f;

				Instance->Type=UI_CONTROL_CURSOR;
				Instance++;
				instanceCount++;
				break;
			}
		}
	}

	// At the end of the instance buffer, build a list of non-instanced UI controls
	for(uint32_t i=0;i<controlCount;i++)
	{
		UI_Control_t *Control=List_GetPointer(&UI->Controls, i);

		if(Control->Type==UI_CONTROL_SPRITE)
		{
			Instance->PositionSize.x=Control->Position.x;
			Instance->PositionSize.y=Control->Position.y;
			Instance->PositionSize.z=Control->Sprite.Size.x;
			Instance->PositionSize.w=Control->Sprite.Size.y;

			Instance->ColorValue.x=Control->Color.x;
			Instance->ColorValue.y=Control->Color.y;
			Instance->ColorValue.z=Control->Color.z;
			Instance->ColorValue.w=Control->Sprite.Rotation;

			Instance->Type=UI_CONTROL_SPRITE;
			Instance++;
		}
	}

	// Flush instance buffer caches, mostly needed for Android and maybe some iGPUs
	vkFlushMappedMemoryRanges(Context.Device, 1, &(VkMappedMemoryRange)
	{
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		VK_NULL_HANDLE,
		UI->InstanceBuffer.DeviceMemory,
		0, VK_WHOLE_SIZE
	});

	vkCmdBindPipeline(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI->Pipeline.Pipeline);

	// Bind vertex data buffer
	vkCmdBindVertexBuffers(perFrame[index].commandBuffer, 0, 1, &UI->VertexBuffer.Buffer, &(VkDeviceSize) { 0 });
	// Bind object instance buffer
	vkCmdBindVertexBuffers(perFrame[index].commandBuffer, 1, 1, &UI->InstanceBuffer.Buffer, &(VkDeviceSize) { 0 });

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
		UIPC.Viewport=Vec2((float)Swapchain.Extent.width, (float)Swapchain.Extent.height);

	UIPC.mvp=MatrixMult(MatrixMult(MatrixMult(MatrixScale(UIPC.Viewport.x/UIPC.Viewport.y, 1.0f, 1.0f), MatrixTranslate(0.0f, 0.0f, z)), headPose), projection[eye]);

	vkCmdPushConstants(perFrame[index].commandBuffer, UI->PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UIPC), &UIPC);

	// Draw sprites, they need descriptor set changes and aren't easy to draw instanced...
	uint32_t spriteCount=instanceCount;
	for(uint32_t i=0;i<controlCount;i++)
	{
		UI_Control_t *Control=List_GetPointer(&UI->Controls, i);

		if(Control->Type==UI_CONTROL_SPRITE)
		{
			vkuDescriptorSet_UpdateBindingImageInfo(&UI->DescriptorSet, 0, Control->Sprite.Image);
			vkuAllocateUpdateDescriptorSet(&UI->DescriptorSet, perFrame[index].descriptorPool);
			vkCmdBindDescriptorSets(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI->PipelineLayout, 0, 1, &UI->DescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

			// Use the last unused instanced data slot for drawing
			vkCmdDraw(perFrame[index].commandBuffer, 4, 1, 0, spriteCount++);
		}
	}

	vkuDescriptorSet_UpdateBindingImageInfo(&UI->DescriptorSet, 0, &UI->BlankImage);
	vkuAllocateUpdateDescriptorSet(&UI->DescriptorSet, perFrame[index].descriptorPool);
	vkCmdBindDescriptorSets(perFrame[index].commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI->PipelineLayout, 0, 1, &UI->DescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	// Draw instanced UI elements
	vkCmdDraw(perFrame[index].commandBuffer, 4, instanceCount, 0, 0);

	return true;
}
