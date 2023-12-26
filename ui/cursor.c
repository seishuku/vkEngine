#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

// Add a cursor to the UI.
// Returns an ID, or UINT32_MAX on failure.
uint32_t UI_AddCursor(UI_t *UI, vec2 Position, float Radius, vec3 Color)
{
	uint32_t ID=UI->IDBase++;

	if(ID==UINT32_MAX||ID>=UI_HASHTABLE_MAX)
		return UINT32_MAX;

	UI_Control_t Control=
	{
		.Type=UI_CONTROL_CURSOR,
		.ID=ID,
		.Position=Position,
		.Color=Color,
		.Cursor.Radius=Radius,
	};

	if(!List_Add(&UI->Controls, &Control))
		return UINT32_MAX;

	UI->Controls_Hashtable[ID]=List_GetPointer(&UI->Controls, List_GetCount(&UI->Controls)-1);

	return ID;
}

// Update UI cursor parameters.
// Returns true on success, false on failure.
// Also individual parameter update functions.
bool UI_UpdateCursor(UI_t *UI, uint32_t ID, vec2 Position, float Radius, vec3 Color)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_CURSOR)
	{
		Control->Position=Position;
		Control->Color=Color;

		Control->Cursor.Radius=Radius;

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateCursorPosition(UI_t *UI, uint32_t ID, vec2 Position)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_CURSOR)
	{
		Control->Position=Position;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateCursorRadius(UI_t *UI, uint32_t ID, float Radius)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_CURSOR)
	{
		Control->Cursor.Radius=Radius;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateCursorColor(UI_t *UI, uint32_t ID, vec3 Color)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_CURSOR)
	{
		Control->Color=Color;
		return true;
	}

	// Not found
	return false;
}
