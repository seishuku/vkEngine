#include <windows.h>
#include <xinput.h>
#include "../../math/math.h"
#include "../../input/input.h"

// XInput gamepad deadzone constants
#define THUMBSTICK_DEADZONE 0.1f
#define TRIGGER_DEADZONE 0.1f

static void UpdateGamepadState(void)
{
	XINPUT_STATE state;
	ZeroMemory(&state, sizeof(XINPUT_STATE));

	if(XInputGetState(0, &state)==ERROR_SUCCESS)
	{
		Input_t *input=Input_GetState();
		input->gamepad.connected=true;

		XINPUT_GAMEPAD *pad=&state.Gamepad;

		input->gamepad.buttons[GAMEPAD_BUTTON_A]=(pad->wButtons&XINPUT_GAMEPAD_A)!=0;
		input->gamepad.buttons[GAMEPAD_BUTTON_B]=(pad->wButtons&XINPUT_GAMEPAD_B)!=0;
		input->gamepad.buttons[GAMEPAD_BUTTON_X]=(pad->wButtons&XINPUT_GAMEPAD_X)!=0;
		input->gamepad.buttons[GAMEPAD_BUTTON_Y]=(pad->wButtons&XINPUT_GAMEPAD_Y)!=0;
		input->gamepad.buttons[GAMEPAD_BUTTON_LB]=(pad->wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER)!=0;
		input->gamepad.buttons[GAMEPAD_BUTTON_RB]=(pad->wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER)!=0;
		input->gamepad.buttons[GAMEPAD_BUTTON_BACK]=(pad->wButtons&XINPUT_GAMEPAD_BACK)!=0;
		input->gamepad.buttons[GAMEPAD_BUTTON_START]=(pad->wButtons&XINPUT_GAMEPAD_START)!=0;
		input->gamepad.buttons[GAMEPAD_BUTTON_LEFT_STICK]=(pad->wButtons&XINPUT_GAMEPAD_LEFT_THUMB)!=0;
		input->gamepad.buttons[GAMEPAD_BUTTON_RIGHT_STICK]=(pad->wButtons&XINPUT_GAMEPAD_RIGHT_THUMB)!=0;

		vec2 left=Vec2((float)pad->sThumbLX/32768.0f, (float)pad->sThumbLY/32768.0f);
		float len=Vec2_Dot(left, left);

		if(len>THUMBSTICK_DEADZONE*THUMBSTICK_DEADZONE)
			input->gamepad.leftStick=left;
		else
			input->gamepad.leftStick=Vec2b(0.0f);

		vec2 right=Vec2((float)pad->sThumbRX/32768.0f, (float)pad->sThumbRY/32768.0f);
		float rlen=Vec2_Dot(right, right);

		if(rlen>THUMBSTICK_DEADZONE*THUMBSTICK_DEADZONE)
			input->gamepad.rightStick=right;
		else
			input->gamepad.rightStick=Vec2b(0.0f);

		// Update triggers with deadzone
		float lt=(float)pad->bLeftTrigger/255.0f;
		input->gamepad.leftTrigger=(lt>TRIGGER_DEADZONE)?lt:0.0f;

		float rt=(float)pad->bRightTrigger/255.0f;
		input->gamepad.rightTrigger=(rt>TRIGGER_DEADZONE)?rt:0.0f;
	}
	else
	{
		Input_t *input=Input_GetState();
		input->gamepad.connected=false;
		ZeroMemory(&input->gamepad, sizeof(GamepadState_t));
	}
}

void Input_Platform_Update(void)
{
	UpdateGamepadState();
}

void Input_PlatformInit(void)
{
}

void Input_PlatformDestroy(void)
{
}
