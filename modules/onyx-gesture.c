/**
 * @file onyx-gesture.c
 * Onyx gesture interface module for the Mode Control Entity
 * <p>
 * Copyright © 2016 Kimmo Lindholm
 * <p>
 * @author Kimmo Lindholm <kimmo.lindholm@eke.fi>
 *
 * mce is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * mce is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with mce.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../mce.h"
#include "../mce-log.h"
#include "../mce-dbus.h"

#include <gmodule.h>
#include <linux/input.h>

/** Module name */
#define MODULE_NAME		"onyx-gesture"

/** Functionality provided by this module */
static const gchar *const provides[] = { MODULE_NAME, NULL };

/** Module information */
G_MODULE_EXPORT module_info_struct module_info = {
	/** Name of the module */
	.name = MODULE_NAME,
	/** Module provides */
	.provides = provides,
	/** Module priority */
	.priority = 250
};

 /** Constants */
 /* onyx specific gesture key definitions */
#define KEY_GESTURE_CIRCLE      250 // draw circle to lunch camera
#define KEY_GESTURE_TWO_SWIPE   251 // swipe two finger vertically to play/pause
#define KEY_GESTURE_V           252 // draw v to toggle flashlight
#define KEY_GESTURE_LEFT_V      253 // draw left arrow for previous track
#define KEY_GESTURE_RIGHT_V     254 // draw right arrow for next track

/** Gesture event callback
 *
 * @param input_event struct
 */
static void onyx_gesture_trigger(gconstpointer const data)
{
    mce_log(LL_DEBUG, "Gesture arrived in module!");

    struct input_event const *const *evp;
    struct input_event const *ev;

    if( !(evp = data) )
        goto EXIT;

    if( !(ev = *evp) )
        goto EXIT;

    mce_log(LL_DEBUG, "Gesture data, type=%d code=%d value=%d", ev->type, ev->code, ev->value);

    switch (ev->code)
    {
        case KEY_GESTURE_CIRCLE:
            mce_log(LL_DEBUG, "Camera");
            break;
        case KEY_GESTURE_LEFT_V:
        case KEY_GESTURE_RIGHT_V:
        case KEY_GESTURE_TWO_SWIPE:
            mce_log(LL_DEBUG, "Music");
            break;
        case KEY_GESTURE_V:
            mce_log(LL_DEBUG, "Flashlight");
            break;
        default: break;
    }

EXIT:
    return;
}

/**
 * Init function for the interface module
 *
 * @param module Unused
 * @return NULL on success, a string with an error message on failure
 */
G_MODULE_EXPORT const gchar *g_module_check_init(GModule *module);
const gchar *g_module_check_init(GModule *module)
{
	(void)module;
    
    append_output_trigger_to_datapipe(&onyx_gesture_pipe, onyx_gesture_trigger);

	return NULL;
}

/**
 * Exit function for the interface module
 *
 * @param module Unused
 */
G_MODULE_EXPORT void g_module_unload(GModule *module);
void g_module_unload(GModule *module)
{
	(void)module;
    
    remove_output_trigger_from_datapipe(&onyx_gesture_pipe, onyx_gesture_trigger);

	return;
}
