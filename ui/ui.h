#ifndef __UI_H__
#define __UI_H__

#include <stdint.h>
#include <stdbool.h>
#include "../math/math.h"
#include "../utils/pipeline.h"
#include "../utils/list.h"

// Does the callback really need args? (userdata?)
typedef void (*UIControlCallback)(void *arg);

#define UI_CONTROL_WINDOW_TITLEBAR_HEIGHT 16.0f
#define UI_CONTROL_WINDOW_BORDER 10.0f

#define UI_CONTROL_TITLETEXT_MAX 128

#define UI_HASHTABLE_MAX 8192

typedef enum
{
	UI_CONTROL_BUTTON=0,
	UI_CONTROL_CHECKBOX,
	UI_CONTROL_BARGRAPH,
	UI_CONTROL_SPRITE,
	UI_CONTROL_CURSOR,
	UI_CONTROL_WINDOW,
	UI_CONTROL_TEXT,
	UI_NUM_CONTROLTYPE
} UI_ControlType;

typedef struct
{
	// Common to all controls
	UI_ControlType type;
	uint32_t ID;
	vec2 position;
	vec3 color;
	uint32_t childParentID;

	// Specific to type
	union
	{
		// Button type
		struct
		{
			uint32_t titleTextID;
			vec2 size;
			UIControlCallback callback;
		} button;

		// CheckBox type, should this also have a callback for flexibility?
		struct
		{
			uint32_t titleTextID;
			float radius;
			bool value;
		} checkBox;

		// BarGraph type
		struct
		{
			uint32_t titleTextID;
			vec2 size;
			bool Readonly;
			float Min, Max, value, curValue;
		} barGraph;

		// Sprite type
		struct
		{
			VkuImage_t *image;
			vec2 size;
			float rotation;
		} sprite;

		// Cursor type
		struct
		{
			float radius;
		} cursor;

		// Window type
		struct
		{
			uint32_t titleTextID;
			vec2 size;
			vec2 hitOffset;
			List_t children;
		} window;

		// Text type
		struct
		{
			char titleText[UI_CONTROL_TITLETEXT_MAX];
			float size;
		} text;
		
	};
} UI_Control_t;

typedef struct
{
	// Position and size of whole UI system
	vec2 position, size;

	// Unique Vulkan data
	Pipeline_t pipeline;

	VkuImage_t blankImage;

	VkuBuffer_t vertexBuffer;

	VkuBuffer_t instanceBuffer;
	void *instanceBufferPtr;

	// Base ID for generating IDs
	uint32_t baseID;

	// List of controls in UI
	List_t controls;

	// Hashtable for quick lookup by ID
	UI_Control_t *controlsHashtable[UI_HASHTABLE_MAX];
} UI_t;

bool UI_Init(UI_t *UI, vec2 position, vec2 size);
void UI_Destroy(UI_t *UI);

UI_Control_t *UI_FindControlByID(UI_t *UI, uint32_t ID);

// Buttons
uint32_t UI_AddButton(UI_t *UI, vec2 position, vec2 size, vec3 color, const char *titleText, UIControlCallback callback);

bool UI_UpdateButton(UI_t *UI, uint32_t ID, vec2 position, vec2 size, vec3 color, const char *titleText, UIControlCallback callback);
bool UI_UpdateButtonPosition(UI_t *UI, uint32_t ID, vec2 position);
bool UI_UpdateButtonSize(UI_t *UI, uint32_t ID, vec2 size);
bool UI_UpdateButtonColor(UI_t *UI, uint32_t ID, vec3 color);
bool UI_UpdateButtonTitleText(UI_t *UI, uint32_t ID, const char *titleText);
bool UI_UpdateButtonCallback(UI_t *UI, uint32_t ID, UIControlCallback callback);

// Check boxes
uint32_t UI_AddCheckBox(UI_t *UI, vec2 position, float radius, vec3 color, const char *titleText, bool value);

bool UI_UpdateCheckBox(UI_t *UI, uint32_t ID, vec2 position, float radius, vec3 color, const char *titleText, bool value);
bool UI_UpdateCheckBoxPosition(UI_t *UI, uint32_t ID, vec2 position);
bool UI_UpdateCheckBoxRadius(UI_t *UI, uint32_t ID, float radius);
bool UI_UpdateCheckBoxColor(UI_t *UI, uint32_t ID, vec3 color);
bool UI_UpdateCheckBoxTitleText(UI_t *UI, uint32_t ID, const char *titleText);
bool UI_UpdateCheckBoxValue(UI_t *UI, uint32_t ID, bool value);

