#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../perframe.h"
#include "../math/math.h"
#include "../utils/id.h"
#include "../utils/list.h"
#include "../utils/pipeline.h"
#include "../font/font.h"
#include "../vr/vr.h"
#include "ui.h"

// external Vulkan data and font
extern VkuContext_t vkContext;

extern matrix modelView, projection[2], headPose;
// ---

typedef struct
{
	vec4 positionSize;
	vec4 colorValue;
	uint32_t type, flag;
	vec2 extra;
} UI_Instance_t;

static bool UI_VulkanVertex(UI_t *UI)
{
	VkuBuffer_t stagingBuffer;

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
	if(!stagingBuffer.memory->mappedPointer)
		return false;

	vec4 *vecPtr=(vec4 *)stagingBuffer.memory->mappedPointer;

	*vecPtr++=Vec4(-0.5f, 0.5f, -1.0f, 1.0f);	// XYUV
	*vecPtr++=Vec4(-0.5f, -0.5f, -1.0f, -1.0f);
	*vecPtr++=Vec4(0.5f, 0.5f, 1.0f, 1.0f);
	*vecPtr++=Vec4(0.5f, -0.5f, 1.0f, -1.0f);

	VkCommandBuffer CopyCommand=vkuOneShotCommandBufferBegin(&vkContext);
	vkCmdCopyBuffer(CopyCommand, stagingBuffer.buffer, UI->vertexBuffer.buffer, 1, &(VkBufferCopy) {.srcOffset=0, .dstOffset=0, .size=sizeof(vec4)*4 });
	vkuOneShotCommandBufferEnd(&vkContext, CopyCommand);

	// Delete staging data
	vkuDestroyBuffer(&vkContext, &stagingBuffer);
	// ---

	// Create instance buffer and map it
	vkuCreateHostBuffer(&vkContext, &UI->instanceBuffer, sizeof(UI_Instance_t)*UI_HASHTABLE_MAX, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

	UI->instanceBufferPtr=UI->instanceBuffer.memory->mappedPointer;
	// ---

	return true;
}

static bool UI_VulkanPipeline(UI_t *UI)
{
	if(!CreatePipeline(&vkContext, &UI->pipeline, UI->renderPass, "pipelines/ui_sdf.pipeline"))
		return false;

	UI_VulkanVertex(UI);

	return true;
}

// Initialize UI system.
bool UI_Init(UI_t *UI, vec2 position, vec2 size, VkRenderPass renderPass)
{
	if(UI==NULL)
		return false;

	//UI->baseID=0;
	ID_Init(UI->baseID);

	UI->renderPass=renderPass;

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
	// Find any window controls and delete children list, and handle other misc control deletes
	for(uint32_t i=0;i<List_GetCount(&UI->controls);i++)
	{
		UI_Control_t *control=(UI_Control_t *)List_GetPointer(&UI->controls, i);

		if(control->type==UI_CONTROL_WINDOW)
			List_Destroy(&control->window.children);

		if(control->type==UI_CONTROL_TEXT)
			Zone_Free(zone, control->text.titleText);

		if(control->type==UI_CONTROL_EDITTEXT)
			Zone_Free(zone, control->editText.buffer);
	}

	List_Destroy(&UI->controls);

	vkuDestroyBuffer(&vkContext, &UI->instanceBuffer);

	vkuDestroyBuffer(&vkContext, &UI->vertexBuffer);

	vkuDestroyImageBuffer(&vkContext, &UI->blankImage);

	DestroyPipeline(&vkContext, &UI->pipeline);
}

bool UI_AddControl(UI_t *UI, UI_Control_t *control)
{
	// Add control to controls list
	if(!List_Add(&UI->controls, control))
		return false;

	// Refresh hashmap pointers, controls list may reallocate and change pointers, so just refresh everything any time a control is added.
	for(uint32_t i=0;i<List_GetCount(&UI->controls);i++)
	{
		UI_Control_t *ptr=(UI_Control_t *)List_GetPointer(&UI->controls, i);
		UI->controlsHashtable[ptr->ID]=ptr;
	}

	return true;
}

UI_Control_t *UI_FindControlByID(UI_t *UI, uint32_t ID)
{
	if(UI==NULL||ID>=UI_HASHTABLE_MAX||ID==UINT32_MAX)
		return NULL;

	UI_Control_t *control=UI->controlsHashtable[ID];

	if(control->ID==ID)
		return control;

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
		UI_Control_t *control=List_GetPointer(&UI->controls, i);

		// Only test non-child and visible controls here
		if(control->childParentID!=UINT32_MAX||control->visibility)
			continue;

		switch(control->type)
		{
			case UI_CONTROL_BUTTON:
			{
				if(position.x>=control->position.x&&position.x<=control->position.x+control->button.size.x&&
					position.y>=control->position.y&&position.y<=control->position.y+control->button.size.y)
				{
					// TODO: This could potentionally be an issue if the callback blocks
					if(control->button.callback)
						control->button.callback(NULL);

					return control->ID;
				}
				break;
			}

			case UI_CONTROL_CHECKBOX:
			{
				if(Vec2_DistanceSq(control->position, position)<=control->checkBox.radius*control->checkBox.radius)
				{
					control->checkBox.value=!control->checkBox.value;
					return control->ID;
				}
				break;
			}

			case UI_CONTROL_BARGRAPH:
			{
				if(!control->barGraph.mutability)
				{
					// If hit inside control area, map hit position to point on bargraph and set the value scaled to the set min and max based on it's direction
					if(control->barGraph.direction)
					{
						if(position.x>=control->position.x&&position.x<=control->position.x+control->barGraph.size.x&&
						   position.y>=control->position.y&&position.y<=control->position.y+control->barGraph.size.y)
						{
							control->barGraph.value=((position.y-control->position.y)/control->barGraph.size.y)*(control->barGraph.max-control->barGraph.min)+control->barGraph.min;
							return control->ID;
						}
					}
					else
					{
						if(position.x>=control->position.x&&position.x<=control->position.x+control->barGraph.size.x&&
						   position.y>=control->position.y&&position.y<=control->position.y+control->barGraph.size.y)
						{
							control->barGraph.value=((position.x-control->position.x)/control->barGraph.size.x)*(control->barGraph.max-control->barGraph.min)+control->barGraph.min;
							return control->ID;
						}
					}
				}
				break;
			}

			case UI_CONTROL_EDITTEXT:
			{
				if(!control->editText.mutability)
				{
					// If hit inside control area, map hit position to point on bargraph and set the value scaled to the set min and max
					if(position.x>=control->position.x&&position.x<=control->position.x+control->editText.size.x&&
					   position.y>=control->position.y&&position.y<=control->position.y+control->editText.size.y)
					{
						return control->ID;
					}
				}
				break;
			}

			case UI_CONTROL_SPRITE:
				break;

			case UI_CONTROL_CURSOR:
				break;

			case UI_CONTROL_VIRTUALSTICK:
			{
				if(Vec2_DistanceSq(control->position, position)<=control->virtualStick.radius*control->virtualStick.radius)
				{
					control->virtualStick.value=Vec2_Clamp(Vec2_Muls(Vec2_Subv(position, control->position), 1.0f/control->virtualStick.radius), -1.0f, 1.0f);
					return control->ID;
				}
				break;
			}


			case UI_CONTROL_WINDOW:
			{
				// Hit test children
				for(uint32_t j=0;j<List_GetCount(&control->window.children);j++)
				{
					uint32_t *childID=List_GetPointer(&control->window.children, j);
					UI_Control_t *child=UI_FindControlByID(UI, *childID);

					if(child->visibility)
						continue;

					vec2 childPos=Vec2_Addv(control->position, child->position);

					switch(child->type)
					{
						case UI_CONTROL_BUTTON:
						{
							if(position.x>=childPos.x&&position.x<=childPos.x+child->button.size.x&&
								position.y>=childPos.y&&position.y<=childPos.y+child->button.size.y)
							{
								// TODO: This could potentionally be an issue if the callback blocks
								if(child->button.callback)
									child->button.callback(NULL);

								return child->ID;
							}
							break;
						}

						case UI_CONTROL_CHECKBOX:
						{
							if(Vec2_DistanceSq(childPos, position)<=child->checkBox.radius*child->checkBox.radius)
							{
								child->checkBox.value=!child->checkBox.value;
								return child->ID;
							}
							break;
						}

						// Only return the ID of this control
						case UI_CONTROL_BARGRAPH:
						{
							if(!child->barGraph.mutability)
							{
								// If hit inside control area, map hit position to point on bargraph and set the value scaled to the set min and max based on it's direction
								if(child->barGraph.direction)
								{
									if(position.x>=childPos.x&&position.x<=childPos.x+child->barGraph.size.x&&
									   position.y>=childPos.y&&position.y<=childPos.y+child->barGraph.size.y)
									{
										child->barGraph.value=((position.y-childPos.y)/child->barGraph.size.y)*(child->barGraph.max-child->barGraph.min)+child->barGraph.min;
										return child->ID;
									}
								}
								else
								{
									if(position.x>=childPos.x&&position.x<=childPos.x+child->barGraph.size.x&&
									   position.y>=childPos.y&&position.y<=childPos.y+child->barGraph.size.y)
									{
										child->barGraph.value=((position.x-childPos.x)/child->barGraph.size.x)*(child->barGraph.max-child->barGraph.min)+child->barGraph.min;
										return child->ID;
									}
								}
							}
							break;
						}

						case UI_CONTROL_EDITTEXT:
						{
							if(!child->editText.mutability)
							{
								// If hit inside control area, map hit position to point on bargraph and set the value scaled to the set min and max
								if(position.x>=childPos.x&&position.x<=childPos.x+child->editText.size.x&&
								   position.y>=childPos.y&&position.y<=childPos.y+child->editText.size.y)
								{
									return child->ID;
								}
							}
							break;
						}

						default:
							break;
					}
				}

				// Didn't hit on any children, so test the window itself
				vec2 padding=Vec2(control->window.size.x+UI_CONTROL_WINDOW_BORDER*2.0f, control->window.size.y+UI_CONTROL_WINDOW_BORDER*2.0f+UI_CONTROL_WINDOW_TITLEBAR_HEIGHT);

				// Top-left corner of the whole window area (with borders/title bar)
				vec2 windowTopLeft=Vec2(control->position.x-UI_CONTROL_WINDOW_BORDER*0.5f, control->position.y+UI_CONTROL_WINDOW_TITLEBAR_HEIGHT);

				// Bottom-left corner (where the actual "window" origin would be if drawn centered vertically)
				vec2 windowBottomLeft=Vec2(windowTopLeft.x, windowTopLeft.y-padding.y);

				// Hit test against this full rectangle
				if(position.x>=windowBottomLeft.x&&position.x<=windowBottomLeft.x+padding.x&&
					position.y>=windowBottomLeft.y&&position.y<=windowBottomLeft.y+padding.y)
				{
					control->window.hitOffset=Vec2_Subv(position, control->position);
					return control->ID;
				}

				break;
			}

			default:
				break;
		}
	}

	// Nothing found, empty list
	return UINT32_MAX;
}

