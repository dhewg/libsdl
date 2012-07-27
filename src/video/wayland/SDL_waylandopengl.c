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

#include "SDL_waylandopengl.h"
#include "SDL_waylandwindow.h"
#include "SDL_waylandevents_c.h"

#include <dlfcn.h>

void
Wayland_GL_SwapWindow(_THIS, SDL_Window *window)
{
    SDL_WaylandWindow *wind = (SDL_WaylandWindow *) window->driverdata;
    SDL_WaylandData *data = _this->driverdata;

    eglSwapBuffers(data->edpy, wind->esurf);

    wayland_schedule_write(data);
}

int
Wayland_GL_LoadLibrary(_THIS, const char *path)
{
    /* FIXME: dlopen the library here? */
    SDL_WaylandData *data = _this->driverdata;
    int major, minor;
    EGLint num_config;
    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 0,
        EGL_GREEN_SIZE, 0,
        EGL_BLUE_SIZE, 0,
        EGL_DEPTH_SIZE, 0,
        EGL_ALPHA_SIZE, 0,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };

    config_attribs[ 3] = _this->gl_config.red_size;
    config_attribs[ 5] = _this->gl_config.green_size;
    config_attribs[ 7] = _this->gl_config.blue_size;
    config_attribs[ 9] = _this->gl_config.depth_size;
    config_attribs[11] = _this->gl_config.alpha_size;

    data->edpy = eglGetDisplay(data->display);

    if (!eglInitialize(data->edpy, &major, &minor)) {
        SDL_SetError("failed to initialize display\n");
        return -1;
    }

    eglBindAPI(EGL_OPENGL_API);

    if (!eglChooseConfig(data->edpy, config_attribs,
                         &data->econf, 1, &num_config)) {
        SDL_SetError("failed to choose a config\n");
        return -1;
    }
    if (num_config != 1) {
        SDL_SetError("failed to choose a config\n");
        return -1;
    }
    Wayland_PumpEvents(_this);

    wayland_schedule_write(data);

    return 0;
}

void *
Wayland_GL_GetProcAddress(_THIS, const char *proc)
{
    static char procname[1024];
    void *handle;
    void *retval;

    handle = _this->gl_config.dll_handle;
    retval = eglGetProcAddress(proc);
    if (retval) {
        return retval;
    }

#if defined(__OpenBSD__) && !defined(__ELF__)
#undef dlsym(x,y);
#endif
    retval = dlsym(handle, proc);
    if (!retval && strlen(proc) <= 1022) {
        procname[0] = '_';
        strcpy(procname + 1, proc);
        retval = dlsym(handle, procname);
    }
    return retval;
}

void
Wayland_GL_UnloadLibrary(_THIS)
{
    SDL_WaylandData *data = _this->driverdata;

    if (_this->gl_config.driver_loaded) {
        eglTerminate(data->edpy);

        dlclose(_this->gl_config.dll_handle);

        _this->gl_config.dll_handle = NULL;
        _this->gl_config.driver_loaded = 0;
    }

    wayland_schedule_write(data);
}

SDL_GLContext
Wayland_GL_CreateContext(_THIS, SDL_Window *window)
{
    SDL_WaylandData *data = _this->driverdata;

    data->context = eglCreateContext(data->edpy, data->econf,
                                     EGL_NO_CONTEXT, NULL);


    if (data->context == EGL_NO_CONTEXT) {
        SDL_SetError("Could not create EGL context");
        return NULL;
    }

    Wayland_GL_MakeCurrent(_this, window, NULL);

    wayland_schedule_write(data);

    return data->context;
}

int
Wayland_GL_MakeCurrent(_THIS, SDL_Window *window, SDL_GLContext context)
{
    printf("Wayland_GL_MakeCurrent\n");

    SDL_WaylandData *data = _this->driverdata;
    SDL_WaylandWindow *wind = (SDL_WaylandWindow *) window->driverdata;

    if (!eglMakeCurrent(data->edpy, wind->esurf, wind->esurf,
                        data->context)) {
        SDL_SetError("Unable to make EGL context current");
        return -1;
    }

    wayland_schedule_write(data);

    return 0;
}

int
Wayland_GL_SetSwapInterval(_THIS, int interval)
{
    return 0;
}

int
Wayland_GL_GetSwapInterval(_THIS)
{
    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