bool UI_GetCheckBoxValue(UI_t *UI, uint32_t ID);

// Bar graphs
uint32_t UI_AddBarGraph(UI_t *UI, vec2 position, vec2 size, vec3 color, const char *titleText, bool Readonly, float Min, float Max, float value);

bool UI_UpdateBarGraph(UI_t *UI, uint32_t ID, vec2 position, vec2 size, vec3 color, const char *titleText, bool Readonly, float Min, float Max, float value);
bool UI_UpdateBarGraphPosition(UI_t *UI, uint32_t ID, vec2 position);
bool UI_UpdateBarGraphSize(UI_t *UI, uint32_t ID, vec2 size);
bool UI_UpdateBarGraphColor(UI_t *UI, uint32_t ID, vec3 color);
bool UI_UpdateBarGraphTitleText(UI_t *UI, uint32_t ID, const char *titleText);
bool UI_UpdateBarGraphReadonly(UI_t *UI, uint32_t ID, bool Readonly);
bool UI_UpdateBarGraphMin(UI_t *UI, uint32_t ID, float Min);
bool UI_UpdateBarGraphMax(UI_t *UI, uint32_t ID, float Max);
bool UI_UpdateBarGraphValue(UI_t *UI, uint32_t ID, float value);

float UI_GetBarGraphMin(UI_t *UI, uint32_t ID);
float UI_GetBarGraphMax(UI_t *UI, uint32_t ID);
float UI_GetBarGraphValue(UI_t *UI, uint32_t ID);
float *UI_GetBarGraphValuePointer(UI_t *UI, uint32_t ID);

uint32_t UI_AddSprite(UI_t *UI, vec2 position, vec2 size, vec3 color, VkuImage_t *image, float rotation);
bool UI_UpdateSprite(UI_t *UI, uint32_t ID, vec2 position, vec2 size, vec3 color, VkuImage_t *image, float rotation);
bool UI_UpdateSpritePosition(UI_t *UI, uint32_t ID, vec2 position);
bool UI_UpdateSpriteSize(UI_t *UI, uint32_t ID, vec2 size);
bool UI_UpdateSpriteColor(UI_t *UI, uint32_t ID, vec3 color);
bool UI_UpdateSpriteImage(UI_t *UI, uint32_t ID, VkuImage_t *image);
bool UI_UpdateSpriteRotation(UI_t *UI, uint32_t ID, float rotation);

uint32_t UI_AddCursor(UI_t *UI, vec2 position, float radius, vec3 color);
bool UI_UpdateCursor(UI_t *UI, uint32_t ID, vec2 position, float radius, vec3 color);
bool UI_UpdateCursorPosition(UI_t *UI, uint32_t ID, vec2 position);
bool UI_UpdateCursorRadius(UI_t *UI, uint32_t ID, float radius);
bool UI_UpdateCursorColor(UI_t *UI, uint32_t ID, vec3 color);

uint32_t UI_AddWindow(UI_t *UI, vec2 position, vec2 size, vec3 color, const char *titleText);
bool UI_UpdateWindow(UI_t *UI, uint32_t ID, vec2 position, vec2 size, vec3 color, const char *titleText);
bool UI_UpdateWindowPosition(UI_t *UI, uint32_t ID, vec2 position);
bool UI_UpdateWindowSize(UI_t *UI, uint32_t ID, vec2 size);
bool UI_UpdateWindowColor(UI_t *UI, uint32_t ID, vec3 color);
bool UI_UpdateWindowTitleText(UI_t *UI, uint32_t ID, const char *titleText);
bool UI_WindowAddControl(UI_t *UI, uint32_t ID, uint32_t childID);

uint32_t UI_AddText(UI_t *UI, vec2 position, float size, vec3 color, const char *titleText);
bool UI_UpdateText(UI_t *UI, uint32_t ID, vec2 position, float size, vec3 color, const char *titleText);
bool UI_UpdateTextPosition(UI_t *UI, uint32_t ID, vec2 position);
bool UI_UpdateTextSize(UI_t *UI, uint32_t ID, float size);
bool UI_UpdateTextColor(UI_t *UI, uint32_t ID, vec3 color);
bool UI_UpdateTextTitleText(UI_t *UI, uint32_t ID, const char *titleText);
bool UI_UpdateTextTitleTextf(UI_t *UI, uint32_t ID, const char *titleText, ...);

uint32_t UI_TestHit(UI_t *UI, vec2 position);
bool UI_ProcessControl(UI_t *UI, uint32_t ID, vec2 position);
bool UI_Draw(UI_t *UI, uint32_t index, uint32_t eye, float dt);

#endif
