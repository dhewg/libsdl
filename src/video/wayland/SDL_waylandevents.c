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
#include "../../events/scancodes_xfree86.h"

#include "SDL_waylandvideo.h"
#include "SDL_waylandevents_c.h"
#include "SDL_waylandwindow.h"

#include <linux/input.h>
#include <sys/select.h>
#include <sys/mman.h>
#include <errno.h>
#include <unistd.h>
#include <xkbcommon/xkbcommon.h>

struct SDL_WaylandInput {
    SDL_WaylandData *display;
    struct wl_seat *seat;
    struct wl_pointer *pointer;
    struct wl_keyboard *keyboard;
    SDL_WaylandWindow *pointer_focus;
    SDL_WaylandWindow *keyboard_focus;

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
            SDL_SetError("select failed: %m");
            break;
        }
        if (retval == 1)
            wl_display_iterate(d->display, WL_DISPLAY_READABLE);
    } while (retval > 0);
}

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
    struct SDL_WaylandInput *input = data;
    SDL_WaylandWindow *window = wl_surface_get_user_data(surface);

    input->keyboard_focus = window;
    window->keyboard_device = input;
    SDL_SetKeyboardFocus(window->sdlwindow);
}

static void
keyboard_handle_leave(void *data, struct wl_keyboard *keyboard,
                      uint32_t serial, struct wl_surface *surface)
{
    SDL_SetKeyboardFocus(NULL);
}

static void
keyboard_handle_key(void *data, struct wl_keyboard *keyboard,
                    uint32_t serial, uint32_t time, uint32_t key,
                    uint32_t state_w)
{
    struct SDL_WaylandInput *input = data;
    SDL_WaylandWindow *window = input->keyboard_focus;
    enum wl_keyboard_key_state state = state_w;
    const xkb_keysym_t *syms;
    uint32_t scancode;
    char text[8];
    int size;

    if (key < SDL_arraysize(xfree86_scancode_table2)) {
        scancode = xfree86_scancode_table2[key];

        // TODO when do we get WL_KEYBOARD_KEY_STATE_REPEAT?
        if (scancode != SDL_SCANCODE_UNKNOWN)
            SDL_SendKeyboardKey(state == WL_KEYBOARD_KEY_STATE_PRESSED ?
                                SDL_PRESSED : SDL_RELEASED, scancode);
    }

    if (!window || window->keyboard_device != input || !input->xkb.state)
        return;

    // TODO can this happen?
    if (xkb_key_get_syms(input->xkb.state, key + 8, &syms) != 1)
        return;

    if (state) {
        size = xkb_keysym_to_utf8(syms[0], text, sizeof text);

        if (size > 0) {
            text[size] = 0;
            SDL_SendKeyboardText(text);
        }
    }
}

static void
keyboard_handle_modifiers(void *data, struct wl_keyboard *keyboard,
                          uint32_t serial, uint32_t mods_depressed,
                          uint32_t mods_latched, uint32_t mods_locked,
                          uint32_t group)
{
    struct SDL_WaylandInput *input = data;

    xkb_state_update_mask(input->xkb.state, mods_depressed, mods_latched,
                          mods_locked, 0, 0, group);
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

    d->input = input;

    wl_seat_add_listener(input->seat, &seat_listener, input);
    wl_seat_set_user_data(input->seat, input);

    wayland_schedule_write(d);
}

void Wayland_display_destroy_input(SDL_WaylandData *d)
{
    struct SDL_WaylandInput *input = d->input;

    if (!input)
        return;

    wl_seat_destroy(input->seat);

    xkb_state_unref(input->xkb.state);
    input->xkb.state = NULL;

    xkb_map_unref(input->xkb.keymap);
    input->xkb.keymap = NULL;

    free(input);
    d->input = NULL;
}

/* vi: set ts=4 sw=4 expandtab: */
