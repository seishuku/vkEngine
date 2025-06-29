#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

extern VkuContext_t vkContext;

// Add a sprite to the UI.
// Returns an ID, or UINT32_MAX on failure.
uint32_t UI_AddSprite(UI_t *UI, vec2 position, vec2 size, vec3 color, bool hidden, VkuImage_t *image, float rotation)
{
	uint32_t ID=UI->baseID++;

	if(ID==UINT32_MAX||ID>=UI_HASHTABLE_MAX)
		return UINT32_MAX;

	UI_Control_t control=
	{
		.type=UI_CONTROL_SPRITE,
		.ID=ID,
		.position=position,
		.color=color,
		.childParentID=UINT32_MAX,
		.hidden=hidden,
		.sprite.image=image,
		.sprite.size=size,
		.sprite.rotation=rotation
	};

	if(!List_Add(&UI->controls, &control))
		return UINT32_MAX;

	UI->controlsHashtable[ID]=(UI_Control_t *)List_GetPointer(&UI->controls, List_GetCount(&UI->controls)-1);

	return ID;
}

// Update UI sprite parameters.
// Returns true on success, false on failure.
// Also individual parameter update function as well.
bool UI_UpdateSprite(UI_t *UI, uint32_t ID, vec2 position, vec2 size, vec3 color, bool hidden, VkuImage_t *image, float rotation)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_SPRITE)
	{
		control->position=position;
		control->color=color;
		control->hidden=hidden;

		control->sprite.image=image,
		control->sprite.rotation=rotation;
		control->sprite.size=size;

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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_SPRITE)
	{
		control->position=position;
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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_SPRITE)
	{
		control->sprite.size=size;
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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_SPRITE)
	{
		control->color=color;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateSpriteVisibility(UI_t *UI, uint32_t ID, bool hidden)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_SPRITE)
	{
		control->hidden=hidden;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateSpriteImage(UI_t *UI, uint32_t ID, VkuImage_t *image)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_SPRITE)
	{
		control->sprite.image=image;
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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_SPRITE)
	{
		control->sprite.rotation=rotation;
		return true;
	}

	// Not found
	return false;
}
