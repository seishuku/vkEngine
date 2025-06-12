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

MemZone_t *zone;

char szAppName[]="Vulkan";

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

static void handleKey(void *data, struct wl_keyboard *wl_keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
    xkb_keysym_t keysym=xkb_state_key_get_one_sym(xkbState, key+8);
    uint32_t code=0;

    // Convert lowercase to uppercase
    if(keysym>=XKB_KEY_a&&keysym<=XKB_KEY_z)
        keysym-=0x0020;

    if(keysym==XKB_KEY_Escape)
        isDone=true;

    switch(keysym)
    {
        case XKB_KEY_BackSpace:	    code=KB_BACKSPACE;				break;	// Backspace
        case XKB_KEY_Tab:		    code=KB_TAB;					break;	// Tab
        case XKB_KEY_Return:	    code=KB_ENTER;					break;	// Enter
        case XKB_KEY_Pause:		    code=KB_PAUSE;					break;	// Pause
        case XKB_KEY_Caps_Lock:	    code=KB_CAPS_LOCK;				break;	// Caps Lock
        case XKB_KEY_Escape:	    code=KB_ESCAPE;					break;	// Esc
        case XKB_KEY_Prior:		    code=KB_PAGE_UP;				break;	// Page Up
        case XKB_KEY_Next:		    code=KB_PAGE_DOWN;				break;	// Page Down
        case XKB_KEY_End:		    code=KB_END;					break;	// End
        case XKB_KEY_Home:		    code=KB_HOME;					break;	// Home
        case XKB_KEY_Left:		    code=KB_LEFT;					break;	// Left
        case XKB_KEY_Up:		    code=KB_UP;						break;	// Up
        case XKB_KEY_Right:		    code=KB_RIGHT;					break;	// Right
        case XKB_KEY_Down:		    code=KB_DOWN;					break;	// Down
        case XKB_KEY_Print:		    code=KB_PRINT_SCREEN;			break;	// Prnt Scrn
        case XKB_KEY_Insert:	    code=KB_INSERT;					break;	// Insert
        case XKB_KEY_Delete:	    code=KB_DEL;					break;	// Delete
        case XKB_KEY_Super_L:	    code=KB_LSUPER;					break;	// Left Windows
        case XKB_KEY_Super_R:	    code=KB_RSUPER;					break;	// Right Windows
        case XKB_KEY_Menu:		    code=KB_MENU;					break;	// Application
        case XKB_KEY_KP_0:		    code=KB_NP_0;					break;	// Num 0
        case XKB_KEY_KP_1:		    code=KB_NP_1;					break;	// Num 1
        case XKB_KEY_KP_2:		    code=KB_NP_2;					break;	// Num 2
        case XKB_KEY_KP_3:		    code=KB_NP_3;					break;	// Num 3
        case XKB_KEY_KP_4:		    code=KB_NP_4;					break;	// Num 4
        case XKB_KEY_KP_5:		    code=KB_NP_5;					break;	// Num 5
        case XKB_KEY_KP_6:		    code=KB_NP_6;					break;	// Num 6
        case XKB_KEY_KP_7:		    code=KB_NP_7;					break;	// Num 7
        case XKB_KEY_KP_8:		    code=KB_NP_8;					break;	// Num 8
        case XKB_KEY_KP_9:		    code=KB_NP_9;					break;	// Num 9
        case XKB_KEY_KP_Multiply:   code=KB_NP_MULTIPLY;			break;	// Num *
        case XKB_KEY_KP_Add:		code=KB_NP_ADD;					break;	// Num +
        case XKB_KEY_KP_Subtract:   code=KB_NP_SUBTRACT;			break;	// Num -
        case XKB_KEY_KP_Decimal:	code=KB_NP_DECIMAL;				break;	// Num Del
        case XKB_KEY_KP_Divide: 	code=KB_NP_DIVIDE;				break;	// Num /
        case XKB_KEY_F1:			code=KB_F1;						break;	// F1
        case XKB_KEY_F2:			code=KB_F2;						break;	// F2
        case XKB_KEY_F3:			code=KB_F3;						break;	// F3
        case XKB_KEY_F4:			code=KB_F4;						break;	// F4
        case XKB_KEY_F5:			code=KB_F5;						break;	// F5
        case XKB_KEY_F6:			code=KB_F6;						break;	// F6
        case XKB_KEY_F7:			code=KB_F7;						break;	// F7
        case XKB_KEY_F8:			code=KB_F8;						break;	// F8
        case XKB_KEY_F9:			code=KB_F9;						break;	// F9
        case XKB_KEY_F10:		    code=KB_F10;					break;	// F10
        case XKB_KEY_F11:		    code=KB_F11;					break;	// F11
        case XKB_KEY_F12:		    code=KB_F12;					break;	// F12
        case XKB_KEY_Num_Lock:	    code=KB_NUM_LOCK;				break;	// Num Lock
        case XKB_KEY_Scroll_Lock:   code=KB_SCROLL_LOCK;			break;	// Scroll Lock
        case XKB_KEY_Shift_L:	    code=KB_LSHIFT;					break;	// Shift
        case XKB_KEY_Shift_R:	    code=KB_RSHIFT;					break;	// Right Shift
        case XKB_KEY_Control_L:	    code=KB_LCTRL;					break;	// Left control
        case XKB_KEY_Control_R:	    code=KB_RCTRL;					break;	// Right control
        case XKB_KEY_Alt_L:		    code=KB_LALT;					break;	// Left alt
        case XKB_KEY_Alt_R:		    code=KB_RALT;					break;	// Left alt
        default:			        code=keysym;					break;	// All others
    }

    if(state==WL_KEYBOARD_KEY_STATE_PRESSED)
        Event_Trigger(EVENT_KEYDOWN, &code);
	else if(state==WL_KEYBOARD_KEY_STATE_RELEASED)
        Event_Trigger(EVENT_KEYUP, &code);
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
        if(button==BTN_LEFT)
            MouseEvent.button|=MOUSE_BUTTON_1;

        if(button==BTN_MIDDLE)
            MouseEvent.button|=MOUSE_BUTTON_3;

        if(button==BTN_RIGHT)
            MouseEvent.button|=MOUSE_BUTTON_2;

        Event_Trigger(EVENT_MOUSEDOWN, &MouseEvent);
    }
    else if(state==WL_POINTER_BUTTON_STATE_RELEASED)
    {
        if(button==BTN_LEFT)
            MouseEvent.button&=~MOUSE_BUTTON_1;

        if(button==BTN_MIDDLE)
            MouseEvent.button&=~MOUSE_BUTTON_3;

        if(button==BTN_RIGHT)
            MouseEvent.button&=~MOUSE_BUTTON_2;

        Event_Trigger(EVENT_MOUSEUP, &MouseEvent);
    }
}

static void handleRelativePointerMotion(void *data, struct zwp_relative_pointer_v1 *relative_pointer, uint32_t utime_hi, uint32_t utime_lo, wl_fixed_t dx, wl_fixed_t dy, wl_fixed_t dx_unaccel, wl_fixed_t dy_unaccel)
{
    double deltaX=wl_fixed_to_double(dx);
    double deltaY=wl_fixed_to_double(dy);

    MouseEvent.dx=deltaX;
    MouseEvent.dy=-deltaY;

    Event_Trigger(EVENT_MOUSEMOVE, &MouseEvent);
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
