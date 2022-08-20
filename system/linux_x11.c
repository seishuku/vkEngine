#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <sys/time.h>
#include <strings.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../utils/list.h"
#include "system.h"

char szAppName[]="OpenGL";

bool Key[65536];

extern VkInstance Instance;
extern VkuContext_t Context;
extern uint32_t Width, Height;
extern Lights_t Lights;

extern float RotateX, RotateY, PanX, PanY, Zoom;

uint64_t Frequency, StartTime, EndTime;
float avgfps=0.0f, fps=0.0f, fTimeStep, fTime=0.0f;
uint32_t Frames=0;

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

			XNextEvent(dpy, &Event);

			switch(Event.type)
			{
				case MotionNotify:
					dx=ox-Event.xmotion.x;
					dy=oy-Event.xmotion.y;

					if(Event.xmotion.state&Button1Mask)
					{
						RotateX+=(dx*0.01f);
						RotateY-=(dy*0.01f);
					}

					if(Event.xmotion.state&Button2Mask)
					{
						Zoom+=dy;
					}

					if(Event.xmotion.state&Button3Mask)
					{
						PanX+=dx;
						PanY+=dy;
					}
					break;

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
					Keysym=XLookupKeysym(&Event.xkey, 0);
					Key[Keysym]=true;

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
								}, 50.0f,
								(vec4)
								{
									(float)rand()/RAND_MAX,
									(float)rand()/RAND_MAX,
									(float)rand()/RAND_MAX,
									1.0f
								});
							}
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
						default:
							break;
					}
					break;
			}
		}

		StartTime=rdtsc();
		Render();
		EndFrameTime=rdtsc();

		// Total screen time in seconds
		fTimeStep=(float)(EndTime-StartTime)/Frequency;
		// Running time
		fTime+=fTimeStep;

		// Accumulate frames per second
		avgfps+=1.0f/fTimeStep;

		// Average over 100 frames
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
	DBGPRINTF("Opening X display...\n");
	Context.Dpy=XOpenDisplay(NULL);

	if(Context.Dpy==NULL)
	{
		DBGPRINTF("\t...can't open display.\n");

		return -1;
	}

	int32_t Screen=DefaultScreen(display)
	Windows Root=RootWindow(Context.Dpy, Screen);

	DBGPRINTF("Creating X11 Window...\n");
	Context.Win=XCreateSimpleWindow(Context.Dpy, Root, 10, 10, Width, Height, 1, BlackPixel(Context.Dpy, Screen), WhitePixel(Context.Dpy, Screen));
	XSelectInput(Context.Dpy, Context.Win, ExposureMask|KeyPressMask);
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
	vkuCreateSwapchain(&Context, Width, Height, VK_TRUE);

	DBGPRINTF("Initalizing Vulkan resources...\n");
	if(!Init())
	{
		DBGPRINTF("\t...failed.\n");

		DestroyVulkan(Instance, &Context);
		vkDestroyInstance(Instance, VK_NULL_HANDLE);

		XDestroyWindow(Context.Dpy, Context.Win);
		XCloseDisplay(Context.Dpy);

		return -1;
	}

	XMapWindow(Context.Dpy, Context.Win);

	Frequency=GetFrequency();
	DBGPRINTF("\nCPU freqency: %0.2fGHz\n", (float)Frequency/1000000000);

	DBGPRINTF("\nStarting main loop.\n");
	EventLoop();

	DBGPRINTF("Shutting down...\n");
	Destroy();
	DestroyVulkan(Instance, &Context);
	vkDestroyInstance(Instance, VK_NULL_HANDLE);

	XDestroyWindow(Context.Dpy, Context.Win);
	XCloseDisplay(Context.Dpy);

	DBGPRINTF("Exit\n");

	return 0;
}
