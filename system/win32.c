#include <windows.h>
#include <hidusage.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../system/system.h"
#include "../vulkan/vulkan.h"
#include "../math/math.h"
#include "../camera/camera.h"
#include "../utils/list.h"
#include "../lights/lights.h"
#include "../utils/event.h"
#include "../utils/input.h"
#include "../vr/vr.h"

MemZone_t *Zone;

char szAppName[]="Vulkan";

bool Done=false;
bool ToggleFullscreen=true;

bool IsVR=true;

extern VkInstance Instance;
extern VkuContext_t Context;

extern VkuMemZone_t *VkZone;

extern VkuSwapchain_t Swapchain;

extern uint32_t Width, Height;

float fps=0.0f, fTimeStep, fTime=0.0f;

void Render(void);
bool Init(void);
void RecreateSwapchain(void);
void Destroy(void);

double GetClock(void)
{
	static uint64_t Frequency=0;
	uint64_t Count;

	if(!Frequency)
		QueryPerformanceFrequency((LARGE_INTEGER *)&Frequency);

	QueryPerformanceCounter((LARGE_INTEGER *)&Count);

	return (double)Count/Frequency;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
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
			//RecreateSwapchain();
			break;

		case WM_SYSKEYUP:
			if(HIWORD(lParam)&KF_ALTDOWN&&LOWORD(wParam)==VK_RETURN)
			{
				static uint32_t OldWidth, OldHeight;

				if(ToggleFullscreen)
				{
					ToggleFullscreen=false;
					DBGPRINTF(DEBUG_INFO, "Going full screen...\n");

					OldWidth=Width;
					OldHeight=Height;

					Width=GetSystemMetrics(SM_CXSCREEN);
					Height=GetSystemMetrics(SM_CYSCREEN);
					SetWindowPos(Context.hWnd, HWND_TOPMOST, 0, 0, Width, Height, 0);
				}
				else
				{
					ToggleFullscreen=true;
					DBGPRINTF(DEBUG_INFO, "Going windowed...\n");

					Width=OldWidth;
					Height=OldHeight;
					SetWindowPos(Context.hWnd, HWND_TOPMOST, 0, 0, Width, Height, 0);
				}
			}
			break;

		case WM_INPUT:
		{
			UINT dwSize=0;
			BYTE bRawMessage[64];

			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, bRawMessage, &dwSize, sizeof(RAWINPUTHEADER));

			RAWINPUT *Input=(RAWINPUT *)bRawMessage;

			switch(Input->header.dwType)
			{
				case RIM_TYPEKEYBOARD:
				{
					RAWKEYBOARD Keyboard=Input->data.keyboard;

					if(Keyboard.VKey==0xFF)
						break;

					// Specific case for escape key to quit application
					if(Keyboard.VKey==VK_ESCAPE)
					{
						PostQuitMessage(0);
						return 0;
					}

					// Specific case to remap the shift/control/alt virtual keys
					if(Keyboard.VKey==VK_SHIFT)
						Keyboard.VKey=MapVirtualKey(Keyboard.MakeCode, MAPVK_VSC_TO_VK_EX);

					if(Keyboard.VKey==VK_CONTROL)
						Keyboard.VKey=MapVirtualKey(Keyboard.MakeCode, MAPVK_VSC_TO_VK_EX);

					if(Keyboard.VKey==VK_MENU)
						Keyboard.VKey=MapVirtualKey(Keyboard.MakeCode, MAPVK_VSC_TO_VK_EX);

					// Remap from Windows virtual keys to application key enums
					// (will probably reformat this later)
					uint32_t code=KB_UNKNOWN;

					switch(Keyboard.VKey)
					{
						case VK_BACK:		code=KB_BACKSPACE;				break;	// Backspace
						case VK_TAB:		code=KB_TAB;					break;	// Tab
						case VK_RETURN:		if(Keyboard.Flags&RI_KEY_E0)
												code=KB_NP_ENTER;					// Numpad enter
											else
												code=KB_ENTER;						// Enter
											break;
						case VK_PAUSE:		code=KB_PAUSE;					break;	// Pause
						case VK_CAPITAL:	code=KB_CAPS_LOCK;				break;	// Caps Lock
						case VK_ESCAPE:		code=KB_ESCAPE;					break;	// Esc
						case VK_SPACE:		code=KB_SPACE;					break;	// Space
						case VK_PRIOR:		code=KB_PAGE_UP;				break;	// Page Up
						case VK_NEXT:		code=KB_PAGE_DOWN;				break;	// Page Down
						case VK_END:		code=KB_END;					break;	// End
						case VK_HOME:		code=KB_HOME;					break;	// Home
						case VK_LEFT:		code=KB_LEFT;					break;	// Left
						case VK_UP:			code=KB_UP;						break;	// Up
						case VK_RIGHT:		code=KB_RIGHT;					break;	// Right
						case VK_DOWN:		code=KB_DOWN;					break;	// Down
						case VK_SNAPSHOT:	code=KB_PRINT_SCREEN;			break;	// Prnt Scrn
						case VK_INSERT:		code=KB_INSERT;					break;	// Insert
						case VK_DELETE:		code=KB_DEL;					break;	// Delete
						case VK_LWIN:		code=KB_LSUPER;					break;	// Left Windows
						case VK_RWIN:		code=KB_RSUPER;					break;	// Right Windows
						case VK_APPS:		code=KB_MENU;					break;	// Application
						case VK_NUMPAD0:	code=KB_NP_0;					break;	// Num 0
						case VK_NUMPAD1:	code=KB_NP_1;					break;	// Num 1
						case VK_NUMPAD2:	code=KB_NP_2;					break;	// Num 2
						case VK_NUMPAD3:	code=KB_NP_3;					break;	// Num 3
						case VK_NUMPAD4:	code=KB_NP_4;					break;	// Num 4
						case VK_NUMPAD5:	code=KB_NP_5;					break;	// Num 5
						case VK_NUMPAD6:	code=KB_NP_6;					break;	// Num 6
						case VK_NUMPAD7:	code=KB_NP_7;					break;	// Num 7
						case VK_NUMPAD8:	code=KB_NP_8;					break;	// Num 8
						case VK_NUMPAD9:	code=KB_NP_9;					break;	// Num 9
						case VK_MULTIPLY:	code=KB_NP_MULTIPLY;			break;	// Num *
						case VK_ADD:		code=KB_NP_ADD;					break;	// Num +
						case VK_SUBTRACT:	code=KB_NP_SUBTRACT;			break;	// Num -
						case VK_DECIMAL:	code=KB_NP_DECIMAL;				break;	// Num Del
						case VK_DIVIDE:		code=KB_NP_DIVIDE;				break;	// Num /
						case VK_F1:			code=KB_F1;						break;	// F1
						case VK_F2:			code=KB_F2;						break;	// F2
						case VK_F3:			code=KB_F3;						break;	// F3
						case VK_F4:			code=KB_F4;						break;	// F4
						case VK_F5:			code=KB_F5;						break;	// F5
						case VK_F6:			code=KB_F6;						break;	// F6
						case VK_F7:			code=KB_F7;						break;	// F7
						case VK_F8:			code=KB_F8;						break;	// F8
						case VK_F9:			code=KB_F9;						break;	// F9
						case VK_F10:		code=KB_F10;					break;	// F10
						case VK_F11:		code=KB_F11;					break;	// F11
						case VK_F12:		code=KB_F12;					break;	// F12
						case VK_NUMLOCK:	code=KB_NUM_LOCK;				break;	// Num Lock
						case VK_SCROLL:		code=KB_SCROLL_LOCK;			break;	// Scroll Lock
						case VK_LSHIFT:		code=KB_LSHIFT;					break;	// Shift
						case VK_RSHIFT:		code=KB_RSHIFT;					break;	// Right Shift
						case VK_OEM_PLUS:	code=KB_EQUAL;					break;	// =
						case VK_OEM_MINUS:	code=KB_MINUS;					break;	// -
						case VK_OEM_PERIOD:	code=KB_PERIOD;					break;	// .
						case VK_OEM_1:		code=KB_SEMICOLON;				break;	// ;
						case VK_OEM_2:		code=KB_SLASH;					break;	// Frontslash
						case VK_OEM_3:		code=KB_GRAVE_ACCENT;			break;	// `
						case VK_OEM_4:		code=KB_LEFT_BRACKET;			break;	// [
						case VK_OEM_5:		code=KB_BACKSLASH;				break;	// Backslash
						case VK_OEM_6:		code=KB_RIGHT_BRACKET;			break;	// ]
						case VK_OEM_7:		code=KB_APOSTROPHE;				break;	// '
						case VK_LCONTROL:	if(Keyboard.Flags&RI_KEY_E0)
												code=KB_RCTRL;						// Right control
											 else
												code=KB_LCTRL;						// Left control
											break;
						case VK_LMENU:		if(Keyboard.Flags&RI_KEY_E0)
												code=KB_RALT;						// Right alt
											 else
												code=KB_LALT;						// Left alt
											break;
						default:			code=Keyboard.VKey;	break;	// All others
					}

					if(Keyboard.Flags&RI_KEY_BREAK)
						Event_Trigger(EVENT_KEYUP, &code);
					else
						Event_Trigger(EVENT_KEYDOWN, &code);

					break;
				}

				case RIM_TYPEMOUSE:
                {
					SetCursorPos(Width>>1, Height>>1);
					SetCursor(NULL);
					
					RAWMOUSE Mouse=Input->data.mouse;
					static MouseEvent_t MouseEvent={ 0, 0, 0, 0 };

					if(Mouse.usButtonFlags&RI_MOUSE_BUTTON_1_DOWN)
						MouseEvent.button|=MOUSE_BUTTON_1;

					if(Mouse.usButtonFlags&RI_MOUSE_BUTTON_1_UP)
						MouseEvent.button&=~MOUSE_BUTTON_1;

					if(Mouse.usButtonFlags&RI_MOUSE_BUTTON_2_DOWN)
						MouseEvent.button|=MOUSE_BUTTON_2;

					if(Mouse.usButtonFlags&RI_MOUSE_BUTTON_2_UP)
						MouseEvent.button&=~MOUSE_BUTTON_2;

					if(Mouse.usButtonFlags&RI_MOUSE_BUTTON_3_DOWN)
						MouseEvent.button|=MOUSE_BUTTON_3;

					if(Mouse.usButtonFlags&RI_MOUSE_BUTTON_3_UP)
						MouseEvent.button&=~MOUSE_BUTTON_3;

					if(Mouse.usButtonFlags&RI_MOUSE_BUTTON_4_DOWN)
						MouseEvent.button|=MOUSE_BUTTON_4;

					if(Mouse.usButtonFlags&RI_MOUSE_BUTTON_4_UP)
						MouseEvent.button&=~MOUSE_BUTTON_4;

					if(Mouse.usButtonFlags&RI_MOUSE_BUTTON_5_DOWN)
						MouseEvent.button|=MOUSE_BUTTON_5;

					if(Mouse.usButtonFlags&RI_MOUSE_BUTTON_5_UP)
						MouseEvent.button&=~MOUSE_BUTTON_5;

					if(Mouse.usButtonFlags&RI_MOUSE_WHEEL)
						MouseEvent.dz=Mouse.usButtonData;

					if(Mouse.usFlags==MOUSE_MOVE_RELATIVE)
					{
						if(Mouse.lLastX!=0||Mouse.lLastY!=0)
						{
							MouseEvent.dx=Mouse.lLastX;
							MouseEvent.dy=-Mouse.lLastY;
							Event_Trigger(EVENT_MOUSEMOVE, &MouseEvent);
						}
					}

					if(Mouse.usButtonFlags&(RI_MOUSE_BUTTON_1_DOWN|RI_MOUSE_BUTTON_2_DOWN|RI_MOUSE_BUTTON_3_DOWN|RI_MOUSE_BUTTON_4_DOWN|RI_MOUSE_BUTTON_5_DOWN))
						Event_Trigger(EVENT_MOUSEDOWN, &MouseEvent);
					else if(Mouse.usButtonFlags&(RI_MOUSE_BUTTON_1_UP|RI_MOUSE_BUTTON_2_UP|RI_MOUSE_BUTTON_3_UP|RI_MOUSE_BUTTON_4_UP|RI_MOUSE_BUTTON_5_UP))
						Event_Trigger(EVENT_MOUSEUP, &MouseEvent);
					break;
				}

				default:
					break;
			}
		}

		default:
			break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

