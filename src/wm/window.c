#include "custard.h"
#include "geometry.h"
#include "grid.h"
#include "monitor.h"
#include "rules.h"
#include "window.h"

#include "../vector.h"

#include "../xcb/connection.h"
#include "../xcb/ewmh.h"
#include "../xcb/window.h"

vector_t* windows = NULL;
xcb_window_t focused_window = XCB_WINDOW_NONE;

unsigned short window_should_be_managed(xcb_window_t window_id) {
    if (window_is_managed(window_id)) return 0;

    xcb_get_window_attributes_cookie_t window_attributes_cookie;
    window_attributes_cookie = xcb_get_window_attributes(xcb_connection,
        window_id);

    xcb_get_window_attributes_reply_t* attributes;
    attributes = xcb_get_window_attributes_reply(xcb_connection,
        window_attributes_cookie, NULL);

    if (attributes && attributes->override_redirect) return 0;

    xcb_ewmh_get_atoms_reply_t window_type;
    xcb_get_property_cookie_t window_type_cookie;
    window_type_cookie = xcb_ewmh_get_wm_window_type(ewmh_connection,
        window_id);

    if (xcb_ewmh_get_wm_window_type_reply(ewmh_connection, window_type_cookie,
        &window_type, NULL)) {
        xcb_atom_t atom;

        for (unsigned int index = 0; index < window_type.atoms_len; index++) {
            atom = window_type.atoms[index];

            if (atom == ewmh_connection->_NET_WM_WINDOW_TYPE_TOOLBAR       ||
                atom == ewmh_connection->_NET_WM_WINDOW_TYPE_MENU          ||
                atom == ewmh_connection->_NET_WM_WINDOW_TYPE_DROPDOWN_MENU ||
                atom == ewmh_connection->_NET_WM_WINDOW_TYPE_POPUP_MENU    ||
                atom == ewmh_connection->_NET_WM_WINDOW_TYPE_DND           ||
                atom == ewmh_connection->_NET_WM_WINDOW_TYPE_DOCK          ||
                atom == ewmh_connection->_NET_WM_WINDOW_TYPE_DESKTOP       ||
                atom == ewmh_connection->_NET_WM_WINDOW_TYPE_NOTIFICATION) {
                xcb_ewmh_get_atoms_reply_wipe(&window_type);
                return 0;
            } else if (atom == ewmh_connection->_NET_WM_WINDOW_TYPE_SPLASH) {
                xcb_ewmh_get_atoms_reply_wipe(&window_type);
                // do something else

                return 0;
            }
        }

        xcb_ewmh_get_atoms_reply_wipe(&window_type);
    }

    return 1;
}

unsigned short window_is_managed(xcb_window_t window_id) {
    if (window_id == xcb_screen->root || window_id == ewmh_window)
        return 0;

    window_t* window = get_window_by_id(window_id);

    if (!window)
        return 0;

    return 1;
}

window_t* get_window_by_id(xcb_window_t window_id) {
    if (window_id == xcb_screen->root || window_id == ewmh_window)
        return NULL;

    if (windows) {
        window_t* window;
        for (unsigned int index = 0; index < windows->size; index++) {
            window = get_from_vector(windows, index);

            if (window->id == window_id)
                return window;
        }
    }

    return NULL;
}

window_t* manage_window(xcb_window_t window_id) {
    window_t* window = (window_t*)malloc(sizeof(window_t));
    window->id = window_id;
    window->parent = XCB_WINDOW_NONE;

    /* Geometry */
    monitor_t* monitor = monitor_with_cursor_residence();

    grid_geometry_t* geometry = (grid_geometry_t*)malloc(
        sizeof(grid_geometry_t));

    unsigned short window_follows_rule = 0;
    if (rules) {
        rule_t* rule;
        for (unsigned int index = 0; index < rules->size; index++) {
            rule = get_from_vector(rules, index);

            char* subject;
            if (rule->attribute == class)
                subject = class_of_window(window_id);
            else if (rule->attribute == name)
                subject = name_of_window(window_id);
            else
                subject = name_of_window(window_id); // unimplemented?

            if (expression_matches(rule->expression, subject)) {
                window_follows_rule = 1;
                break;
            }
        }
    }

    if (window_follows_rule) {
        // TODO: this
    }

    geometry->x = calculate_default_x(monitor);
    geometry->y = calculate_default_y(monitor);
    geometry->height = calculate_default_height(monitor);
    geometry->width = calculate_default_width(monitor);

    set_window_geometry(window, geometry);

    if (!windows)
        windows = construct_vector();

    push_to_vector(windows, window);

    log("Window(%08x) managed", window_id);

    return window;
}

void unmanage_window(xcb_window_t window_id) {
    window_t* window;
    for (unsigned int index = 0; index < windows->size; index++) {
        window = get_from_vector(windows, index);

        if (window->id == window_id) {
            pull_from_vector(windows, index);
            log("Window(%08x) unmanaged", window_id);
            return;
        }
    }
}

void set_window_geometry(window_t* window, grid_geometry_t* geometry) {
    window->geometry = geometry;

    monitor_t* monitor = monitor_with_cursor_residence();
    screen_geometry_t* screen_geometry;
    screen_geometry = get_equivalent_screen_geometry(geometry, monitor);

    change_window_geometry(window->id,
        (unsigned int)screen_geometry->x,
        (unsigned int)screen_geometry->y,
        (unsigned int)screen_geometry->height,
        (unsigned int)screen_geometry->width);
    free(screen_geometry);

    log("Window(%08x) window geometry set", window->id);
}

void focus_on_window(window_t* window) {
    if (focused_window == window->id)
        return;

    xcb_window_t previous_window = focused_window;
    focused_window = XCB_WINDOW_NONE;

    xcb_grab_button(xcb_connection, 0, previous_window,
        XCB_EVENT_MASK_BUTTON_PRESS,
        XCB_GRAB_MODE_ASYNC, XCB_GRAB_MODE_ASYNC,
        XCB_NONE, XCB_NONE,
        XCB_BUTTON_INDEX_ANY, XCB_MOD_MASK_ANY);
    if (window_is_managed(previous_window))
        border_update(get_window_by_id(previous_window));

    focused_window = window->id;
    xcb_ungrab_button(xcb_connection,
        XCB_BUTTON_INDEX_ANY, window->id, XCB_MOD_MASK_ANY);
    focus_window(window->id);
    border_update(window);
}

void border_update(window_t* window) {
    //
}
