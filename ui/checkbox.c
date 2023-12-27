#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

// Add a checkbox to the UI.
// Returns an ID, or UINT32_MAX on failure.
uint32_t UI_AddCheckBox(UI_t *UI, vec2 position, float radius, vec3 color, const char *titleText, bool value)
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
		.checkBox.radius=radius,
		.checkBox.value=value
	};

	snprintf(Control.checkBox.titleText, UI_CONTROL_TITLETEXT_MAX, "%s", titleText);

	if(!List_Add(&UI->controls, &Control))
		return UINT32_MAX;

	UI->controlsHashtable[ID]=List_GetPointer(&UI->controls, List_GetCount(&UI->controls)-1);

	return ID;
}

// Update UI checkbox parameters.
// Returns true on success, false on failure.
// Also individual parameter update functions.
bool UI_UpdateCheckBox(UI_t *UI, uint32_t ID, vec2 position, float radius, vec3 color, const char *titleText, bool value)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_CHECKBOX)
	{
		Control->position=position;
		Control->color=color;

		snprintf(Control->checkBox.titleText, UI_CONTROL_TITLETEXT_MAX, "%s", titleText);
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

bool UI_UpdateCheckBoxTitleText(UI_t *UI, uint32_t ID, const char *titleText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_CHECKBOX)
	{
		snprintf(Control->checkBox.titleText, UI_CONTROL_TITLETEXT_MAX, "%s", titleText);
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
