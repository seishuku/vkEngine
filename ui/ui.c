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

extern uint32_t Width, Height;	// Window width/height from main app.
// ---

VkuDescriptorSet_t UIDescriptorSet;
VkPipelineLayout UIPipelineLayout;
VkuPipeline_t UIPipeline;

// Vertex data handles
VkuBuffer_t UIVertexBuffer;

// Instance data handles
VkuBuffer_t UIInstanceBuffer;
void *UIInstanceBufferPtr;

typedef struct
{
	vec4 PositionSize;
	vec4 ColorValue;
	uint32_t Type;
	uint32_t Pad[3];
} UI_Instance_t;

static bool UI_VulkanPipeline(void)
{
	vkuInitDescriptorSet(&UIDescriptorSet, &Context);
	//vkuDescriptorSet_AddBinding(&UIDescriptorSet, 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
	vkuAssembleDescriptorSetLayout(&UIDescriptorSet);

	vkCreatePipelineLayout(Context.Device, &(VkPipelineLayoutCreateInfo)
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount=1,
		.pSetLayouts=&UIDescriptorSet.DescriptorSetLayout,
		.pushConstantRangeCount=1,
		.pPushConstantRanges=&(VkPushConstantRange)
		{
			.offset=0,
			.size=sizeof(uint32_t)*2,
			.stageFlags=VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT,
		},		
	}, 0, &UIPipelineLayout);

	vkuInitPipeline(&UIPipeline, &Context);

	vkuPipeline_SetPipelineLayout(&UIPipeline, UIPipelineLayout);

	UIPipeline.Topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
	UIPipeline.CullMode=VK_CULL_MODE_BACK_BIT;
	UIPipeline.RasterizationSamples=VK_SAMPLE_COUNT_1_BIT;

	UIPipeline.Blend=VK_TRUE;
	UIPipeline.SrcColorBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
	UIPipeline.DstColorBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	UIPipeline.ColorBlendOp=VK_BLEND_OP_ADD;
	UIPipeline.SrcAlphaBlendFactor=VK_BLEND_FACTOR_SRC_ALPHA;
	UIPipeline.DstAlphaBlendFactor=VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	UIPipeline.AlphaBlendOp=VK_BLEND_OP_ADD;

	if(!vkuPipeline_AddStage(&UIPipeline, "./shaders/ui_sdf.vert.spv", VK_SHADER_STAGE_VERTEX_BIT))
		return false;

	if(!vkuPipeline_AddStage(&UIPipeline, "./shaders/ui_sdf.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT))
		return false;

	vkuPipeline_AddVertexBinding(&UIPipeline, 0, sizeof(vec4), VK_VERTEX_INPUT_RATE_VERTEX);
	vkuPipeline_AddVertexAttribute(&UIPipeline, 0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*0);

	vkuPipeline_AddVertexBinding(&UIPipeline, 1, sizeof(UI_Instance_t), VK_VERTEX_INPUT_RATE_INSTANCE);
	vkuPipeline_AddVertexAttribute(&UIPipeline, 1, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*0);
	vkuPipeline_AddVertexAttribute(&UIPipeline, 2, 1, VK_FORMAT_R32G32B32A32_SFLOAT, sizeof(vec4)*1);
	vkuPipeline_AddVertexAttribute(&UIPipeline, 3, 1, VK_FORMAT_R32G32B32A32_UINT, sizeof(vec4)*2);

	VkPipelineRenderingCreateInfo PipelineRenderingCreateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
		.colorAttachmentCount=1,
		.pColorAttachmentFormats=&Swapchain.SurfaceFormat.format,
	};

	if(!vkuAssemblePipeline(&UIPipeline, &PipelineRenderingCreateInfo))
		return false;

	return true;
}

