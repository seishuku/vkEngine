#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <sys/time.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../camera/camera.h"
#include "../utils/list.h"
#include "../lights/lights.h"
#include "system.h"

MemZone_t *Zone;

char szAppName[]="Vulkan";

bool Key[65536];
bool ToggleFullscreen=true;

extern VkInstance Instance;
extern VkuContext_t Context;

extern VulkanMemZone_t *VkZone;

extern uint32_t Width, Height;
extern Lights_t Lights;
extern Camera_t Camera;

extern float RotateX, RotateY, PanX, PanY, Zoom;

uint64_t Frequency, StartTime, EndTime;
float avgfps=0.0f, fps=0.0f, fTimeStep, fTime=0.0f;

void Render(void);
bool Init(void);
void vkuCreateSwapchain(VkuContext_t *Context, uint32_t Width, uint32_t Height, int VSync);
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
	int32_t Keysym;
	XEvent Event;
	int32_t ox, oy, dx, dy;
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
					dx=Event.xmotion.x-ox;
					dy=Event.xmotion.y-oy;

					if(Event.xmotion.state&Button1Mask)
					{
						Camera.Yaw+=(float)dx/800.0f;
						Camera.Pitch+=(float)dy/800.0f;
						//RotateX+=(dx*0.01f);
						//RotateY+=(dy*0.01f);
					}

					if(Event.xmotion.state&Button2Mask)
					{
						//PanX+=dx;
						//PanY-=dy;
					}

					if(Event.xmotion.state&Button3Mask)
					{
						//Zoom-=dy;
					}
					break;

				case Expose:
					break;

				case ConfigureNotify:
					Width=Event.xconfigure.width;
					Height=Event.xconfigure.height;
					break;

				case ButtonPress:
					break;

				case KeyPress:
					Keysym=XLookupKeysym(&Event.xkey, 0);
					Key[Keysym]=true;

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

					switch(Keysym)
					{
						case 'o':
							for(uint32_t i=0;i<10;i++)
							{
								Lights_Add(&Lights, 
								(vec3)
								{
									(((float)rand()/RAND_MAX)*2.0f-1.0f)*400.0f,
									(((float)rand()/RAND_MAX)*2.0f-1.0f)*100.0f,
									(((float)rand()/RAND_MAX)*2.0f-1.0f)*400.0f
								}, 200.0f,
								(vec4)
								{
									(float)rand()/RAND_MAX,
									(float)rand()/RAND_MAX,
									(float)rand()/RAND_MAX,
									1.0f
								});
							}
							break;

						case 'w':
							Camera.key_w=true;
							break;

						case 's':
							Camera.key_s=true;
							break;

						case 'a':
							Camera.key_a=true;
							break;

						case 'd':
							Camera.key_d=true;
							break;

						case 'v':
							Camera.key_v=true;
							break;

						case 'c':
							Camera.key_c=true;
							break;

						case 'q':
							Camera.key_q=true;
							break;

						case 'e':
							Camera.key_e=true;
							break;

						case XK_Up:
							Camera.key_up=true;
							break;

						case XK_Down:
							Camera.key_down=true;
							break;

						case XK_Left:
							Camera.key_left=true;
							break;

						case XK_Right:
							Camera.key_right=true;
							break;

						case XK_Escape:
							Done=true;
							break;

						default:
							break;
					}
					break;

				case KeyRelease:
					Keysym=XLookupKeysym(&Event.xkey, 0);
					Key[Keysym]=false;

					switch(Keysym)
					{
						case 'w':
							Camera.key_w=false;
							break;

						case 's':
							Camera.key_s=false;
							break;

						case 'a':
							Camera.key_a=false;
							break;

						case 'd':
							Camera.key_d=false;
							break;

						case 'v':
							Camera.key_v=false;
							break;

						case 'c':
							Camera.key_c=false;
							break;

						case 'q':
							Camera.key_q=false;
							break;

						case 'e':
							Camera.key_e=false;
							break;

						case XK_Up:
							Camera.key_up=false;
							break;

						case XK_Down:
							Camera.key_down=false;
							break;

						case XK_Left:
							Camera.key_left=false;
							break;

						case XK_Right:
							Camera.key_right=false;
							break;

						default:
							break;
					}
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
	DBGPRINTF("Allocating zone memory...\n");
	Zone=Zone_Init(32*1000*1000);

	if(Zone==NULL)
	{
		DBGPRINTF("\t...zone allocation failed!\n");

		return -1;
	}

	DBGPRINTF("Opening X display...\n");
	Context.Dpy=XOpenDisplay(NULL);

	if(Context.Dpy==NULL)
	{
		DBGPRINTF("\t...can't open display.\n");

		return -1;
	}

	int32_t Screen=DefaultScreen(Context.Dpy);
	Window Root=RootWindow(Context.Dpy, Screen);

	DBGPRINTF("Creating X11 Window...\n");
	Context.Win=XCreateSimpleWindow(Context.Dpy, Root, 10, 10, Width, Height, 1, BlackPixel(Context.Dpy, Screen), WhitePixel(Context.Dpy, Screen));
	XSelectInput(Context.Dpy, Context.Win, StructureNotifyMask|PointerMotionMask|ExposureMask|ButtonPressMask|KeyPressMask|KeyReleaseMask);
	XStoreName(Context.Dpy, Context.Win, szAppName);

	DBGPRINTF("Creating Vulkan Instance...\n");
	if(!CreateVulkanInstance(&Instance))
	{
		DBGPRINTF("...failed.\n");
		return -1;
	}

	DBGPRINTF("Creating Vulkan Context...\n");
	if(!CreateVulkanContext(Instance, &Context))
	{
		DBGPRINTF("...failed.\n");
		return -1;
	}

	DBGPRINTF("Creating Vulkan Swapchain...\n");
	vkuCreateSwapchain(&Context, Width, Height, VK_FALSE);

	DBGPRINTF("Initalizing Vulkan resources...\n");
	if(!Init())
	{
		DBGPRINTF("\t...failed.\n");

		DestroyVulkan(Instance, &Context);
		vkDestroyInstance(Instance, VK_NULL_HANDLE);

		XDestroyWindow(Context.Dpy, Context.Win);
		//XCloseDisplay(Context.Dpy);

		return -1;
	}

	XMapWindow(Context.Dpy, Context.Win);

	Frequency=GetFrequency();
	DBGPRINTF("\nCPU freqency: %0.2fGHz\n", (float)Frequency/1000000000);

	DBGPRINTF("\nCurrent system zone memory allocations:\n");
	Zone_Print(Zone);

	DBGPRINTF("\nCurrent vulkan zone memory allocations:\n");
	VulkanMem_Print(VkZone);

	DBGPRINTF("\nStarting main loop.\n");
	EventLoop();

	DBGPRINTF("Shutting down...\n");
	Destroy();
	DestroyVulkan(Instance, &Context);
	vkDestroyInstance(Instance, VK_NULL_HANDLE);

	XDestroyWindow(Context.Dpy, Context.Win);

	// TODO: Segfaulting on XCloseDisplay for some reason?
	//			Not sure what's going on here.
	//XCloseDisplay(Context.Dpy);

	DBGPRINTF("Zone remaining block list:\n");
	Zone_Print(Zone);
	Zone_Destroy(Zone);

	DBGPRINTF("Exit\n");

	return 0;
}
