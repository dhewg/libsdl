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
#include "SDL_stdinc.h"
#include "SDL_assert.h"

#include "../../events/SDL_sysevents.h"
#include "../../events/SDL_events_c.h"

#include "SDL_waylandvideo.h"
#include "SDL_waylandevents_c.h"
#include "SDL_waylandwindow.h"

#include <xkbcommon/xkbcommon.h>
#include <linux/input.h>

#include "../../events/scancodes_xfree86.h"

typedef uint32_t KeySym;
#include "../x11/imKStoUCS.h"

#include <errno.h>

#include <sys/select.h>
#include <sys/mman.h>
#include <unistd.h>

struct SDL_WaylandInput {
    SDL_WaylandData *display;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_keyboard *keyboard;
    SDL_WaylandWindow *pointer_focus;
    SDL_WaylandWindow *keyboard_focus;
    uint32_t modifiers;

    struct {
        struct xkb_keymap *keymap;
        struct xkb_state *state;
    } xkb;
};

void
Wayland_PumpEvents(_THIS)
{
    SDL_WaylandData *d = _this->driverdata;
    struct timeval tv;
    fd_set rfds;
    int retval;

    if (!(d->event_mask & WL_DISPLAY_READABLE))
        return;

    tv.tv_sec  = 0;
    tv.tv_usec = 0;
    do {
        FD_ZERO(&rfds);
        FD_SET(d->event_fd, &rfds);

        retval = select(d->event_fd + 1, &rfds, NULL, NULL, &tv);
        if (retval < 0) {
            SDL_SetError("select faild: %s\n", strerror(errno));
            break;
        }
        if (retval == 1)
            wl_display_iterate(d->display, WL_DISPLAY_READABLE);
    } while (retval > 0);
}

#if 0
static char *
keysym_to_utf8(uint32_t sym)
{
    char *text = NULL;
    uint32_t inbuf[2];

    inbuf[0] = X11_KeySymToUcs4(sym);
    if (inbuf[0] == 0)
        return NULL;
    inbuf[1] = 0;

    text = SDL_iconv_string("UTF-8", "UCS-4",
                            (const char *) inbuf, sizeof inbuf);

    return text;
}

static void
window_handle_key(void *data, struct wl_input_device *input_device,
                  uint32_t time, uint32_t key, uint32_t state)
{
    struct SDL_WaylandInput *input = data;
    SDL_WaylandWindow *window = input->keyboard_focus;
    SDL_WaylandData *d = window->waylandData;
    uint32_t code, sym, level = 0;
    char *text;

    code = key + d->xkb->min_key_code;
    if (window->keyboard_device != input)
        return;

    SDL_assert(key < d->input_table_size);
    SDL_SendKeyboardKey(state ? SDL_PRESSED:SDL_RELEASED, d->input_table[key]);

    level = 0;
    if (input->modifiers & XKB_COMMON_SHIFT_MASK &&
        XkbKeyGroupWidth(d->xkb, code, 0) > 1)
        level = 1;

    sym = XkbKeySymEntry(d->xkb, code, level, 0);

    if (state) {
        text = keysym_to_utf8(sym);
        if (text != NULL) {
            SDL_SendKeyboardText(text);
            SDL_free(text);
        }
    }

    if (state)
        input->modifiers |= d->xkb->map->modmap[code];
    else
        input->modifiers &= ~d->xkb->map->modmap[code];
}

static void
window_handle_keyboard_enter(void *data,
                             struct wl_input_device *input_device,
                             uint32_t time,
                             struct wl_surface *surface,
                             struct wl_array *keys)
{
    struct SDL_WaylandInput *input = data;
    SDL_WaylandWindow *window = input->keyboard_focus;
    SDL_WaylandData *d = input->display;
    uint32_t *k, *end;

    if (window)
        window->keyboard_device = NULL;

    if (surface)
        input->keyboard_focus = wl_surface_get_user_data(surface);
    else
        input->keyboard_focus = NULL;

