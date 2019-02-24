#include <stdio.h>
#include <stdlib.h>

#include "custard.h"
#include "xcb.h"

xcb_connection_t *xcb_connection;
xcb_screen_t *screen;
xcb_visualtype_t *visual;
xcb_colormap_t colormap;
int xcb_file_descriptor;

unsigned short initialize_xcb_connection() {

    unsigned int data[1] = {
        XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_ENTER_WINDOW |
        XCB_EVENT_MASK_PROPERTY_CHANGE
    };

    xcb_generic_error_t *xcb_error;
    xcb_connection = xcb_connect(NULL, NULL);

    if (xcb_connection_has_error(xcb_connection)) {
        debug_output("Error opening XCB connection.");
        return 0;
    }

    screen = xcb_setup_roots_iterator(xcb_get_setup(xcb_connection)).data;

    xcb_error = xcb_request_check(xcb_connection,
        xcb_change_window_attributes_checked(xcb_connection, screen->root,

            XCB_CW_EVENT_MASK, data));

    if (xcb_error) {
        debug_output("%s",
            "Unable to open XCB connection, window manager check failed");
        printf("%s%s\n", "Unable to start custard, ",
            "it appears that another window manager may be running.");

        free(xcb_error);
        return 0;
    }

    data[0] = XCB_EVENT_MASK_SUBSTRUCTURE_REDIRECT |
        XCB_EVENT_MASK_SUBSTRUCTURE_NOTIFY |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        XCB_EVENT_MASK_PROPERTY_CHANGE;

    xcb_change_window_attributes_checked(xcb_connection, screen->root,
        XCB_CW_EVENT_MASK, data);

    /* Visual to draw borders */

    xcb_depth_iterator_t depth_iterator = xcb_screen_allowed_depths_iterator(
        screen);
    xcb_visualtype_iterator_t visual_iterator;

    if (depth_iterator.data) {

        for (; depth_iterator.rem; xcb_depth_next(&depth_iterator)) {
            if (depth_iterator.data->depth == 32) {

                visual_iterator = xcb_depth_visuals_iterator(
                    depth_iterator.data);
                for (; visual_iterator.rem; xcb_visualtype_next(
                    &visual_iterator)) {
                    visual = visual_iterator.data;
                    break;
                }

            }
        }

    }

    if (!visual)
        debug_output("Visual not found for border drawing. Fallback needed.");

    colormap = xcb_generate_id(xcb_connection);
    xcb_create_colormap(xcb_connection, XCB_COLORMAP_ALLOC_NONE, colormap,
        screen->root, visual->visual_id);

    xcb_ungrab_key(xcb_connection, XCB_GRAB_ANY, screen->root,
        XCB_MOD_MASK_ANY);


    xcb_file_descriptor = xcb_get_file_descriptor(xcb_connection);

    commit();

    return 1;

}

void finalize_xcb_connection() {
    xcb_free_colormap(xcb_connection, colormap);

    if (xcb_connection) {
        debug_output("Called");

        xcb_disconnect(xcb_connection);

        return;
    }

    debug_output("Connection already closed.");

    return;
}

void commit() {
    debug_output("Flushed to X");
    xcb_flush(xcb_connection);
}
