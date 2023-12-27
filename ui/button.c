#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

// Add a button to the UI.
// Returns an ID, or UINT32_MAX on failure.
uint32_t UI_AddButton(UI_t *UI, vec2 position, vec2 size, vec3 color, const char *titleText, UIControlCallback callback)
{
	uint32_t ID=UI->baseID++;

	if(ID==UINT32_MAX||ID>=UI_HASHTABLE_MAX)
		return UINT32_MAX;

	UI_Control_t Control=
	{
		.type=UI_CONTROL_BUTTON,
		.ID=ID,
		.position=position,
		.color=color,
		.button.size=size,
		.button.callback=callback
	};

	snprintf(Control.button.titleText, UI_CONTROL_TITLETEXT_MAX, "%s", titleText);

	if(!List_Add(&UI->controls, &Control))
		return UINT32_MAX;

	UI->controlsHashtable[ID]=List_GetPointer(&UI->controls, List_GetCount(&UI->controls)-1);

	return ID;
}

// Update UI button parameters.
// Returns true on success, false on failure.
// Also individual parameter update function as well.
bool UI_UpdateButton(UI_t *UI, uint32_t ID, vec2 position, vec2 size, vec3 color, const char *titleText, UIControlCallback callback)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BUTTON)
	{
		Control->position=position;
		Control->color=color;

		snprintf(Control->button.titleText, UI_CONTROL_TITLETEXT_MAX, "%s", titleText);
		Control->button.size=size;
		Control->button.callback=callback;

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateButtonPosition(UI_t *UI, uint32_t ID, vec2 position)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BUTTON)
	{
		Control->position=position;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateButtonSize(UI_t *UI, uint32_t ID, vec2 size)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BUTTON)
	{
		Control->button.size=size;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateButtonColor(UI_t *UI, uint32_t ID, vec3 color)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BUTTON)
	{
		Control->color=color;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateButtonTitleText(UI_t *UI, uint32_t ID, const char *titleText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BUTTON)
	{
		snprintf(Control->button.titleText, UI_CONTROL_TITLETEXT_MAX, "%s", titleText);
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateButtonCallback(UI_t *UI, uint32_t ID, UIControlCallback callback)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BUTTON)
	{
		Control->button.callback=callback;
		return true;
	}

	// Not found
	return false;
}
