#include "SDL_config.h"

#include "../SDL_sysvideo.h"
#include "SDL_waylandwindow.h"
#include "SDL_waylandvideo.h"


void Wayland_ShowWindow(_THIS, SDL_Window * window)
{
	SDL_WaylandWindow *data = (SDL_WaylandWindow*) window->driverdata;

	wl_surface_map(data->surface,
		       window->x, window->y,
		       window->w, window->h);
}


int Wayland_CreateWindow(_THIS, SDL_Window * window)
{

    SDL_WaylandWindow *data;
    struct wl_visual *visual;
	int i;
	SDL_WaylandData *c;
	
	data = malloc(sizeof *data);
	if (data == NULL)
		return 0;
    
    c = _this->driverdata;
    window->driverdata = data;
    
    if (!(window->flags & SDL_WINDOW_OPENGL)) {
        SDL_GL_LoadLibrary(NULL);
        window->flags &= SDL_WINDOW_OPENGL;
    }

	if (window->x == SDL_WINDOWPOS_UNDEFINED) {
		window->x = 0;
	}
	if (window->y == SDL_WINDOWPOS_UNDEFINED) {
		window->y = 0;
	}
	
	data->waylandData = c;
	data->sdlwindow = window;
	EGLint name, stride, attribs[] = {
		EGL_WIDTH,	0,
		EGL_HEIGHT,	0,
		EGL_DRM_BUFFER_FORMAT_MESA, EGL_DRM_BUFFER_FORMAT_ARGB32_MESA,
		EGL_DRM_BUFFER_USE_MESA,    EGL_DRM_BUFFER_USE_SCANOUT_MESA,
		EGL_NONE
	};
    
	data->surface =
		wl_compositor_create_surface(c->compositor);
	wl_surface_set_user_data(data->surface, data);


	visual = wl_display_get_premultiplied_argb_visual(c->display);
	for (i = 0; i < 2; i++) {
		attribs[1] = window->w;
		attribs[3] = window->h;
		data->image[i] =
			eglCreateDRMImageMESA(c->edpy, attribs);
		if (data->image[i] == NULL) {
			SDL_SetError("Failed to create  DRM-Image with attribs: width: %d, height: %d\n",
				     window->w, window->h);
			free(data);
			return -1;
		}
		eglExportDRMImageMESA(c->edpy, data->image[i],
				      &name, NULL, &stride);
		data->buffer[i] =
			wl_drm_create_buffer(c->drm, name,
					     window->w, window->h,
					     stride, visual);
	}
	data->current = 0;
	data->rbos_generated = 0;


    return 0;
}

void Wayland_DestroyWindow(_THIS, SDL_Window * window)
{
	SDL_WaylandWindow *data = (SDL_WaylandWindow*) window->driverdata;
	SDL_WaylandData *d;
	window->driverdata = NULL;
	int i;

	if (data) {
		d = data->waylandData;

		if (data->rbos_generated) {
			glDeleteRenderbuffers(1, &data->depth_rbo);
			glDeleteRenderbuffers(2, data->color_rbo);
			data->rbos_generated = 0;
		}
		for (i = 0; i < 2; ++i) {
			wl_buffer_destroy(data->buffer[i]);
			eglDestroyImageKHR(d->edpy, data->image[i]);
		}

		wl_surface_destroy(data->surface);
		SDL_free(data);
	}
}
