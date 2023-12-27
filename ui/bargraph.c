#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

uint32_t UI_AddBarGraph(UI_t *UI, vec2 position, vec2 size, vec3 color, const char *titleText, bool Readonly, float Min, float Max, float value)
{
	uint32_t ID=UI->baseID++;

	if(ID==UINT32_MAX||ID>=UI_HASHTABLE_MAX)
		return UINT32_MAX;

	UI_Control_t Control=
	{
		.type=UI_CONTROL_BARGRAPH,
		.ID=ID,
		.position=position,
		.color=color,
		.BarGraph.size=size,
		.BarGraph.Readonly=Readonly,
		.BarGraph.Min=Min,
		.BarGraph.Max=Max,
		.BarGraph.value=value
	};

	snprintf(Control.BarGraph.titleText, UI_CONTROL_TITLETEXT_MAX, "%s", titleText);

	if(!List_Add(&UI->controls, &Control))
		return UINT32_MAX;

	UI->controlsHashtable[ID]=List_GetPointer(&UI->controls, List_GetCount(&UI->controls)-1);

	return ID;
}

bool UI_UpdateBarGraph(UI_t *UI, uint32_t ID, vec2 position, vec2 size, vec3 color, const char *titleText, bool Readonly, float Min, float Max, float value)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BARGRAPH)
	{
		Control->position=position;
		Control->color=color;

		snprintf(Control->BarGraph.titleText, UI_CONTROL_TITLETEXT_MAX, "%s", titleText);
		Control->BarGraph.size=size;
		Control->BarGraph.Readonly=Readonly;
		Control->BarGraph.Min=Min;
		Control->BarGraph.Max=Max;
		Control->BarGraph.value=value;

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateBarGraphPosition(UI_t *UI, uint32_t ID, vec2 position)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BARGRAPH)
	{
		Control->position=position;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateBarGraphSize(UI_t *UI, uint32_t ID, vec2 size)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BARGRAPH)
	{
		Control->Button.size=size;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateBarGraphColor(UI_t *UI, uint32_t ID, vec3 color)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BARGRAPH)
	{
		Control->color=color;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateBarGraphTitleText(UI_t *UI, uint32_t ID, const char *titleText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BARGRAPH)
	{
		snprintf(Control->BarGraph.titleText, UI_CONTROL_TITLETEXT_MAX, "%s", titleText);
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateBarGraphReadonly(UI_t *UI, uint32_t ID, bool Readonly)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BARGRAPH)
	{
		Control->BarGraph.Readonly=Readonly;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateBarGraphMin(UI_t *UI, uint32_t ID, float Min)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BARGRAPH)
	{
		Control->BarGraph.Min=Min;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateBarGraphMax(UI_t *UI, uint32_t ID, float Max)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BARGRAPH)
	{
		Control->BarGraph.Max=Max;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateBarGraphValue(UI_t *UI, uint32_t ID, float value)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BARGRAPH)
	{
		Control->BarGraph.value=value;
		return true;
	}

	// Not found
	return false;
}

float UI_GetBarGraphMin(UI_t *UI, uint32_t ID)
{
	if(UI==NULL||ID==UINT32_MAX)
		return NAN;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BARGRAPH)
		return Control->BarGraph.Min;

	// Not found
	return NAN;
}

float UI_GetBarGraphMax(UI_t *UI, uint32_t ID)
{
	if(UI==NULL||ID==UINT32_MAX)
		return NAN;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BARGRAPH)
		return Control->BarGraph.Max;

	// Not found
	return NAN;
}

float UI_GetBarGraphValue(UI_t *UI, uint32_t ID)
{
	if(UI==NULL||ID==UINT32_MAX)
		return NAN;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BARGRAPH)
		return Control->BarGraph.value;

	// Not found
	return NAN;
}

float *UI_GetBarGraphValuePointer(UI_t *UI, uint32_t ID)
{
	if(UI==NULL||ID==UINT32_MAX)
		return NULL;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BARGRAPH)
		return &Control->BarGraph.value;

	// Not found
	return NULL;
}
