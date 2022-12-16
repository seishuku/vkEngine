#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <sys/time.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../camera/camera.h"
#include "../utils/list.h"
#include "../utils/event.h"
#include "../utils/input.h"
#include "../vr/vr.h"

MemZone_t *Zone;

char szAppName[]="Vulkan";

bool ToggleFullscreen=true;
bool IsVR=true;

extern VkInstance Instance;
extern VkuContext_t Context;

extern VkuMemZone_t *VkZone;

extern VkuSwapchain_t Swapchain;

extern uint32_t Width, Height;
extern Camera_t Camera;

uint64_t Frequency, StartTime, EndTime;
float avgfps=0.0f, fps=0.0f, fTimeStep, fTime=0.0f;

void Render(void);
bool Init(void);
void RecreateSwapchain(void);
void Destroy(void);

uint64_t rdtsc(void)
{
	uint32_t l, h;

	__asm__ __volatile__ ("rdtsc" : "=a" (l), "=d" (h));

	return (uint64_t)l|((uint64_t)h<<32);
}

unsigned long long GetFrequency(void)
{
	uint64_t StartTicks, StopTicks;
	struct timeval TimeStart, TimeStop;
	volatile uint32_t i;

	gettimeofday(&TimeStart, NULL);
	StartTicks=rdtsc();

	for(i=0;i<1000000;i++);

	StopTicks=rdtsc();
	gettimeofday(&TimeStop, NULL);

	return (StopTicks-StartTicks)*1000000/(TimeStop.tv_usec-TimeStart.tv_usec);
}

void EventLoop(void)
{
	KeySym Keysym, temp;
	uint32_t code;
	XEvent Event;
	MouseEvent_t MouseEvent={ 0, 0, 0, 0 };
	int32_t ox, oy;
	bool Done=false;

	while(!Done)
	{
		while(XPending(Context.Dpy)>0)
		{
			ox=Event.xmotion.x;
			oy=Event.xmotion.y;

			XNextEvent(Context.Dpy, &Event);

			switch(Event.type)
			{
				case MotionNotify:
				{
					MouseEvent.dx=Event.xmotion.x-ox;
					MouseEvent.dy=Event.xmotion.y-oy;

					if(Event.xmotion.state&Button1Mask)
						MouseEvent.button|=MOUSE_BUTTON_1;
					else
						MouseEvent.button&=~MOUSE_BUTTON_1;

					if(Event.xmotion.state&Button2Mask)
						MouseEvent.button|=MOUSE_BUTTON_3;
					else
						MouseEvent.button&=~MOUSE_BUTTON_3;

					if(Event.xmotion.state&Button3Mask)
						MouseEvent.button|=MOUSE_BUTTON_2;
					else
						MouseEvent.button&=~MOUSE_BUTTON_2;

					Event_Trigger(EVENT_MOUSE, &MouseEvent);
					break;
				}

				case Expose:
					break;

				case ConfigureNotify:
					Width=Event.xconfigure.width;
					Height=Event.xconfigure.height;
					RecreateSwapchain();
					break;

				case ButtonPress:
					break;

				case KeyPress:
					XConvertCase(XLookupKeysym(&Event.xkey, 0), &temp, &Keysym);

					if(Keysym==XK_Return&&Event.xkey.state&Mod1Mask)
					{
						static uint32_t OldWidth, OldHeight;

						if(ToggleFullscreen)
						{
							ToggleFullscreen=false;
							DBGPRINTF("Going full screen...\n");

							OldWidth=Width;
							OldHeight=Height;

							Width=XDisplayWidth(Context.Dpy, DefaultScreen(Context.Dpy));
							Height=XDisplayHeight(Context.Dpy, DefaultScreen(Context.Dpy));
							XMoveResizeWindow(Context.Dpy, Context.Win, 0, 0, Width, Height);
						}
						else
						{
							ToggleFullscreen=true;
							DBGPRINTF("Going windowed...\n");

							Width=OldWidth;
							Height=OldHeight;
							XMoveResizeWindow(Context.Dpy, Context.Win, 0, 0, Width, Height);
						}
					}

					if(Keysym==XK_Escape)
					{
						Done=true;
						break;
					}

					switch(Keysym)
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
						default:			code=Keysym;					break;	// All others
					}

					Event_Trigger(EVENT_KEYDOWN, &code);
					break;

				case KeyRelease:
					XConvertCase(XLookupKeysym(&Event.xkey, 0), &temp, &Keysym);

					switch(Keysym)
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
						default:			code=Keysym;					break;	// All others
					}

					Event_Trigger(EVENT_KEYUP, &code);
					break;
			}
		}

		StartTime=rdtsc();
		Render();
		EndTime=rdtsc();

		// Total screen time in seconds
		fTimeStep=(float)(EndTime-StartTime)/Frequency;
		// Running time
		fTime+=fTimeStep;

		// Accumulate frames per second
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

