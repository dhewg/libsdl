/*
    SDL - Simple DirectMedia Layer
    Copyright (C) 1997-2010 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

    Sam Lantinga
    slouken@libsdl.org
*/

#include "SDL_config.h"

#include "SDL_video.h"
#include "SDL_mouse.h"

#include "SDL_waylandvideo.h"
#include "SDL_waylandevents_c.h"
#include "SDL_waylandwindow.h"
#include "SDL_waylandopengl.h"

#include <fcntl.h>
#include <xkbcommon/xkbcommon.h>

#define WAYLANDVID_DRIVER_NAME "wayland"

/* Initialization/Query functions */
static int
Wayland_VideoInit(_THIS);

static void
Wayland_GetDisplayModes(_THIS, SDL_VideoDisplay *sdl_display);
static int
Wayland_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode);

static void
Wayland_VideoQuit(_THIS);

/* Wayland driver bootstrap functions */
static int
Wayland_Available(void)
{
    const char *envr = SDL_getenv("SDL_VIDEODRIVER");
    return envr && SDL_strcmp(envr, WAYLANDVID_DRIVER_NAME) == 0;
}

static void
Wayland_DeleteDevice(SDL_VideoDevice *device)
{
    SDL_free(device);
}

static SDL_VideoDevice *
Wayland_CreateDevice(int devindex)
{
    SDL_VideoDevice *device;

    /* Initialize all variables that we clean on shutdown */
    device = SDL_calloc(1, sizeof(SDL_VideoDevice));
    if (!device) {
        SDL_OutOfMemory();
        return NULL;
    }

    /* Set the function pointers */
    device->VideoInit = Wayland_VideoInit;
    device->VideoQuit = Wayland_VideoQuit;
    device->SetDisplayMode = Wayland_SetDisplayMode;
    device->GetDisplayModes = Wayland_GetDisplayModes;

    device->PumpEvents = Wayland_PumpEvents;

    device->GL_SwapWindow = Wayland_GL_SwapWindow;
    device->GL_GetSwapInterval = Wayland_GL_GetSwapInterval;
    device->GL_SetSwapInterval = Wayland_GL_SetSwapInterval;
    device->GL_MakeCurrent = Wayland_GL_MakeCurrent;
    device->GL_CreateContext = Wayland_GL_CreateContext;
    device->GL_LoadLibrary = Wayland_GL_LoadLibrary;
    device->GL_UnloadLibrary = Wayland_GL_UnloadLibrary;
    device->GL_GetProcAddress = Wayland_GL_GetProcAddress;

    device->CreateWindow = Wayland_CreateWindow;
    device->ShowWindow = Wayland_ShowWindow;
    device->DestroyWindow = Wayland_DestroyWindow;

    device->free = Wayland_DeleteDevice;

    return device;
}

VideoBootStrap Wayland_bootstrap = {
    WAYLANDVID_DRIVER_NAME, "SDL Wayland video driver",
    Wayland_Available, Wayland_CreateDevice
};

static void
display_handle_geometry(void *data,
                        struct wl_output *output,
                        int x, int y,
                        int physical_width,
                        int physical_height,
                        int subpixel,
                        const char *make,
                        const char *model,
                        int transform)

{
    SDL_WaylandData *d = data;

    d->screen_allocation.x = x;
    d->screen_allocation.y = y;
}

static void
display_handle_mode(void *data,
                    struct wl_output *wl_output,
                    uint32_t flags,
                    int width,
                    int height,
                    int refresh)
{
    SDL_WaylandData *d = data;

    if (flags & WL_OUTPUT_MODE_CURRENT) {
        d->screen_allocation.width = width;
        d->screen_allocation.height = height;
    }
}

static const struct wl_output_listener output_listener = {
    display_handle_geometry,
    display_handle_mode
};

static void
display_handle_global(struct wl_display *display, uint32_t id,
                      const char *interface, uint32_t version, void *data)
{
    SDL_WaylandData *d = data;

    if (strcmp(interface, "wl_compositor") == 0) {
        d->compositor = wl_display_bind(display, id, &wl_compositor_interface);
    } else if (strcmp(interface, "wl_output") == 0) {
        d->output = wl_display_bind(display, id, &wl_output_interface);
        wl_output_add_listener(d->output, &output_listener, d);
    } else if (strcmp(interface, "wl_seat") == 0) {
        Wayland_display_add_input(d, id);
    } else if (strcmp(interface, "wl_shell") == 0) {
        d->shell = wl_display_bind(display, id, &wl_shell_interface);
    }
}

static int
update_event_mask(uint32_t mask, void *data)
{
    SDL_WaylandData *d = data;

    d->event_mask = mask;

    if (mask & WL_DISPLAY_WRITABLE)
        d->schedule_write = 1;
    else
        d->schedule_write = 0;

    return 0;
}

int
Wayland_VideoInit(_THIS)
{
    SDL_WaylandData *data;

    data = malloc(sizeof *data);
    if (data == NULL)
        return 0;
    data->schedule_write = 0;

    _this->driverdata = data;

    data->display = wl_display_connect(NULL);
    if (data->display == NULL) {
        SDL_SetError("Failed to connect to a Wayland display");
        return 0;
    }

    wl_display_add_global_listener(data->display,
                                   display_handle_global, data);

    wl_display_iterate(data->display, WL_DISPLAY_READABLE);

    data->event_fd = wl_display_get_fd(data->display, update_event_mask, data);

    data->xkb_context = xkb_context_new(0);
    if (!data->xkb_context) {
        SDL_SetError("Failed to create XKB context");
        return 0;
    }

    SDL_VideoDisplay display;
    SDL_DisplayMode mode;

    /* Use a fake 32-bpp desktop mode */
    mode.format = SDL_PIXELFORMAT_RGB888;
    mode.w = data->screen_allocation.width;
    mode.h = data->screen_allocation.height;
    mode.refresh_rate = 0;
    mode.driverdata = NULL;
    SDL_zero(display);
    display.desktop_mode = mode;
    display.current_mode = mode;
    display.driverdata = NULL;
    SDL_AddVideoDisplay(&display);

    wayland_schedule_write(data);

    return 0;
}

static void
Wayland_GetDisplayModes(_THIS, SDL_VideoDisplay *sdl_display)
{
    SDL_WaylandData *data = _this->driverdata;
    SDL_DisplayMode mode;

    Wayland_PumpEvents(_this);

    mode.w = data->screen_allocation.width;
    mode.h = data->screen_allocation.height;
    mode.refresh_rate = 0;
    mode.driverdata = NULL;

    mode.format = SDL_PIXELFORMAT_RGB888;
    SDL_AddDisplayMode(sdl_display, &mode);
    mode.format = SDL_PIXELFORMAT_RGBA8888;
    SDL_AddDisplayMode(sdl_display, &mode);
}

static int
Wayland_SetDisplayMode(_THIS, SDL_VideoDisplay *display, SDL_DisplayMode *mode)
{
    return 0;
}

void
Wayland_VideoQuit(_THIS)
{
    SDL_WaylandData *data = _this->driverdata;

    Wayland_display_destroy_input(data);

    xkb_context_unref(data->xkb_context);
    data->xkb_context = NULL;

    free(data);
    _this->driverdata = NULL;
}

/* vi: set ts=4 sw=4 expandtab: */
