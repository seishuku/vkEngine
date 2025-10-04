#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <sys/time.h>
#include <time.h>
#include "android_native_app_glue.h"
#include "../../system/system.h"
#include "../../system/memzone.h"
#include "../../vulkan/vulkan.h"
#include "../../math/math.h"
#include "../../vr/vr.h"
#include "../../utils/config.h"
#include "../../utils/event.h"

MemZone_t *zone;

bool isDone=false;

extern XruContext_t xrContext;

extern VkInstance vkInstance;
extern VkuContext_t vkContext;

extern VkuSwapchain_t swapchain;

extern uint32_t renderWidth, renderHeight;

float fps=0.0f, fTimeStep=0.0f, fTime=0.0;

static const uint32_t scale=2;

struct
{
	bool running;
	struct android_app *app;
} appState;

typedef struct
{
    bool active;
    int32_t pointerId;
    vec2 start, delta;
} VirtualStick_t;

static VirtualStick_t leftStick={ 0 }, rightStick={ 0 };

static void UpdateStick(VirtualStick_t *stick, bool active, int32_t pointerId, vec2 pos, bool start)
{
	if(start)
	{
		stick->active=true;
		stick->pointerId=pointerId;
		stick->start=pos;
		stick->delta=Vec2b(0.0f);
	}
	else if(active&&stick->active&&stick->pointerId==pointerId)
	{
		stick->delta=Vec2_Subv(pos, stick->start);
	}
}

static void ReleaseStick(VirtualStick_t *stick, int pointerId)
{
	if(stick->active&&stick->pointerId==pointerId)
	{
		stick->active=false;
		stick->delta=Vec2b(0.0f);
	}
}

static vec2 ApplyScaleDeadzone(int32_t x, int32_t y, int32_t maxX, int32_t maxY, float deadzoneThreshold)
{
	vec2 d={ ((float)x/maxX), 1.0f-((float)y/maxY) };

	if(fabsf(d.x)<deadzoneThreshold)
		d.x=0.0f;
	if(fabsf(d.y)<deadzoneThreshold)
		d.y=0.0f;

	return d;
}

extern vec2 leftThumbstick, rightThumbstick;

void Render(void);
bool Init(void);
void RecreateSwapchain(void);
void Destroy(void);

double GetClock(void)
{
	struct timespec ts;

	if(!clock_gettime(CLOCK_MONOTONIC, &ts))
		return ts.tv_sec+(double)ts.tv_nsec/1000000000.0;

	return 0.0;
}