// Processes hit on certain UI controls by ID (returned by UI_TestHit), intended to be used by "mouse move" events.
// Returns false on error
// Position is the cursor position to modify UI controls
bool UI_ProcessControl(UI_t *UI, uint32_t ID, vec2 hitPos)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// offset by UI position
	vec2 position=Vec2_Addv(hitPos, UI->position);

	// Get the control from the ID
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	// If this control has a parent ID set, then it's a child control
	//	so we must offset by the parent position
	//	(subtract, because need to be in "parent" space)
	if(control->childParentID!=UINT32_MAX)
	{
		UI_Control_t *parent=UI_FindControlByID(UI, control->childParentID);
		position=Vec2_Subv(position, parent->position);
	}

	if(control==NULL)
		return false;

	switch(control->type)
	{
		case UI_CONTROL_BUTTON:
			break;

		case UI_CONTROL_CHECKBOX:
			break;

		case UI_CONTROL_BARGRAPH:
			if(!control->barGraph.mutability)
			{
				// If hit inside control area, map hit position to point on bargraph and set the value scaled to the set min and max based on it's direction
				if(control->barGraph.direction)
				{
					if(position.x>=control->position.x&&position.x<=control->position.x+control->barGraph.size.x&&
					   position.y>=control->position.y&&position.y<=control->position.y+control->barGraph.size.y)
						control->barGraph.value=((position.y-control->position.y)/control->barGraph.size.y)*(control->barGraph.max-control->barGraph.min)+control->barGraph.min;
				}
				else
				{
					if(position.x>=control->position.x&&position.x<=control->position.x+control->barGraph.size.x&&
					   position.y>=control->position.y&&position.y<=control->position.y+control->barGraph.size.y)
						control->barGraph.value=((position.x-control->position.x)/control->barGraph.size.x)*(control->barGraph.max-control->barGraph.min)+control->barGraph.min;
				}
			}
			break;

		case UI_CONTROL_SPRITE:
			break;

		case UI_CONTROL_CURSOR:
			break;

		case UI_CONTROL_VIRTUALSTICK:
		{
			// Scale/clamp value to -1.0 to 1.0 value
			control->virtualStick.value=Vec2_Clamp(Vec2_Muls(Vec2_Subv(position, control->position), 1.0f/control->virtualStick.radius), -1.0f, 1.0f);
			break;
		}

		case UI_CONTROL_WINDOW:
		{
			control->position=Vec2_Subv(position, control->window.hitOffset);
			UI_UpdateTextPosition(UI, control->window.titleTextID, Vec2_Add(control->position, 0.0f, 16.0f-(UI_CONTROL_WINDOW_BORDER*0.5f)));
			break;
		}

		default:
			break;
	}

	return true;
}

