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

#ifndef _SDL_waylandvideo_h
#define _SDL_waylandvideo_h

#include <wayland-client.h>
#include <wayland-egl.h>

#include <EGL/egl.h>

struct xkb_context;
struct SDL_WaylandInput;

typedef struct {
    struct wl_display *display;
    struct wl_compositor *compositor;
    struct wl_output *output;
    struct wl_shell *shell;

    struct {
        int32_t x, y, width, height;
    } screen_allocation;

    EGLDisplay edpy;
    EGLContext context;
    EGLConfig econf;

    struct xkb_context *xkb_context;
    struct SDL_WaylandInput *input;

    int event_fd;
    int event_mask;

    int schedule_write;
} SDL_WaylandData;

static inline void
wayland_schedule_write(SDL_WaylandData *data)
{
    if (data->schedule_write)
        wl_display_iterate(data->display, WL_DISPLAY_WRITABLE);
}

#endif /* _SDL_nullvideo_h */

/* vi: set ts=4 sw=4 expandtab: */
