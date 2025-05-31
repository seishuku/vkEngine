#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/XKBlib.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/XInput2.h>
#include <sys/time.h>
#include <time.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "../../system/system.h"
#include "../../vulkan/vulkan.h"
#include "../../math/math.h"
#include "../../camera/camera.h"
#include "../../utils/config.h"
#include "../../utils/list.h"
#include "../../utils/event.h"
#include "../../vr/vr.h"

MemZone_t *zone;

char szAppName[]="Vulkan";

static int _xi_opcode=0;

bool isDone=false;
bool toggleFullscreen=true;

extern XruContext_t xrContext;

extern VkInstance vkInstance;
extern VkuContext_t vkContext;

extern VkuMemZone_t vkZone;

extern VkuSwapchain_t swapchain;

float fps=0.0f, fTimeStep=0.0f, fTime=0.0f;

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

void EventLoop(void)
{
	static MouseEvent_t mouseEvent={ 0, 0, 0, 0 };
	uint32_t code;
	XEvent event;

	while(!isDone)
	{
		while(XPending(vkContext.display)>0)
		{
			XNextEvent(vkContext.display, &event);

			switch(event.type)
			{
				case Expose:
					break;

				case ConfigureNotify:
					config.windowWidth=event.xconfigure.width;
					config.windowHeight=event.xconfigure.height;
					break;
			}

			if(XGetEventData(vkContext.display, &event.xcookie)&&(event.xcookie.type==GenericEvent)&&(event.xcookie.extension==_xi_opcode))
			{
				switch(event.xcookie.evtype)
				{
					case XI_RawMotion:
					{
						XIRawEvent *re=(XIRawEvent *)event.xcookie.data;

						if(re->valuators.mask_len)
						{
							const double *values=re->raw_values;

							if(XIMaskIsSet(re->valuators.mask, 0))
								mouseEvent.dx=*values++;

							if(XIMaskIsSet(re->valuators.mask, 1))
								mouseEvent.dy=-*values++;

							Event_Trigger(EVENT_MOUSEMOVE, &mouseEvent);

							XWarpPointer(vkContext.display, None, vkContext.window, 0, 0, 0, 0, config.windowWidth/2, config.windowHeight/2);
							XFlush(vkContext.display);
						}
						break;
					}

					//--------------------------------------------------------------
					// X11 Mouse Button Codes
					// 1: left
					// 2: middle
					// 3: right
					// 4: wheel up
					// 5: wheel down
					// 6: wheel left
					// 7: wheel right
					// 8: back
					// 9: forward
					case XI_ButtonPress:
					{
						XIDeviceEvent *de=(XIDeviceEvent *)event.xcookie.data;

						if(de->detail==1)
							mouseEvent.button|=MOUSE_BUTTON_1;

						if(de->detail==2)
							mouseEvent.button|=MOUSE_BUTTON_3;

						if(de->detail==3)
							mouseEvent.button|=MOUSE_BUTTON_2;

						Event_Trigger(EVENT_MOUSEDOWN, &mouseEvent);
						break;
					}

					case XI_ButtonRelease:
					{
						XIDeviceEvent *de=(XIDeviceEvent *)event.xcookie.data;

						if(de->detail==1)
							mouseEvent.button&=~MOUSE_BUTTON_1;

						if(de->detail==2)
							mouseEvent.button&=~MOUSE_BUTTON_3;

						if(de->detail==3)
							mouseEvent.button&=~MOUSE_BUTTON_2;

						Event_Trigger(EVENT_MOUSEUP, &mouseEvent);
						break;
					}

					case XI_KeyPress:
					case XI_KeyRelease:
					{
						XIDeviceEvent *de=(XIDeviceEvent *)event.xcookie.data;
						KeySym keysym=XkbKeycodeToKeysym(vkContext.display, (KeyCode)de->detail, 0, 0), temp;

						// Convert lowercase to uppercase
						if(keysym>=XK_a&&keysym<=XK_z)
        					keysym-=0x0020;

						if(keysym==XK_Escape)
						{
							isDone=true;
							break;
						}

						switch(keysym)
						{
							case XK_BackSpace:	code=KB_BACKSPACE;				break;	// Backspace
							case XK_Tab:		code=KB_TAB;					break;	// Tab
							case XK_Return:		code=KB_ENTER;					break;	// Enter
							case XK_Pause:		code=KB_PAUSE;					break;	// Pause
							case XK_Caps_Lock:	code=KB_CAPS_LOCK;				break;	// Caps Lock
							case XK_Escape:		code=KB_ESCAPE;					break;	// Esc
							case XK_Prior:		code=KB_PAGE_UP;				break;	// Page Up
							case XK_Next:		code=KB_PAGE_DOWN;				break;	// Page Down
							case XK_End:		code=KB_END;					break;	// End
							case XK_Home:		code=KB_HOME;					break;	// Home
							case XK_Left:		code=KB_LEFT;					break;	// Left
							case XK_Up:			code=KB_UP;						break;	// Up
							case XK_Right:		code=KB_RIGHT;					break;	// Right
							case XK_Down:		code=KB_DOWN;					break;	// Down
							case XK_Print:		code=KB_PRINT_SCREEN;			break;	// Prnt Scrn
							case XK_Insert:		code=KB_INSERT;					break;	// Insert
							case XK_Delete:		code=KB_DEL;					break;	// Delete
							case XK_Super_L:	code=KB_LSUPER;					break;	// Left Windows
							case XK_Super_R:	code=KB_RSUPER;					break;	// Right Windows
							case XK_Menu:		code=KB_MENU;					break;	// Application
							case XK_KP_0:		code=KB_NP_0;					break;	// Num 0
							case XK_KP_1:		code=KB_NP_1;					break;	// Num 1
							case XK_KP_2:		code=KB_NP_2;					break;	// Num 2
							case XK_KP_3:		code=KB_NP_3;					break;	// Num 3
							case XK_KP_4:		code=KB_NP_4;					break;	// Num 4
							case XK_KP_5:		code=KB_NP_5;					break;	// Num 5
							case XK_KP_6:		code=KB_NP_6;					break;	// Num 6
							case XK_KP_7:		code=KB_NP_7;					break;	// Num 7
							case XK_KP_8:		code=KB_NP_8;					break;	// Num 8
							case XK_KP_9:		code=KB_NP_9;					break;	// Num 9
							case XK_KP_Multiply:code=KB_NP_MULTIPLY;			break;	// Num *
							case XK_KP_Add:		code=KB_NP_ADD;					break;	// Num +
							case XK_KP_Subtract:code=KB_NP_SUBTRACT;			break;	// Num -
							case XK_KP_Decimal:	code=KB_NP_DECIMAL;				break;	// Num Del
							case XK_KP_Divide:	code=KB_NP_DIVIDE;				break;	// Num /
							case XK_F1:			code=KB_F1;						break;	// F1
							case XK_F2:			code=KB_F2;						break;	// F2
							case XK_F3:			code=KB_F3;						break;	// F3
							case XK_F4:			code=KB_F4;						break;	// F4
							case XK_F5:			code=KB_F5;						break;	// F5
							case XK_F6:			code=KB_F6;						break;	// F6
							case XK_F7:			code=KB_F7;						break;	// F7
							case XK_F8:			code=KB_F8;						break;	// F8
							case XK_F9:			code=KB_F9;						break;	// F9
							case XK_F10:		code=KB_F10;					break;	// F10
							case XK_F11:		code=KB_F11;					break;	// F11
							case XK_F12:		code=KB_F12;					break;	// F12
							case XK_Num_Lock:	code=KB_NUM_LOCK;				break;	// Num Lock
							case XK_Scroll_Lock:code=KB_SCROLL_LOCK;			break;	// Scroll Lock
							case XK_Shift_L:	code=KB_LSHIFT;					break;	// Shift
							case XK_Shift_R:	code=KB_RSHIFT;					break;	// Right Shift
							case XK_Control_L:	code=KB_LCTRL;					break;	// Left control
							case XK_Control_R:	code=KB_RCTRL;					break;	// Right control
							case XK_Alt_L:		code=KB_LALT;					break;	// Left alt
							case XK_Alt_R:		code=KB_RALT;					break;	// Left alt
							default:			code=keysym;					break;	// All others
						}

						if(event.xcookie.evtype==XI_KeyPress)
							Event_Trigger(EVENT_KEYDOWN, &code);
						else if(event.xcookie.evtype==XI_KeyRelease)
							Event_Trigger(EVENT_KEYUP, &code);
						break;
					}
				}

				XFreeEventData(vkContext.display, &event.xcookie);
			}
		}

		static float avgfps=0.0f;

		double StartTime=GetClock();
		Render();

		fTimeStep=(float)(GetClock()-StartTime);
		fTime+=fTimeStep;
		avgfps+=1.0f/fTimeStep;

		// Average over 100 frames
		static uint32_t Frames=0;
		if(Frames++>100)
		{
			fps=avgfps/Frames;
			avgfps=0.0f;
			Frames=0;
		}
	}
}

