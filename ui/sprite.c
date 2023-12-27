#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

extern VkuContext_t vkContext;

// Add a button to the UI.
// Returns an ID, or UINT32_MAX on failure.
uint32_t UI_AddSprite(UI_t *UI, vec2 position, vec2 size, vec3 color, VkuImage_t *Image, float rotation)
{
	uint32_t ID=UI->baseID++;

	if(ID==UINT32_MAX||ID>=UI_HASHTABLE_MAX)
		return UINT32_MAX;

	UI_Control_t Control=
	{
		.type=UI_CONTROL_SPRITE,
		.ID=ID,
		.position=position,
		.color=color,
		.Sprite.Image=Image,
		.Sprite.size=size,
		.Sprite.rotation=rotation
	};

	if(!List_Add(&UI->controls, &Control))
		return UINT32_MAX;

	UI->controlsHashtable[ID]=List_GetPointer(&UI->controls, List_GetCount(&UI->controls)-1);

	return ID;
}

// Update UI button parameters.
// Returns true on success, false on failure.
// Also individual parameter update function as well.
bool UI_UpdateSprite(UI_t *UI, uint32_t ID, vec2 position, vec2 size, vec3 color, VkuImage_t *Image, float rotation)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_SPRITE)
	{
		Control->position=position;
		Control->color=color;

		Control->Sprite.Image=Image,
		Control->Sprite.rotation=rotation;
		Control->Sprite.size=size;

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateSpritePosition(UI_t *UI, uint32_t ID, vec2 position)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_SPRITE)
	{
		Control->position=position;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateSpriteSize(UI_t *UI, uint32_t ID, vec2 size)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_SPRITE)
	{
		Control->Button.size=size;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateSpriteColor(UI_t *UI, uint32_t ID, vec3 color)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_SPRITE)
	{
		Control->color=color;
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

	if(Control!=NULL&&Control->type==UI_CONTROL_SPRITE)
	{
		Control->Sprite.Image=Image;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateSpriteRotation(UI_t *UI, uint32_t ID, float rotation)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_SPRITE)
	{
		Control->Sprite.rotation=rotation;
		return true;
	}

	// Not found
	return false;
}
