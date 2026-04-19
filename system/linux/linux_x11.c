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
#include "../../input/input.h"

MemZone_t *zone;

char szAppName[]="Vulkan";

static int _xi_opcode=0;

bool isDone=false;
bool toggleFullscreen=true;

extern XruContext_t xrContext;

extern VkInstance vkInstance;
extern VkuContext_t vkContext;

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

static Keycodes_t ConvertKeymap(uint32_t keysym)
{
	switch (keysym)
	{
		case XK_BackSpace:		return KB_BACKSPACE;
		case XK_Tab:			return KB_TAB;
		case XK_Return:			return KB_ENTER;
		case XK_Pause:			return KB_PAUSE;
		case XK_Escape:			return KB_ESCAPE;
		case XK_space:			return KB_SPACE;
		case XK_Prior:			return KB_PAGE_UP;
		case XK_Next:			return KB_PAGE_DOWN;
		case XK_End:			return KB_END;
		case XK_Home:			return KB_HOME;
		case XK_Left:			return KB_LEFT;
		case XK_Up:				return KB_UP;
		case XK_Right:			return KB_RIGHT;
		case XK_Down:			return KB_DOWN;
		case XK_Print:			return KB_PRINT_SCREEN;
		case XK_Insert:			return KB_INSERT;
		case XK_Delete:			return KB_DEL;
		case XK_Super_L:		return KB_LSUPER;
		case XK_Super_R:		return KB_RSUPER;
		case XK_Menu:			return KB_MENU;
		case XK_KP_0:			return KB_NP_0;
		case XK_KP_1:			return KB_NP_1;
		case XK_KP_2:			return KB_NP_2;
		case XK_KP_3:			return KB_NP_3;
		case XK_KP_4:			return KB_NP_4;
		case XK_KP_5:			return KB_NP_5;
		case XK_KP_6:			return KB_NP_6;
		case XK_KP_7:			return KB_NP_7;
		case XK_KP_8:			return KB_NP_8;
		case XK_KP_9:			return KB_NP_9;
		case XK_KP_Multiply:	return KB_NP_MULTIPLY;
		case XK_KP_Add:			return KB_NP_ADD;
		case XK_KP_Subtract:	return KB_NP_SUBTRACT;
		case XK_KP_Decimal:		return KB_NP_DECIMAL;
		case XK_KP_Divide:		return KB_NP_DIVIDE;
		case XK_KP_Enter:		return KB_NP_ENTER;
		case XK_KP_Equal:		return KB_NP_EQUAL;
		case XK_F1:				return KB_F1;
		case XK_F2:				return KB_F2;
		case XK_F3:				return KB_F3;
		case XK_F4:				return KB_F4;
		case XK_F5:				return KB_F5;
		case XK_F6:				return KB_F6;
		case XK_F7:				return KB_F7;
		case XK_F8:				return KB_F8;
		case XK_F9:				return KB_F9;
		case XK_F10:			return KB_F10;
		case XK_F11:			return KB_F11;
		case XK_F12:			return KB_F12;
		case XK_Num_Lock:		return KB_NUM_LOCK;
		case XK_Scroll_Lock:	return KB_SCROLL_LOCK;
		case XK_Shift_L:		return KB_LSHIFT;
		case XK_Shift_R:		return KB_RSHIFT;
		case XK_Control_L:		return KB_LCTRL;
		case XK_Control_R:		return KB_RCTRL;
		case XK_Alt_L:			return KB_LALT;
		case XK_Alt_R:			return KB_RALT;
		case XK_Caps_Lock:		return KB_CAPS_LOCK;
		case XK_apostrophe:		return KB_APOSTROPHE;
		case XK_comma:			return KB_COMMA;
		case XK_minus:			return KB_MINUS;
		case XK_period:			return KB_PERIOD;
		case XK_slash:			return KB_SLASH;
		case XK_semicolon:		return KB_SEMICOLON;
		case XK_equal:			return KB_EQUAL;
		case XK_bracketleft:	return KB_LEFT_BRACKET;
		case XK_backslash:		return KB_BACKSLASH;
		case XK_bracketright:	return KB_RIGHT_BRACKET;
		case XK_grave:			return KB_GRAVE_ACCENT;
		default:
			// Handle ASCII codes
			if(keysym>='A'&&keysym<='Z')
				return (Keycodes_t)keysym;
			
			if(keysym>='a'&&keysym<='z')
				return (Keycodes_t)(keysym-'a'+'A');  // Convert to uppercase

			if(keysym>='0'&&keysym<='9')
				return (Keycodes_t)keysym;

			return KB_UNKNOWN;
	}
}

static void EventLoop(void)
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

							Input_OnMouseEvent(&mouseEvent, Vec2(mouseEvent.dx, mouseEvent.dy));

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

						Mousecodes_t button=0;
						if(de->detail==1)
							button=MOUSE_BUTTON_1;
						else if(de->detail==2)
							button=MOUSE_BUTTON_3;
						else if(de->detail==3)
							button=MOUSE_BUTTON_2;

						if(button)
							Input_OnMouseButtonEvent(button, true);

						// Keep button state accumulated for movement events
						mouseEvent.button|=(de->detail==1?MOUSE_BUTTON_1:0);
						mouseEvent.button|=(de->detail==2?MOUSE_BUTTON_3:0);
						mouseEvent.button|=(de->detail==3?MOUSE_BUTTON_2:0);
						break;
					}

					case XI_ButtonRelease:
					{
						XIDeviceEvent *de=(XIDeviceEvent *)event.xcookie.data;

						Mousecodes_t button=0;
						if(de->detail==1)
							button=MOUSE_BUTTON_1;
						else if(de->detail==2)
							button=MOUSE_BUTTON_3;
						else if(de->detail==3)
							button=MOUSE_BUTTON_2;

						if(button)
							Input_OnMouseButtonEvent(button, false);

						// Keep button state accumulated for movement events
						mouseEvent.button&=~(de->detail==1?MOUSE_BUTTON_1:0);
						mouseEvent.button&=~(de->detail==2?MOUSE_BUTTON_3:0);
						mouseEvent.button&=~(de->detail==3?MOUSE_BUTTON_2:0);
						break;
					}

					case XI_KeyPress:
					case XI_KeyRelease:
					{
						XIDeviceEvent *de=(XIDeviceEvent *)event.xcookie.data;
						KeySym keysym=XkbKeycodeToKeysym(vkContext.display, (KeyCode)de->detail, 0, 0), temp;

						if(keysym==XK_Escape)
						{
							isDone=true;
							break;
						}

						Keycodes_t code=ConvertKeymap(keysym);
						bool pressed=(event.xcookie.evtype==XI_KeyPress);

						Input_OnKeyEvent(code, pressed);
						break;
					}
				}

				XFreeEventData(vkContext.display, &event.xcookie);
			}
		}

		static float avgfps=0.0f;

		double StartTime=GetClock();
		Input_Update();
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
	if(!vkuCreateSwapchain(&vkContext, &swapchain, config.vsync))
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
		config.renderWidth=xrContext.swapchain[0].extent.width;
		config.renderHeight=xrContext.swapchain[0].extent.height;
		config.windowWidth=config.renderWidth;
		config.windowHeight=config.renderHeight;
		XMoveResizeWindow(vkContext.display, vkContext.window, 0, 0, config.windowWidth/2, config.windowHeight/2);
		config.isVR=true;
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
