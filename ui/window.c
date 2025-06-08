#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

uint32_t UI_AddWindow(UI_t *UI, vec2 position, vec2 size, vec3 color, const char *titleText)
{
	uint32_t ID=UI->baseID++;

	if(ID==UINT32_MAX||ID>=UI_HASHTABLE_MAX)
		return UINT32_MAX;

	UI_Control_t Control=
	{
		.type=UI_CONTROL_WINDOW,
		.ID=ID,
		.position=position,
		.color=color,
		.child=false,
		.window.size=size,
	};

	if(!List_Init(&Control.window.children, sizeof(UI_Control_t), 0, NULL))
		return UINT32_MAX;

	if(!List_Add(&UI->controls, &Control))
		return UINT32_MAX;

	UI->controlsHashtable[ID]=List_GetPointer(&UI->controls, List_GetCount(&UI->controls)-1);

	// TODO:
	// This is bit annoying...
	// The control's title text needs to be added after the actual control, otherwise it will be rendered under this control.
	// I suppose this would be fixed with proper render order sorting, maybe later.
	UI->controlsHashtable[ID]->window.titleTextID=UI_AddText(UI, position, 1.0f, color, titleText);

	return ID;
}

bool UI_UpdateWindow(UI_t *UI, uint32_t ID, vec2 position, vec2 size, vec3 color, const char *titleText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_WINDOW)
	{
		Control->position=position;
		Control->color=color;

		UI_UpdateTextTitleText(UI, Control->window.titleTextID, titleText);
		Control->window.size=size;

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateWindowPosition(UI_t *UI, uint32_t ID, vec2 position)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_WINDOW)
	{
		Control->position=position;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateWindowSize(UI_t *UI, uint32_t ID, vec2 size)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_WINDOW)
	{
		Control->window.size=size;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateWindowColor(UI_t *UI, uint32_t ID, vec3 color)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_WINDOW)
	{
		Control->color=color;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateWindowTitleText(UI_t *UI, uint32_t ID, const char *titleText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_WINDOW)
	{
		UI_UpdateTextTitleText(UI, Control->window.titleTextID, titleText);
		return true;
	}

	// Not found
	return false;
}

bool UI_WindowAddControl(UI_t *UI, uint32_t ID, uint32_t childID)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_WINDOW)
	{
		UI_Control_t *childControl=UI_FindControlByID(UI, childID);

		if(childControl!=NULL&&childControl->type!=UI_CONTROL_WINDOW)
		{
			childControl->child=true;
			if(childControl->type==UI_CONTROL_BUTTON||childControl->type==UI_CONTROL_BARGRAPH||childControl->type==UI_CONTROL_CHECKBOX)
				UI->controlsHashtable[childControl->button.titleTextID]->child=true;

			if(List_Add(&Control->window.children, childControl))
				return true;
		}
		else
			return false;
	}

	// Not found
	return false;
}
