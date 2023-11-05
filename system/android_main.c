#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <sys/time.h>
#include <time.h>
#include "android_native_app_glue.h"
#include "../system/system.h"
#include "../system/memzone.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../vr/vr.h"
#include "../utils/event.h"
#include "../utils/input.h"

MemZone_t *Zone;

bool Done=false;
bool IsVR=false;

extern VkInstance Instance;
extern VkuContext_t Context;

extern VkuMemZone_t *VkZone;

extern VkuSwapchain_t Swapchain;

extern uint32_t Width, Height;

float fps=0.0f, fTimeStep, fTime=0.0;

struct
{
	int32_t initialFormat;
	bool running;
	float x, y, z;
	struct android_app *app;
} appState;

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
			switch(AInputEvent_getSource(event))
			{
				case AINPUT_SOURCE_TOUCHSCREEN:
					switch(AKeyEvent_getAction(event)&AMOTION_EVENT_ACTION_MASK)
					{
						case AMOTION_EVENT_ACTION_DOWN:
							MouseEvent.button|=MOUSE_BUTTON_LEFT;
							Event_Trigger(EVENT_MOUSEDOWN, &MouseEvent);
							break;

						case AMOTION_EVENT_ACTION_UP:
							MouseEvent.button&=~MOUSE_BUTTON_LEFT;
							Event_Trigger(EVENT_MOUSEUP, &MouseEvent);
							break;

						case AMOTION_EVENT_ACTION_MOVE:
							MouseEvent.dx=AMotionEvent_getX(event, 0)-AMotionEvent_getHistoricalX(event, 0, 0);
							MouseEvent.dy=AMotionEvent_getHistoricalY(event, 0, 0)-AMotionEvent_getY(event, 0);
							Event_Trigger(EVENT_MOUSEMOVE, &MouseEvent);
							break;
					}
					break;
			}
		break;

		case AINPUT_EVENT_TYPE_KEY:
			DBGPRINTF(DEBUG_INFO, "Key event: action=%d keyCode=%d metaState=0x%x", AKeyEvent_getAction(event), AKeyEvent_getKeyCode(event), AKeyEvent_getMetaState(event));
			break;

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
			appState.initialFormat=ANativeWindow_getFormat(app->window);

			Width=ANativeWindow_getWidth(app->window);
			Height=ANativeWindow_getHeight(app->window);

			//ANativeWindow_setBuffersGeometry(app->window, Width, Height, AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM);

			Context.Win=app->window;

			DBGPRINTF(DEBUG_INFO, "Allocating zone memory...\n");
			Zone=Zone_Init(256*1000*1000);

			rtWidth=Width;
			rtHeight=Height;

			if(Zone==NULL)
			{
				DBGPRINTF(DEBUG_ERROR, "\t...zone allocation failed!\n");
				appState.app->destroyRequested=true;
				ANativeActivity_finish(app->activity);
				return;
			}

			DBGPRINTF(DEBUG_INFO, "Creating Vulkan instance...\n");
			if(!CreateVulkanInstance(&Instance))
			{
				DBGPRINTF(DEBUG_ERROR, "\t...create Vulkan instance failed.\n");
				appState.app->destroyRequested=true;
				ANativeActivity_finish(app->activity);
				return;
			}

			DBGPRINTF(DEBUG_INFO, "Creating Vulkan context...\n");
			if(!CreateVulkanContext(Instance, &Context))
			{
				DBGPRINTF(DEBUG_ERROR, "\t...create Vulkan context failed.\n");
				appState.app->destroyRequested=true;
				ANativeActivity_finish(app->activity);
				return;
			}

			DBGPRINTF(DEBUG_INFO, "Creating swapchain...\n");
			if(!vkuCreateSwapchain(&Context, &Swapchain, VK_TRUE))
			{
				DBGPRINTF(DEBUG_ERROR, "\t...create swapchain failed.\n");
				appState.app->destroyRequested=true;
				ANativeActivity_finish(app->activity);
				return;
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
			Zone_Print(Zone);

			DBGPRINTF(DEBUG_WARNING, "\nCurrent vulkan zone memory allocations:\n");
			vkuMem_Print(VkZone);

			DBGPRINTF(DEBUG_INFO, "\nStarting main loop.\n");
			appState.running=true;
			break;

		case APP_CMD_TERM_WINDOW:
			appState.app->destroyRequested=true;
			appState.running=false;

			DBGPRINTF(DEBUG_INFO, "Shutting down...\n");
			Destroy();
			DestroyVulkan(Instance, &Context);
			vkDestroyInstance(Instance, VK_NULL_HANDLE);

			DBGPRINTF(DEBUG_WARNING, "Zone remaining block list:\n");
			Zone_Print(Zone);
			Zone_Destroy(Zone);

			//ANativeWindow_setBuffersGeometry(app->window, ANativeWindow_getWidth(app->window), ANativeWindow_getHeight(app->window), appState.initialFormat);
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

	while(1)
	{
		int ident, events;
		struct android_poll_source *source;

		while((ident=ALooper_pollAll(appState.running?0:-1, NULL, &events, (void **)&source))>=0)
		{
			// Process this event.
			if(source!=NULL)
				source->process(app, source);

			// Check if we are exiting.
			if(app->destroyRequested!=0)
				return;
		}

		if(Done)
			ANativeActivity_finish(appState.app->activity);

		if(appState.running)
		{
			static float avgfps=0.0f;

			double StartTime=GetClock();
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