static int32_t app_handle_input(struct android_app *app, AInputEvent *event)
{
	static MouseEvent_t MouseEvent={ 0, 0, 0, 0 };

	switch(AInputEvent_getType(event))
	{
		case AINPUT_EVENT_TYPE_MOTION:
		{
			int32_t action=AMotionEvent_getAction(event);
			size_t pointerIndex=(action&AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)>>AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
			int32_t pointerId=AMotionEvent_getPointerId(event, pointerIndex);

			float x=AMotionEvent_getX(event, pointerIndex);
			float y=AMotionEvent_getY(event, pointerIndex);

			MouseEvent.dx=x;
			MouseEvent.dy=y;
			MouseEvent.button=MOUSE_TOUCH;
			Event_Trigger(EVENT_MOUSEDOWN, &MouseEvent);
			Event_Trigger(EVENT_MOUSEUP, &MouseEvent);

			switch(action&AMOTION_EVENT_ACTION_MASK)
			{
				case AMOTION_EVENT_ACTION_DOWN:
				case AMOTION_EVENT_ACTION_POINTER_DOWN:
				{
					vec2 pos=ApplyScaleDeadzone(x, y, config.windowWidth/4, config.windowHeight/4, 0.1);

					if(x<(float)config.windowWidth/2.0f)
						UpdateStick(&leftStick, true, pointerId, pos, true);
					else
						UpdateStick(&rightStick, true, pointerId, pos, true);
					break;
				}

				case AMOTION_EVENT_ACTION_MOVE:
				{
					size_t count=AMotionEvent_getPointerCount(event);

					for(size_t i=0;i<count;i++)
					{
						int32_t pid=AMotionEvent_getPointerId(event, i);
						float px=AMotionEvent_getX(event, i);
						float py=AMotionEvent_getY(event, i);
						vec2 pos=ApplyScaleDeadzone(px, py, config.windowWidth/4, config.windowHeight/4, 0.1);

						UpdateStick(&leftStick, true, pid, pos, false);
						UpdateStick(&rightStick, true, pid, pos, false);
					}
					break;
				}

				case AMOTION_EVENT_ACTION_UP:
				case AMOTION_EVENT_ACTION_POINTER_UP:
				case AMOTION_EVENT_ACTION_CANCEL:
					ReleaseStick(&leftStick, pointerId);
					ReleaseStick(&rightStick, pointerId);
					break;
			}

			break;
		}

		case AINPUT_EVENT_TYPE_KEY:
		{
			uint32_t code=0, KeyCode=AKeyEvent_getKeyCode(event);

			switch(KeyCode)
			{
				case AKEYCODE_DEL:				code=KB_BACKSPACE;				break;	// Backspace
				case AKEYCODE_TAB:				code=KB_TAB;					break;	// Tab
				case AKEYCODE_ENTER:			code=KB_ENTER;					break;	// Enter
				case AKEYCODE_BREAK:			code=KB_PAUSE;					break;	// Pause
				case AKEYCODE_CAPS_LOCK:		code=KB_CAPS_LOCK;				break;	// Caps Lock
				case AKEYCODE_ESCAPE:			code=KB_ESCAPE;					break;	// Esc
				case AKEYCODE_PAGE_UP:			code=KB_PAGE_UP;				break;	// Page Up
				case AKEYCODE_PAGE_DOWN:		code=KB_PAGE_DOWN;				break;	// Page Down
				case AKEYCODE_MOVE_END:			code=KB_END;					break;	// End
				case AKEYCODE_MOVE_HOME:		code=KB_HOME;					break;	// Home
				case AKEYCODE_DPAD_LEFT:		code=KB_LEFT;					break;	// Left
				case AKEYCODE_DPAD_UP:			code=KB_UP;						break;	// Up
				case AKEYCODE_DPAD_RIGHT:		code=KB_RIGHT;					break;	// Right
				case AKEYCODE_DPAD_DOWN:		code=KB_DOWN;					break;	// Down
				case AKEYCODE_SYSRQ:			code=KB_PRINT_SCREEN;			break;	// Prnt Scrn
				case AKEYCODE_INSERT:			code=KB_INSERT;					break;	// Insert
				case AKEYCODE_FORWARD_DEL:		code=KB_DEL;					break;	// Delete
				case AKEYCODE_META_LEFT:		code=KB_LSUPER;					break;	// Left Windows?
				case AKEYCODE_META_RIGHT:		code=KB_RSUPER;					break;	// Right Windows?
				case AKEYCODE_MENU:				code=KB_MENU;					break;	// Application
				case AKEYCODE_NUMPAD_0:			code=KB_NP_0;					break;	// Num 0
				case AKEYCODE_NUMPAD_1:			code=KB_NP_1;					break;	// Num 1
				case AKEYCODE_NUMPAD_2:			code=KB_NP_2;					break;	// Num 2
				case AKEYCODE_NUMPAD_3:			code=KB_NP_3;					break;	// Num 3
				case AKEYCODE_NUMPAD_4:			code=KB_NP_4;					break;	// Num 4
				case AKEYCODE_NUMPAD_5:			code=KB_NP_5;					break;	// Num 5
				case AKEYCODE_NUMPAD_6:			code=KB_NP_6;					break;	// Num 6
				case AKEYCODE_NUMPAD_7:			code=KB_NP_7;					break;	// Num 7
				case AKEYCODE_NUMPAD_8:			code=KB_NP_8;					break;	// Num 8
				case AKEYCODE_NUMPAD_9:			code=KB_NP_9;					break;	// Num 9
				case AKEYCODE_NUMPAD_MULTIPLY:	code=KB_NP_MULTIPLY;			break;	// Num *
				case AKEYCODE_NUMPAD_ADD:		code=KB_NP_ADD;					break;	// Num +
				case AKEYCODE_NUMPAD_SUBTRACT:	code=KB_NP_SUBTRACT;			break;	// Num -
				case AKEYCODE_NUMPAD_DOT:		code=KB_NP_DECIMAL;				break;	// Num Del
				case AKEYCODE_NUMPAD_DIVIDE:	code=KB_NP_DIVIDE;				break;	// Num /
				case AKEYCODE_F1:				code=KB_F1;						break;	// F1
				case AKEYCODE_F2:				code=KB_F2;						break;	// F2
				case AKEYCODE_F3:				code=KB_F3;						break;	// F3
				case AKEYCODE_F4:				code=KB_F4;						break;	// F4
				case AKEYCODE_F5:				code=KB_F5;						break;	// F5
				case AKEYCODE_F6:				code=KB_F6;						break;	// F6
				case AKEYCODE_F7:				code=KB_F7;						break;	// F7
				case AKEYCODE_F8:				code=KB_F8;						break;	// F8
				case AKEYCODE_F9:				code=KB_F9;						break;	// F9
				case AKEYCODE_F10:				code=KB_F10;					break;	// F10
				case AKEYCODE_F11:				code=KB_F11;					break;	// F11
				case AKEYCODE_F12:				code=KB_F12;					break;	// F12
				case AKEYCODE_NUM_LOCK:			code=KB_NUM_LOCK;				break;	// Num Lock
				case AKEYCODE_SCROLL_LOCK:		code=KB_SCROLL_LOCK;			break;	// Scroll Lock
				case AKEYCODE_SHIFT_LEFT:		code=KB_LSHIFT;					break;	// Shift
				case AKEYCODE_SHIFT_RIGHT:		code=KB_RSHIFT;					break;	// Right Shift
				case AKEYCODE_CTRL_LEFT:		code=KB_LCTRL;					break;	// Left control
				case AKEYCODE_CTRL_RIGHT:		code=KB_RCTRL;					break;	// Right control
				case AKEYCODE_ALT_LEFT:			code=KB_LALT;					break;	// Left alt
				case AKEYCODE_ALT_RIGHT:		code=KB_RALT;					break;	// Left alt
				default:						code=KeyCode-AKEYCODE_A+'A';	break;	// All others
			}

			switch(AKeyEvent_getAction(event))
			{
				case AKEY_EVENT_ACTION_DOWN:
					Event_Trigger(EVENT_KEYDOWN, &code);
					break;

				case AKEY_EVENT_ACTION_UP:
					Event_Trigger(EVENT_KEYUP, &code);
					break;
			}
			break;
		}

		default:
			break;
	}

	return 0;
}

