#include "SDL_config.h"

#include "SDL_waylandgl.h"
#include "SDL_waylandwindow.h"


void Wayland_GL_SwapWindow(_THIS, SDL_Window * window)
{
    printf("Wayland_GL_SwapWindow\n");
    SDL_WaylandWindow *data = window->driverdata;
    data->current ^= 1;

    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                  GL_COLOR_ATTACHMENT0,
                  GL_RENDERBUFFER,
                  data->rbo[data->current]);

    wl_surface_attach(data->surface,
              data->buffer[data->current ^ 1]);
    wl_surface_damage(data->surface, 0, 0,
                  window->w, window->h);
}

int
Wayland_GL_MakeCurrent(_THIS, SDL_Window *window, SDL_GLContext context)
{
    printf("Wayland_GL_MakeCurrent\n");
    SDL_WaylandData *data = _this->driverdata;
    SDL_WaylandWindow *wind = (SDL_WaylandWindow *) window->driverdata;
    if (!eglMakeCurrent(data->edpy,EGL_NO_SURFACE,
                        EGL_NO_SURFACE,wind->context)) {
        SDL_SetError("Unable to make EGL context current");
        return -1;
    }

    return 1;
}

int
Wayland_GL_LoadLibrary(_THIS, const char *path)
{
/* void *handle;
    int dlopen_flags;

    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

    if (_this->gles_data->egl_active) {
        SDL_SetError("OpenGL ES context already created");
        return -1;
    }
#ifdef RTLD_GLOBAL
    dlopen_flags = RTLD_LAZY | RTLD_GLOBAL;
#else
    dlopen_flags = RTLD_LAZY;
#endif
    handle = dlopen(path, dlopen_flags);
    /* Catch the case where the application isn't linked with EGL *//*
    if ((dlsym(handle, "eglChooseConfig") == NULL) && (path == NULL)) {

        dlclose(handle);
        path = getenv("SDL_VIDEO_GL_DRIVER");
        if (path == NULL) {
            path = DEFAULT_OPENGL;
        }
        handle = dlopen(path, dlopen_flags);
    }

    if (handle == NULL) {
        SDL_SetError("Could not load OpenGL ES/EGL library");
        return -1;
    }

    /* Unload the old driver and reset the pointers *//*
    Wayland_GL_UnloadLibrary(_this);



    _this->gles_data->egl_display =
        _this->gles_data->eglGetDisplay((NativeDisplayType) data->display);

    if (!_this->gles_data->egl_display) {
        SDL_SetError("Could not get EGL display");
        return -1;
    }

    if (eglInitialize(_this->gles_data->egl_display, NULL,
                      NULL) != EGL_TRUE) {
        SDL_SetError("Could not initialize EGL");
        return -1;
    }

    _this->gl_config.dll_handle = handle;
    _this->gl_config.driver_loaded = 1;

    if (path) {
        strncpy(_this->gl_config.driver_path, path,
                sizeof(_this->gl_config.driver_path) - 1);
    } else {
        strcpy(_this->gl_config.driver_path, "");
    }*/
    SDL_WaylandData *data = _this->driverdata;
    
    data->edpy = eglGetDRMDisplayMESA(data->drm_fd);

	int major, minor;
	if (!eglInitialize(data->edpy, &major, &minor)) {
		fprintf(stderr, "failed to initialize display\n");
		return -1;
	}

/*	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		fprintf(stderr, "failed to bind EGL_OPENGL_ES_API\n");
		return -1;
	}

	data->context = eglCreateContext(data->edpy, NULL,
					   EGL_NO_CONTEXT, context_attribs);
	if (data->context == NULL) {
		fprintf(stderr, "failed to create context\n");
		return -1;
	}

	if (!eglMakeCurrent(data->edpy, EGL_NO_SURFACE,
			    EGL_NO_SURFACE, data->context)) {
		fprintf(stderr, "failed to make context current\n");
		return -1;
	}*/
    return 0;
}

void
Wayland_GL_UnloadLibrary(_THIS)
{
    /*if (_this->gl_config.driver_loaded) {
        _this->gles_data->eglTerminate(_this->gles_data->egl_display);

        dlclose(_this->gl_config.dll_handle);

        _this->gles_data->eglGetProcAddress = NULL;
        _this->gles_data->eglChooseConfig = NULL;
        _this->gles_data->eglCreateContext = NULL;
        _this->gles_data->eglCreateWindowSurface = NULL;
        _this->gles_data->eglDestroyContext = NULL;
        _this->gles_data->eglDestroySurface = NULL;
        _this->gles_data->eglMakeCurrent = NULL;
        _this->gles_data->eglSwapBuffers = NULL;
        _this->gles_data->eglGetDisplay = NULL;
        _this->gles_data->eglTerminate = NULL;

        _this->gl_config.dll_handle = NULL;
        _this->gl_config.driver_loaded = 0;
    }*/
}

SDL_GLContext
Wayland_GL_CreateContext(_THIS, SDL_Window * window)
{
    SDL_WaylandData *data = _this->driverdata;
    SDL_WaylandWindow *wind = (SDL_WaylandWindow *) window->driverdata;
    //Display *display = data->videodata->display;


	static const EGLint context_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 2,
		EGL_NONE
	};
    wind->context = eglCreateContext(data->edpy, NULL, EGL_NO_CONTEXT, context_attribs);


    if (wind->context == EGL_NO_CONTEXT) {
        SDL_SetError("Could not create EGL context");
        return NULL;
    }

    //data->egl_active = 1;


    return 1;
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
