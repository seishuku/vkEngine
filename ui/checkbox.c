#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../utils/genid.h"
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

// Add a checkbox to the UI.
// Returns an ID, or UINT32_MAX on failure.
uint32_t UI_AddCheckBox(UI_t *UI, vec2 Position, float Radius, vec3 Color, const char *TitleText, bool Value)
{
	uint32_t ID=GenID();

	if(ID==UINT32_MAX)
		return UINT32_MAX;

	UI_Control_t Control=
	{
		.Type=UI_CONTROL_CHECKBOX,
		.ID=ID,
		.Position=Position,
		.Color=Color,
		.CheckBox.Radius=Radius,
		.CheckBox.Value=Value
	};

	snprintf(Control.TitleText, UI_CONTROL_TITLETEXT_MAX, "%s", TitleText);

	if(!List_Add(&UI->Controls, &Control))
		return UINT32_MAX;

	return ID;
}

// Update UI checkbox parameters.
// Returns true on success, false on failure.
// Also individual parameter update functions.
bool UI_UpdateCheckBox(UI_t *UI, uint32_t ID, vec2 Position, float Radius, vec3 Color, const char *TitleText, bool Value)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_CHECKBOX)
	{
		Control->Position=Position;
		Control->Color=Color;
		snprintf(Control->TitleText, UI_CONTROL_TITLETEXT_MAX, "%s", TitleText);

		Control->CheckBox.Radius=Radius;
		Control->CheckBox.Value=Value;

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateCheckBoxPosition(UI_t *UI, uint32_t ID, vec2 Position)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_CHECKBOX)
	{
		Control->Position=Position;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateCheckBoxRadius(UI_t *UI, uint32_t ID, float Radius)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_CHECKBOX)
	{
		Control->CheckBox.Radius=Radius;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateCheckBoxColor(UI_t *UI, uint32_t ID, vec3 Color)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_CHECKBOX)
	{
		Control->Color=Color;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateCheckBoxTitleText(UI_t *UI, uint32_t ID, const char *TitleText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_CHECKBOX)
	{
		snprintf(Control->TitleText, UI_CONTROL_TITLETEXT_MAX, "%s", TitleText);
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateCheckBoxValue(UI_t *UI, uint32_t ID, bool Value)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_CHECKBOX)
	{
		Control->CheckBox.Value=Value;
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
	for(uint32_t i=0;i<List_GetCount(&UI->Controls);i++)
	{
		UI_Control_t *Control=(UI_Control_t *)List_GetPointer(&UI->Controls, i);

		// Check for matching ID and type
		if(Control->ID==ID&&Control->Type==UI_CONTROL_CHECKBOX)
			return Control->CheckBox.Value;
	}

	// Not found
	return false;
}
