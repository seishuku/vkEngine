#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../utils/genid.h"
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

uint32_t UI_AddBarGraph(UI_t *UI, vec2 Position, vec2 Size, vec3 Color, const char *TitleText, bool Readonly, float Min, float Max, float Value)
{
	uint32_t ID=GenID();

	if(ID==UINT32_MAX)
		return UINT32_MAX;

	UI_Control_t Control=
	{
		.Type=UI_CONTROL_BARGRAPH,
		.ID=ID,
		.Position=Position,
		.Color=Color,
		.BarGraph.Size=Size,
		.BarGraph.Readonly=Readonly,
		.BarGraph.Min=Min,
		.BarGraph.Max=Max,
		.BarGraph.Value=Value
	};

	snprintf(Control.TitleText, UI_CONTROL_TITLETEXT_MAX, "%s", TitleText);

	if(!List_Add(&UI->Controls, &Control))
		return UINT32_MAX;

	return ID;
}

bool UI_UpdateBarGraph(UI_t *UI, uint32_t ID, vec2 Position, vec2 Size, vec3 Color, const char *TitleText, bool Readonly, float Min, float Max, float Value)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_BARGRAPH)
	{
		Control->Position=Position;
		Control->Color=Color;
		snprintf(Control->TitleText, UI_CONTROL_TITLETEXT_MAX, "%s", TitleText);

		Control->BarGraph.Size=Size;
		Control->BarGraph.Readonly=Readonly;
		Control->BarGraph.Min=Min;
		Control->BarGraph.Max=Max;
		Control->BarGraph.Value=Value;

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateBarGraphPosition(UI_t *UI, uint32_t ID, vec2 Position)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_BARGRAPH)
	{
		Control->Position=Position;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateBarGraphSize(UI_t *UI, uint32_t ID, vec2 Size)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_BARGRAPH)
	{
		Control->Button.Size=Size;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateBarGraphColor(UI_t *UI, uint32_t ID, vec3 Color)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_BARGRAPH)
	{
		Control->Color=Color;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateBarGraphTitleText(UI_t *UI, uint32_t ID, const char *TitleText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_BARGRAPH)
	{
		snprintf(Control->TitleText, UI_CONTROL_TITLETEXT_MAX, "%s", TitleText);
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

	if(Control!=NULL&&Control->Type==UI_CONTROL_BARGRAPH)
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

	if(Control!=NULL&&Control->Type==UI_CONTROL_BARGRAPH)
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

	if(Control!=NULL&&Control->Type==UI_CONTROL_BARGRAPH)
	{
		Control->BarGraph.Max=Max;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateBarGraphValue(UI_t *UI, uint32_t ID, float Value)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->Type==UI_CONTROL_BARGRAPH)
	{
		Control->BarGraph.Value=Value;
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
	for(uint32_t i=0;i<List_GetCount(&UI->Controls);i++)
	{
		UI_Control_t *Control=(UI_Control_t *)List_GetPointer(&UI->Controls, i);

		// Check for matching ID and type
		if(Control->ID==ID&&Control->Type==UI_CONTROL_BARGRAPH)
			return Control->BarGraph.Min;
	}

	// Not found
	return NAN;
}

float UI_GetBarGraphMax(UI_t *UI, uint32_t ID)
{
	if(UI==NULL||ID==UINT32_MAX)
		return NAN;

	// Search list
	for(uint32_t i=0;i<List_GetCount(&UI->Controls);i++)
	{
		UI_Control_t *Control=(UI_Control_t *)List_GetPointer(&UI->Controls, i);

		// Check for matching ID and type
		if(Control->ID==ID&&Control->Type==UI_CONTROL_BARGRAPH)
			return Control->BarGraph.Max;
	}

	// Not found
	return NAN;
}

float UI_GetBarGraphValue(UI_t *UI, uint32_t ID)
{
	if(UI==NULL||ID==UINT32_MAX)
		return NAN;

	// Search list
	for(uint32_t i=0;i<List_GetCount(&UI->Controls);i++)
	{
		UI_Control_t *Control=(UI_Control_t *)List_GetPointer(&UI->Controls, i);

		// Check for matching ID and type
		if(Control->ID==ID&&Control->Type==UI_CONTROL_BARGRAPH)
			return Control->BarGraph.Value;
	}

	// Not found
	return NAN;
}
