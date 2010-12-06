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

#include "../SDL_sysvideo.h"
#include </home/joel/install/include/wayland-client.h>
#include </home/joel/install/include/wayland-client-protocol.h>
#define GL_GLEXT_PROTOTYPES
#define EGL_EGLEXT_PROTOTYPES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>


typedef struct
{
    struct wl_display *display;
    struct wl_compositor *compositor;
    struct wl_drm *drm;
    struct wl_output *output;
    struct wl_shell *shell;
    
    struct {
        int32_t x, y, width, height;
    } screen_allocation;
    
    char *device_name;
    int authenticated;

    int drm_fd;
    
    EGLDisplay edpy;
    
} SDL_WaylandData;

#endif /* _SDL_nullvideo_h */

/* vi: set ts=4 sw=4 expandtab: */
