#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../vulkan/vulkanmem.h"
#include "../math/math.h"
#include "../camera/camera.h"
#include "../utils/list.h"
#include "../lights/lights.h"

MemZone_t *Zone;

char szAppName[]="Vulkan";

bool Done=0, Key[256];
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
uint32_t Frames=0;

void Render(void);
bool Init(void);
void vkuCreateSwapchain(VkuContext_t *Context, uint32_t Width, uint32_t Height, int VSync);
void RecreateSwapchain(void);
void Destroy(void);

unsigned __int64 rdtsc(void)
{
	return __rdtsc();
}

unsigned __int64 GetFrequency(void)
{
	unsigned __int64 TimeStart, TimeStop, TimeFreq;
	unsigned __int64 StartTicks, StopTicks;
	volatile unsigned __int64 i;

	QueryPerformanceFrequency((LARGE_INTEGER *)&TimeFreq);

	QueryPerformanceCounter((LARGE_INTEGER *)&TimeStart);
	StartTicks=rdtsc();

	for(i=0;i<1000000;i++);

	StopTicks=rdtsc();
	QueryPerformanceCounter((LARGE_INTEGER *)&TimeStop);

	return (StopTicks-StartTicks)*TimeFreq/(TimeStop-TimeStart);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static POINT old;
	POINT pos, delta;

	switch(uMsg)
	{
		case WM_CREATE:
			break;

		case WM_CLOSE:
			PostQuitMessage(0);
			break;

		case WM_DESTROY:
			break;

		case WM_SIZE:
			Width=max(LOWORD(lParam), 2);
			Height=max(HIWORD(lParam), 2);
			break;

		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_RBUTTONDOWN:
			SetCapture(hWnd);
			ShowCursor(FALSE);

			GetCursorPos(&pos);
			old.x=pos.x;
			old.y=pos.y;
			break;

		case WM_LBUTTONUP:
		case WM_MBUTTONUP:
		case WM_RBUTTONUP:
			ShowCursor(TRUE);
			ReleaseCapture();
			break;

		case WM_MOUSEMOVE:
			GetCursorPos(&pos);

			if(!wParam)
			{
				old.x=pos.x;
				old.y=pos.y;
				break;
			}

			delta.x=pos.x-old.x;
			delta.y=old.y-pos.y;

			if(!delta.x&&!delta.y)
				break;

			SetCursorPos(old.x, old.y);

			switch(wParam)
			{
				case MK_LBUTTON:
					Camera.Yaw-=(float)delta.x/800.0f;
					Camera.Pitch+=(float)delta.y/800.0f;
					//RotateX+=(delta.x*0.01f);
					//RotateY-=(delta.y*0.01f);
					break;

				case MK_MBUTTON:
					//PanX+=delta.x;
					//PanY+=delta.y;
					break;

				case MK_RBUTTON:
					//Zoom+=delta.y;
					break;
			}
			break;

		case WM_SYSKEYUP:
			if(HIWORD(lParam)&KF_ALTDOWN&&LOWORD(wParam)==VK_RETURN)
			{
				static uint32_t OldWidth, OldHeight;

				if(ToggleFullscreen)
				{
					ToggleFullscreen=false;
					DBGPRINTF("Going full screen...\n");

					OldWidth=Width;
					OldHeight=Height;

					Width=GetSystemMetrics(SM_CXSCREEN);
					Height=GetSystemMetrics(SM_CYSCREEN);
					SetWindowPos(Context.hWnd, HWND_TOPMOST, 0, 0, Width, Height, 0);
				}
				else
				{
					ToggleFullscreen=true;
					DBGPRINTF("Going windowed...\n");

					Width=OldWidth;
					Height=OldHeight;
					SetWindowPos(Context.hWnd, HWND_TOPMOST, 0, 0, Width, Height, 0);
				}
			}
			break;

		case WM_KEYDOWN:
			Key[wParam]=1;

			switch(wParam)
			{
				case 'O':
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

				case 'W':
					Camera.key_w=true;
					break;

				case 'S':
					Camera.key_s=true;
					break;

				case 'A':
					Camera.key_a=true;
					break;

				case 'D':
					Camera.key_d=true;
					break;

				case 'V':
					Camera.key_v=true;
					break;

				case 'C':
					Camera.key_c=true;
					break;

				case 'Q':
					Camera.key_q=true;
					break;

				case 'E':
					Camera.key_e=true;
					break;

				case VK_UP:
					Camera.key_up=true;
					break;

				case VK_DOWN:
					Camera.key_down=true;
					break;

				case VK_LEFT:
					Camera.key_left=true;
					break;

				case VK_RIGHT:
					Camera.key_right=true;
					break;

				case VK_ESCAPE:
					PostQuitMessage(0);
					break;

				default:
					break;
			}
			break;

		case WM_KEYUP:
			Key[wParam]=0;

			switch(wParam)
			{
				case 'W':
					Camera.key_w=false;
					break;

				case 'S':
					Camera.key_s=false;
					break;

				case 'A':
					Camera.key_a=false;
					break;

				case 'D':
					Camera.key_d=false;
					break;

				case 'V':
					Camera.key_v=false;
					break;

				case 'C':
					Camera.key_c=false;
					break;

				case 'Q':
					Camera.key_q=false;
					break;

				case 'E':
					Camera.key_e=false;
					break;

				case VK_UP:
					Camera.key_up=false;
					break;

				case VK_DOWN:
					Camera.key_down=false;
					break;

				case VK_LEFT:
					Camera.key_left=false;
					break;

				case VK_RIGHT:
					Camera.key_right=false;
					break;

				default:
					break;
			}
			break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int iCmdShow)
{
	if(AllocConsole())
	{
		FILE *fDummy;
		freopen_s(&fDummy, "CONOUT$", "w", stdout);
		freopen_s(&fDummy, "CONOUT$", "w", stderr);
		freopen_s(&fDummy, "CONIN$", "r", stdin);
	}

	DBGPRINTF("Allocating zone memory...\n");
	Zone=Zone_Init(32*1000*1000);

	if(Zone==NULL)
	{
		DBGPRINTF("\t...zone allocation failed!\n");

		return -1;
	}

	RegisterClass(&(WNDCLASS)
	{
		.style=CS_VREDRAW|CS_HREDRAW|CS_OWNDC,
		.lpfnWndProc=WndProc,
		.cbClsExtra=0,
		.cbWndExtra=0,
		.hInstance=hInstance,
		.hIcon=LoadIcon(NULL, IDI_WINLOGO),
		.hCursor=LoadCursor(NULL, IDC_ARROW),
		.hbrBackground=GetStockObject(BLACK_BRUSH),
		.lpszMenuName=NULL,
		.lpszClassName=szAppName,
	});

	RECT Rect;

	SetRect(&Rect, 0, 0, Width, Height);
	AdjustWindowRect(&Rect, WS_OVERLAPPEDWINDOW, FALSE);

	Context.hWnd=CreateWindow(szAppName, szAppName, WS_OVERLAPPEDWINDOW|WS_CLIPSIBLINGS, CW_USEDEFAULT, CW_USEDEFAULT, Rect.right-Rect.left, Rect.bottom-Rect.top, NULL, NULL, hInstance, NULL);

	ShowWindow(Context.hWnd, SW_SHOW);
	SetForegroundWindow(Context.hWnd);

	DBGPRINTF("Creating Vulkan instance...\n");
	if(!CreateVulkanInstance(&Instance))
	{
		DBGPRINTF("\t...failed.\n");
		return -1;
	}

	DBGPRINTF("Creating Vulkan context...\n");
	if(!CreateVulkanContext(Instance, &Context))
	{
		DBGPRINTF("\t...failed.\n");
		return -1;
	}

	DBGPRINTF("Creating swapchain...\n");
	vkuCreateSwapchain(&Context, Width, Height, VK_TRUE);

	DBGPRINTF("Initalizing Vulkan resources...\n");
	if(!Init())
	{
		DBGPRINTF("\t...failed.\n");
		return -1;
	}

	Frequency=GetFrequency();
	DBGPRINTF("\nCPU freqency: %0.2fGHz\n", (float)Frequency/1000000000);

	DBGPRINTF("\nCurrent system zone memory allocations:\n");
	Zone_Print(Zone);

	DBGPRINTF("\nCurrent vulkan zone memory allocations:\n");
	VulkanMem_Print(VkZone);

	DBGPRINTF("\nStarting main loop.\n");
	while(!Done)
	{
		MSG msg;

		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if(msg.message==WM_QUIT)
				Done=1;
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			StartTime=rdtsc();
			Render();
			EndTime=rdtsc();

			fTimeStep=(float)(EndTime-StartTime)/Frequency;
			fTime+=fTimeStep;
			avgfps+=1.0f/fTimeStep;

			if(Frames++>100)
			{
				fps=avgfps/Frames;
				avgfps=0.0f;
				Frames=0;
			}
		}
	}

	DBGPRINTF("Shutting down...\n");
	Destroy();
	DestroyVulkan(Instance, &Context);
	vkDestroyInstance(Instance, VK_NULL_HANDLE);
	DestroyWindow(Context.hWnd);

	DBGPRINTF("Zone remaining block list:\n");
	Zone_Print(Zone);
	Zone_Destroy(Zone);

	system("pause");

	FreeConsole();

	return 0;
}
