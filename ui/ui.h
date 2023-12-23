#ifndef __UI_H__
#define __UI_H__

#include <stdint.h>
#include <stdbool.h>
#include "../utils/list.h"

// Does the callback really need args? (userdata?)
typedef void (*UIControlCallback)(void *arg);

#define UI_CONTROL_TITLETEXT_MAX 128

#define UI_HASHTABLE_MAX 8192

typedef enum
{
	UI_CONTROL_BUTTON=0,
	UI_CONTROL_CHECKBOX,
	UI_CONTROL_BARGRAPH,
	UI_CONTROL_SPRITE,
	UI_CONTROL_CURSOR,
	UI_NUM_CONTROLTYPE
} UI_ControlType;

typedef struct
{
	// Common to all controls
	UI_ControlType Type;
	uint32_t ID;
	vec2 Position;
	vec3 Color;

	// Specific to type
	union
	{
		// Button type
		struct
		{
			char TitleText[UI_CONTROL_TITLETEXT_MAX];
			vec2 Size;
			UIControlCallback Callback;
		} Button;

		// CheckBox type, should this also have a callback for flexibility?
		struct
		{
			char TitleText[UI_CONTROL_TITLETEXT_MAX];
			float Radius;
			bool Value;
		} CheckBox;

		// BarGraph type
		struct
		{
			char TitleText[UI_CONTROL_TITLETEXT_MAX];
			vec2 Size;
			bool Readonly;
			float Min, Max, Value;
		} BarGraph;

		// Sprite type
		struct
		{
			VkuImage_t *Image;
			vec2 Size;
			float Rotation;
		} Sprite;

		// Cursur type
		struct
		{
			float Radius;
		} Cursor;
	};
} UI_Control_t;

typedef struct
{
	// Position and size of whole UI system
	vec2 Position, Size;

	// Unique Vulkan data
	VkPipelineLayout PipelineLayout;
	VkuPipeline_t Pipeline;

	VkuImage_t BlankImage;

	VkuBuffer_t VertexBuffer;

	VkuBuffer_t InstanceBuffer;
	void *InstanceBufferPtr;

	VkuDescriptorSet_t DescriptorSet;

	// Base ID for generating IDs
	uint32_t IDBase;

	// List of controls in UI
	List_t Controls;

	// Hashtable for quick lookup by ID
	UI_Control_t *Controls_Hashtable[UI_HASHTABLE_MAX];
} UI_t;

bool UI_Init(UI_t *UI, vec2 Position, vec2 Size);
void UI_Destroy(UI_t *UI);

UI_Control_t *UI_FindControlByID(UI_t *UI, uint32_t ID);

// Buttons
uint32_t UI_AddButton(UI_t *UI, vec2 Position, vec2 Size, vec3 Color, const char *TitleText, UIControlCallback Callback);

bool UI_UpdateButton(UI_t *UI, uint32_t ID, vec2 Position, vec2 Size, vec3 Color, const char *TitleText, UIControlCallback Callback);
bool UI_UpdateButtonPosition(UI_t *UI, uint32_t ID, vec2 Position);
bool UI_UpdateButtonSize(UI_t *UI, uint32_t ID, vec2 Size);
bool UI_UpdateButtonColor(UI_t *UI, uint32_t ID, vec3 Color);
bool UI_UpdateButtonTitleText(UI_t *UI, uint32_t ID, const char *TitleText);
bool UI_UpdateButtonCallback(UI_t *UI, uint32_t ID, UIControlCallback Callback);

// Check boxes
uint32_t UI_AddCheckBox(UI_t *UI, vec2 Position, float Radius, vec3 Color, const char *TitleText, bool Value);

bool UI_UpdateCheckBox(UI_t *UI, uint32_t ID, vec2 Position, float Radius, vec3 Color, const char *TitleText, bool Value);
bool UI_UpdateCheckBoxPosition(UI_t *UI, uint32_t ID, vec2 Position);
bool UI_UpdateCheckBoxRadius(UI_t *UI, uint32_t ID, float Radius);
bool UI_UpdateCheckBoxColor(UI_t *UI, uint32_t ID, vec3 Color);
bool UI_UpdateCheckBoxTitleText(UI_t *UI, uint32_t ID, const char *TitleText);
bool UI_UpdateCheckBoxValue(UI_t *UI, uint32_t ID, bool Value);

bool UI_GetCheckBoxValue(UI_t *UI, uint32_t ID);

// Bar graphs
uint32_t UI_AddBarGraph(UI_t *UI, vec2 Position, vec2 Size, vec3 Color, const char *TitleText, bool Readonly, float Min, float Max, float Value);

bool UI_UpdateBarGraph(UI_t *UI, uint32_t ID, vec2 Position, vec2 Size, vec3 Color, const char *TitleText, bool Readonly, float Min, float Max, float Value);
bool UI_UpdateBarGraphPosition(UI_t *UI, uint32_t ID, vec2 Position);
bool UI_UpdateBarGraphSize(UI_t *UI, uint32_t ID, vec2 Size);
bool UI_UpdateBarGraphColor(UI_t *UI, uint32_t ID, vec3 Color);
bool UI_UpdateBarGraphTitleText(UI_t *UI, uint32_t ID, const char *TitleText);
bool UI_UpdateBarGraphReadonly(UI_t *UI, uint32_t ID, bool Readonly);
bool UI_UpdateBarGraphMin(UI_t *UI, uint32_t ID, float Min);
bool UI_UpdateBarGraphMax(UI_t *UI, uint32_t ID, float Max);
bool UI_UpdateBarGraphValue(UI_t *UI, uint32_t ID, float Value);

float UI_GetBarGraphMin(UI_t *UI, uint32_t ID);
float UI_GetBarGraphMax(UI_t *UI, uint32_t ID);
float UI_GetBarGraphValue(UI_t *UI, uint32_t ID);
float *UI_GetBarGraphValuePointer(UI_t *UI, uint32_t ID);

uint32_t UI_AddSprite(UI_t *UI, vec2 Position, vec2 Size, vec3 Color, VkuImage_t *Image, float Rotation);
bool UI_UpdateSprite(UI_t *UI, uint32_t ID, vec2 Position, vec2 Size, vec3 Color, VkuImage_t *Image, float Rotation);
bool UI_UpdateSpritePosition(UI_t *UI, uint32_t ID, vec2 Position);
bool UI_UpdateSpriteSize(UI_t *UI, uint32_t ID, vec2 Size);
bool UI_UpdateSpriteColor(UI_t *UI, uint32_t ID, vec3 Color);
bool UI_UpdateSpriteImage(UI_t *UI, uint32_t ID, VkuImage_t *Image);
bool UI_UpdateSpriteRotation(UI_t *UI, uint32_t ID, float Rotation);

uint32_t UI_AddCursor(UI_t *UI, vec2 Position, float Radius, vec3 Color);
bool UI_UpdateCursor(UI_t *UI, uint32_t ID, vec2 Position, float Radius, vec3 Color);
bool UI_UpdateCursorPosition(UI_t *UI, uint32_t ID, vec2 Position);
bool UI_UpdateCursorRadius(UI_t *UI, uint32_t ID, float Radius);
bool UI_UpdateCursorColor(UI_t *UI, uint32_t ID, vec3 Color);

uint32_t UI_TestHit(UI_t *UI, vec2 Position);
bool UI_ProcessControl(UI_t *UI, uint32_t ID, vec2 Position);
bool UI_Draw(UI_t *UI, uint32_t Index, uint32_t Eye);

#endif
