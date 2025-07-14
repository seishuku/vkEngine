#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../system/system.h"
#include "../math/math.h"
#include "../utils/id.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

uint32_t UI_AddEditText(UI_t *UI, vec2 position, vec2 size, vec3 color, UI_ControlVisibility visibility, UI_ControlMutability mutability, uint32_t maxLength, const char *initialText)
{
	uint32_t ID=ID_Generate(UI->baseID);

	if(ID==UINT32_MAX||ID>=UI_HASHTABLE_MAX)
		return UINT32_MAX;

	UI_Control_t control=
	{
		.type=UI_CONTROL_EDITTEXT,
		.ID=ID,
		.position=position,
		.color=color,
		.childParentID=UINT32_MAX,
		.visibility=visibility,
		.editText.size=size,
		.editText.mutability=mutability,
		.editText.maxLength=maxLength,
		.editText.cursorPos=0,
		.editText.textOffset=0,
	};

	control.editText.buffer=(char *)Zone_Malloc(zone, maxLength+1);

	if(!control.editText.buffer)
		return UINT32_MAX;

	memset(control.editText.buffer, 0, maxLength+1);

	if(initialText)
	{
		strncpy(control.editText.buffer, initialText, maxLength);
		control.editText.cursorPos=(uint32_t)strlen(control.editText.buffer);
	}

	if(!UI_AddControl(UI, &control))
		return UINT32_MAX;

	// Get base length of title text
	const float textLength=Font_StringBaseWidth(control.editText.buffer);

	// Scale text size based on the base control size and length of text, but no bigger than 80% of button height
	const float textSize=fminf(size.x/textLength*0.8f, size.y*0.8f);

	// Print the text left justified
	vec2 textPosition=Vec2(position.x+8.0f, position.y+(size.y*0.5f));

	UI->controlsHashtable[ID]->editText.titleTextID=UI_AddText(UI, textPosition, textSize, Vec3b(1.0f), visibility, control.editText.buffer);

	return ID;
}

bool UI_UpdateEditText(UI_t *UI, uint32_t ID, vec2 position, vec2 size, vec3 color, UI_ControlVisibility visibility, UI_ControlMutability mutability, uint32_t maxLength, const char *initialText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_EDITTEXT)
	{
		control->position=position;
		control->color=color;
		control->visibility=visibility;
		control->editText.maxLength=maxLength;

		const float textLength=Font_StringBaseWidth(control->editText.buffer);
		const float textSize=fminf(size.x/textLength*0.8f, size.y*0.8f);
		vec2 textPosition=Vec2(position.x+8.0f, position.y+(size.y*0.5f));
		UI_UpdateText(UI, control->editText.titleTextID, textPosition, textSize, Vec3b(1.0f), visibility, initialText);

		control->editText.size=size;
		control->editText.mutability=mutability;

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

		const float textLength=Font_StringBaseWidth(control->editText.buffer);
		const float textSize=fminf(control->editText.size.x/textLength*0.8f, control->editText.size.y*0.8f);
		vec2 textPosition=Vec2(position.x+8.0f, position.y+(control->editText.size.y*0.5f));
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

		const float textLength=Font_StringBaseWidth(control->editText.buffer);
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

bool UI_UpdateEditTextVisibility(UI_t *UI, uint32_t ID, UI_ControlVisibility visibility)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_EDITTEXT)
	{
		control->visibility=visibility;
		UI_UpdateTextVisibility(UI, control->editText.titleTextID, visibility);

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

bool UI_UpdateEditTextMutability(UI_t *UI, uint32_t ID, UI_ControlMutability mutability)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_EDITTEXT)
	{
		control->editText.mutability=mutability;
		return true;
	}

	// Not found
	return false;
}

