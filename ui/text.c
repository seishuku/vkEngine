#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include "../system/system.h"
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

uint32_t UI_AddText(UI_t *UI, vec2 position, float size, vec3 color, bool hidden, const char *titleText)
{
	if(UI==NULL||titleText==NULL)
		return false;

	uint32_t ID=UI->baseID++;

	if(ID==UINT32_MAX||ID>=UI_HASHTABLE_MAX)
		return UINT32_MAX;

	UI_Control_t control=
	{
		.type=UI_CONTROL_TEXT,
		.ID=ID,
		.position=position,
		.color=color,
		.childParentID=UINT32_MAX,
		.hidden=hidden,
		.text.size=size,
	};

	control.text.titleTextLength=strlen(titleText)+1;
	control.text.titleText=(char *)Zone_Malloc(zone, control.text.titleTextLength);

	if(control.text.titleText==NULL)
		return false;

	memcpy(control.text.titleText, titleText, control.text.titleTextLength);

	if(!List_Add(&UI->controls, &control))
		return UINT32_MAX;

	UI->controlsHashtable[ID]=(UI_Control_t *)List_GetPointer(&UI->controls, List_GetCount(&UI->controls)-1);

	return ID;
}

bool UI_UpdateText(UI_t *UI, uint32_t ID, vec2 position, float size, vec3 color, bool hidden, const char *titleText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_TEXT)
	{
		control->position=position;
		control->color=color;
		control->hidden=hidden;

		Zone_Free(zone, control->text.titleText);

		control->text.titleTextLength=strlen(titleText)+1;
		control->text.titleText=(char *)Zone_Malloc(zone, control->text.titleTextLength);

		if(control->text.titleText==NULL)
			return false;

		memcpy(control->text.titleText, titleText, control->text.titleTextLength);

		control->text.size=size;

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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_TEXT)
	{
		control->position=position;
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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_TEXT)
	{
		control->text.size=size;
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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_TEXT)
	{
		control->color=color;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateTextVisibility(UI_t *UI, uint32_t ID, bool hidden)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_TEXT)
	{
		control->hidden=hidden;
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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_TEXT)
	{
		Zone_Free(zone, control->text.titleText);

		control->text.titleTextLength=strlen(titleText)+1;
		control->text.titleText=(char *)Zone_Malloc(zone, control->text.titleTextLength);

		if(control->text.titleText==NULL)
			return false;

		memcpy(control->text.titleText, titleText, control->text.titleTextLength);

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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_TEXT)
	{
		Zone_Free(zone, control->text.titleText);

		va_list args;
		va_start(args, titleText);

		va_list argsCopy;
		va_copy(argsCopy, args);
		control->text.titleTextLength=vsnprintf(NULL, 0, titleText, argsCopy);
		va_end(argsCopy);

		control->text.titleText=(char *)Zone_Malloc(zone, control->text.titleTextLength+1);

		if(control->text.titleText==NULL)
		{
			va_end(args);
			return false;
		}

		vsnprintf(control->text.titleText, control->text.titleTextLength+1, titleText, args);
		va_end(args);

		return true;
	}

	// Not found
	return false;
}
