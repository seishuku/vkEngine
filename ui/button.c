#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

// Add a button to the UI.
// Returns an ID, or UINT32_MAX on failure.
uint32_t UI_AddButton(UI_t *UI, vec2 position, vec2 size, vec3 color, const char *titleText, UIControlCallback callback)
{
	uint32_t ID=UI->baseID++;

	if(ID==UINT32_MAX||ID>=UI_HASHTABLE_MAX)
		return UINT32_MAX;

	UI_Control_t Control=
	{
		.type=UI_CONTROL_BUTTON,
		.ID=ID,
		.position=position,
		.color=color,
		.childParentID=UINT32_MAX,
		.button.size=size,
		.button.callback=callback
	};

	if(!List_Add(&UI->controls, &Control))
		return UINT32_MAX;

	UI->controlsHashtable[ID]=List_GetPointer(&UI->controls, List_GetCount(&UI->controls)-1);

	// TODO:
	// This is bit annoying...
	// The control's title text needs to be added after the actual control, otherwise it will be rendered under this control.
	// I suppose this would be fixed with proper render order sorting, maybe later.

	// Get base length of title text
	float textLength=Font_StringBaseWidth(titleText);

	// Scale text size based on the button size and length of text, but no bigger than 80% of button height
	float textSize=fminf(size.x/textLength*0.8f, size.y*0.8f);

	// Print the text centered
	vec2 textPosition=Vec2(position.x-(textLength*textSize)*0.5f+size.x*0.5f, position.y+(size.y*0.5f));
	UI->controlsHashtable[ID]->barGraph.titleTextID=UI_AddText(UI, textPosition, textSize, Vec3(1.0f, 1.0f, 1.0f), titleText);

	// Left justified
	//	control->position.x,
	//	control->position.y-(textSize*0.5f)+control->button.size.y*0.5f,

	// right justified
	//	control->position.x-(textLength*textSize)+control->button.size.x,
	//	control->position.y-(textSize*0.5f)+control->button.size.y*0.5f,

	return ID;
}

// Update UI button parameters.
// Returns true on success, false on failure.
// Also individual parameter update function as well.
bool UI_UpdateButton(UI_t *UI, uint32_t ID, vec2 position, vec2 size, vec3 color, const char *titleText, UIControlCallback callback)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BUTTON)
	{
		Control->position=position;
		Control->color=color;
		Control->button.size=size;
		Control->button.callback=callback;

		float textLength=Font_StringBaseWidth(titleText);
		float textSize=fminf(Control->button.size.x/textLength*0.8f, Control->button.size.y*0.8f);
		vec2 textPosition=Vec2(Control->position.x-(textLength*textSize)*0.5f+Control->button.size.x*0.5f, Control->position.y+(Control->button.size.y*0.5f));
		UI_UpdateText(UI, Control->button.titleTextID, textPosition, textSize, Vec3(1.0f, 1.0f, 1.0f), titleText);

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateButtonPosition(UI_t *UI, uint32_t ID, vec2 position)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BUTTON)
	{
		Control->position=position;

		UI_Control_t *textControl=UI_FindControlByID(UI, Control->button.titleTextID);

		const float textLength=Font_StringBaseWidth(textControl->text.titleText);
		const float textSize=fminf(Control->button.size.x/textLength*0.8f, Control->button.size.y*0.8f);
		vec2 textPosition=Vec2(Control->position.x-(textLength*textSize)*0.5f+Control->button.size.x*0.5f, Control->position.y+(Control->button.size.y*0.5f));
		UI_UpdateTextPosition(UI, Control->button.titleTextID, textPosition);
		UI_UpdateTextSize(UI, Control->button.titleTextID, textSize);

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateButtonSize(UI_t *UI, uint32_t ID, vec2 size)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BUTTON)
	{
		Control->button.size=size;

		UI_Control_t *textControl=UI_FindControlByID(UI, Control->button.titleTextID);

		const float textLength=Font_StringBaseWidth(textControl->text.titleText);
		const float textSize=fminf(Control->button.size.x/textLength*0.8f, Control->button.size.y*0.8f);
		vec2 textPosition=Vec2(Control->position.x-(textLength*textSize)*0.5f+Control->button.size.x*0.5f, Control->position.y+(Control->button.size.y*0.5f));
		UI_UpdateTextPosition(UI, Control->button.titleTextID, textPosition);
		UI_UpdateTextSize(UI, Control->button.titleTextID, textSize);

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateButtonColor(UI_t *UI, uint32_t ID, vec3 color)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BUTTON)
	{
		Control->color=color;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateButtonTitleText(UI_t *UI, uint32_t ID, const char *titleText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BUTTON)
	{
		UI_UpdateTextTitleText(UI, Control->button.titleTextID, titleText);
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateButtonCallback(UI_t *UI, uint32_t ID, UIControlCallback callback)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *Control=UI_FindControlByID(UI, ID);

	if(Control!=NULL&&Control->type==UI_CONTROL_BUTTON)
	{
		Control->button.callback=callback;
		return true;
	}

	// Not found
	return false;
}