static void UI_UpdateEditTextRender(UI_t *UI, UI_Control_t *control)
{
	const float textSize=UI_FindControlByID(UI, control->editText.titleTextID)->text.size;
	const uint32_t len=(uint32_t)strlen(control->editText.buffer);
	const float maxWidth=control->editText.size.x-16.0f;

	while(1)
	{
		float cursor=0.0f;

		if(control->editText.cursorPos>=control->editText.textOffset)
		{
			for(uint32_t i=control->editText.textOffset;i<control->editText.cursorPos&&i<len;i++)
				cursor+=Font_CharacterBaseWidth(control->editText.buffer[i])*textSize;
		}
		else
			cursor=-1.0f;

		if(cursor>maxWidth)
		{
			if(control->editText.textOffset<len)
				control->editText.textOffset++;
			else
				break;
		}
		else if(cursor<0.0f)
		{
			if(control->editText.textOffset>0)
				control->editText.textOffset--;
			else
				break;
		}
		else
			break;
	}

	char tempBuf[512];
	uint32_t visIdx=0;
	float width=0.0f;
	uint32_t offset=control->editText.textOffset;

	while(offset<len&&visIdx<sizeof(tempBuf)-2)
	{
		if(offset==control->editText.cursorPos)
		{
			tempBuf[visIdx++]='|';

			width+=Font_CharacterBaseWidth('|')*textSize;

			if(width>maxWidth)
				break;
		}

		const char c=control->editText.buffer[offset];
		const float charWidth=Font_CharacterBaseWidth(c)*textSize;

		if(width+charWidth>maxWidth)
			break;

		tempBuf[visIdx++]=c;
		width+=charWidth;
		offset++;
	}

	if(control->editText.cursorPos==len&&visIdx<sizeof(tempBuf)-1)
	{
		if(width+Font_CharacterBaseWidth('|')*textSize<=maxWidth)
			tempBuf[visIdx++]='|';
	}

	tempBuf[visIdx]='\0';

	UI_UpdateTextTitleTextf(UI, control->editText.titleTextID, "%s", tempBuf);
}

bool UI_EditTextInsertChar(UI_t *UI, uint32_t ID, char c)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(!control||control->type!=UI_CONTROL_EDITTEXT||control->editText.mutability)
		return false;

	uint32_t len=(uint32_t)strlen(control->editText.buffer);

	if(len>=control->editText.maxLength)
		return false;

	if(control->editText.cursorPos>len)
		control->editText.cursorPos=len;

	memmove(&control->editText.buffer[control->editText.cursorPos+1], &control->editText.buffer[control->editText.cursorPos], len-control->editText.cursorPos+1);

	control->editText.buffer[control->editText.cursorPos]=c;
	control->editText.cursorPos++;

	UI_UpdateEditTextRender(UI, control);

	return true;
}

bool UI_EditTextMoveCursor(UI_t *UI, uint32_t ID, int32_t offset)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(!control||control->type!=UI_CONTROL_EDITTEXT||control->editText.mutability)
		return false;

	int32_t newPos=(int32_t)control->editText.cursorPos+offset;

	if(newPos<0)
		newPos=0;

	uint32_t len=(uint32_t)strlen(control->editText.buffer);

	if((uint32_t)newPos>len)
		newPos=len;

	control->editText.cursorPos=(uint32_t)newPos;

	UI_UpdateEditTextRender(UI, control);

	return true;
}

bool UI_EditTextBackspace(UI_t *UI, uint32_t ID)
{
	UI_Control_t *control=UI_FindControlByID(UI, ID);
	if(!control||control->type!=UI_CONTROL_EDITTEXT||control->editText.mutability)
		return false;

	if(control->editText.cursorPos==0)
		return false;

	memmove(
		&control->editText.buffer[control->editText.cursorPos-1],
		&control->editText.buffer[control->editText.cursorPos],
		strlen(&control->editText.buffer[control->editText.cursorPos])+1
	);

	control->editText.cursorPos--;

	UI_UpdateEditTextRender(UI, control);
	return true;
}

bool UI_EditTextDelete(UI_t *UI, uint32_t ID)
{
	UI_Control_t *control=UI_FindControlByID(UI, ID);
	if(!control||control->type!=UI_CONTROL_EDITTEXT||control->editText.mutability)
		return false;

	uint32_t len=(uint32_t)strlen(control->editText.buffer);
	if(control->editText.cursorPos>=len)
		return false;

	memmove(
		&control->editText.buffer[control->editText.cursorPos],
		&control->editText.buffer[control->editText.cursorPos+1],
		len-control->editText.cursorPos
	);

	UI_UpdateEditTextRender(UI, control);
	return true;
}