bool RegisterRawInput(HWND hWnd)
{
	RAWINPUTDEVICE devices[2];

	// Keyboard
	devices[0].usUsagePage=HID_USAGE_PAGE_GENERIC;
	devices[0].usUsage=HID_USAGE_GENERIC_KEYBOARD;
	devices[0].dwFlags=0; // RIDEV_NOLEGACY ?
	devices[0].hwndTarget=hWnd;

	// Mouse
	devices[1].usUsagePage=HID_USAGE_PAGE_GENERIC;
	devices[1].usUsage=HID_USAGE_GENERIC_MOUSE;
	devices[1].dwFlags=RIDEV_NOLEGACY;
	devices[1].hwndTarget=hWnd;

	DBGPRINTF(DEBUG_INFO, "Registering raw input devices...\n");

	if(RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE)))
	{
		DBGPRINTF(DEBUG_INFO, "\t...registered raw input devices.\n");
		return true;
	}
	else
	{
		DBGPRINTF(DEBUG_ERROR, "\t...failed to register raw input devices.\n");
		return false;
	}
}

#ifndef _CONSOLE
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int iCmdShow)
{
	if(AllocConsole())
	{
		FILE *fDummy;
		freopen_s(&fDummy, "CONOUT$", "w", stdout);
		freopen_s(&fDummy, "CONOUT$", "w", stderr);
		freopen_s(&fDummy, "CONIN$", "r", stdin);
		
		HANDLE hOutput=GetStdHandle(STD_OUTPUT_HANDLE);
		DWORD dwMode;

		GetConsoleMode(hOutput, &dwMode);
		SetConsoleMode(hOutput, dwMode|ENABLE_PROCESSED_OUTPUT|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
	}
#else
int main(int argc, char **argv)
{
	HINSTANCE hInstance=GetModuleHandle(NULL);
	HANDLE hOutput=GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD dwMode;
	
	GetConsoleMode(hOutput, &dwMode);
	SetConsoleMode(hOutput, dwMode|ENABLE_PROCESSED_OUTPUT|ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif

	DBGPRINTF(DEBUG_INFO, "Allocating zone memory...\n");
	Zone=Zone_Init(256*1000*1000);

	if(Zone==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "\t...zone allocation failed!\n");

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
	AdjustWindowRect(&Rect, WS_POPUP, FALSE);

	Context.hWnd=CreateWindow(szAppName, szAppName, WS_POPUP|WS_CLIPSIBLINGS, 0, 0, Rect.right-Rect.left, Rect.bottom-Rect.top, NULL, NULL, hInstance, NULL);

	RegisterRawInput(Context.hWnd);

	ShowWindow(Context.hWnd, SW_SHOW);
	SetForegroundWindow(Context.hWnd);

	DBGPRINTF(DEBUG_INFO, "Creating Vulkan instance...\n");
	if(!CreateVulkanInstance(&Instance))
	{
		DBGPRINTF(DEBUG_ERROR, "\t...failed.\n");
		return -1;
	}

	DBGPRINTF(DEBUG_INFO, "Creating Vulkan context...\n");
	if(!CreateVulkanContext(Instance, &Context))
	{
		DBGPRINTF(DEBUG_ERROR, "\t...failed.\n");
		return -1;
	}

	DBGPRINTF(DEBUG_INFO, "Creating swapchain...\n");
	if(!vkuCreateSwapchain(&Context, &Swapchain, VK_TRUE))
	{
		DBGPRINTF(DEBUG_ERROR, "\t...failed.\n");
		return -1;
	}

	DBGPRINTF(DEBUG_INFO, "Initializing VR...\n");
	if(!VR_Init(Instance, &Context))
	{
		DBGPRINTF(DEBUG_ERROR, "\t...failed, turning off VR support.\n");
		IsVR=false;
		rtWidth=Width;
		rtHeight=Height;
	}
	else
		MoveWindow(Context.hWnd, 0, 0, rtWidth, rtHeight/2, TRUE);

	DBGPRINTF(DEBUG_INFO, "Initializing Vulkan resources...\n");
	if(!Init())
	{
		DBGPRINTF(DEBUG_ERROR, "\t...initializing failed.\n");
		return -1;
	}

	DBGPRINTF(DEBUG_INFO, "\nCurrent system zone memory allocations:\n");
	Zone_Print(Zone);

	DBGPRINTF(DEBUG_INFO, "\nCurrent vulkan zone memory allocations:\n");
	vkuMem_Print(VkZone);

	DBGPRINTF(DEBUG_INFO, "\nStarting main loop.\n");
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

	DBGPRINTF(DEBUG_INFO, "Shutting down...\n");
	Destroy();
	DestroyVulkan(Instance, &Context);
	vkDestroyInstance(Instance, VK_NULL_HANDLE);
	DestroyWindow(Context.hWnd);

	DBGPRINTF(DEBUG_INFO, "Zone remaining block list:\n");
	Zone_Print(Zone);
	Zone_Destroy(Zone);

#ifndef _CONSOLE
	system("pause");

	FreeConsole();
#endif

	return 0;
}
