#include "input.h"
#include <string.h>

extern XruContext_t xrContext;

static vec3 lastLeftHandPosition={ 0 };
static bool leftTriggerWasPressed=false;

// Forward decl for VR input
void Input_UpdateVR(void);
void Input_ProcessVRInteraction(void);

// Global input state
static Input_t input_state={ 0 };

// Get pointer to global input state
Input_t *Input_GetState(void)
{
	return &input_state;
}

// Update is called once per frame from the main loop
void Input_Update(void)
{
	// Clear per-frame deltas
	input_state.mouseDelta=Vec2b(0.0f);
	input_state.mouseWheel=0.0f;

	// Platform-specific update (gamepad polling, etc.)
	Input_Platform_Update();

	// VR input update
	// Check if VR is initialized
	if(!xrContext.sessionRunning)
	{
		input_state.vr.connected=false;
		return;
	}

	input_state.vr.connected=true;

	// Poll left hand
	XrPosef leftHandPose=VR_GetActionPose(&xrContext, xrContext.handPose, xrContext.leftHandSpace, 0);
	input_state.vr.hand[0].orientation=Vec4(leftHandPose.orientation.x, leftHandPose.orientation.y, leftHandPose.orientation.z, leftHandPose.orientation.w);
	input_state.vr.hand[0].position=Vec3(leftHandPose.position.x, leftHandPose.position.y, leftHandPose.position.z);
	input_state.vr.hand[0].trigger=VR_GetActionFloat(&xrContext, xrContext.handTrigger, 0);
	input_state.vr.hand[0].grip=VR_GetActionFloat(&xrContext, xrContext.handGrip, 0);
	input_state.vr.hand[0].thumbstick=VR_GetActionVec2(&xrContext, xrContext.handThumbstick, 0);

	// Poll right hand
	XrPosef rightHandPose=VR_GetActionPose(&xrContext, xrContext.handPose, xrContext.rightHandSpace, 1);
	input_state.vr.hand[1].orientation=Vec4(rightHandPose.orientation.x, rightHandPose.orientation.y, rightHandPose.orientation.z, rightHandPose.orientation.w);
	input_state.vr.hand[1].position=Vec3(rightHandPose.position.x, rightHandPose.position.y, rightHandPose.position.z);
	input_state.vr.hand[1].trigger=VR_GetActionFloat(&xrContext, xrContext.handTrigger, 1);
	input_state.vr.hand[1].grip=VR_GetActionFloat(&xrContext, xrContext.handGrip, 1);
	input_state.vr.hand[1].thumbstick=VR_GetActionVec2(&xrContext, xrContext.handThumbstick, 1);

	if(!input_state.vr.connected)
		return;

	// Use left hand trigger for mouse interaction
	float leftTrigger=input_state.vr.hand[0].trigger;
	vec3 leftHandPos=input_state.vr.hand[0].position;

	if (leftTrigger>0.1f)
	{
		vec3 deltaPos=Vec3_Subv(leftHandPos, lastLeftHandPosition);

		// Scale to mouse delta (approximate scaling)
		MouseEvent_t mouseEvent={
			.dx=(int32_t)(deltaPos.x*100.0f),
			.dy=(int32_t)(deltaPos.y*100.0f),
			.dz=0
		};

		// Trigger mouse move event
		Event_Trigger(EVENT_MOUSEMOVE, &mouseEvent);

		// Trigger click when trigger fully pressed
		if(leftTrigger>0.75f&&!leftTriggerWasPressed)
		{
			leftTriggerWasPressed=true;
			mouseEvent.button=MOUSE_BUTTON_LEFT;

			Event_Trigger(EVENT_MOUSEDOWN, &mouseEvent);
		}
		else if(leftTrigger<0.5f&&leftTriggerWasPressed)
		{
			leftTriggerWasPressed=false;
			mouseEvent.button=MOUSE_BUTTON_LEFT;

			Event_Trigger(EVENT_MOUSEUP, &mouseEvent);
		}
	}
	else
	{
		if(leftTriggerWasPressed)
		{
			leftTriggerWasPressed=false;
			MouseEvent_t mouseEvent={ .button=MOUSE_BUTTON_LEFT };

			Event_Trigger(EVENT_MOUSEUP, &mouseEvent);
		}

		lastLeftHandPosition=leftHandPos;
	}
}