static bool register_input(Display *_display, Window _window)
{
	int xi_event, xi_error;
	if(!XQueryExtension(_display, "XInputExtension", &_xi_opcode, &xi_event, &xi_error))
	{
		DBGPRINTF(DEBUG_ERROR, "X Input extension not available\n");
		return false;
	}

	int major=2, minor=2;
	if(XIQueryVersion(_display, &major, &minor)!=Success)
	{
		DBGPRINTF(DEBUG_ERROR, "XI2.2 not available\n");
		return false;
	}

	XIEventMask mask[2];

	mask[0].deviceid=XIAllMasterDevices;
	mask[0].mask_len=XIMaskLen(XI_LASTEVENT);
	mask[0].mask=(unsigned char *)calloc(mask[0].mask_len, sizeof(char));
	XISetMask(mask[0].mask, XI_ButtonPress);
	XISetMask(mask[0].mask, XI_ButtonRelease);
	XISetMask(mask[0].mask, XI_KeyPress);
	XISetMask(mask[0].mask, XI_KeyRelease);
	XISetMask(mask[0].mask, XI_Enter);
	XISetMask(mask[0].mask, XI_Leave);
	XISetMask(mask[0].mask, XI_FocusIn);
	XISetMask(mask[0].mask, XI_FocusOut);

	mask[1].deviceid=XIAllMasterDevices;
	mask[1].mask_len=XIMaskLen(XI_LASTEVENT);
	mask[1].mask=(unsigned char *)calloc(mask[1].mask_len, sizeof(char));
	XISetMask(mask[1].mask, XI_RawMotion);

	XISelectEvents(_display, _window, &mask[0], 1);
	XISelectEvents(_display, DefaultRootWindow(_display), &mask[1], 1);

	free(mask[0].mask);
	free(mask[1].mask);

	XSelectInput(_display, _window, 0);

	return true;
}

