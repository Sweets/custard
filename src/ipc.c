#include <stdlib.h>
#include <string.h>

#include "custard.h"
#include "grid.h"
#include "ipc.h"
#include "rules.h"
#include "window.h"
#include "workspaces.h"
#include "xcb.h"

struct ipc_command ipc_commands[][number_of_ipc_commands] = {
    {{ "halt",              ipc_command_wm_halt }},
    {{ "configure",         ipc_command_wm_configure }},

    {{ "geometry",          ipc_command_new_geometry }},
    {{ "rule",              ipc_command_new_window_rule }},

    {{ "window.close",      ipc_command_window_close }},
    {{ "window.raise",      ipc_command_window_raise}},
    {{ "window.lower",      ipc_command_window_lower}},
    {{ "window.geometry",   ipc_command_window_change_geometry }},

    {{ "workspace",         ipc_command_change_workspace }},
};

void process_input(char *data) {
    debug_output("Input data\n\t%s", data);

    char *token;
    char delimiter[2] = { 29, '\0' };

    char *descriptor;
    char *arguments[32];

    unsigned int index = 0;

    while ((token = strsep(&data, delimiter))) {
        if (index == 32) {
            debug_output("Input has more arguments than accepted, ignoring");
            return;
        }

        if (index < 1)
            descriptor = token;
        else
            arguments[index - 1] = token;
        index++;
    }

    arguments[index] = "\0"; // \0\0

    if (!descriptor)
        return;

    index = 0;

    unsigned short flush;
    struct ipc_command *command = NULL;

    for (; index < number_of_ipc_commands; index++) {
        command = ipc_commands[index];

        if (strcmp(descriptor, command->command))
            continue;

        flush = command->routine(arguments);

        if (flush)
            commit();
    }
}

unsigned short parse_boolean(char *string) {
    if (string && (!strcmp(string, "True") || !strcmp(string, "true")))
        return 1;

    return 0;
}

unsigned int parse_unsigned_integer(char *string) {
    if (string)
        return atoi(string);

    return 0;
}

unsigned int parse_rgba_color(char *string) {
    unsigned int rgba = 0xffffffff;

    if (!string)
        return rgba;

    unsigned int length = strlen(string);
    if (length > 9 || length < 3 || length == 6 || length == 8)
        return rgba;
    else if (string[0] != '#')
        return rgba;

    unsigned int offset = 0;
    if (length < 6)
        offset = 4;

    char group[8] = "000000ff";
    for (unsigned int index = 0; index < (length - 1); index++)
        group[index + offset] = string[index + 1];

    rgba = strtol(group, NULL, 16);

    unsigned int alpha, red, green, blue;
    if (offset) {
        alpha = (rgba & 0x000F) * 0x11;
        red   = ((rgba & 0xF000) / 0x1000) * 0x11;
        green = ((rgba & 0x0F00) / 0x100) * 0x11;
        blue  = ((rgba & 0x00F0) / 0x10) * 0x11;
    } else {
        alpha = (rgba & 0x000000FF);
        red   = (rgba & 0xFF000000) / 0x1000000;
        green = (rgba & 0x00FF0000) / 0x10000;
        blue  = (rgba & 0x0000FF00) / 0x100;
    }

    red   = (red * alpha) / 0xFF;
    green = (green * alpha) / 0xFF;
    blue  = (blue * alpha) / 0xFF;

    alpha *= 0x1000000;
    red   *= 0x10000;
    green *= 0x100;

    return alpha | red | green | blue;
}

/* IPC commands */

unsigned short ipc_command_wm_halt(char **arguments) {
    suppress_unused(arguments);

    window_manager_is_running = 0;
    return 0;
}

