#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../vulkan/vulkan.h"
#include "../perframe.h"
#include "../utils/genid.h"
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

// external Vulkan context data/functions for this module:
extern VkuContext_t Context;
extern VkSampleCountFlags MSAA;
extern VkuSwapchain_t Swapchain;
// ---

typedef struct
{
	vec4 PositionSize;
	vec4 ColorValue;
	uint32_t Type;
	uint32_t DescriptorSetOffset;
	uint32_t Pad[2];
} UI_Instance_t;

static bool UI_VulkanVertex(UI_t *UI)
{
	VkuBuffer_t stagingBuffer;
	void *data=NULL;

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

	return true;
}

static bool UI_VulkanPipeline(UI_t *UI)
{
	VkDescriptorSetLayoutBinding DescriptorSetLayoutBinding=
	{
		.binding=0,
		.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.descriptorCount=1,
		.stageFlags=VK_SHADER_STAGE_FRAGMENT_BIT,
		.pImmutableSamplers=VK_NULL_HANDLE
	};

	VkDescriptorSetLayoutCreateInfo LayoutCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount=1,
		.pBindings=&DescriptorSetLayoutBinding,
	};

	if(vkCreateDescriptorSetLayout(Context.Device, &LayoutCreateInfo, NULL, &UI->DescriptorSetLayout)!=VK_SUCCESS)
		return VK_FALSE;

	vkCreateDescriptorPool(Context.Device, &(VkDescriptorPoolCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.maxSets=1024, // Max number of descriptor sets that can be allocated from this pool
		.poolSizeCount=1,
		.pPoolSizes=(VkDescriptorPoolSize[])
		{
			{
				.type=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.descriptorCount=1,
			},
		},
	}, VK_NULL_HANDLE, &UI->DescriptorPool);
	
	VkDescriptorSetLayout Layout[32];

	for(uint32_t i=0;i<32;i++)
		Layout[i]=UI->DescriptorSetLayout;

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=32,
		.pSetLayouts=Layout,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=sizeof(uint32_t)*2,
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},		
	}, 0, &UI->PipelineLayout);

	// Create instance buffer and map it
	vkuCreateHostBuffer(&Context, &UI->InstanceBuffer, sizeof(float)*8*255, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	vkMapMemory(Context.Device, UI->InstanceBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void *)&UI->InstanceBufferPtr);
	// ---

	vkuInitPipeline(&UI->Pipeline, &Context);

	vkuPipeline_SetPipelineLayout(&UI->Pipeline, UI->PipelineLayout);

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

	if(!vkuPipeline_AddStage(&UI->Pipeline, "./shaders/ui_sdf.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&UI->Pipeline, "./shaders/ui_sdf.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
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

	if(!vkuAssemblePipeline(&UI->Pipeline, &PipelineRenderingCreateInfo))
		return false;

	UI_VulkanVertex(UI);

	return true;
}

// Initialize UI system.
bool UI_Init(UI_t *UI, vec2 Position, vec2 Size)
{
	if(UI==NULL)
		return false;

	// Set screen width/height
	UI->Position=Position;
	UI->Size=Size;

	// Initial 10 pre-allocated list of buttons, uninitialized
	List_Init(&UI->Controls, sizeof(UI_Control_t), 10, NULL);

	List_Init(&UI->DescriptorSets, sizeof(VkDescriptorSet), 10, NULL);

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

	vkDestroyPipeline(Context.Device, UI->Pipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, UI->PipelineLayout, VK_NULL_HANDLE);

	vkDestroyDescriptorSetLayout(Context.Device, UI->DescriptorSetLayout, VK_NULL_HANDLE);

	vkDestroyDescriptorPool(Context.Device, UI->DescriptorPool, VK_NULL_HANDLE);

	List_Destroy(&UI->DescriptorSets);
}

UI_Control_t *UI_FindControlByID(UI_t *UI, uint32_t ID)
{
	for(uint32_t i=0;i<List_GetCount(&UI->Controls);i++)
	{
		UI_Control_t *Control=(UI_Control_t *)List_GetPointer(&UI->Controls, i);

		// Check for matching ID and type
		if(Control->ID==ID)
			return Control;
	}

	return NULL;
}

// Checks hit on UI controls, also processes certain controls, intended to be used on mouse button down events
// Returns ID of hit, otherwise returns UINT32_MAX
// Position is the cursor position to test against UI controls
uint32_t UI_TestHit(UI_t *UI, vec2 Position)
{
	if(UI==NULL)
		return UINT32_MAX;

	// Invert y, this depends on how the system draws (bottom up vs top down)
	Position.y=UI->Size.y-Position.y;

	// Offset by UI position
	Position=Vec2_Addv(Position, UI->Position);

	// Loop through all controls in the UI
	for(uint32_t i=0;i<List_GetCount(&UI->Controls);i++)
	{
		UI_Control_t *Control=List_GetPointer(&UI->Controls, i);

		switch(Control->Type)
		{
			case UI_CONTROL_BUTTON:
				if(Position.x>=Control->Position.x&&Position.x<=Control->Position.x+Control->Button.Size.x&&
				   Position.y>=Control->Position.y&&Position.y<=Control->Position.y+Control->Button.Size.y)
				{
					// TODO: This could potentionally be an issue if the callback blocks
					if(Control->Button.Callback)
						Control->Button.Callback(NULL);

					return Control->ID;
				}
				break;

			case UI_CONTROL_CHECKBOX:
				vec2 Normal=Vec2_Subv(Control->Position, Position);

				if(Vec2_Dot(Normal, Normal)<=Control->CheckBox.Radius*Control->CheckBox.Radius)
				{
					Control->CheckBox.Value=!Control->CheckBox.Value;
					return Control->ID;
				}
				break;

			// Only return the ID of this control
			case UI_CONTROL_BARGRAPH:
				if(!Control->BarGraph.Readonly)
				{
					// If hit inside control area, map hit position to point on bargraph and set the value scaled to the set min and max
					if(Position.x>=Control->Position.x&&Position.x<=Control->Position.x+Control->BarGraph.Size.x&&
					   Position.y>=Control->Position.y&&Position.y<=Control->Position.y+Control->BarGraph.Size.y)
						return Control->ID;
				}
				break;

			case UI_CONTROL_SPRITE:
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

	// Invert y, this depends on how the system draws (bottom up vs top down)
	Position.y=UI->Size.y-Position.y;

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
	}

	return true;
}

bool UI_Draw(UI_t *UI, uint32_t Index)
{
	if(UI==NULL)
		return false;

	UI_Instance_t *Instance=(UI_Instance_t *)UI->InstanceBufferPtr;
	uint32_t Count=0;

	for(uint32_t i=0;i<List_GetCount(&UI->Controls);i++)
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
				Font_Print(
					TextSize,
					Control->Position.x-(textlen*TextSize)*0.5f+Control->BarGraph.Size.x*0.5f,
					Control->Position.y-(TextSize*0.5f)+(Control->BarGraph.Size.y*0.5f),
					"%s", Control->BarGraph.TitleText
				);

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
				Count++;
				break;
			}

			case UI_CONTROL_CHECKBOX:
			{
				// Text size is the radius of the checkbox, placed radius length away horizontally, centered vertically
				Font_Print(
					Control->CheckBox.Radius,
					Control->Position.x+Control->CheckBox.Radius,
					Control->Position.y-(Control->CheckBox.Radius/2.0f),
					"%s", Control->CheckBox.TitleText
				);

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
				Count++;
				break;
			}

			case UI_CONTROL_BARGRAPH:
			{
				// Get base length of title text
				float textlen=Font_StringBaseWidth(Control->BarGraph.TitleText);

				// Scale text size based on the button size and length of text, but no bigger than 80% of button height
				float TextSize=min(Control->BarGraph.Size.x/textlen*0.8f, Control->BarGraph.Size.y*0.8f);

				// Print the text centered
				Font_Print(
					TextSize,
					Control->Position.x-(textlen*TextSize)*0.5f+Control->BarGraph.Size.x*0.5f,
					Control->Position.y-(TextSize*0.5f)+(Control->BarGraph.Size.y*0.5f),
					"%s", Control->BarGraph.TitleText
				);

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
				Count++;
				break;
			}

			case UI_CONTROL_SPRITE:
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
				Instance->DescriptorSetOffset=Control->Sprite.DescriptorSetOffset;
				Instance++;
				Count++;
				break;
			}
		}
	}

	vkCmdBindPipeline(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI->Pipeline.Pipeline);

	uint32_t NumDescriptors=(uint32_t)List_GetCount(&UI->DescriptorSets);

	if(NumDescriptors)
		vkCmdBindDescriptorSets(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI->PipelineLayout, 0, NumDescriptors, List_GetPointer(&UI->DescriptorSets, 0), 0, VK_NULL_HANDLE);

	// Bind vertex data buffer
	vkCmdBindVertexBuffers(PerFrame[Index].CommandBuffer, 0, 1, &UI->VertexBuffer.Buffer, &(VkDeviceSize) { 0 });
	// Bind object instance buffer
	vkCmdBindVertexBuffers(PerFrame[Index].CommandBuffer, 1, 1, &UI->InstanceBuffer.Buffer, &(VkDeviceSize) { 0 });

	uint32_t viewport[2]={ (uint32_t)UI->Size.x, (uint32_t)UI->Size.y };
	vkCmdPushConstants(PerFrame[Index].CommandBuffer, UI->PipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t)*2, viewport);

	vkCmdDraw(PerFrame[Index].CommandBuffer, 4, Count, 0, 0);

	return true;
}