    end = keys->data + keys->size;
    for (k = keys->data; k < end; k++)
        input->modifiers |= d->xkb->map->modmap[*k];

    window = input->keyboard_focus;
    if (window){
        window->keyboard_device = input;
        SDL_SetKeyboardFocus(window->sdlwindow);
    }else{
        SDL_SetKeyboardFocus(NULL);
    }

}
static void
window_handle_keyboard_leave(void *data,
                             struct wl_input_device *input_device,
                             uint32_t time,
                             struct wl_surface *surface)
{
    struct SDL_WaylandInput *input = data;

    input->keyboard_focus = NULL;
    SDL_SetKeyboardFocus(NULL);
}

static void
input_handle_touch_down(void *data,
                        struct wl_input_device *wl_input_device,
                        uint32_t time, struct wl_surface *surface,
                        int32_t id, int32_t x, int32_t y)
{
}

static void
input_handle_touch_up(void *data,
                      struct wl_input_device *wl_input_device,
                      uint32_t time, int32_t id)
{
}

static void
input_handle_touch_motion(void *data,
                          struct wl_input_device *wl_input_device,
                          uint32_t time, int32_t id, int32_t x, int32_t y)
{
}

static void
input_handle_touch_frame(void *data,
                         struct wl_input_device *wl_input_device)
{
}

static void
input_handle_touch_cancel(void *data,
                          struct wl_input_device *wl_input_device)
{
}

static const struct wl_input_device_listener input_device_listener = {
    window_handle_motion,
    window_handle_button,
    window_handle_key,
    window_handle_pointer_enter,
    window_handle_pointer_leave,
    window_handle_keyboard_enter,
    window_handle_keyboard_leave,
    input_handle_touch_down,
    input_handle_touch_up,
    input_handle_touch_motion,
    input_handle_touch_frame,
    input_handle_touch_cancel,
};
#endif

static void
pointer_handle_enter(void *data, struct wl_pointer *pointer,
                     uint32_t serial, struct wl_surface *surface,
                     wl_fixed_t sx_w, wl_fixed_t sy_w)
{
    struct SDL_WaylandInput *input = data;
    SDL_WaylandWindow *window;

    if (!surface) {
        /* enter event for a window we've just destroyed */
        return;
    }

    input->pointer_focus = wl_surface_get_user_data(surface);
    window = input->pointer_focus;
    SDL_SetMouseFocus(window->sdlwindow);
}

static void
pointer_handle_leave(void *data, struct wl_pointer *pointer,
                     uint32_t serial, struct wl_surface *surface)
{
    struct SDL_WaylandInput *input = data;

    SDL_SetMouseFocus(NULL);
    input->pointer_focus = NULL;
}

static void
pointer_handle_motion(void *data, struct wl_pointer *pointer,
                      uint32_t time, wl_fixed_t sx_w, wl_fixed_t sy_w)
{
    struct SDL_WaylandInput *input = data;
    SDL_WaylandWindow *window = input->pointer_focus;
    int sx = wl_fixed_to_int(sx_w);
    int sy = wl_fixed_to_int(sy_w);

    SDL_SendMouseMotion(window->sdlwindow, 0, sx, sy);
}

static void
pointer_handle_button(void *data, struct wl_pointer *pointer, uint32_t serial,
                      uint32_t time, uint32_t button, uint32_t state_w)
{
    struct SDL_WaylandInput *input = data;
    SDL_WaylandWindow *window = input->pointer_focus;
    enum wl_pointer_button_state state = state_w;
    uint32_t sdl_button;

    switch (button) {
    case BTN_LEFT:
        sdl_button = SDL_BUTTON_LEFT;
        break;
    case BTN_MIDDLE:
        sdl_button = SDL_BUTTON_MIDDLE;
        break;
    case BTN_RIGHT:
        sdl_button = SDL_BUTTON_RIGHT;
        break;
    case BTN_SIDE:
        sdl_button = SDL_BUTTON_X1;
        break;
    case BTN_EXTRA:
        sdl_button = SDL_BUTTON_X2;
        break;
    default:
        return;
    }

    SDL_SendMouseButton(window->sdlwindow,
                        state ? SDL_PRESSED : SDL_RELEASED, sdl_button);
}