int main(int argc, char **argv)
{
	DBGPRINTF(DEBUG_INFO, "Allocating zone memory...\n");
	Zone=Zone_Init(256*1000*1000);

	if(Zone==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "\t...zone allocation failed!\n");

		return -1;
	}

	DBGPRINTF(DEBUG_INFO, "Opening X display...\n");
	Context.Dpy=XOpenDisplay(NULL);

	if(Context.Dpy==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "\t...can't open display.\n");

		return -1;
	}

	int32_t Screen=DefaultScreen(Context.Dpy);
	Window Root=RootWindow(Context.Dpy, Screen);

	DBGPRINTF(DEBUG_INFO, "Creating X11 Window...\n");
	Context.Win=XCreateSimpleWindow(Context.Dpy, Root, 10, 10, Width, Height, 1, BlackPixel(Context.Dpy, Screen), WhitePixel(Context.Dpy, Screen));
	XSelectInput(Context.Dpy, Context.Win, StructureNotifyMask|PointerMotionMask|ExposureMask|ButtonPressMask|KeyPressMask|KeyReleaseMask);
	XStoreName(Context.Dpy, Context.Win, szAppName);

	DBGPRINTF(DEBUG_INFO, "Initalizing OpenVR...\n");
	if(!InitOpenVR())
	{
		DBGPRINTF(DEBUG_ERROR, "\t...failed, turning off VR support.\n");
		IsVR=false;
		rtWidth=Width;
		rtHeight=Height;
	}

	DBGPRINTF(DEBUG_INFO, "Creating Vulkan Instance...\n");
	if(!CreateVulkanInstance(&Instance))
	{
		DBGPRINTF(DEBUG_ERROR, "...failed.\n");
		return -1;
	}

	DBGPRINTF(DEBUG_INFO, "Creating Vulkan Context...\n");
	if(!CreateVulkanContext(Instance, &Context))
	{
		DBGPRINTF(DEBUG_ERROR, "...failed.\n");
		return -1;
	}

	DBGPRINTF(DEBUG_INFO, "Creating Vulkan Swapchain...\n");
	vkuCreateSwapchain(&Context, &Swapchain, Width, Height, VK_TRUE);

	DBGPRINTF(DEBUG_INFO, "Initalizing Vulkan resources...\n");
	if(!Init())
	{
		DBGPRINTF(DEBUG_ERROR, "\t...failed.\n");

		DestroyVulkan(Instance, &Context);
		vkDestroyInstance(Instance, VK_NULL_HANDLE);

		XDestroyWindow(Context.Dpy, Context.Win);
		//XCloseDisplay(Context.Dpy);

		return -1;
	}

	XMapWindow(Context.Dpy, Context.Win);

	Frequency=GetFrequency();
	DBGPRINTF(DEBUG_INFO, "\nCPU freqency: %0.2fGHz\n", (float)Frequency/1000000000);

	DBGPRINTF(DEBUG_WARNING, "\nCurrent system zone memory allocations:\n");
	Zone_Print(Zone);

	DBGPRINTF(DEBUG_WARNING, "\nCurrent vulkan zone memory allocations:\n");
	vkuMem_Print(VkZone);

	DBGPRINTF(DEBUG_INFO, "\nStarting main loop.\n");
	EventLoop();

	DBGPRINTF(DEBUG_INFO, "Shutting down...\n");
	Destroy();
	DestroyVulkan(Instance, &Context);
	vkDestroyInstance(Instance, VK_NULL_HANDLE);

	XDestroyWindow(Context.Dpy, Context.Win);

	// TODO: Segfaulting on XCloseDisplay for some reason?
	//			Not sure what's going on here.
	//XCloseDisplay(Context.Dpy);

	DBGPRINTF(DEBUG_WARNING, "Zone remaining block list:\n");
	Zone_Print(Zone);
	Zone_Destroy(Zone);

	DBGPRINTF(DEBUG_INFO, "Exit\n");

	return 0;
}
