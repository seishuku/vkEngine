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

	UI_Control_t control=
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

	if(!List_Add(&UI->controls, &control))
		return UINT32_MAX;

	UI->controlsHashtable[ID]=(UI_Control_t *)List_GetPointer(&UI->controls, List_GetCount(&UI->controls)-1);

	// TODO:
	// This is bit annoying...
	// The control's title text needs to be added after the actual control, otherwise it will be rendered under this control.
	// I suppose this would be fixed with proper render order sorting, maybe later.

	// Text size is the radius of the checkbox, placed radius length away horizontally, centered vertically
	UI->controlsHashtable[ID]->checkBox.titleTextID=UI_AddText(UI,
		Vec2(position.x+radius, position.y), radius,
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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_CHECKBOX)
	{
		control->position=position;
		control->color=color;
		control->hidden=hidden;

		UI_UpdateText(UI, control->checkBox.titleTextID,
			Vec2(position.x+radius, position.y), radius,
			Vec3(1.0f, 1.0f, 1.0f),
			hidden,
			titleText);
		control->checkBox.radius=radius;
		control->checkBox.value=value;

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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_CHECKBOX)
	{
		control->position=position;
		UI_UpdateTextPosition(UI, control->checkBox.titleTextID,
			Vec2(control->position.x+control->checkBox.radius, control->position.y));
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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_CHECKBOX)
	{
		control->checkBox.radius=radius;
		UI_UpdateTextPosition(UI, control->checkBox.titleTextID,
			Vec2(control->position.x+control->checkBox.radius, control->position.y));
		UI_UpdateTextSize(UI, control->checkBox.titleTextID, control->checkBox.radius);
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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_CHECKBOX)
	{
		control->color=color;
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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_CHECKBOX)
	{
		control->hidden=hidden;
		UI_UpdateTextVisibility(UI, control->checkBox.titleTextID, hidden);

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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_CHECKBOX)
	{
		UI_UpdateTextTitleText(UI, control->checkBox.titleTextID, titleText);
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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_CHECKBOX)
	{
		control->checkBox.value=value;
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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_CHECKBOX)
	{
		return control->checkBox.value;
	}

	// Not found
	return false;
}