int main(int argc, char **argv)
{
	DBGPRINTF(DEBUG_INFO, "Allocating zone memory (%dMiB)...\n", MEMZONE_SIZE/1024/1024);
	zone=Zone_Init(MEMZONE_SIZE);

	if(zone==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "\t...zone allocation failed!\n");

		return -1;
	}

	if(!Config_ReadINI(&config, "config.ini"))
	{
		DBGPRINTF(DEBUG_ERROR, "Unable to read config.ini.\n");
		return -1;
	}

	DBGPRINTF(DEBUG_INFO, "Opening X display...\n");
	vkContext.display=XOpenDisplay(NULL);

	if(vkContext.display==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "\t...can't open display.\n");

		return -1;
	}

	int32_t Screen=DefaultScreen(vkContext.display);
	Window Root=RootWindow(vkContext.display, Screen);

	DBGPRINTF(DEBUG_INFO, "Creating X11 Window...\n");
	vkContext.window=XCreateSimpleWindow(vkContext.display, Root, 10, 10, config.windowWidth, config.windowHeight, 1, BlackPixel(vkContext.display, Screen), WhitePixel(vkContext.display, Screen));
	XSelectInput(vkContext.display, vkContext.window, StructureNotifyMask|PointerMotionMask|ExposureMask|ButtonPressMask|KeyPressMask|KeyReleaseMask);
	XStoreName(vkContext.display, vkContext.window, szAppName);

	XWarpPointer(vkContext.display, None, vkContext.window, 0, 0, 0, 0, config.windowWidth/2, config.windowHeight/2);
	XFixesHideCursor(vkContext.display, vkContext.window);

	register_input(vkContext.display, vkContext.window);

	XFlush(vkContext.display);
	XSync(vkContext.display, False);

	DBGPRINTF(DEBUG_INFO, "Creating Vulkan Instance...\n");
	if(!vkuCreateInstance(&vkInstance))
	{
		DBGPRINTF(DEBUG_ERROR, "...failed.\n");
		return -1;
	}

	vkContext.deviceIndex=config.deviceIndex;

	DBGPRINTF(DEBUG_INFO, "Creating Vulkan Context...\n");
	if(!vkuCreateContext(vkInstance, &vkContext))
	{
		DBGPRINTF(DEBUG_ERROR, "...failed.\n");
		return -1;
	}

	DBGPRINTF(DEBUG_INFO, "Creating Vulkan Swapchain...\n");
	if(!vkuCreateSwapchain(&vkContext, &swapchain, VK_TRUE))
	{
		DBGPRINTF(DEBUG_ERROR, "...failed.\n");
		return -1;
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
		config.windowWidth=config.renderWidth;
		config.windowHeight=config.renderHeight;
		XMoveResizeWindow(vkContext.display, vkContext.window, 0, 0, config.windowWidth/2, config.windowHeight/2);
	}

	DBGPRINTF(DEBUG_INFO, "Initializing Vulkan resources...\n");
	if(!Init())
	{
		DBGPRINTF(DEBUG_ERROR, "\t...failed.\n");

		vkuDestroyContext(vkInstance, &vkContext);
		vkDestroyInstance(vkInstance, VK_NULL_HANDLE);

		XDestroyWindow(vkContext.display, vkContext.window);
		XCloseDisplay(vkContext.display);

		return -1;
	}

	XMapWindow(vkContext.display, vkContext.window);

	DBGPRINTF(DEBUG_WARNING, "\nCurrent system zone memory allocations:\n");
	Zone_Print(zone);

	DBGPRINTF(DEBUG_INFO, "\nStarting main loop.\n");
	EventLoop();

	DBGPRINTF(DEBUG_INFO, "Shutting down...\n");
	Destroy();
	vkuDestroyContext(vkInstance, &vkContext);
	vkuDestroyInstance(vkInstance);

	XDestroyWindow(vkContext.display, vkContext.window);
	XCloseDisplay(vkContext.display);

	DBGPRINTF(DEBUG_WARNING, "Zone remaining block list:\n");
	Zone_Print(zone);
	Zone_Destroy(zone);

	DBGPRINTF(DEBUG_INFO, "Exit\n");

	return 0;
}