static bool UI_AddControlInstance(UI_Instance_t **instance, uint32_t *instanceCount, UI_Control_t *control, vec2 offset, float dt)
{
	switch(control->type)
	{
		case UI_CONTROL_BARGRAPH:
		{
			const float speed=10.0f;
			control->barGraph.curValue+=(control->barGraph.value-control->barGraph.curValue)*(1-exp(-speed*dt));
			float normalize_value=(control->barGraph.curValue-control->barGraph.min)/(control->barGraph.max-control->barGraph.min);

			(*instance)->positionSize.x=offset.x+(control->position.x+control->barGraph.size.x*0.5f);
			(*instance)->positionSize.y=offset.y+(control->position.y+control->barGraph.size.y*0.5f);
			(*instance)->positionSize.z=control->barGraph.size.x;
			(*instance)->positionSize.w=control->barGraph.size.y;

			(*instance)->colorValue.x=control->color.x;
			(*instance)->colorValue.y=control->color.y;
			(*instance)->colorValue.z=control->color.z;
			(*instance)->colorValue.w=normalize_value;

			(*instance)->type=UI_CONTROL_BARGRAPH;
			(*instance)->flag=control->barGraph.direction;

			(*instance)++;
			(*instanceCount)++;

			return true;
		}

		case UI_CONTROL_BUTTON:
		{
			(*instance)->positionSize.x=offset.x+(control->position.x+control->button.size.x*0.5f);
			(*instance)->positionSize.y=offset.y+(control->position.y+control->button.size.y*0.5f);
			(*instance)->positionSize.z=control->button.size.x;
			(*instance)->positionSize.w=control->button.size.y;

			(*instance)->colorValue.x=control->color.x;
			(*instance)->colorValue.y=control->color.y;
			(*instance)->colorValue.z=control->color.z;
			(*instance)->colorValue.w=0.0f;

			(*instance)->type=UI_CONTROL_BUTTON;

			(*instance)++;
			(*instanceCount)++;

			return true;
		}

		case UI_CONTROL_CHECKBOX:
		{
			(*instance)->positionSize.x=offset.x+control->position.x;
			(*instance)->positionSize.y=offset.y+control->position.y;
			(*instance)->positionSize.z=control->checkBox.radius*2;
			(*instance)->positionSize.w=control->checkBox.radius*2;

			(*instance)->colorValue.x=control->color.x;
			(*instance)->colorValue.y=control->color.y;
			(*instance)->colorValue.z=control->color.z;

			if(control->checkBox.value)
				(*instance)->colorValue.w=1.0f;
			else
				(*instance)->colorValue.w=0.0f;

			(*instance)->type=UI_CONTROL_CHECKBOX;

			(*instance)++;
			(*instanceCount)++;

			return true;
		}

		case UI_CONTROL_CURSOR:
		{
			(*instance)->positionSize.x=offset.x+(control->position.x+control->cursor.radius);
			(*instance)->positionSize.y=offset.y+(control->position.y-control->cursor.radius);
			(*instance)->positionSize.z=control->cursor.radius*2;
			(*instance)->positionSize.w=control->cursor.radius*2;

			(*instance)->colorValue.x=control->color.x;
			(*instance)->colorValue.y=control->color.y;
			(*instance)->colorValue.z=control->color.z;
			(*instance)->colorValue.w=0.0f;

			(*instance)->type=UI_CONTROL_CURSOR;

			(*instance)++;
			(*instanceCount)++;

			return true;
		}

		case UI_CONTROL_EDITTEXT:
		{
			(*instance)->positionSize.x=offset.x+(control->position.x+control->editText.size.x*0.5f);
			(*instance)->positionSize.y=offset.y+(control->position.y+control->editText.size.y*0.5f);
			(*instance)->positionSize.z=control->editText.size.x;
			(*instance)->positionSize.w=control->editText.size.y;

			(*instance)->colorValue.x=control->color.x;
			(*instance)->colorValue.y=control->color.y;
			(*instance)->colorValue.z=control->color.z;
			(*instance)->colorValue.w=0.0f;

			(*instance)->type=UI_CONTROL_EDITTEXT;

			(*instance)++;
			(*instanceCount)++;

			return true;
		}

		case UI_CONTROL_VIRTUALSTICK:
		{
			if(!control->virtualStick.active)
			{
				const float lambda=8.0f;
				const float decay=expf(-lambda*dt);

				control->virtualStick.value=Vec2_Muls(control->virtualStick.value, decay);
			}

			(*instance)->positionSize.x=offset.x+control->position.x;
			(*instance)->positionSize.y=offset.y+control->position.y;
			(*instance)->positionSize.z=control->virtualStick.radius*2;
			(*instance)->positionSize.w=control->virtualStick.radius*2;

			(*instance)->colorValue.x=control->color.x;
			(*instance)->colorValue.y=control->color.y;
			(*instance)->colorValue.z=control->color.z;
			(*instance)->colorValue.w=control->virtualStick.active?1.0f:0.0f;

			(*instance)->type=UI_CONTROL_VIRTUALSTICK;

			// Clamp to +/- 0.75 only for visual appearence
			(*instance)->extra=Vec2_Clamp(control->virtualStick.value, -0.75f, 0.75f);

			(*instance)++;
			(*instanceCount)++;

			return true;
		}

		case UI_CONTROL_WINDOW:
		{
			(*instance)->positionSize.x=offset.x+((control->position.x-(UI_CONTROL_WINDOW_BORDER*0.5f))+(control->window.size.x+UI_CONTROL_WINDOW_BORDER)*0.5f);
			(*instance)->positionSize.y=offset.y+((control->position.y-((UI_CONTROL_WINDOW_BORDER-UI_CONTROL_WINDOW_TITLEBAR_HEIGHT)*0.5f))-(control->window.size.y-UI_CONTROL_WINDOW_BORDER)*0.5f);
			(*instance)->positionSize.z=control->window.size.x+(UI_CONTROL_WINDOW_BORDER*2.0f);
			(*instance)->positionSize.w=control->window.size.y+(UI_CONTROL_WINDOW_BORDER*2.0f)+UI_CONTROL_WINDOW_TITLEBAR_HEIGHT;

			(*instance)->colorValue.x=control->color.x;
			(*instance)->colorValue.y=control->color.y;
			(*instance)->colorValue.z=control->color.z;
			(*instance)->colorValue.w=0.0f;

			(*instance)->type=UI_CONTROL_WINDOW;

			(*instance)++;
			(*instanceCount)++;

			return true;
		}

		case UI_CONTROL_TEXT:
		{
			if(control->text.titleText==NULL)
				return false;

			const float sx=offset.x+control->position.x;
			float x=sx;
			float y=offset.y+control->position.y;
			vec3 color=control->color;

			// Loop through the text string until EOL
			for(char *ptr=control->text.titleText;*ptr!='\0';ptr++)
			{
				// Decrement 'y' for any CR's
				if(*ptr=='\n')
				{
					x=sx;
					y-=control->text.size;
					continue;
				}

				// Just advance spaces instead of rendering empty quads
				if(*ptr==' ')
				{
					x+=Font_CharacterBaseWidth(*ptr)*control->text.size;
					continue;
				}

				// ANSI color escape codes
				if(*ptr=='\x1B')
				{
					ptr++;
						 if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='0'&&*(ptr+3)=='m')	{ color.x=0.0f; color.y=0.0f; color.z=0.0f; ptr+=4; } // BLACK
					else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='1'&&*(ptr+3)=='m')	{ color.x=0.5f; color.y=0.0f; color.z=0.0f; ptr+=4; } // DARK RED
					else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='2'&&*(ptr+3)=='m')	{ color.x=0.0f; color.y=0.5f; color.z=0.0f; ptr+=4; } // DARK GREEN
					else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='3'&&*(ptr+3)=='m')	{ color.x=0.5f; color.y=0.5f; color.z=0.0f; ptr+=4; } // DARK YELLOW
					else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='4'&&*(ptr+3)=='m')	{ color.x=0.0f; color.y=0.0f; color.z=0.5f; ptr+=4; } // DARK BLUE
					else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='5'&&*(ptr+3)=='m')	{ color.x=0.5f; color.y=0.0f; color.z=0.5f; ptr+=4; } // DARK MAGENTA
					else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='6'&&*(ptr+3)=='m')	{ color.x=0.0f; color.y=0.5f; color.z=0.5f; ptr+=4; } // DARK CYAN
					else if(*(ptr+0)=='['&&*(ptr+1)=='3'&&*(ptr+2)=='7'&&*(ptr+3)=='m')	{ color.x=0.5f; color.y=0.5f; color.z=0.5f; ptr+=4; } // GREY
					else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='0'&&*(ptr+3)=='m')	{ color.x=0.5f; color.y=0.5f; color.z=0.5f; ptr+=4; } // GREY
					else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='1'&&*(ptr+3)=='m')	{ color.x=1.0f; color.y=0.0f; color.z=0.0f; ptr+=4; } // RED
					else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='2'&&*(ptr+3)=='m')	{ color.x=0.0f; color.y=1.0f; color.z=0.0f; ptr+=4; } // GREEN
					else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='3'&&*(ptr+3)=='m')	{ color.x=1.0f; color.y=1.0f; color.z=0.0f; ptr+=4; } // YELLOW
					else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='4'&&*(ptr+3)=='m')	{ color.x=0.0f; color.y=0.0f; color.z=1.0f; ptr+=4; } // BLUE
					else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='5'&&*(ptr+3)=='m')	{ color.x=1.0f; color.y=0.0f; color.z=1.0f; ptr+=4; } // MAGENTA
					else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='6'&&*(ptr+3)=='m')	{ color.x=0.0f; color.y=1.0f; color.z=1.0f; ptr+=4; } // CYAN
					else if(*(ptr+0)=='['&&*(ptr+1)=='9'&&*(ptr+2)=='7'&&*(ptr+3)=='m')	{ color.x=1.0f; color.y=1.0f; color.z=1.0f; ptr+=4; } // WHITE
					else if(*(ptr+0)=='['&&*(ptr+1)=='0'&&*(ptr+2)=='m')				{ color=control->color; ptr+=3; }					  // CANCEL COLOR
				}

				// Advance one character
				x+=Font_CharacterBaseWidth(*ptr)*control->text.size;

				(*instance)->positionSize.x=x-((Font_CharacterBaseWidth(*ptr)*0.5f)*control->text.size); // TODO: SDF render is centered X/Y, this offsets by half
				(*instance)->positionSize.y=y;
				(*instance)->positionSize.z=(float)(*ptr);
				(*instance)->positionSize.w=control->text.size;

				(*instance)->colorValue=Vec4_Vec3(color, 0.0f);

				(*instance)->type=UI_CONTROL_TEXT;

				(*instance)++;
				(*instanceCount)++;
			}

			return true;
		}

		case UI_CONTROL_SPRITE:
		default:
			return false;
	}	
}

