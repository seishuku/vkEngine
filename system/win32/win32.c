#include <windows.h>
#include <hidusage.h>
#include <xinput.h>
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
#include "../../input/input.h"

MemZone_t *zone;

char szAppName[]="Vulkan";

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

static Keycodes_t ConvertKeymap(uint32_t vkey)
{
	switch (vkey)
	{
		case VK_BACK:			return KB_BACKSPACE;
		case VK_TAB:			return KB_TAB;
		case VK_RETURN:			return KB_ENTER;
		case VK_PAUSE:			return KB_PAUSE;
		case VK_CAPITAL:		return KB_CAPS_LOCK;
		case VK_ESCAPE:			return KB_ESCAPE;
		case VK_SPACE:			return KB_SPACE;
		case VK_PRIOR:			return KB_PAGE_UP;
		case VK_NEXT:			return KB_PAGE_DOWN;
		case VK_END:			return KB_END;
		case VK_HOME:			return KB_HOME;
		case VK_LEFT:			return KB_LEFT;
		case VK_UP:				return KB_UP;
		case VK_RIGHT:			return KB_RIGHT;
		case VK_DOWN:			return KB_DOWN;
		case VK_SNAPSHOT:		return KB_PRINT_SCREEN;
		case VK_INSERT:			return KB_INSERT;
		case VK_DELETE:			return KB_DEL;
		case VK_LWIN:			return KB_LSUPER;
		case VK_RWIN:			return KB_RSUPER;
		case VK_APPS:			return KB_MENU;
		case VK_NUMPAD0:		return KB_NP_0;
		case VK_NUMPAD1:		return KB_NP_1;
		case VK_NUMPAD2:		return KB_NP_2;
		case VK_NUMPAD3:		return KB_NP_3;
		case VK_NUMPAD4:		return KB_NP_4;
		case VK_NUMPAD5:		return KB_NP_5;
		case VK_NUMPAD6:		return KB_NP_6;
		case VK_NUMPAD7:		return KB_NP_7;
		case VK_NUMPAD8:		return KB_NP_8;
		case VK_NUMPAD9:		return KB_NP_9;
		case VK_MULTIPLY:		return KB_NP_MULTIPLY;
		case VK_ADD:			return KB_NP_ADD;
		case VK_SUBTRACT:		return KB_NP_SUBTRACT;
		case VK_DECIMAL:		return KB_NP_DECIMAL;
		case VK_DIVIDE:			return KB_NP_DIVIDE;
		case VK_F1:				return KB_F1;
		case VK_F2:				return KB_F2;
		case VK_F3:				return KB_F3;
		case VK_F4:				return KB_F4;
		case VK_F5:				return KB_F5;
		case VK_F6:				return KB_F6;
		case VK_F7:				return KB_F7;
		case VK_F8:				return KB_F8;
		case VK_F9:				return KB_F9;
		case VK_F10:			return KB_F10;
		case VK_F11:			return KB_F11;
		case VK_F12:			return KB_F12;
		case VK_NUMLOCK:		return KB_NUM_LOCK;
		case VK_SCROLL:			return KB_SCROLL_LOCK;
		case VK_LSHIFT:			return KB_LSHIFT;
		case VK_RSHIFT:			return KB_RSHIFT;
		case VK_LCONTROL:		return KB_LCTRL;
		case VK_RCONTROL:		return KB_RCTRL;
		case VK_LMENU:			return KB_LALT;
		case VK_RMENU:			return KB_RALT;
		case VK_OEM_PLUS:		return KB_EQUAL;
		case VK_OEM_MINUS:		return KB_MINUS;
		case VK_OEM_PERIOD:		return KB_PERIOD;
		case VK_OEM_COMMA:		return KB_COMMA;
		case VK_OEM_1:			return KB_SEMICOLON;
		case VK_OEM_2:			return KB_SLASH;
		case VK_OEM_3:			return KB_GRAVE_ACCENT;
		case VK_OEM_4:			return KB_LEFT_BRACKET;
		case VK_OEM_5:			return KB_BACKSLASH;
		case VK_OEM_6:			return KB_RIGHT_BRACKET;
		case VK_OEM_7:			return KB_APOSTROPHE;
		default:
			// Handle ASCII codes (A-Z, 0-9)
			if(vkey>='A'&&vkey<='Z')
				return (Keycodes_t)vkey;
			if (vkey >= '0' && vkey <= '9')
				return (Keycodes_t)vkey;

			return KB_UNKNOWN;
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

					// Convert Windows virtual key to engine keycode
					Keycodes_t code=ConvertKeymap(keyboard.VKey);

					// Route through input system
					bool pressed=!(keyboard.Flags&RI_KEY_BREAK);
					Input_OnKeyEvent(code, pressed);

					break;
				}

				case RIM_TYPEMOUSE:
				{
					RAWMOUSE mouse=input->data.mouse;
					static MouseEvent_t mouseEvent={ 0, 0, 0, 0 };
					static vec2 mousePos = {0, 0};

					if(mouse.usButtonFlags & RI_MOUSE_BUTTON_1_DOWN)
			        {
						mouseEvent.button|=MOUSE_BUTTON_1;
				        Input_OnMouseButtonEvent(MOUSE_BUTTON_1, true);
			        }
			        if(mouse.usButtonFlags & RI_MOUSE_BUTTON_1_UP)
			        {
						mouseEvent.button&=~MOUSE_BUTTON_1;
				        Input_OnMouseButtonEvent(MOUSE_BUTTON_1, false);
			        }
			        if(mouse.usButtonFlags & RI_MOUSE_BUTTON_2_DOWN)
			        {
						mouseEvent.button|=MOUSE_BUTTON_2;
				        Input_OnMouseButtonEvent(MOUSE_BUTTON_2, true);
			        }
			        if(mouse.usButtonFlags & RI_MOUSE_BUTTON_2_UP)
			        {
						mouseEvent.button&=~MOUSE_BUTTON_2;
				        Input_OnMouseButtonEvent(MOUSE_BUTTON_2, false);
			        }
			        if(mouse.usButtonFlags & RI_MOUSE_BUTTON_3_DOWN)
			        {
						mouseEvent.button|=MOUSE_BUTTON_3;
				        Input_OnMouseButtonEvent(MOUSE_BUTTON_3, true);
			        }
			        if(mouse.usButtonFlags & RI_MOUSE_BUTTON_3_UP)
			        {
						mouseEvent.button&=~MOUSE_BUTTON_3;
				        Input_OnMouseButtonEvent(MOUSE_BUTTON_3, false);
			        }
			        if(mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN)
			        {
						mouseEvent.button|=MOUSE_BUTTON_4;
				        Input_OnMouseButtonEvent(MOUSE_BUTTON_4, true);
			        }
			        if(mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP)
			        {
						mouseEvent.button&=~MOUSE_BUTTON_4;
				        Input_OnMouseButtonEvent(MOUSE_BUTTON_4, false);
			        }
			        if(mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN)
			        {
						mouseEvent.button|=MOUSE_BUTTON_5;
				        Input_OnMouseButtonEvent(MOUSE_BUTTON_5, true);
			        }
			        if(mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP)
			        {
						mouseEvent.button&=~MOUSE_BUTTON_5;
				        Input_OnMouseButtonEvent(MOUSE_BUTTON_5, false);
			        }

					if(mouse.usButtonFlags&RI_MOUSE_WHEEL)
						mouseEvent.dz=mouse.usButtonData;

					if(mouse.usFlags==MOUSE_MOVE_RELATIVE)
					{
						if(mouse.lLastX!=0||mouse.lLastY!=0)
						{
							mouseEvent.dx=mouse.lLastX;
							mouseEvent.dy=-mouse.lLastY;

							// Route through input system (cursor is at window center after warp)
							Input_OnMouseEvent(&mouseEvent, Vec2(config.windowWidth/2.0f, config.windowHeight/2.0f));

							// Warp cursor back to center of window
							RECT rc;
							GetWindowRect(vkContext.hWnd, &rc);
							SetCursorPos((rc.left+rc.right)/2, (rc.top+rc.bottom)/2);
						}
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
	if(!vkuCreateSwapchain(&vkContext, &swapchain, config.vsync))
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
		config.renderWidth=xrContext.swapchain[0].extent.width;
		config.renderHeight=xrContext.swapchain[0].extent.height;
		config.windowWidth=config.renderWidth;
		config.windowHeight=config.renderHeight;
		MoveWindow(vkContext.hWnd, 0, 0, config.windowWidth/2, config.windowHeight/2, TRUE);
		config.isVR=true;
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
			// Render frame
			static float avgFPS=0.0f;

			double startTime=GetClock();

#if 0
			// RenderDoc frame capture for VR mode
			if(captureThisFrame&&rdoc_api)
				rdoc_api->StartFrameCapture(NULL, NULL);
#endif

			Input_Update();
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
