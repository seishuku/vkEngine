#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../utils/genid.h"
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

// Add a button to the UI.
// Returns an ID, or UINT32_MAX on failure.
uint32_t UI_AddButton(UI_t *UI, vec2 Position, vec2 Size, vec3 Color, const char *TitleText, UIControlCallback Callback)
{
	uint32_t ID=UI->IDBase++;

	if(ID==UINT32_MAX||ID>=UI_HASHTABLE_MAX)
		return UINT32_MAX;

	UI_Control_t Control=
	{
		.Type=UI_CONTROL_BUTTON,
		.ID=ID,
		.Position=Position,
		.Color=Color,
		.Button.Size=Size,
		.Button.Callback=Callback
	};

	snprintf(Control.Button.TitleText, UI_CONTROL_TITLETEXT_MAX, "%s", TitleText);

	if(!List_Add(&UI->Controls, &Control))
		return UINT32_MAX;

	UI->Controls_Hashtable[ID]=List_GetPointer(&UI->Controls, List_GetCount(&UI->Controls)-1);

	return ID;
}

// Update UI button parameters.
// Returns true on success, false on failure.
// Also individual parameter update function as well.
bool UI_UpdateButton(UI_t *UI, uint32_t ID, vec2 Position, vec2 Size, vec3 Color, const char *TitleText, UIControlCallback Callback)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_BUTTON)
	{
		Control->Position=Position;
		Control->Color=Color;

		snprintf(Control->Button.TitleText, UI_CONTROL_TITLETEXT_MAX, "%s", TitleText);
		Control->Button.Size=Size;
		Control->Button.Callback=Callback;

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateButtonPosition(UI_t *UI, uint32_t ID, vec2 Position)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_BUTTON)
	{
		Control->Position=Position;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateButtonSize(UI_t *UI, uint32_t ID, vec2 Size)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_BUTTON)
	{
		Control->Button.Size=Size;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateButtonColor(UI_t *UI, uint32_t ID, vec3 Color)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_BUTTON)
	{
		Control->Color=Color;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateButtonTitleText(UI_t *UI, uint32_t ID, const char *TitleText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_BUTTON)
	{
		snprintf(Control->Button.TitleText, UI_CONTROL_TITLETEXT_MAX, "%s", TitleText);
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateButtonCallback(UI_t *UI, uint32_t ID, UIControlCallback Callback)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_BUTTON)
	{
		Control->Button.Callback=Callback;
		return true;
	}

	// Not found
	return false;
}
