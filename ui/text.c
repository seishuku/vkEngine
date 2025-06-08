#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

uint32_t UI_AddText(UI_t *UI, vec2 position, float size, vec3 color, const char *titleText)
{
	uint32_t ID=UI->baseID++;

	if(ID==UINT32_MAX||ID>=UI_HASHTABLE_MAX)
		return UINT32_MAX;

	UI_Control_t Control=
	{
		.type=UI_CONTROL_TEXT,
		.ID=ID,
		.position=position,
		.color=color,
		.child=false,
		.text.size=size,
	};

	strncpy(Control.text.titleText, titleText, UI_CONTROL_TITLETEXT_MAX);

	if(!List_Add(&UI->controls, &Control))
		return UINT32_MAX;

	UI->controlsHashtable[ID]=List_GetPointer(&UI->controls, List_GetCount(&UI->controls)-1);

	return ID;
}

bool UI_UpdateText(UI_t *UI, uint32_t ID, vec2 position, float size, vec3 color, const char *titleText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_TEXT)
	{
		Control->position=position;
		Control->color=color;

		strncpy(Control->text.titleText, titleText, UI_CONTROL_TITLETEXT_MAX);
		Control->text.size=size;

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateTextPosition(UI_t *UI, uint32_t ID, vec2 position)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_TEXT)
	{
		Control->position=position;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateTextSize(UI_t *UI, uint32_t ID, float size)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_TEXT)
	{
		Control->text.size=size;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateTextColor(UI_t *UI, uint32_t ID, vec3 color)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_TEXT)
	{
		Control->color=color;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateTextTitleText(UI_t *UI, uint32_t ID, const char *titleText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_TEXT)
	{
		strncpy(Control->text.titleText, titleText, UI_CONTROL_TITLETEXT_MAX);
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateTextTitleTextf(UI_t *UI, uint32_t ID, const char *titleText, ...)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_TEXT)
	{
		va_list	ap;
		va_start(ap, titleText);
		vsnprintf(Control->text.titleText, UI_CONTROL_TITLETEXT_MAX-1, titleText, ap);
		va_end(ap);

		return true;
	}

	// Not found
	return false;
}
