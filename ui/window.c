#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

uint32_t UI_AddWindow(UI_t *UI, vec2 position, vec2 size, vec3 color, bool hidden, const char *titleText)
{
	uint32_t ID=UI->baseID++;

	if(ID==UINT32_MAX||ID>=UI_HASHTABLE_MAX)
		return UINT32_MAX;

	UI_Control_t control=
	{
		.type=UI_CONTROL_WINDOW,
		.ID=ID,
		.position=position,
		.color=color,
		.childParentID=UINT32_MAX,
		.hidden=hidden,
		.window.size=size,
		.window.hitOffset=Vec2b(0.0f),
	};

	if(!List_Init(&control.window.children, sizeof(uint32_t), 0, NULL))
		return UINT32_MAX;

	if(!UI_AddControl(UI, &control))
		return UINT32_MAX;

	// TODO:
	// This is bit annoying...
	// The control's title text needs to be added after the actual control, otherwise it will be rendered under this control.
	// I suppose this would be fixed with proper render order sorting, maybe later.
	UI->controlsHashtable[ID]->window.titleTextID=UI_AddText(UI, Vec2_Add(position, 0.0f, 16.0f-(UI_CONTROL_WINDOW_BORDER*0.5f)), 16.0f, Vec3b(1.0f), hidden, titleText);

	return ID;
}

bool UI_UpdateWindow(UI_t *UI, uint32_t ID, vec2 position, vec2 size, vec3 color, bool hidden, const char *titleText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_WINDOW)
	{
		control->position=position;
		control->color=color;
		control->hidden=hidden;

		UI_UpdateTextTitleText(UI, control->window.titleTextID, titleText);
		UI_UpdateTextVisibility(UI, control->window.titleTextID, hidden);
		control->window.size=size;

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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_WINDOW)
	{
		UI_UpdateTextPosition(UI, control->window.titleTextID, Vec2_Add(position, 0.0f, 16.0f-(UI_CONTROL_WINDOW_BORDER*0.5f)));
		control->position=position;
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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_WINDOW)
	{
		control->window.size=size;
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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_WINDOW)
	{
		control->color=color;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateWindowVisibility(UI_t *UI, uint32_t ID, bool hidden)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_WINDOW)
	{
		control->hidden=hidden;
		UI_UpdateTextVisibility(UI, control->window.titleTextID, hidden);

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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_WINDOW)
	{
		UI_UpdateTextTitleText(UI, control->window.titleTextID, titleText);
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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_WINDOW)
	{
		UI_Control_t *childControl=UI_FindControlByID(UI, childID);

		if(childControl!=NULL&&childControl->type!=UI_CONTROL_WINDOW)
		{
			childControl->childParentID=ID;

			if(childControl->type==UI_CONTROL_BUTTON||childControl->type==UI_CONTROL_BARGRAPH||childControl->type==UI_CONTROL_CHECKBOX)
				UI->controlsHashtable[childControl->button.titleTextID]->childParentID=childID;

			if(List_Add(&control->window.children, &childID))
			{
				if(childControl->type==UI_CONTROL_BUTTON||childControl->type==UI_CONTROL_BARGRAPH||childControl->type==UI_CONTROL_CHECKBOX)
				{
					if(List_Add(&control->window.children, &childControl->button.titleTextID))
						return true;
				}
				else
					return true;
			}
		}
		else
			return false;
	}

	// Not found
	return false;
}
