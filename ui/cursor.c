#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

// Add a cursor to the UI.
// Returns an ID, or UINT32_MAX on failure.
uint32_t UI_AddCursor(UI_t *UI, vec2 position, float radius, vec3 color)
{
	uint32_t ID=UI->baseID++;

	if(ID==UINT32_MAX||ID>=UI_HASHTABLE_MAX)
		return UINT32_MAX;

	UI_Control_t Control=
	{
		.type=UI_CONTROL_CURSOR,
		.ID=ID,
		.position=position,
		.color=color,
		.cursor.radius=radius,
	};

	if(!List_Add(&UI->controls, &Control))
		return UINT32_MAX;

	UI->controlsHashtable[ID]=List_GetPointer(&UI->controls, List_GetCount(&UI->controls)-1);

	return ID;
}

// Update UI cursor parameters.
// Returns true on success, false on failure.
// Also individual parameter update functions.
bool UI_UpdateCursor(UI_t *UI, uint32_t ID, vec2 position, float radius, vec3 color)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_CURSOR)
	{
		Control->position=position;
		Control->color=color;

		Control->cursor.radius=radius;

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateCursorPosition(UI_t *UI, uint32_t ID, vec2 position)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_CURSOR)
	{
		Control->position=position;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateCursorRadius(UI_t *UI, uint32_t ID, float radius)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_CURSOR)
	{
		Control->cursor.radius=radius;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateCursorColor(UI_t *UI, uint32_t ID, vec3 color)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_CURSOR)
	{
		Control->color=color;
		return true;
	}

	// Not found
	return false;
}
