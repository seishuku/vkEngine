#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <syscall.h>
#include <time.h>
#include <unistd.h>
#include <wayland-client.h>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>
#include "wayland/xdg-shell.h"
#include "wayland/relative-pointer.h"
#include "wayland/pointer-constraints.h"
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
	struct timespec ts;

	if(!clock_gettime(CLOCK_MONOTONIC, &ts))
		return ts.tv_sec+(double)ts.tv_nsec/1000000000.0;

	return 0.0;
}

static struct wl_registry *registry=NULL;
static struct wl_compositor *compositor=NULL;
static struct wl_shm *shm=NULL;

static struct wl_seat *seat=NULL;
static struct wl_keyboard *keyboard=NULL;
static struct wl_pointer *pointer=NULL;

static struct zwp_relative_pointer_manager_v1 *relativePointerManager=NULL;
static struct zwp_relative_pointer_v1 *relativePointer=NULL;

static struct zwp_pointer_constraints_v1 *pointerConstraints=NULL;

static struct xkb_context *xkbContext=NULL;
static struct xkb_keymap *keymap=NULL;
static struct xkb_state *xkbState=NULL;

static struct xdg_wm_base *shell=NULL;
static struct xdg_surface *shellSurface=NULL;
static struct xdg_toplevel *toplevel=NULL;

static void handleShellPing(void *data, struct xdg_wm_base *shell, uint32_t serial)
{
    xdg_wm_base_pong(shell, serial);
}

static void handleShellSurfaceConfigure(void *data, struct xdg_surface *shellSurface, uint32_t serial)
{
    xdg_surface_ack_configure(shellSurface, serial);
}

static void handleToplevelConfigure(void *data, struct xdg_toplevel *toplevel, int32_t width, int32_t height, struct wl_array *states)
{
}

static void handleToplevelClose(void *data, struct xdg_toplevel *toplevel)
{
    isDone=true;
}

static void handleKeymap(void *data, struct wl_keyboard *wl_keyboard, uint32_t format, int fd, uint32_t size)
{
	char *keymapString=mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);

    xkb_keymap_unref(keymap);
	keymap=xkb_keymap_new_from_string(xkbContext, keymapString, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
	munmap(keymapString, size);
	close(fd);
	xkb_state_unref(xkbState);
	xkbState=xkb_state_new(keymap);
}

static void handleEnter(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
}

static void handleLeave(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, struct wl_surface *surface)
{
}

