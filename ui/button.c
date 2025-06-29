#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

// Add a button to the UI.
// Returns an ID, or UINT32_MAX on failure.
uint32_t UI_AddButton(UI_t *UI, vec2 position, vec2 size, vec3 color, bool hidden, const char *titleText, UIControlCallback callback)
{
	uint32_t ID=UI->baseID++;

	if(ID==UINT32_MAX||ID>=UI_HASHTABLE_MAX)
		return UINT32_MAX;

	UI_Control_t control=
	{
		.type=UI_CONTROL_BUTTON,
		.ID=ID,
		.position=position,
		.color=color,
		.childParentID=UINT32_MAX,
		.hidden=hidden,
		.button.size=size,
		.button.callback=callback
	};

	if(!List_Add(&UI->controls, &control))
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
	UI->controlsHashtable[ID]->button.titleTextID=UI_AddText(UI, textPosition, textSize, Vec3b(1.0f), hidden, titleText);

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
bool UI_UpdateButton(UI_t *UI, uint32_t ID, vec2 position, vec2 size, vec3 color, bool hidden, const char *titleText, UIControlCallback callback)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BUTTON)
	{
		control->position=position;
		control->color=color;
		control->hidden=hidden;
		control->button.size=size;
		control->button.callback=callback;

		float textLength=Font_StringBaseWidth(titleText);
		float textSize=fminf(control->button.size.x/textLength*0.8f, control->button.size.y*0.8f);
		vec2 textPosition=Vec2(control->position.x-(textLength*textSize)*0.5f+control->button.size.x*0.5f, control->position.y+(control->button.size.y*0.5f));
		UI_UpdateText(UI, control->button.titleTextID, textPosition, textSize, Vec3b(1.0f), hidden, titleText);

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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BUTTON)
	{
		control->position=position;

		UI_Control_t *textControl=UI_FindControlByID(UI, control->button.titleTextID);

		const float textLength=Font_StringBaseWidth(textControl->text.titleText);
		const float textSize=fminf(control->button.size.x/textLength*0.8f, control->button.size.y*0.8f);
		vec2 textPosition=Vec2(control->position.x-(textLength*textSize)*0.5f+control->button.size.x*0.5f, control->position.y+(control->button.size.y*0.5f));
		UI_UpdateTextPosition(UI, control->button.titleTextID, textPosition);
		UI_UpdateTextSize(UI, control->button.titleTextID, textSize);

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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BUTTON)
	{
		control->button.size=size;

		UI_Control_t *textControl=UI_FindControlByID(UI, control->button.titleTextID);

		const float textLength=Font_StringBaseWidth(textControl->text.titleText);
		const float textSize=fminf(control->button.size.x/textLength*0.8f, control->button.size.y*0.8f);
		vec2 textPosition=Vec2(control->position.x-(textLength*textSize)*0.5f+control->button.size.x*0.5f, control->position.y+(control->button.size.y*0.5f));
		UI_UpdateTextPosition(UI, control->button.titleTextID, textPosition);
		UI_UpdateTextSize(UI, control->button.titleTextID, textSize);

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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BUTTON)
	{
		control->color=color;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateButtonVisibility(UI_t *UI, uint32_t ID, bool hidden)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BUTTON)
	{
		control->hidden=hidden;
		UI_UpdateTextVisibility(UI, control->button.titleTextID, hidden);

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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BUTTON)
	{
		UI_UpdateTextTitleText(UI, control->button.titleTextID, titleText);
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
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_BUTTON)
	{
		control->button.callback=callback;
		return true;
	}

	// Not found
	return false;
}