static bool UI_VulkanVertex(void)
{
	VkuBuffer_t stagingBuffer;
	void *data=NULL;

	// Create static vertex data buffer
	if(!vkuCreateGPUBuffer(&Context, &UIVertexBuffer, sizeof(vec4)*4, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT))
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
	vkCmdCopyBuffer(CopyCommand, stagingBuffer.Buffer, UIVertexBuffer.Buffer, 1, &(VkBufferCopy) {.srcOffset=0, .dstOffset=0, .size=sizeof(vec4)*4 });
	vkuOneShotCommandBufferEnd(&Context, CopyCommand);

	// Delete staging data
	vkuDestroyBuffer(&Context, &stagingBuffer);
	// ---

	// Create instance buffer and map it
	vkuCreateHostBuffer(&Context, &UIInstanceBuffer, sizeof(float)*8*255, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	vkMapMemory(Context.Device, UIInstanceBuffer.DeviceMemory, 0, VK_WHOLE_SIZE, 0, (void *)&UIInstanceBufferPtr);
	// ---

	return true;
}

// Initialize UI system.
bool UI_Init(UI_t *UI, uint32_t Width, uint32_t Height)
{
	if(UI==NULL||Width==0||Height==0)
		return false;

	// Set screen width/height
	UI->Width=Width;
	UI->Height=Height;

	// Initial 10 pre-allocated list of buttons, uninitialized
	List_Init(&UI->Controls, sizeof(UI_Control_t), 10, NULL);

	// Vulkan stuff
	if(!UI_VulkanPipeline())
		return false;

	if(!UI_VulkanVertex())
		return false;

	return true;
}

void UI_Destroy(UI_t *UI)
{
	List_Destroy(&UI->Controls);

	if(UIInstanceBuffer.DeviceMemory)
		vkUnmapMemory(Context.Device, UIInstanceBuffer.DeviceMemory);

	vkuDestroyBuffer(&Context, &UIInstanceBuffer);

	vkuDestroyBuffer(&Context, &UIVertexBuffer);

	vkDestroyPipeline(Context.Device, UIPipeline.Pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(Context.Device, UIPipelineLayout, VK_NULL_HANDLE);

	vkDestroyDescriptorSetLayout(Context.Device, UIDescriptorSet.DescriptorSetLayout, VK_NULL_HANDLE);
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

// Returns ID of hit, otherwise returns UINT32_MAX
// Position could be mouse coords in the area where the controls are placed
uint32_t UI_TestHit(UI_t *UI, vec2 Position)
{
	if(UI==NULL)
		return UINT32_MAX;

	// Invert y, this depends on how the system draws (bottom up vs top down)
	Position.y=UI->Height-Position.y;

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

			case UI_CONTROL_BARGRAPH:
				if(!Control->BarGraph.Readonly)
				{
					// If hit inside control area, map hit position to point on bargraph and set the value scaled to the set min and max
					if(Position.x>=Control->Position.x&&Position.x<=Control->Position.x+Control->BarGraph.Size.x&&
					   Position.y>=Control->Position.y&&Position.y<=Control->Position.y+Control->BarGraph.Size.y)
					{
						Control->BarGraph.Value=((Position.x-Control->Position.x)/Control->BarGraph.Size.x)*(Control->BarGraph.Max-Control->BarGraph.Min)+Control->BarGraph.Min;
						return Control->ID;
					}
				}
				break;
		}
	}

	// Nothing found
	return UINT32_MAX;
}

bool UI_Draw(UI_t *UI, uint32_t Index)
{
	if(UI==NULL)
		return false;

	UI_Instance_t *Instance=(UI_Instance_t *)UIInstanceBufferPtr;
	uint32_t Count=0;

	for(uint32_t i=0;i<List_GetCount(&UI->Controls);i++)
	{
		UI_Control_t *Control=List_GetPointer(&UI->Controls, i);

		switch(Control->Type)
		{
			case UI_CONTROL_BUTTON:
			{
				// Length of title text
				uint32_t textlen=(uint32_t)strlen(Control->TitleText);

				// Scale text size based on the button size and length of text, but no bigger than 80% of button height
				float TextSize=min(Control->Button.Size.x/textlen*0.8f, Control->Button.Size.y*0.8f);

				// Print the text centered
				Font_Print(
					TextSize,
					Control->Position.x-(textlen*TextSize)*0.5f+Control->Button.Size.x*0.5f,
					Control->Position.y-(TextSize*0.5f)+Control->Button.Size.y*0.5f,
					"%s", Control->TitleText
				);

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
					"%s", Control->TitleText
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
				// Length of title text
				uint32_t textlen=(uint32_t)strlen(Control->TitleText);

				// Scale text size based on the bargraph size and length of text, but no bigger than 80% of bargraph height
				float TextSize=min(Control->BarGraph.Size.x/textlen*0.8f, Control->BarGraph.Size.y*0.8f);

				// Print the text centered
				Font_Print(
					TextSize,
					Control->Position.x-(textlen*TextSize)/2.0f+Control->BarGraph.Size.x*0.5f,
					Control->Position.y-(TextSize/2.0f)+Control->BarGraph.Size.y*0.5f,
					"%s", Control->TitleText
				);

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
		}
	}

	vkCmdBindPipeline(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UIPipeline.Pipeline);

	vkuAllocateUpdateDescriptorSet(&UIDescriptorSet, PerFrame[Index].DescriptorPool);
	vkCmdBindDescriptorSets(PerFrame[Index].CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UIPipelineLayout, 0, 1, &UIDescriptorSet.DescriptorSet, 0, VK_NULL_HANDLE);

	// Bind vertex data buffer
	vkCmdBindVertexBuffers(PerFrame[Index].CommandBuffer, 0, 1, &UIVertexBuffer.Buffer, &(VkDeviceSize) { 0 });
	// Bind object instance buffer
	vkCmdBindVertexBuffers(PerFrame[Index].CommandBuffer, 1, 1, &UIInstanceBuffer.Buffer, &(VkDeviceSize) { 0 });

	uint32_t viewport[2]={ Width, Height };
	vkCmdPushConstants(PerFrame[Index].CommandBuffer, UIPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t)*2, viewport);

	vkCmdDraw(PerFrame[Index].CommandBuffer, 4, Count, 0, 0);

	return true;
}
