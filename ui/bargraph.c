#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

uint32_t UI_AddBarGraph(UI_t *UI, vec2 position, vec2 size, vec3 color, bool hidden, const char *titleText, bool readonly, float min, float max, float value)
{
	uint32_t ID=UI->baseID++;

	if(ID==UINT32_MAX||ID>=UI_HASHTABLE_MAX)
		return UINT32_MAX;

	UI_Control_t control=
	{
		.type=UI_CONTROL_BARGRAPH,
		.ID=ID,
		.position=position,
		.color=color,
		.childParentID=UINT32_MAX,
		.hidden=hidden,
		.barGraph.size=size,
		.barGraph.readonly=readonly,
		.barGraph.min=min,
		.barGraph.max=max,
		.barGraph.value=value,
		.barGraph.curValue=value,
	};

	if(!UI_AddControl(UI, &control))
		return UINT32_MAX;

	// TODO:
	// This is bit annoying...
	// The control's title text needs to be added after the actual control, otherwise it will be rendered under this control.
	// I suppose this would be fixed with proper render order sorting, maybe later.

	// Get base length of title text
	const float textLength=Font_StringBaseWidth(titleText);

	// Scale text size based on the base control size and length of text, but no bigger than 80% of button height
	const float textSize=fminf(size.x/textLength*0.8f, size.y*0.8f);

	// Print the text centered
	vec2 textPosition=Vec2(position.x-(textLength*textSize)*0.5f+size.x*0.5f, position.y+(size.y*0.5f));

	UI->controlsHashtable[ID]->barGraph.titleTextID=UI_AddText(UI, textPosition, textSize, Vec3b(1.0f), hidden, titleText);

	return ID;
}

bool UI_UpdateBarGraph(UI_t *UI, uint32_t ID, vec2 position, vec2 size, vec3 color, bool hidden, const char *titleText, bool readonly, float Min, float Max, float value)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BARGRAPH)
	{
		control->position=position;
		control->color=color;
		control->hidden=hidden;

		const float textLength=Font_StringBaseWidth(titleText);
		const float textSize=fminf(size.x/textLength*0.8f, size.y*0.8f);
		vec2 textPosition=Vec2(position.x-(textLength*textSize)*0.5f+size.x*0.5f, position.y+(size.y*0.5f));
		UI_UpdateText(UI, control->barGraph.titleTextID, textPosition, textSize, Vec3b(1.0f), hidden, titleText);

		control->barGraph.size=size;
		control->barGraph.readonly=readonly;
		control->barGraph.min=Min;
		control->barGraph.max=Max;
		control->barGraph.value=value;

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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BARGRAPH)
	{
		control->position=position;

		UI_Control_t *textControl=UI_FindControlByID(UI, control->barGraph.titleTextID);

		const float textLength=Font_StringBaseWidth(textControl->text.titleText);
		const float textSize=fminf(control->barGraph.size.x/textLength*0.8f, control->barGraph.size.y*0.8f);
		vec2 textPosition=Vec2(control->position.x-(textLength*textSize)*0.5f+control->barGraph.size.x*0.5f, control->position.y+(control->barGraph.size.y*0.5f));
		UI_UpdateTextPosition(UI, control->barGraph.titleTextID, textPosition);
		UI_UpdateTextSize(UI, control->barGraph.titleTextID, textSize);

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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BARGRAPH)
	{
		control->barGraph.size=size;

		UI_Control_t *textControl=UI_FindControlByID(UI, control->barGraph.titleTextID);

		const float textLength=Font_StringBaseWidth(textControl->text.titleText);
		const float textSize=fminf(control->barGraph.size.x/textLength*0.8f, control->barGraph.size.y*0.8f);
		vec2 textPosition=Vec2(control->position.x-(textLength*textSize)*0.5f+control->barGraph.size.x*0.5f, control->position.y+(control->barGraph.size.y*0.5f));
		UI_UpdateTextPosition(UI, control->barGraph.titleTextID, textPosition);
		UI_UpdateTextSize(UI, control->barGraph.titleTextID, textSize);

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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BARGRAPH)
	{
		control->color=color;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateBarGraphVisibility(UI_t *UI, uint32_t ID, bool hidden)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BARGRAPH)
	{
		control->hidden=hidden;
		UI_UpdateTextVisibility(UI, control->barGraph.titleTextID, hidden);

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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BARGRAPH)
	{
		UI_UpdateTextTitleText(UI, control->barGraph.titleTextID, titleText);
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateBarGraphReadonly(UI_t *UI, uint32_t ID, bool readonly)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BARGRAPH)
	{
		control->barGraph.readonly=readonly;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateBarGraphMin(UI_t *UI, uint32_t ID, float min)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BARGRAPH)
	{
		control->barGraph.min=min;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateBarGraphMax(UI_t *UI, uint32_t ID, float max)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BARGRAPH)
	{
		control->barGraph.max=max;
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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BARGRAPH)
	{
		control->barGraph.value=value;
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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BARGRAPH)
		return control->barGraph.min;

	// Not found
	return NAN;
}

float UI_GetBarGraphMax(UI_t *UI, uint32_t ID)
{
	if(UI==NULL||ID==UINT32_MAX)
		return NAN;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BARGRAPH)
		return control->barGraph.max;

	// Not found
	return NAN;
}

float UI_GetBarGraphValue(UI_t *UI, uint32_t ID)
{
	if(UI==NULL||ID==UINT32_MAX)
		return NAN;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BARGRAPH)
		return control->barGraph.value;

	// Not found
	return NAN;
}

float *UI_GetBarGraphValuePointer(UI_t *UI, uint32_t ID)
{
	if(UI==NULL||ID==UINT32_MAX)
		return NULL;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BARGRAPH)
		return &control->barGraph.value;

	// Not found
	return NULL;
}