static void app_handle_cmd(struct android_app *app, int32_t cmd)
{
	switch(cmd)
	{
		case APP_CMD_INIT_WINDOW:
			config.windowWidth=ANativeWindow_getWidth(app->window)/scale;
			config.windowHeight=ANativeWindow_getHeight(app->window)/scale;
			ANativeWindow_setBuffersGeometry(app->window, config.windowWidth, config.windowHeight, AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM);

			vkContext.window=app->window;

			DBGPRINTF(DEBUG_INFO, "Allocating zone memory (%dMiB)...\n", MEMZONE_SIZE/1024/1024);
			zone=Zone_Init(MEMZONE_SIZE);

			if(zone==NULL)
			{
				DBGPRINTF(DEBUG_ERROR, "\t...zone allocation failed!\n");
				appState.app->destroyRequested=true;
				ANativeActivity_finish(app->activity);
				return;
			}

			if(!Config_ReadINI(&config, "config.ini"))
			{
				DBGPRINTF(DEBUG_ERROR, "Unable to read config.ini.\n");
				return;
			}

			vkContext.deviceIndex=0;

			DBGPRINTF(DEBUG_INFO, "Creating Vulkan instance...\n");
			if(!vkuCreateInstance(&vkInstance))
			{
				DBGPRINTF(DEBUG_ERROR, "\t...create Vulkan instance failed.\n");
				appState.app->destroyRequested=true;
				ANativeActivity_finish(app->activity);
				return;
			}

			DBGPRINTF(DEBUG_INFO, "Creating Vulkan context...\n");
			if(!vkuCreateContext(vkInstance, &vkContext))
			{
				DBGPRINTF(DEBUG_ERROR, "\t...create Vulkan context failed.\n");
				appState.app->destroyRequested=true;
				ANativeActivity_finish(app->activity);
				return;
			}

			DBGPRINTF(DEBUG_INFO, "Creating swapchain...\n");
			if(!vkuCreateSwapchain(&vkContext, &swapchain, VK_TRUE))
			{
				DBGPRINTF(DEBUG_ERROR, "\t...create swapchain failed.\n");
				appState.app->destroyRequested=true;
				ANativeActivity_finish(app->activity);
				return;
			}
			else
			{
				config.renderWidth=swapchain.extent.width;
				config.renderHeight=swapchain.extent.height;
			}

			DBGPRINTF(DEBUG_INFO, "Initializing VR...\n");
			if(!VR_Init(&xrContext, vkInstance, &vkContext))
			{
				DBGPRINTF(DEBUG_ERROR, "\t...failed, turning off VR support.\n");
				config.isVR=false;
			}
			else
			{
				config.renderWidth=xrContext.swapchainExtent.width;
				config.renderHeight=xrContext.swapchainExtent.height;
				config.isVR=true;
			}

			DBGPRINTF(DEBUG_INFO, "Init...\n");
			if(!Init())
			{
				DBGPRINTF(DEBUG_ERROR, "\t...init failed.\n");
				appState.app->destroyRequested=true;
				ANativeActivity_finish(app->activity);
				return;
			}

			DBGPRINTF(DEBUG_WARNING, "\nCurrent system zone memory allocations:\n");
			Zone_Print(zone);

			DBGPRINTF(DEBUG_INFO, "\nStarting main loop.\n");
			appState.running=true;
			break;

		case APP_CMD_SAVE_STATE:
			appState.app->destroyRequested=true;
			ANativeActivity_finish(app->activity);
			break;

		case APP_CMD_TERM_WINDOW:
			appState.app->destroyRequested=true;
			appState.running=false;

			DBGPRINTF(DEBUG_INFO, "Shutting down...\n");
			Destroy();
			vkuDestroyContext(vkInstance, &vkContext);
			vkuDestroyInstance(vkInstance);

			DBGPRINTF(DEBUG_WARNING, "Zone remaining block list:\n");
			Zone_Print(zone);
			Zone_Destroy(zone);
			break;

		case APP_CMD_WINDOW_RESIZED:
		case APP_CMD_CONFIG_CHANGED:
			RecreateSwapchain();
			break;

		case APP_CMD_LOST_FOCUS:
			appState.running=false;
			break;

		case APP_CMD_GAINED_FOCUS:
			appState.running=true;
			break;
	}
}