// Keyboard query API
bool Input_IsKeyPressed(Keycodes_t keycode)
{
	if(keycode<0||keycode>=512)
		return false;

	return input_state.keys[keycode];
}

// Mouse query API
bool Input_IsMouseButtonPressed(Mousecodes_t button)
{
	return (input_state.mouseButtons&button)!=0;
}

vec2 Input_GetMouseDelta(void)
{
	return input_state.mouseDelta;
}

vec2 Input_GetMousePos(void)
{
	return input_state.mousePos;
}

float Input_GetMouseWheel(void)
{
	return input_state.mouseWheel;
}

// Gamepad query API
bool Input_IsGamepadConnected(void)
{
	return input_state.gamepad.connected;
}

bool Input_IsGamepadButtonPressed(GamepadButton_t button)
{
	if(button>=GAMEPAD_BUTTON_MAX)
		return false;

	return input_state.gamepad.buttons[button];
}

vec2 Input_GetGamepadStick(uint32_t stick)
{
	if(stick==0)
		return input_state.gamepad.leftStick;
	else if(stick==1)
		return input_state.gamepad.rightStick;

	return Vec2b(0.0f);
}

float Input_GetGamepadTrigger(uint32_t trigger)
{
	if(trigger==0)
		return input_state.gamepad.leftTrigger;
	else if(trigger==1)
		return input_state.gamepad.rightTrigger;

	return 0.0f;
}

// VR query API
bool Input_IsVRConnected(void)
{
	return input_state.vr.connected;
}

vec4 Input_GetVRHandOrientation(uint32_t hand)
{
	if(hand<2)
		return input_state.vr.hand[hand].orientation;

	return Vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

vec3 Input_GetVRHandPosition(uint32_t hand)
{
	if(hand<2)
		return input_state.vr.hand[hand].position;

	return Vec3b(0.0f);
}

float Input_GetVRTrigger(uint32_t hand)
{
	if(hand<2)
		return input_state.vr.hand[hand].trigger;

	return 0.0f;
}

float Input_GetVRGrip(uint32_t hand)
{
	if(hand<2)
		return input_state.vr.hand[hand].grip;

	return 0.0f;
}

vec2 Input_GetVRThumbstick(uint32_t hand)
{
	if(hand<2)
		return input_state.vr.hand[hand].thumbstick;

	return Vec2b(0.0f);
}

// Event callbacks - called by platform layers
void Input_OnKeyEvent(Keycodes_t keycode, bool pressed)
{
	if(keycode>=0&&keycode<512)
		input_state.keys[keycode]=pressed;

	// Send to event system
	Event_Trigger(pressed?EVENT_KEYDOWN:EVENT_KEYUP, &keycode);
}

void Input_OnMouseButtonEvent(Mousecodes_t button, bool pressed)
{
	if(pressed)
		input_state.mouseButtons|=button;
	else
		input_state.mouseButtons&=~button;

	// Send to event system with accumulated button state (for UI compatibility)
	MouseEvent_t event={ .button=input_state.mouseButtons };
	Event_Trigger(pressed?EVENT_MOUSEDOWN:EVENT_MOUSEUP, &event);
}

void Input_OnMouseEvent(const MouseEvent_t *event, vec2 mousePos)
{
	if(event==NULL)
		return;

	input_state.mousePos=mousePos;
	input_state.mouseDelta=Vec2(event->dx, event->dy);
	input_state.mouseWheel=(float)event->dz;

	// Send to event system
	MouseEvent_t mouseEvent=*event; // Make a local copy to pass
	Event_Trigger(EVENT_MOUSEMOVE, &mouseEvent);
}