static Keycodes_t ConvertKeymap(uint32_t xkb_keycode)
{
	switch (xkb_keycode)
	{
		case XKB_KEY_BackSpace:		return KB_BACKSPACE;
		case XKB_KEY_Tab:			return KB_TAB;
		case XKB_KEY_Return:		return KB_ENTER;
		case XKB_KEY_Pause:			return KB_PAUSE;
		case XKB_KEY_Escape:		return KB_ESCAPE;
		case XKB_KEY_space:			return KB_SPACE;
		case XKB_KEY_Prior:			return KB_PAGE_UP;
		case XKB_KEY_Next:			return KB_PAGE_DOWN;
		case XKB_KEY_End:			return KB_END;
		case XKB_KEY_Home:			return KB_HOME;
		case XKB_KEY_Left:			return KB_LEFT;
		case XKB_KEY_Up:			return KB_UP;
		case XKB_KEY_Right:			return KB_RIGHT;
		case XKB_KEY_Down:			return KB_DOWN;
		case XKB_KEY_Print:			return KB_PRINT_SCREEN;
		case XKB_KEY_Insert:		return KB_INSERT;
		case XKB_KEY_Delete:		return KB_DEL;
		case XKB_KEY_Super_L:		return KB_LSUPER;
		case XKB_KEY_Super_R:		return KB_RSUPER;
		case XKB_KEY_Menu:			return KB_MENU;
		case XKB_KEY_KP_0:			return KB_NP_0;
		case XKB_KEY_KP_1:			return KB_NP_1;
		case XKB_KEY_KP_2:			return KB_NP_2;
		case XKB_KEY_KP_3:			return KB_NP_3;
		case XKB_KEY_KP_4:			return KB_NP_4;
		case XKB_KEY_KP_5:			return KB_NP_5;
		case XKB_KEY_KP_6:			return KB_NP_6;
		case XKB_KEY_KP_7:			return KB_NP_7;
		case XKB_KEY_KP_8:			return KB_NP_8;
		case XKB_KEY_KP_9:			return KB_NP_9;
		case XKB_KEY_KP_Multiply:	return KB_NP_MULTIPLY;
		case XKB_KEY_KP_Add:		return KB_NP_ADD;
		case XKB_KEY_KP_Subtract:	return KB_NP_SUBTRACT;
		case XKB_KEY_KP_Decimal:	return KB_NP_DECIMAL;
		case XKB_KEY_KP_Divide:		return KB_NP_DIVIDE;
		case XKB_KEY_KP_Enter:		return KB_NP_ENTER;
		case XKB_KEY_KP_Equal:		return KB_NP_EQUAL;
		case XKB_KEY_F1:			return KB_F1;
		case XKB_KEY_F2:			return KB_F2;
		case XKB_KEY_F3:			return KB_F3;
		case XKB_KEY_F4:			return KB_F4;
		case XKB_KEY_F5:			return KB_F5;
		case XKB_KEY_F6:			return KB_F6;
		case XKB_KEY_F7:			return KB_F7;
		case XKB_KEY_F8:			return KB_F8;
		case XKB_KEY_F9:			return KB_F9;
		case XKB_KEY_F10:			return KB_F10;
		case XKB_KEY_F11:			return KB_F11;
		case XKB_KEY_F12:			return KB_F12;
		case XKB_KEY_Num_Lock:		return KB_NUM_LOCK;
		case XKB_KEY_Scroll_Lock:	return KB_SCROLL_LOCK;
		case XKB_KEY_Shift_L:		return KB_LSHIFT;
		case XKB_KEY_Shift_R:		return KB_RSHIFT;
		case XKB_KEY_Control_L:		return KB_LCTRL;
		case XKB_KEY_Control_R:		return KB_RCTRL;
		case XKB_KEY_Alt_L:			return KB_LALT;
		case XKB_KEY_Alt_R:			return KB_RALT;
		case XKB_KEY_Caps_Lock:		return KB_CAPS_LOCK;
		case XKB_KEY_apostrophe:	return KB_APOSTROPHE;
		case XKB_KEY_comma:			return KB_COMMA;
		case XKB_KEY_minus:			return KB_MINUS;
		case XKB_KEY_period:		return KB_PERIOD;
		case XKB_KEY_slash:			return KB_SLASH;
		case XKB_KEY_semicolon:		return KB_SEMICOLON;
		case XKB_KEY_equal:			return KB_EQUAL;
		case XKB_KEY_bracketleft:	return KB_LEFT_BRACKET;
		case XKB_KEY_backslash:		return KB_BACKSLASH;
		case XKB_KEY_bracketright:	return KB_RIGHT_BRACKET;
		case XKB_KEY_grave:			return KB_GRAVE_ACCENT;
		default:
			// Handle ASCII codes
			if(xkb_keycode>='A'&&xkb_keycode<='Z')
				return (Keycodes_t)xkb_keycode;

			if(xkb_keycode>='a'&&xkb_keycode<='z')
				return (Keycodes_t)(xkb_keycode-'a'+'A');  // Convert to uppercase

			if(xkb_keycode>='0'&&xkb_keycode<='9')
				return (Keycodes_t)xkb_keycode;

			return KB_UNKNOWN;
	}
}

static void handleKey(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    xkb_keysym_t keysym=xkb_state_key_get_one_sym(xkbState, key+8);

    if(keysym==XKB_KEY_Escape)
        isDone=true;

	Keycodes_t code=ConvertKeymap(keysym);
	bool pressed=(state==WL_KEYBOARD_KEY_STATE_PRESSED);

	Input_OnKeyEvent(code, pressed);
}