static void
pointer_handle_axis(void *data, struct wl_pointer *pointer,
                    uint32_t time, uint32_t axis, wl_fixed_t value)
{
}

static const struct wl_pointer_listener pointer_listener = {
    pointer_handle_enter,
    pointer_handle_leave,
    pointer_handle_motion,
    pointer_handle_button,
    pointer_handle_axis,
};

static void
keyboard_handle_keymap(void *data, struct wl_keyboard *keyboard,
                       uint32_t format, int fd, uint32_t size)
{
    struct SDL_WaylandInput *input = data;
    char *map_str;

    if (!data) {
        close(fd);
        return;
    }

    if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
        close(fd);
        return;
    }

    map_str = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
    if (map_str == MAP_FAILED) {
        close(fd);
        return;
    }

    input->xkb.keymap = xkb_map_new_from_string(input->display->xkb_context,
                                                map_str,
                                                XKB_KEYMAP_FORMAT_TEXT_V1,
                                                0);
    munmap(map_str, size);
    close(fd);

    if (!input->xkb.keymap) {
        fprintf(stderr, "failed to compile keymap\n");
        return;
    }

    input->xkb.state = xkb_state_new(input->xkb.keymap);
    if (!input->xkb.state) {
        fprintf(stderr, "failed to create XKB state\n");
        xkb_map_unref(input->xkb.keymap);
        input->xkb.keymap = NULL;
        return;
    }
}

static void
keyboard_handle_enter(void *data, struct wl_keyboard *keyboard,
                      uint32_t serial, struct wl_surface *surface,
                      struct wl_array *keys)
{
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
                      uint32_t serial, struct wl_surface *surface)
{
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
                    uint32_t serial, uint32_t time, uint32_t key,
                    uint32_t state_w)
{
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
                          uint32_t serial, uint32_t mods_depressed,
                          uint32_t mods_latched, uint32_t mods_locked,
                          uint32_t group)
{
}

static const struct wl_keyboard_listener keyboard_listener = {
    keyboard_handle_keymap,
    keyboard_handle_enter,
    keyboard_handle_leave,
    keyboard_handle_key,
    keyboard_handle_modifiers,
};

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
                         enum wl_seat_capability caps)
{
    struct SDL_WaylandInput *input = data;

    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !input->pointer) {
        input->pointer = wl_seat_get_pointer(seat);
        wl_pointer_set_user_data(input->pointer, input);
        wl_pointer_add_listener(input->pointer, &pointer_listener,
                                input);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && input->pointer) {
        wl_pointer_destroy(input->pointer);
        input->pointer = NULL;
    }

    if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !input->keyboard) {
        input->keyboard = wl_seat_get_keyboard(seat);
        wl_keyboard_set_user_data(input->keyboard, input);
        wl_keyboard_add_listener(input->keyboard, &keyboard_listener,
                                 input);
    } else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && input->keyboard) {
        wl_keyboard_destroy(input->keyboard);
        input->keyboard = NULL;
    }
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
};

void
Wayland_display_add_input(SDL_WaylandData *d, uint32_t id)
{
    struct SDL_WaylandInput *input;

    input = malloc(sizeof *input);
    if (input == NULL)
        return;

    memset(input, 0, sizeof *input);
    input->display = d;
    input->seat = wl_display_bind(d->display, id, &wl_seat_interface);
    input->pointer_focus = NULL;
    input->keyboard_focus = NULL;

    wl_seat_add_listener(input->seat, &seat_listener, input);
    wl_seat_set_user_data(input->seat, input);

    wayland_schedule_write(d);
}

/* vi: set ts=4 sw=4 expandtab: */
