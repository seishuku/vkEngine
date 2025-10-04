#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../utils/id.h"
#include "../math/math.h"
#include "../utils/list.h"
#include "../font/font.h"
#include "ui.h"

// Add a virtual thumbstick to the UI.
// Returns an ID, or UINT32_MAX on failure.
uint32_t UI_AddVirtualStick(UI_t *UI, vec2 position, float radius, vec3 color, UI_ControlVisibility visibility, const char *titleText)
{
	uint32_t ID=ID_Generate(UI->baseID);

	if(ID==UINT32_MAX||ID>=UI_HASHTABLE_MAX)
		return UINT32_MAX;

	UI_Control_t control=
	{
		.type=UI_CONTROL_VIRTUALSTICK,
		.ID=ID,
		.position=position,
		.color=color,
		.childParentID=UINT32_MAX,
		.visibility=visibility,
		.virtualStick.radius=radius,
		.virtualStick.value=Vec2b(0.0f),
	};

	if(!UI_AddControl(UI, &control))
		return UINT32_MAX;

	// TODO:
	// This is bit annoying...
	// The control's title text needs to be added after the actual control, otherwise it will be rendered under this control.
	// I suppose this would be fixed with proper render order sorting, maybe later.

	// Text size is the radius of the checkbox, placed radius length away horizontally, centered vertically
	UI->controlsHashtable[ID]->virtualStick.titleTextID=UI_AddText(UI,
		Vec2(position.x+radius, position.y), radius,
		Vec3(1.0f, 1.0f, 1.0f),
		visibility,
		titleText);

	return ID;
}

// Update UI checkbox parameters.
// Returns true on success, false on failure.
// Also individual parameter update functions.
bool UI_UpdateVirtualStick(UI_t *UI, uint32_t ID, vec2 position, float radius, vec3 color, UI_ControlVisibility visibility, const char *titleText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_VIRTUALSTICK)
	{
		control->position=position;
		control->color=color;
		control->visibility=visibility;

		UI_UpdateText(UI, control->virtualStick.titleTextID,
			Vec2(position.x+radius, position.y), radius,
			Vec3(1.0f, 1.0f, 1.0f),
			visibility,
			titleText);
		control->virtualStick.radius=radius;

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateVirtualStickPosition(UI_t *UI, uint32_t ID, vec2 position)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_VIRTUALSTICK)
	{
		control->position=position;
		UI_UpdateTextPosition(UI, control->virtualStick.titleTextID,
			Vec2(control->position.x+control->virtualStick.radius, control->position.y));
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateVirtualStickRadius(UI_t *UI, uint32_t ID, float radius)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_VIRTUALSTICK)
	{
		control->checkBox.radius=radius;
		UI_UpdateTextPosition(UI, control->checkBox.titleTextID,
			Vec2(control->position.x+control->checkBox.radius, control->position.y));
		UI_UpdateTextSize(UI, control->checkBox.titleTextID, control->checkBox.radius);
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateVirtualStickColor(UI_t *UI, uint32_t ID, vec3 color)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_VIRTUALSTICK)
	{
		control->color=color;
		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateVirtualStickVisibility(UI_t *UI, uint32_t ID, UI_ControlVisibility visibility)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_VIRTUALSTICK)
	{
		control->visibility=visibility;
		UI_UpdateTextVisibility(UI, control->virtualStick.titleTextID, visibility);

		return true;
	}

	// Not found
	return false;
}

bool UI_UpdateVirtualStickTitleText(UI_t *UI, uint32_t ID, const char *titleText)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_VIRTUALSTICK)
	{
		UI_UpdateTextTitleText(UI, control->virtualStick.titleTextID, titleText);
		return true;
	}

	// Not found
	return false;
}

// Get the value of the current virtual thumbstick position by ID
// Returns 0,0 on error
vec2 UI_GetVirtualStickValue(UI_t *UI, uint32_t ID)
{
	if(UI==NULL||ID==UINT32_MAX)
		return Vec2b(0.0f);

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_VIRTUALSTICK)
		return control->virtualStick.value;

	// Not found
	return Vec2b(0.0f);
}

// Set the active flag of the virtual thumbstick by ID
// Returns false on error
bool UI_SetVirtualStickActive(UI_t *UI, uint32_t ID, bool active)
{
	if(UI==NULL||ID==UINT32_MAX)
		return false;

	// Search list
	UI_Control_t *control=UI_FindControlByID(UI, ID);

	if(control!=NULL&&control->type==UI_CONTROL_VIRTUALSTICK)
	{
		control->virtualStick.active=active;
		return true;
	}

	// Not found
	return false;
}
