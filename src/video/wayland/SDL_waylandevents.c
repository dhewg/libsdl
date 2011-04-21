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

#include <X11/extensions/XKBcommon.h>
#include <linux/input.h>

#include "../../events/scancodes_xfree86.h"

typedef uint32_t KeySym;
#include "../x11/imKStoUCS.h"

#include <errno.h>

#include <sys/select.h>


const char *option_xkb_layout = "de";
const char *option_xkb_variant = "nodeadkeys";
const char *option_xkb_options = "";

void
Wayland_init_xkb(SDL_WaylandData *d)
{
    struct xkb_rule_names names;

    names.rules = "evdev";
    names.model = "evdev";
    names.layout = option_xkb_layout;
    names.variant = option_xkb_variant;
    names.options = option_xkb_options;

    d->xkb = xkb_compile_keymap_from_rules(&names);
    if (!d->xkb) {
        SDL_SetError("failed to compile keymap\n");
        exit(1);
    }

    d->input_table = xfree86_scancode_table2;
    d->input_table_size = SDL_arraysize(xfree86_scancode_table2);
}

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

static void
window_handle_motion(void *data, struct wl_input_device *input_device,
                     uint32_t time,
                     int32_t x, int32_t y, int32_t sx, int32_t sy)
{
    struct SDL_WaylandInput *input = data;
    SDL_WaylandWindow *window = input->pointer_focus;
    //int location, pointer = POINTER_LEFT_PTR;

    input->x = x;
    input->y = y;
    input->sx = sx;
    input->sy = sy;
    SDL_SendMouseMotion(window->sdlwindow, 0, sx, sy);
    //location = get_pointer_location(window, input->sx, input->sy);

    //set_pointer_image(input, time, pointer);
}

static void
window_handle_button(void *data,
                     struct wl_input_device *input_device,
                     uint32_t time, uint32_t button, uint32_t state)
{
    struct SDL_WaylandInput *input = data;
    SDL_WaylandWindow *window = input->pointer_focus;
    uint32_t sdl_button;

    switch (button) {
    case BTN_LEFT:
    default:
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
    }

    SDL_SendMouseButton(window->sdlwindow,
                        state ? SDL_PRESSED : SDL_RELEASED, sdl_button);
}

static char *
keysym_to_utf8(uint32_t sym)
{
    char *text = NULL;
    uint32_t inbuf[2];
    unsigned ucs4;

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
window_handle_pointer_focus(void *data,
                            struct wl_input_device *input_device,
                            uint32_t time, struct wl_surface *surface,
                            int32_t x, int32_t y, int32_t sx, int32_t sy)
{
    struct SDL_WaylandInput *input = data;
    SDL_WaylandWindow *window;
    /*int pointer;*/

    if (surface) {
        input->pointer_focus = wl_surface_get_user_data(surface);
        window = input->pointer_focus;
        SDL_SetMouseFocus(window->sdlwindow);
        /*pointer = POINTER_LEFT_PTR;

          set_pointer_image(input, time, pointer);*/
    } else {
        SDL_SetMouseFocus(NULL);
        input->pointer_focus = NULL;
        //input->current_pointer_image = POINTER_UNSET;
    }
}

static void
window_handle_keyboard_focus(void *data,
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

static const struct wl_input_device_listener input_device_listener = {
    window_handle_motion,
    window_handle_button,
    window_handle_key,
    window_handle_pointer_focus,
    window_handle_keyboard_focus,
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
    input->input_device = wl_input_device_create(d->display, id, 1);
    input->pointer_focus = NULL;
    input->keyboard_focus = NULL;

    wl_input_device_add_listener(input->input_device,
                                 &input_device_listener, input);
    wl_input_device_set_user_data(input->input_device, input);

    wayland_schedule_write(d);
}

/* vi: set ts=4 sw=4 expandtab: */
