#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../system/system.h"
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

uint32_t UI_AddEditText(UI_t *UI, vec2 position, vec2 size, vec3 color, bool hidden,  bool readonly, uint32_t maxLength, const char *initialText)
{
	uint32_t ID=UI->baseID++;

	if(ID==UINT32_MAX||ID>=UI_HASHTABLE_MAX)
		return UINT32_MAX;

	UI_Control_t control=
	{
		.type=UI_CONTROL_EDITTEXT,
		.ID=ID,
		.position=position,
		.color=color,
		.childParentID=UINT32_MAX,
		.hidden=hidden,
		.editText.size=size,
		.editText.readonly=readonly,
		.editText.maxLength=maxLength,
		.editText.cursorPos=0,
	};

	control.editText.buffer=(char *)Zone_Malloc(zone, maxLength+1);

	if(!control.editText.buffer)
		return UINT32_MAX;

	if(!UI_AddControl(UI, &control))
		return UINT32_MAX;

	// Get base length of title text
	const float textLength=Font_StringBaseWidth(initialText);

	// Scale text size based on the base control size and length of text, but no bigger than 80% of button height
	const float textSize=fminf(size.x/textLength*0.8f, size.y*0.8f);

	// Print the text left justified
	vec2 textPosition=Vec2(position.x+8.0f, position.y+(size.y*0.5f));

	UI->controlsHashtable[ID]->editText.titleTextID=UI_AddText(UI, textPosition, textSize, Vec3b(1.0f), hidden, initialText);

	return ID;
}

bool UI_UpdateEditText(UI_t *UI, uint32_t ID, vec2 position, vec2 size, vec3 color, bool hidden, bool readonly, uint32_t maxLength, const char *initialText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_EDITTEXT)
	{
		control->position=position;
		control->color=color;
		control->hidden=hidden;
		control->editText.maxLength=maxLength;

		const float textLength=Font_StringBaseWidth(initialText);
		const float textSize=fminf(size.x/textLength*0.8f, size.y*0.8f);
		vec2 textPosition=Vec2(position.x+8.0f, position.y+(size.y*0.5f));
		UI_UpdateText(UI, control->editText.titleTextID, textPosition, textSize, Vec3b(1.0f), hidden, initialText);

		control->editText.size=size;
		control->editText.readonly=readonly;

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateEditTextPosition(UI_t *UI, uint32_t ID, vec2 position)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_EDITTEXT)
	{
		control->position=position;

		UI_Control_t *textControl=UI_FindControlByID(UI, control->editText.titleTextID);

		const float textLength=Font_StringBaseWidth(textControl->text.titleText);
		const float textSize=fminf(control->editText.size.x/textLength*0.8f, control->editText.size.y*0.8f);
		vec2 textPosition=Vec2(control->position.x+8.0f, control->position.y+(control->editText.size.y*0.5f));
		UI_UpdateTextPosition(UI, control->editText.titleTextID, textPosition);
		UI_UpdateTextSize(UI, control->editText.titleTextID, textSize);

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateEditTextSize(UI_t *UI, uint32_t ID, vec2 size)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_EDITTEXT)
	{
		control->editText.size=size;

		UI_Control_t *textControl=UI_FindControlByID(UI, control->editText.titleTextID);

		const float textLength=Font_StringBaseWidth(textControl->text.titleText);
		const float textSize=fminf(control->editText.size.x/textLength*0.8f, control->editText.size.y*0.8f);
		vec2 textPosition=Vec2(control->position.x+8.0f, control->position.y+(control->editText.size.y*0.5f));
		UI_UpdateTextPosition(UI, control->editText.titleTextID, textPosition);
		UI_UpdateTextSize(UI, control->editText.titleTextID, textSize);

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateEditTextColor(UI_t *UI, uint32_t ID, vec3 color)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_EDITTEXT)
	{
		control->color=color;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateEditTextVisibility(UI_t *UI, uint32_t ID, bool hidden)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_EDITTEXT)
	{
		control->hidden=hidden;
		UI_UpdateTextVisibility(UI, control->editText.titleTextID, hidden);

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateEditTextTitleText(UI_t *UI, uint32_t ID, const char *titleText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_EDITTEXT)
	{
		UI_UpdateTextTitleText(UI, control->editText.titleTextID, titleText);
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateEditTextReadonly(UI_t *UI, uint32_t ID, bool readonly)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_EDITTEXT)
	{
		control->editText.readonly=readonly;
		return true;
	}

	// Not found
	return false;
}