static void handleModifiers(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{
    xkb_state_update_mask(xkbState, mods_depressed, mods_latched, mods_locked, 0, 0, group);
}

static void handlePointerEnter(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface, wl_fixed_t sx, wl_fixed_t sy)
{
	wl_pointer_set_cursor(wl_pointer, serial, NULL, 0, 0);
}

static void handlePointerLeave(void *data, struct wl_pointer *wl_pointer, uint32_t serial, struct wl_surface *surface)
{
}

static MouseEvent_t MouseEvent={ 0, 0, 0, 0 };

static void handlePointerMotion(void *data, struct wl_pointer *wl_pointer, uint32_t time, wl_fixed_t sx, wl_fixed_t sy)
{
    // static int oldSx=0;
    // static int oldSy=0;

    // int curSx=wl_fixed_to_int(sx);
    // int curSy=wl_fixed_to_int(sy);

    // int deltaX=curSx-oldSx;
    // int deltaY=curSy-oldSy;

    // oldSx=curSx;
    // oldSy=curSy;

    // MouseEvent.dx=deltaX;
    // MouseEvent.dy=-deltaY;

    // Event_Trigger(EVENT_MOUSEMOVE, &MouseEvent);
}

static void handlePointerButton(void *data, struct wl_pointer *wl_pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state)
{
    if(state==WL_POINTER_BUTTON_STATE_PRESSED)
    {
        // Keep button state for movement events
        if(button==BTN_LEFT)
            MouseEvent.button|=MOUSE_BUTTON_1;
        if(button==BTN_MIDDLE)
            MouseEvent.button|=MOUSE_BUTTON_3;
        if(button==BTN_RIGHT)
            MouseEvent.button|=MOUSE_BUTTON_2;

		Input_OnMouseButtonEvent(&MouseEvent, true);
    }
    else if(state==WL_POINTER_BUTTON_STATE_RELEASED)
    {
        if(button==BTN_LEFT)
            MouseEvent.button&=~MOUSE_BUTTON_1;
        if(button==BTN_MIDDLE)
            MouseEvent.button&=~MOUSE_BUTTON_3;
        if(button==BTN_RIGHT)
            MouseEvent.button&=~MOUSE_BUTTON_2;

		Input_OnMouseButtonEvent(&MouseEvent, false);
    }
}

static void handleRelativePointerMotion(void *data, struct zwp_relative_pointer_v1 *relative_pointer, uint32_t utime_hi, uint32_t utime_lo, wl_fixed_t dx, wl_fixed_t dy, wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel)
{
    double deltaX=wl_fixed_to_double(dx);
    double deltaY=wl_fixed_to_double(dy);

    MouseEvent.dx=deltaX;
    MouseEvent.dy=-deltaY;

    Input_OnMouseEvent(&MouseEvent);
}

static void handlePointerAxis(void *data, struct wl_pointer *wl_pointer, uint32_t time, uint32_t axis, wl_fixed_t value)
{
}

static void handleLocked(void *data, struct zwp_locked_pointer_v1 *locked_pointer)
{
}

static void handleUnlocked(void *data, struct zwp_locked_pointer_v1 *locked_pointer)
{
}

static void handleRegistry(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
static void handleSeatCapabilities(void *data, struct wl_seat *seat, uint32_t caps);

static const struct wl_registry_listener registryListener={ .global=handleRegistry };
static const struct xdg_wm_base_listener shellListener={ .ping=handleShellPing };
static const struct xdg_surface_listener shellSurfaceListener={ .configure=handleShellSurfaceConfigure };
static const struct xdg_toplevel_listener toplevelListener = { .configure=handleToplevelConfigure, .close=handleToplevelClose };
static const struct wl_keyboard_listener keyboardListener={ .keymap=handleKeymap, .enter=handleEnter, .leave=handleLeave, .key=handleKey, .modifiers=handleModifiers };
static const struct wl_pointer_listener pointerListener={ .enter=handlePointerEnter, .leave=handlePointerLeave, .motion=handlePointerMotion, .button=handlePointerButton, .axis=handlePointerAxis };
static const struct wl_seat_listener seatListener={ .capabilities=handleSeatCapabilities };
static const struct zwp_relative_pointer_v1_listener relativePointerListener={ .relative_motion=handleRelativePointerMotion };
static const struct zwp_locked_pointer_v1_listener lockedPointerListener={ .locked=handleLocked, .unlocked=handleUnlocked };

static void handleRegistry(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    if(strcmp(interface, wl_compositor_interface.name)==0)
    {
        compositor=wl_registry_bind(registry, name, &wl_compositor_interface, 1);
    }
    else if(strcmp(interface, xdg_wm_base_interface.name)==0)
    {
        shell=wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(shell, &shellListener, NULL);
    }
    else if(strcmp(interface, zwp_relative_pointer_manager_v1_interface.name)==0)
    {
        relativePointerManager=wl_registry_bind(registry, name, &zwp_relative_pointer_manager_v1_interface, 1);
    }
    else if(strcmp(interface, wl_seat_interface.name)==0)
    {
        seat=wl_registry_bind(registry, name, &wl_seat_interface, 1);
        wl_seat_add_listener(seat, &seatListener, NULL);
    }
	else if(strcmp(interface, zwp_pointer_constraints_v1_interface.name)==0)
	{
		pointerConstraints=wl_registry_bind(registry, name, &zwp_pointer_constraints_v1_interface, 1);
	}
}

static void handleSeatCapabilities(void *data, struct wl_seat *seat, uint32_t caps)
{
    if(caps&WL_SEAT_CAPABILITY_KEYBOARD)
    {
        keyboard=wl_seat_get_keyboard(seat);
        wl_keyboard_add_listener(keyboard, &keyboardListener, NULL);

        xkbContext=xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    }

    if(caps&WL_SEAT_CAPABILITY_POINTER)
    {
        pointer=wl_seat_get_pointer(seat);
        wl_pointer_add_listener(pointer, &pointerListener, NULL);

        relativePointer=zwp_relative_pointer_manager_v1_get_relative_pointer(relativePointerManager, pointer);
        zwp_relative_pointer_v1_add_listener(relativePointer, &relativePointerListener, NULL);
    }
}

int main(int argc, char** argv)
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
    
    DBGPRINTF(DEBUG_INFO, "Opening Wayland display...\n");
    vkContext.wlDisplay=wl_display_connect(NULL);

	if(vkContext.wlDisplay==NULL)
	{
		DBGPRINTF(DEBUG_ERROR, "\t...can't open display.\n");
		return -1;
	}

    registry=wl_display_get_registry(vkContext.wlDisplay);
    wl_registry_add_listener(registry, &registryListener, NULL);
    wl_display_roundtrip(vkContext.wlDisplay);

    vkContext.wlSurface=wl_compositor_create_surface(compositor);

    shellSurface=xdg_wm_base_get_xdg_surface(shell, vkContext.wlSurface);
    xdg_surface_add_listener(shellSurface, &shellSurfaceListener, NULL);

    toplevel=xdg_surface_get_toplevel(shellSurface);
    xdg_toplevel_add_listener(toplevel, &toplevelListener, vkContext.wlSurface);

    xdg_toplevel_set_title(toplevel, szAppName);
    xdg_toplevel_set_app_id(toplevel, szAppName);

    wl_surface_commit(vkContext.wlSurface);
    wl_display_roundtrip(vkContext.wlDisplay);

	struct zwp_locked_pointer_v1 *lockedPointer=zwp_pointer_constraints_v1_lock_pointer(pointerConstraints, vkContext.wlSurface, pointer, NULL, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
	zwp_locked_pointer_v1_add_listener(lockedPointer, &lockedPointerListener, NULL);

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

    swapchain.extent.width=config.windowWidth;
    swapchain.extent.height=config.windowHeight;

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
        config.isVR=true;
    }

	DBGPRINTF(DEBUG_INFO, "Initializing Vulkan resources...\n");
	if(!Init())
	{
		DBGPRINTF(DEBUG_ERROR, "\t...failed.\n");
		return -1;
	}

	DBGPRINTF(DEBUG_WARNING, "\nCurrent system zone memory allocations:\n");
	Zone_Print(zone);

	DBGPRINTF(DEBUG_INFO, "\nStarting main loop.\n");
    while(!isDone)
    {
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

        wl_display_roundtrip(vkContext.wlDisplay);
    }

	DBGPRINTF(DEBUG_INFO, "Shutting down...\n");
	Destroy();
	vkuDestroyContext(vkInstance, &vkContext);
	vkuDestroyInstance(vkInstance);

    xdg_toplevel_destroy(toplevel);
    xdg_surface_destroy(shellSurface);
    wl_surface_destroy(vkContext.wlSurface);
    xdg_wm_base_destroy(shell);
    wl_compositor_destroy(compositor);
    wl_registry_destroy(registry);
    wl_display_disconnect(vkContext.wlDisplay);

	DBGPRINTF(DEBUG_WARNING, "Zone remaining block list:\n");
	Zone_Print(zone);
	Zone_Destroy(zone);

	DBGPRINTF(DEBUG_INFO, "Exit\n");

    return 0;
}