bool UI_Draw(UI_t *UI, VkCommandBuffer commandBuffer, VkDescriptorPool descriptorPool, matrix mvp, float dt)
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

		// Only add controls that are non-child and visible controls
		if(control->childParentID!=UINT32_MAX||control->visibility)
			continue;

		UI_AddControlInstance(&instance, &instanceCount, control, Vec2b(0.0f), dt);

		// If it was a window control, add children control instances
		if(control->type==UI_CONTROL_WINDOW)
		{
			for(uint32_t j=0;j<List_GetCount(&control->window.children);j++)
			{
				uint32_t *childID=List_GetPointer(&control->window.children, j);
				UI_Control_t *child=UI_FindControlByID(UI, *childID);

				// Add child control instances with offset of parent control, but only if visible
				if(!child->visibility)
					UI_AddControlInstance(&instance, &instanceCount, child, control->position, dt);
			}
		}
	}

	// Flush instance buffer caches, mostly needed for Android and maybe some iGPUs
	vkFlushMappedMemoryRanges(vkContext.device, 1, &(VkMappedMemoryRange)
	{
		VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
		VK_NULL_HANDLE,
		UI->instanceBuffer.memory->deviceMemory,
		0, VK_WHOLE_SIZE
	});

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI->pipeline.pipeline.pipeline);

	// Bind vertex data buffer
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &UI->vertexBuffer.buffer, &(VkDeviceSize) { 0 });
	// Bind object instance buffer
	vkCmdBindVertexBuffers(commandBuffer, 1, 1, &UI->instanceBuffer.buffer, &(VkDeviceSize) { 0 });

	struct
	{
		vec2 viewport;
		vec2 pad;
		matrix mvp;
	} UIPC;

	UIPC.viewport=UI->size;
	UIPC.mvp=mvp;

	vkCmdPushConstants(commandBuffer, UI->pipeline.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT|VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(UIPC), &UIPC);

	// Draw sprites, they need descriptor set changes and aren't easy to draw instanced...
	uint32_t spriteCount=instanceCount;
	for(uint32_t i=0;i<controlCount;i++)
	{
		UI_Control_t *control=(UI_Control_t *)List_GetPointer(&UI->controls, i);

		if(control->type==UI_CONTROL_SPRITE&&!control->visibility)
		{
			// TODO: This may be a problem on integrated or mobile platforms, flush needed?
			instance->positionSize.x=control->position.x;
			instance->positionSize.y=control->position.y;
			instance->positionSize.z=control->sprite.size.x;
			instance->positionSize.w=control->sprite.size.y;

			instance->colorValue.x=control->color.x;
			instance->colorValue.y=control->color.y;
			instance->colorValue.z=control->color.z;
			instance->colorValue.w=control->sprite.rotation;

			instance->type=UI_CONTROL_SPRITE;

			instance->flag=control->sprite.frame;

			instance->extra=control->sprite.cropSize;

			instance++;

			vkuDescriptorSet_UpdateBindingImageInfo(&UI->pipeline.descriptorSet, 0, control->sprite.image->sampler, control->sprite.image->imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			vkuAllocateUpdateDescriptorSet(&UI->pipeline.descriptorSet, descriptorPool);
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI->pipeline.pipelineLayout, 0, 1, &UI->pipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

			// Use the last unused instanced data slot for drawing
			vkCmdDraw(commandBuffer, 4, 1, 0, spriteCount++);
		}
	}

	vkuDescriptorSet_UpdateBindingImageInfo(&UI->pipeline.descriptorSet, 0, UI->blankImage.sampler, UI->blankImage.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	vkuAllocateUpdateDescriptorSet(&UI->pipeline.descriptorSet, descriptorPool);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, UI->pipeline.pipelineLayout, 0, 1, &UI->pipeline.descriptorSet.descriptorSet, 0, VK_NULL_HANDLE);

	// Draw instanced UI elements
	vkCmdDraw(commandBuffer, 4, instanceCount, 0, 0);

	return true;
}
