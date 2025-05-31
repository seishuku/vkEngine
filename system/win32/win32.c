#include <windows.h>
#include <hidusage.h>
#include <Xinput.h>
#include <stdio.h>
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

bool isDone=false;
bool toggleFullscreen=true;

extern XruContext_t xrContext;

extern VkInstance vkInstance;
extern VkuContext_t vkContext;

extern VkuSwapchain_t swapchain;

float fps=0.0f, fTimeStep=0.0f, fTime=0.0f;

extern vec2 leftThumbstick, rightThumbstick;

void Render(void);
bool Init(void);
void RecreateSwapchain(void);
void Destroy(void);

double GetClock(void)
{
	static uint64_t frequency=0;
	uint64_t count;

	if(!frequency)
		QueryPerformanceFrequency((LARGE_INTEGER *)&frequency);

	QueryPerformanceCounter((LARGE_INTEGER *)&count);

	return (double)count/frequency;
}

static bool RegisterRawInput(HWND hWnd)
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

static bool UnregisterRawInput(void)
{
	RAWINPUTDEVICE devices[2];

	// Keyboard
	devices[0].usUsagePage=HID_USAGE_PAGE_GENERIC;
	devices[0].usUsage=HID_USAGE_GENERIC_KEYBOARD;
	devices[0].dwFlags=RIDEV_REMOVE;
	devices[0].hwndTarget=NULL;

	// Mouse
	devices[1].usUsagePage=HID_USAGE_PAGE_GENERIC;
	devices[1].usUsage=HID_USAGE_GENERIC_MOUSE;
	devices[1].dwFlags=RIDEV_REMOVE;
	devices[1].hwndTarget=NULL;

	DBGPRINTF(DEBUG_INFO, "Unregistering raw input devices...\n");

	if(RegisterRawInputDevices(devices, 2, sizeof(RAWINPUTDEVICE)))
	{
		DBGPRINTF(DEBUG_INFO, "\t...unregistered raw input devices.\n");
		return true;
	}
	else
	{
		DBGPRINTF(DEBUG_ERROR, "\t...failed to unregister raw input devices.\n");
		return false;
	}
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
			config.windowWidth=LOWORD(lParam);
			config.windowHeight=HIWORD(lParam);
			break;

		case WM_ACTIVATE:
			if(LOWORD(wParam)!=WA_INACTIVE)
			{
				RECT rc;

				while(!ShowCursor(FALSE));

				GetWindowRect(hWnd, &rc);
				ClipCursor(&rc);
				RegisterRawInput(hWnd);
			}
			else
			{
				ShowCursor(TRUE);
				UnregisterRawInput();
			}
			break;

		case WM_SYSKEYUP:
			if(HIWORD(lParam)&KF_ALTDOWN&&LOWORD(wParam)==VK_RETURN)
			{
				static uint32_t OldWidth, OldHeight;

				if(toggleFullscreen)
				{
					toggleFullscreen=false;
					DBGPRINTF(DEBUG_INFO, "Going full screen...\n");

					OldWidth=config.windowWidth;
					OldHeight=config.windowHeight;

					config.windowWidth=GetSystemMetrics(SM_CXSCREEN);
					config.windowHeight=GetSystemMetrics(SM_CYSCREEN);
					SetWindowPos(vkContext.hWnd, HWND_TOPMOST, 0, 0, config.windowWidth, config.windowHeight, 0);
				}
				else
				{
					toggleFullscreen=true;
					DBGPRINTF(DEBUG_INFO, "Going windowed...\n");

					config.windowWidth=OldWidth;
					config.windowHeight=OldHeight;
					SetWindowPos(vkContext.hWnd, HWND_TOPMOST, 0, 0, config.windowWidth, config.windowHeight, 0);
				}
			}
			break;

		case WM_INPUT:
		{
#define RAWMESSAGE_SIZE 64
			BYTE bRawMessage[RAWMESSAGE_SIZE];
			UINT dwSize=0;

			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));

			// Check if the message size exceeds the buffer size, shouldn't happen have to be safe.
			if(dwSize>RAWMESSAGE_SIZE)
				break;

			GetRawInputData((HRAWINPUT)lParam, RID_INPUT, bRawMessage, &dwSize, sizeof(RAWINPUTHEADER));

			RAWINPUT *input=(RAWINPUT *)bRawMessage;

			switch(input->header.dwType)
			{
				case RIM_TYPEKEYBOARD:
				{
					RAWKEYBOARD keyboard=input->data.keyboard;

					if(keyboard.VKey==0xFF)
						break;

					// Specific case for escape key to quit application
					if(keyboard.VKey==VK_ESCAPE)
					{
						PostQuitMessage(0);
						return 0;
					}

					// Specific case to remap the shift/control/alt virtual keys
					if(keyboard.VKey==VK_SHIFT)
						keyboard.VKey=MapVirtualKey(keyboard.MakeCode, MAPVK_VSC_TO_VK_EX);

					if(keyboard.VKey==VK_CONTROL)
						keyboard.VKey=MapVirtualKey(keyboard.MakeCode, MAPVK_VSC_TO_VK_EX);

					if(keyboard.VKey==VK_MENU)
						keyboard.VKey=MapVirtualKey(keyboard.MakeCode, MAPVK_VSC_TO_VK_EX);

					// Remap from Windows virtual keys to application key enums
					// (will probably reformat this later)
					uint32_t code=KB_UNKNOWN;

					switch(keyboard.VKey)
					{
						case VK_BACK:		code=KB_BACKSPACE;				break;	// Backspace
						case VK_TAB:		code=KB_TAB;					break;	// Tab
						case VK_RETURN:		if(keyboard.Flags&RI_KEY_E0)
							code=KB_NP_ENTER;					// Numpad enter
									  else
							code=KB_ENTER;						// Enter
							break;
						case VK_PAUSE:		code=KB_PAUSE;					break;	// pause
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
						case VK_LCONTROL:	if(keyboard.Flags&RI_KEY_E0)
							code=KB_RCTRL;						// Right control
										else
							code=KB_LCTRL;						// Left control
							break;
						case VK_LMENU:		if(keyboard.Flags&RI_KEY_E0)
							code=KB_RALT;						// Right alt
									 else
							code=KB_LALT;						// Left alt
							break;
						default:			code=keyboard.VKey;	break;	// All others
					}

					if(keyboard.Flags&RI_KEY_BREAK)
						Event_Trigger(EVENT_KEYUP, &code);
					else
						Event_Trigger(EVENT_KEYDOWN, &code);

					break;
				}

				case RIM_TYPEMOUSE:
				{
					RAWMOUSE mouse=input->data.mouse;
					static MouseEvent_t mouseEvent={ 0, 0, 0, 0 };

					if(mouse.usButtonFlags&RI_MOUSE_BUTTON_1_DOWN)
						mouseEvent.button|=MOUSE_BUTTON_1;

					if(mouse.usButtonFlags&RI_MOUSE_BUTTON_1_UP)
						mouseEvent.button&=~MOUSE_BUTTON_1;

					if(mouse.usButtonFlags&RI_MOUSE_BUTTON_2_DOWN)
						mouseEvent.button|=MOUSE_BUTTON_2;

					if(mouse.usButtonFlags&RI_MOUSE_BUTTON_2_UP)
						mouseEvent.button&=~MOUSE_BUTTON_2;

					if(mouse.usButtonFlags&RI_MOUSE_BUTTON_3_DOWN)
						mouseEvent.button|=MOUSE_BUTTON_3;

					if(mouse.usButtonFlags&RI_MOUSE_BUTTON_3_UP)
						mouseEvent.button&=~MOUSE_BUTTON_3;

					if(mouse.usButtonFlags&RI_MOUSE_BUTTON_4_DOWN)
						mouseEvent.button|=MOUSE_BUTTON_4;

					if(mouse.usButtonFlags&RI_MOUSE_BUTTON_4_UP)
						mouseEvent.button&=~MOUSE_BUTTON_4;

					if(mouse.usButtonFlags&RI_MOUSE_BUTTON_5_DOWN)
						mouseEvent.button|=MOUSE_BUTTON_5;

					if(mouse.usButtonFlags&RI_MOUSE_BUTTON_5_UP)
						mouseEvent.button&=~MOUSE_BUTTON_5;

					if(mouse.usButtonFlags&RI_MOUSE_WHEEL)
						mouseEvent.dz=mouse.usButtonData;

					if(mouse.usFlags==MOUSE_MOVE_RELATIVE)
					{
						if(mouse.lLastX!=0||mouse.lLastY!=0)
						{
							mouseEvent.dx=mouse.lLastX;
							mouseEvent.dy=-mouse.lLastY;
							Event_Trigger(EVENT_MOUSEMOVE, &mouseEvent);
						}
					}

					if(mouse.usButtonFlags&(RI_MOUSE_BUTTON_1_DOWN|RI_MOUSE_BUTTON_2_DOWN|RI_MOUSE_BUTTON_3_DOWN|RI_MOUSE_BUTTON_4_DOWN|RI_MOUSE_BUTTON_5_DOWN))
					{
						Event_Trigger(EVENT_MOUSEDOWN, &mouseEvent);
						Event_Trigger(EVENT_MOUSEMOVE, &mouseEvent);
					}
					else if(mouse.usButtonFlags&(RI_MOUSE_BUTTON_1_UP|RI_MOUSE_BUTTON_2_UP|RI_MOUSE_BUTTON_3_UP|RI_MOUSE_BUTTON_4_UP|RI_MOUSE_BUTTON_5_UP))
					{
						Event_Trigger(EVENT_MOUSEUP, &mouseEvent);
						Event_Trigger(EVENT_MOUSEMOVE, &mouseEvent);
					}
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

static vec2 ApplyScaleDeadzone(LONG x, LONG y, LONG maxX, LONG maxY, float deadzoneThreshold)
{
	vec2 d={ ((float)x/maxX), ((float)y/maxY) };

	if(fabsf(d.x)<deadzoneThreshold)
		d.x=0.0f;
	if(fabsf(d.y)<deadzoneThreshold)
		d.y=0.0f;

	return d;
}

#if 0
#include "../../renderdoc_app.h"
RENDERDOC_API_1_6_0 *rdoc_api=NULL;
#endif

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

	SetRect(&Rect, 0, 0, config.windowWidth, config.windowHeight);
	AdjustWindowRect(&Rect, WS_POPUP, FALSE);

	vkContext.hWnd=CreateWindow(szAppName, szAppName, WS_POPUP|WS_CLIPSIBLINGS, 0, 0, Rect.right-Rect.left, Rect.bottom-Rect.top, NULL, NULL, hInstance, NULL);

	ShowWindow(vkContext.hWnd, SW_SHOW);
	SetForegroundWindow(vkContext.hWnd);

	DBGPRINTF(DEBUG_INFO, "Creating Vulkan instance...\n");
	if(!vkuCreateInstance(&vkInstance))
	{
		DBGPRINTF(DEBUG_ERROR, "\t...failed.\n");
		return -1;
	}

	vkContext.deviceIndex=config.deviceIndex;

	DBGPRINTF(DEBUG_INFO, "Creating Vulkan context...\n");
	if(!vkuCreateContext(vkInstance, &vkContext))
	{
		DBGPRINTF(DEBUG_ERROR, "\t...failed.\n");
		return -1;
	}

	DBGPRINTF(DEBUG_INFO, "Creating swapchain...\n");
	if(!vkuCreateSwapchain(&vkContext, &swapchain, VK_TRUE))
	{
		DBGPRINTF(DEBUG_ERROR, "\t...failed.\n");
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
		MoveWindow(vkContext.hWnd, 0, 0, config.windowWidth/2, config.windowHeight/2, TRUE);
	}

	DBGPRINTF(DEBUG_INFO, "Initializing Vulkan resources...\n");
	if(!Init())
	{
		DBGPRINTF(DEBUG_ERROR, "\t...initializing failed.\n");
		return -1;
	}

	DBGPRINTF(DEBUG_INFO, "\nCurrent system zone memory allocations:\n");
	Zone_Print(zone);

#if 0
	// RenderDoc frame capture for VR mode
	HMODULE mod=GetModuleHandleA("renderdoc.dll");
	if(mod)
	{
		pRENDERDOC_GetAPI RENDERDOC_GetAPI=(pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");

		if(!RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_6_0, (void **)&rdoc_api))
			return -1;
	}
	
	bool captureThisFrame=false;
#endif

	DBGPRINTF(DEBUG_INFO, "\nStarting main loop.\n");
	while(!isDone)
	{
		MSG msg;

		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if(msg.message==WM_QUIT)
				isDone=true;
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else
		{
			// TODO: Find a better place for gamepad input handling!
			XINPUT_STATE state={ 0 };

			if(XInputGetState(0, &state)==ERROR_SUCCESS)
			{
				static uint32_t qKeyShot=0, eKeyShot=0, cKeyShot=0, vKeyShot=0;

				leftThumbstick=ApplyScaleDeadzone(state.Gamepad.sThumbLX, state.Gamepad.sThumbLY, INT16_MAX, INT16_MAX, 0.1f);
				rightThumbstick=ApplyScaleDeadzone(state.Gamepad.sThumbRX, state.Gamepad.sThumbRY, INT16_MAX, INT16_MAX, 0.1f);

				if(state.Gamepad.wButtons&XINPUT_GAMEPAD_A)
				{
					if(!cKeyShot)
					{
						cKeyShot=KB_C;
						Event_Trigger(EVENT_KEYDOWN, &cKeyShot);
					}
				}
				else
				{
					if(cKeyShot)
					{
						Event_Trigger(EVENT_KEYUP, &cKeyShot);
						cKeyShot=0;
					}
				}

				if(state.Gamepad.wButtons&XINPUT_GAMEPAD_X)
				{
					if(!vKeyShot)
					{
						vKeyShot=KB_V;
						Event_Trigger(EVENT_KEYDOWN, &vKeyShot);
					}
				}
				else
				{
					if(vKeyShot)
					{
						Event_Trigger(EVENT_KEYUP, &vKeyShot);
						vKeyShot=0;
					}
				}

				if(state.Gamepad.wButtons&XINPUT_GAMEPAD_LEFT_SHOULDER)
				{
					if(!qKeyShot)
					{
						qKeyShot=KB_Q;
						Event_Trigger(EVENT_KEYDOWN, &qKeyShot);
					}
				}
				else
				{
					if(qKeyShot)
					{
						Event_Trigger(EVENT_KEYUP, &qKeyShot);
						qKeyShot=0;
					}
				}

				if(state.Gamepad.wButtons&XINPUT_GAMEPAD_RIGHT_SHOULDER)
				{
					if(!eKeyShot)
					{
						eKeyShot=KB_E;
						Event_Trigger(EVENT_KEYDOWN, &eKeyShot);
					}
				}
				else
				{
					if(eKeyShot)
					{
						Event_Trigger(EVENT_KEYUP, &eKeyShot);
						eKeyShot=0;
					}
				}
			}

			static float avgFPS=0.0f;

			double startTime=GetClock();

#if 0
			// RenderDoc frame capture for VR mode
			if(captureThisFrame&&rdoc_api)
				rdoc_api->StartFrameCapture(NULL, NULL);
#endif

			Render();

			fTimeStep=(float)(GetClock()-startTime);
			fTime+=fTimeStep;

			avgFPS+=1.0f/fTimeStep;

			static uint32_t frameCount=0;

#if 0
			// RenderDoc frame capture for VR mode
			if(captureThisFrame&&rdoc_api)
			{
				captureThisFrame=false;
				rdoc_api->EndFrameCapture(NULL, NULL);
			}
#endif

			if(frameCount++>100)
			{
//				captureThisFrame=true;

				fps=avgFPS/frameCount;
				avgFPS=0.0f;
				frameCount=0;
			}
		}
	}

	DBGPRINTF(DEBUG_INFO, "Shutting down...\n");
	Destroy();
	vkuDestroyContext(vkInstance, &vkContext);
	vkuDestroyInstance(vkInstance);

	DestroyWindow(vkContext.hWnd);

	DBGPRINTF(DEBUG_INFO, "Zone remaining block list:\n");
	Zone_Print(zone);
	Zone_Destroy(zone);

#ifndef _CONSOLE
	system("pause");

	FreeConsole();
#endif

	return 0;
}
