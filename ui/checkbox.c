#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

// Add a checkbox to the UI.
// Returns an ID, or UINT32_MAX on failure.
uint32_t UI_AddCheckBox(UI_t *UI, vec2 position, float radius, vec3 color, bool hidden, const char *titleText, bool value)
{
	uint32_t ID=UI->baseID++;

	if(ID==UINT32_MAX||ID>=UI_HASHTABLE_MAX)
		return UINT32_MAX;

	UI_Control_t Control=
	{
		.type=UI_CONTROL_CHECKBOX,
		.ID=ID,
		.position=position,
		.color=color,
		.childParentID=UINT32_MAX,
		.hidden=hidden,
		.checkBox.radius=radius,
		.checkBox.value=value
	};

	if(!List_Add(&UI->controls, &Control))
		return UINT32_MAX;

	UI->controlsHashtable[ID]=List_GetPointer(&UI->controls, List_GetCount(&UI->controls)-1);

	// TODO:
	// This is bit annoying...
	// The control's title text needs to be added after the actual control, otherwise it will be rendered under this control.
	// I suppose this would be fixed with proper render order sorting, maybe later.

	// Text size is the radius of the checkbox, placed radius length away horizontally, centered vertically
	UI->controlsHashtable[ID]->checkBox.titleTextID=UI_AddText(UI,
		Vec2(position.x+radius, position.y-(radius/2.0f)), radius,
		Vec3(1.0f, 1.0f, 1.0f),
		hidden,
		titleText);

	return ID;
}

// Update UI checkbox parameters.
// Returns true on success, false on failure.
// Also individual parameter update functions.
bool UI_UpdateCheckBox(UI_t *UI, uint32_t ID, vec2 position, float radius, vec3 color, bool hidden, const char *titleText, bool value)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_CHECKBOX)
	{
		Control->position=position;
		Control->color=color;
		Control->hidden=hidden;

		UI_UpdateText(UI, Control->checkBox.titleTextID,
			Vec2(position.x+radius, position.y-(radius/2.0f)), radius,
			Vec3(1.0f, 1.0f, 1.0f),
			hidden,
			titleText);
		Control->checkBox.radius=radius;
		Control->checkBox.value=value;

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateCheckBoxPosition(UI_t *UI, uint32_t ID, vec2 position)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_CHECKBOX)
	{
		Control->position=position;
		UI_UpdateTextPosition(UI, Control->checkBox.titleTextID,
			Vec2(Control->position.x+Control->checkBox.radius, Control->position.y-(Control->checkBox.radius/2.0f)));
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateCheckBoxRadius(UI_t *UI, uint32_t ID, float radius)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_CHECKBOX)
	{
		Control->checkBox.radius=radius;
		UI_UpdateTextPosition(UI, Control->checkBox.titleTextID,
			Vec2(Control->position.x+Control->checkBox.radius, Control->position.y-(Control->checkBox.radius/2.0f)));
		UI_UpdateTextSize(UI, Control->checkBox.titleTextID, Control->checkBox.radius);
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateCheckBoxColor(UI_t *UI, uint32_t ID, vec3 color)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_CHECKBOX)
	{
		Control->color=color;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateCheckBoxVisibility(UI_t *UI, uint32_t ID, bool hidden)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_CHECKBOX)
	{
		Control->hidden=hidden;
		UI_UpdateTextVisibility(UI, Control->checkBox.titleTextID, hidden);

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateCheckBoxTitleText(UI_t *UI, uint32_t ID, const char *titleText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_CHECKBOX)
	{
		UI_UpdateTextTitleText(UI, Control->checkBox.titleTextID, titleText);
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateCheckBoxValue(UI_t *UI, uint32_t ID, bool value)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_CHECKBOX)
	{
		Control->checkBox.value=value;
		return true;
	}

	// Not found
	return false;
}

// Get the value of a checkbox by ID
// Returns false on error, should this return by pointer instead or just outright get a pointer to control's value variable?
bool UI_GetCheckBoxValue(UI_t *UI, uint32_t ID)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	for(uint32_t i=0;i<List_GetCount(&UI->controls);i++)
	{
		UI_Control_t *Control=(UI_Control_t *)List_GetPointer(&UI->controls, i);

		// Check for matching ID and type
		if(Control->ID==ID&&Control->type==UI_CONTROL_CHECKBOX)
			return Control->checkBox.value;
	}

	// Not found
	return false;
}