unsigned short ipc_command_wm_configure(char **arguments) {

    /*
     * custard -- configure NAME VALUE
     */

    char *setting = arguments[0];

    unsigned short boolean = parse_boolean(arguments[1]);
    unsigned int uint = parse_unsigned_integer(arguments[1]);
    unsigned int argb = parse_rgba_color(arguments[1]);

    if (!strcmp("debug", setting))
        debug_mode = boolean;
    else if (!strcmp("grid.rows", setting))
        grid_rows = uint;
    else if (!strcmp("grid.columns", setting))
        grid_columns = uint;
    else if (!strcmp("grid.gap", setting))
        grid_gap = uint;
    else if (!strcmp("grid.offset.top", setting))
        grid_offset_top = uint;
    else if (!strcmp("grid.offset.bottom", setting))
        grid_offset_bottom = uint;
    else if (!strcmp("grid.offset.left", setting))
        grid_offset_left = uint;
    else if (!strcmp("grid.offset.right", setting))
        grid_offset_right = uint;
    else if (!strcmp("border.type", setting))
        border_type = uint;
    else if (!strcmp("border.inner.size", setting))
        border_inner_size = uint;
    else if (!strcmp("border.outer.size", setting))
        border_outer_size = uint;
    else if (!strcmp("border.color.focused", setting))
        border_focused_color = argb;
    else if (!strcmp("border.color.unfocused", setting))
        border_unfocused_color = argb;
    else if (!strcmp("border.color.background", setting))
        border_background_color = argb;
    else if (!strcmp("border.color.switch", setting))
        border_invert_colors = boolean;
    else if (!strcmp("workspaces", setting))
        number_of_workspaces = uint;

    if (border_type == 0)
        border_total_size = 0;
    else if (border_type == 1) {
        border_total_size = border_outer_size;

        if (border_inner_size > border_outer_size)
            border_total_size = border_inner_size;
    } else {
        border_total_size = border_inner_size +
            ((border_type - 1) * border_outer_size);
    }

    apply_configuration_to_grid();

    unsigned int index = 0;

    window_t *window = NULL;
    for (; index < managed_windows->size; index++) {
        window = get_from_vector(managed_windows, index);
        border_update(window->id);
    }

    for (; index < number_of_workspaces; index++)
        create_new_workspace();

    return 1;
}

unsigned short ipc_command_new_geometry(char **arguments) {
    unsigned int x, y, height, width;

    /*
     * custard -- geometry NAME X,Y WIDTHxHEIGHT
     */

    if (!arguments[1] || !arguments[2])
        return 0;

    char *position = strdup(arguments[1]);
    char *position_pointer = position;

    char *size = strdup(arguments[2]);
    char *size_pointer = size;

    char *token;

    token = strsep(&position, ",");

    x = parse_unsigned_integer(token);
    y = parse_unsigned_integer(position);

    token = strsep(&size, "x");

    width = parse_unsigned_integer(token);
    height = parse_unsigned_integer(size);

    create_new_geometry(arguments[0], x, y, height, width);

    free(position_pointer);
    free(size_pointer);

    return 0;
}

unsigned short ipc_command_new_window_rule(char **arguments) {
    
    /*
     * custard -- rule window.VALUE? REGEX ...
     */

    if (!arguments[0] || !arguments[1])
        return 0;

    create_new_rule(arguments);

    return 0;
}

unsigned short ipc_command_window_close(char **arguments) {
    suppress_unused(arguments);

    if (!get_focused_window())
        return 0;

    close_window(focused_window);

    return 1;
}

unsigned short ipc_command_window_raise(char **arguments) {
    suppress_unused(arguments);

    if (!get_focused_window())
        return 0;

    raise_window(focused_window);
    return 1;
}

unsigned short ipc_command_window_lower(char **arguments) {
    suppress_unused(arguments);

    if (!get_focused_window())
        return 0;

    lower_window(focused_window);
    return 1;
}

unsigned short ipc_command_window_change_geometry(char **arguments) {
    if (!get_focused_window())
        return 0;

    named_geometry_t *geometry = NULL;
    for (unsigned int index = 0; index < named_geometries->size; index++) {
        geometry = get_from_vector(named_geometries, index);

        if (!strcmp(geometry->name, arguments[0])) {
            change_window_geometry(focused_window,
                geometry->x, geometry->y, geometry->height, geometry->width);

            if (get_window_from_id(focused_window))
                border_update(focused_window);

            return 1;
        }
    }

    return 0;
}

unsigned short ipc_command_change_workspace(char **arguments) {
    focus_on_workspace(parse_unsigned_integer(arguments[0]));

    return 1;
}