extern AAssetManager *android_asset_manager;

void android_main(struct android_app *app)
{
	memset(&appState, 0, sizeof(appState));

	app->userData=&appState;
	app->onAppCmd=app_handle_cmd;
	app->onInputEvent=app_handle_input;

	appState.app=app;

	android_asset_manager=app->activity->assetManager;

	PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR=XR_NULL_HANDLE;
	xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction *)&xrInitializeLoaderKHR);

	if(xrInitializeLoaderKHR!=NULL)
	{
		XrLoaderInitInfoAndroidKHR loaderInitializeInfoAndroid=
		{
			.type=XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR,
			.applicationVM=app->activity->vm,
			.applicationContext=app->activity->clazz
		};
		xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR *)&loaderInitializeInfoAndroid);
	}

	while(1)
	{
		int ident, events;
		struct android_poll_source *source;

		while((ident=ALooper_pollOnce(appState.running?0:-1, NULL, &events, (void **)&source))>=0)
		{
			// Process this event.
			if(source!=NULL)
				source->process(app, source);

			// Check if we are exiting.
			if(app->destroyRequested!=0)
				return;
		}

		if(isDone)
			ANativeActivity_finish(appState.app->activity);

		if(appState.running)
		{
			static float avgfps=0.0f;

			double StartTime=GetClock();

			leftThumbstick=leftStick.delta;
			rightThumbstick=rightStick.delta;

			Render();

			fTimeStep=(float)(GetClock()-StartTime);
			fTime+=fTimeStep;

			avgfps+=1.0f/fTimeStep;

 			static uint32_t Frames=0;
			if(Frames++>100)
			{
				fps=avgfps/Frames;
				avgfps=0.0f;
				Frames=0;
			}
		}
	}
}
