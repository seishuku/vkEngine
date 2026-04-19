#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <linux/joystick.h>
#include "../../math/math.h"
#include "../../input/input.h"

#define MAX_JS_DEVICES 4
#define JOYSTICK_PATH "/dev/input/js1"

const float JOYSTICK_DEADZONE=0.1f;

typedef struct
{
	int fd;
	uint16_t buttons;
	int16_t axes[8];  // Up to 8 analog axes
	bool connected;
} JSDevice_t;

static JSDevice_t jsDevice={ 0 };

static void TryOpenJoystick(void)
{
	if(jsDevice.fd>=0)
		return;

	jsDevice.fd=open(JOYSTICK_PATH, O_RDONLY|O_NONBLOCK);

	if(jsDevice.fd>=0)
	{
		memset(&jsDevice, 0, sizeof(JSDevice_t));
		jsDevice.connected=true;
	}
}

static void UpdateGamepadState(void)
{
	Input_t *input=Input_GetState();

	if(jsDevice.fd<0)
	{
		input->gamepad.connected=false;
		return;
	}

	struct js_event event;

	while(read(jsDevice.fd, &event, sizeof(event))==sizeof(event))
	{
		switch(event.type&~JS_EVENT_INIT)
		{
			case JS_EVENT_BUTTON:
				if(event.number<GAMEPAD_BUTTON_MAX)
					input->gamepad.buttons[event.number]= (event.value!=0);
				break;

			case JS_EVENT_AXIS:
				jsDevice.axes[event.number]=event.value;

				if(event.number==0||event.number==1)
				{
					vec2 left=Vec2((float)jsDevice.axes[0]/32767.0f, -(float)jsDevice.axes[1]/32767.0f);
					float len=Vec2_Dot(left, left);

					if(len>JOYSTICK_DEADZONE*JOYSTICK_DEADZONE)
						input->gamepad.leftStick=left;
					else
						input->gamepad.leftStick=Vec2b(0.0f);
				}

				if(event.number==3||event.number==4)
				{
					vec2 right=Vec2((float)jsDevice.axes[3]/32767.0f, -(float)jsDevice.axes[4]/32767.0f);
					float len=Vec2_Dot(right, right);

					if(len>JOYSTICK_DEADZONE*JOYSTICK_DEADZONE)
						input->gamepad.rightStick=right;
					else
						input->gamepad.rightStick=Vec2b(0.0f);
				}

				if(event.number==2)
				{
					float val=(jsDevice.axes[2]+32767.0f)/65534.0f;
					input->gamepad.leftTrigger=(val>JOYSTICK_DEADZONE)?val:0.0f;
				}
				
				if (event.number == 5)
				{
					float val=(jsDevice.axes[5]+32767.0f)/65534.0f;
					input->gamepad.rightTrigger=(val>JOYSTICK_DEADZONE)?val:0.0f;
				}
				break;
		}
	}

	input->gamepad.connected=(jsDevice.fd>=0);
}

void Input_Platform_Update(void)
{
	if(jsDevice.fd<0)
		TryOpenJoystick();
	else if(jsDevice.fd>=0)
		UpdateGamepadState();
}

void Input_PlatformInit(void)
{
	memset(&jsDevice, 0, sizeof(JSDevice_t));
	jsDevice.fd=-1;

	TryOpenJoystick();
}

void Input_PlatformDestroy(void)
{
	if(jsDevice.fd>=0)
	{
		close(jsDevice.fd);
		jsDevice.fd=-1;
	}

	jsDevice.connected=false;
}
