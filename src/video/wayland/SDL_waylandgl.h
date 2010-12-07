#include "SDL_config.h"

#ifndef _SDL_waylandgl_h
#define _SDL_waylandgl_h
#include "SDL_waylandwindow.h"
#include <wayland-client-protocol.h>

void Wayland_GL_SwapWindow(_THIS, SDL_Window * window);
int Wayland_GL_GetSwapInterval(_THIS);
int Wayland_GL_SetSwapInterval(_THIS, int interval);
int Wayland_GL_MakeCurrent(_THIS, SDL_Window * window, SDL_GLContext context);
SDL_GLContext Wayland_GL_CreateContext(_THIS, SDL_Window * window);
int Wayland_GL_LoadLibrary(_THIS, const char *path);
void Wayland_GL_UnloadLibrary(_THIS);
#endif
