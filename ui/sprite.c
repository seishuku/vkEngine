#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../vulkan/vulkan.h"
#include "../utils/genid.h"
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

extern VkuContext_t Context;

// Add a button to the UI.
// Returns an ID, or UINT32_MAX on failure.
uint32_t UI_AddSprite(UI_t *UI, vec2 Position, vec2 Size, vec3 Color, VkuImage_t *Image, float Rotation)
{
	uint32_t ID=GenID();
	static uint32_t SpriteDescriptorSetCount=0;

	if(ID==UINT32_MAX)
		return UINT32_MAX;

	UI_Control_t Control=
	{
		.Type=UI_CONTROL_SPRITE,
		.ID=ID,
		.Position=Position,
		.Color=Color,
		.Sprite.DescriptorSetOffset=SpriteDescriptorSetCount++,
		.Sprite.Image=Image,
		.Sprite.Size=Size,
		.Sprite.Rotation=Rotation
	};

	if(!List_Add(&UI->Controls, &Control))
		return UINT32_MAX;

	VkDescriptorSet DescriptorSet=VK_NULL_HANDLE;
	VkDescriptorSetAllocateInfo AllocateInfo=
	{
		.sType=VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool=UI->DescriptorPool,
		.descriptorSetCount=1,
		.pSetLayouts=&UI->DescriptorSetLayout,
	};

	vkAllocateDescriptorSets(Context.Device, &AllocateInfo, &DescriptorSet);

	// Need to update destination set handle now that one has been allocated.
	VkWriteDescriptorSet WriteDescriptorSet=
	{
		.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet=DescriptorSet,
		.dstBinding=0,
		.descriptorCount=1,
		.descriptorType=VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		.pImageInfo=&(VkDescriptorImageInfo) { Image->Sampler, Image->View, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL },
	};

	vkUpdateDescriptorSets(Context.Device, 1, &WriteDescriptorSet, 0, VK_NULL_HANDLE);

	List_Add(&UI->DescriptorSets, &DescriptorSet);

	return ID;
}

// Update UI button parameters.
// Returns true on success, false on failure.
// Also individual parameter update function as well.
bool UI_UpdateSprite(UI_t *UI, uint32_t ID, vec2 Position, vec2 Size, vec3 Color, VkuImage_t *Image, float Rotation)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_SPRITE)
	{
		Control->Position=Position;
		Control->Color=Color;

		Control->Sprite.Image=Image,
		Control->Sprite.Rotation=Rotation;
		Control->Sprite.Size=Size;

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateSpritePosition(UI_t *UI, uint32_t ID, vec2 Position)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_SPRITE)
	{
		Control->Position=Position;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateSpriteSize(UI_t *UI, uint32_t ID, vec2 Size)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_SPRITE)
	{
		Control->Button.Size=Size;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateSpriteColor(UI_t *UI, uint32_t ID, vec3 Color)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_SPRITE)
	{
		Control->Color=Color;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateSpriteImage(UI_t *UI, uint32_t ID, VkuImage_t *Image)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_SPRITE)
	{
		Control->Sprite.Image=Image;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateSpriteRotation(UI_t *UI, uint32_t ID, float Rotation)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_SPRITE)
	{
		Control->Sprite.Rotation=Rotation;
		return true;
	}

	// Not found
	return false;
}
