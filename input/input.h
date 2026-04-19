#ifndef __INPUT_H__
#define __INPUT_H__

#include <stdbool.h>
#include <stdint.h>
#include "../math/math.h"
#include "../utils/event.h"
#include "../vr/vr.h"

// Gamepad button mapping
typedef enum
{
	GAMEPAD_BUTTON_A			=0,
	GAMEPAD_BUTTON_B			=1,
	GAMEPAD_BUTTON_X			=2,
	GAMEPAD_BUTTON_Y			=3,
	GAMEPAD_BUTTON_LB			=4,
	GAMEPAD_BUTTON_RB			=5,
	GAMEPAD_BUTTON_BACK			=6,
	GAMEPAD_BUTTON_START		=7,
	GAMEPAD_BUTTON_LEFT_STICK	=8,
	GAMEPAD_BUTTON_RIGHT_STICK	=9,
	GAMEPAD_BUTTON_MAX			=16,
} GamepadButton_t;

// Unified gamepad state
typedef struct
{
	bool connected;
	bool buttons[GAMEPAD_BUTTON_MAX];
	vec2 leftStick, rightStick;
	float leftTrigger, rightTrigger; // 0.0-1.0
} GamepadState_t;

// Unified VR hand state
typedef struct
{
	vec4 orientation; // Quaternion (x, y, z, w)
	vec3 position; // HMD relative position
	float trigger; // 0.0-1.0
	float grip;    // 0.0-1.0
	vec2 thumbstick;
} VRHandState_t;

// Unified VR state
typedef struct
{
	bool connected;
	VRHandState_t hand[2]; // [0]=left, [1]=right
} VRState_t;

// Unified input state - single source of truth for all input
typedef struct
{
	// Keyboard state: bit array of 512 keys
	bool keys[512];

	// Mouse/Touch state
	vec2 mousePos;
	vec2 mouseDelta;
	uint32_t mouseButtons;
	float mouseWheel;

	// Gamepad state
	GamepadState_t gamepad;

	// VR state
	VRState_t vr;
} Input_t;

// Global input state
void Input_Update(void);
Input_t *Input_GetState(void);

// Keyboard
bool Input_IsKeyPressed(Keycodes_t keycode);

// Mouse
bool Input_IsMouseButtonPressed(Mousecodes_t button);
vec2 Input_GetMouseDelta(void);
vec2 Input_GetMousePos(void);
float Input_GetMouseWheel(void);

// Gamepad
bool Input_IsGamepadConnected(void);
bool Input_IsGamepadButtonPressed(GamepadButton_t button);
vec2 Input_GetGamepadStick(uint32_t stick);      // 0=left, 1=right
float Input_GetGamepadTrigger(uint32_t trigger); // 0=left, 1=right

// VR
bool Input_IsVRConnected(void);
vec4 Input_GetVRHandOrientation(uint32_t hand); // 0=left, 1=right
vec3 Input_GetVRHandPosition(uint32_t hand);
float Input_GetVRTrigger(uint32_t hand);
float Input_GetVRGrip(uint32_t hand);
vec2 Input_GetVRThumbstick(uint32_t hand);

// Platform-specific
void Input_PlatformInit(void);
void Input_PlatformDestroy(void);
void Input_Platform_Update(void);

void Input_OnKeyEvent(Keycodes_t keycode, bool pressed);
void Input_OnMouseEvent(const MouseEvent_t *event, vec2 mousePos);
void Input_OnMouseButtonEvent(Mousecodes_t button, bool pressed);

#endif
