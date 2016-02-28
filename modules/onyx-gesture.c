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
#include "../mce-gconf.h"
#include "../evdev.h"
#include "doubletap.h"

#include <gmodule.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/** Module name */
#define MODULE_NAME        "onyx-gesture"

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

static struct input_event evmimic = {};
static DBusConnection *sessionbus_connection = NULL;

/** Touchscreen double tap gesture enable mode */
static gint doubletap_enable_mode = DBLTAP_ENABLE_DEFAULT;
/** GConf callback ID for doubletap_enable_mode */
static guint doubletap_enable_mode_cb_id = 0;

/** Connect to dbus sessionbus
 *
 * @return TRUE on success, FALSE if connection failed
 */
static gboolean connect_sessionbus(void)
{
    DBusError error = DBUS_ERROR_INIT;
    gboolean ret = true;

    if( !(sessionbus_connection = dbus_bus_get(DBUS_BUS_SESSION, &error)) )
    {
        mce_log(LL_CRIT, "Failed to open connection to session bus; %s", error.message);
        dbus_error_free(&error);
        ret = false;
        goto EXIT;
    }
    dbus_connection_setup_with_g_main(sessionbus_connection, NULL);
EXIT:
    return ret;
}

/** Toggle torch/flashlight brightness
 * toggles from any value to 0, and from 0 to 255
 *
 * @return TRUE on success, FALSE if file open failed
 */
static gboolean toggle_torch_brightness(void)
{
    int fd;
    char buf[5];
    gboolean ret = false;
    int brightness = 0;

    fd = open("/sys/class/leds/led:flash_torch/brightness", O_RDONLY);
    if (fd < 0)
        goto EXIT;

    if (!(read(fd, buf, sizeof(buf)) > 0))
    {
        close(fd);
        goto EXIT;
    }

    close(fd);

    brightness = atoi(buf);

    if (brightness == 0)
        brightness = 255;
    else
        brightness = 0;

    fd = open("/sys/class/leds/led:flash_torch/brightness", O_WRONLY);
    if (fd < 0)
        goto EXIT;

    sprintf(buf, "%d", brightness);
    if (!(write(fd, buf, strlen(buf)) > 0))
    {
        close(fd);
        goto EXIT;
    }

    close(fd);

    ret = true;
EXIT:
    return ret;
}

/** Send method to current musicplayer with mpris2 support
 *
 * @param method name (Play, Pause, Next, Previous, ...)
 */
static void mpris2_control(const gchar *method)
{
    DBusMessage *msg = 0;
    DBusMessage *reply = 0;
    DBusError error = DBUS_ERROR_INIT;

    gchar service[255] = "org.mpris.MediaPlayer2.jolla-mediaplayer";

    msg = dbus_new_method_call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "ListNames");
    if (!(reply = dbus_connection_send_with_reply_and_block(sessionbus_connection, msg, 1000, &error)))
    {
        mce_log(LL_CRIT, "Failed to get services from session bus; %s", error.message);
        dbus_error_free(&error);
    }
    else
    {
        DBusMessageIter iter;
        DBusMessageIter subiter;

        dbus_message_iter_init(reply, &iter);

        switch( dbus_message_iter_get_arg_type(&iter) )
        {
        case DBUS_TYPE_ARRAY:
            switch( dbus_message_iter_get_element_type(&iter) )
            {
            case DBUS_TYPE_STRING:

                dbus_message_iter_recurse(&iter, &subiter);

                while ( dbus_message_iter_get_arg_type(&subiter) == DBUS_TYPE_STRING )
                {
                    const char *tmp = 0;
                    dbus_message_iter_get_basic(&subiter, &tmp);
                    dbus_message_iter_next(&subiter);

                    if (g_ascii_strncasecmp(tmp, "org.mpris.MediaPlayer2.", 23) == 0)
                    {
                        g_strlcpy(tmp, service, 254);
                        mce_log(LL_INFO, "Found mpris2 service: %s", service);
                    }
                }
                break;
            default:
                break;
            }
        default:
            break;
        }
    }

    msg = 0;

    msg = dbus_new_method_call(&service, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", method);
    if (dbus_connection_send(sessionbus_connection, msg, NULL) == FALSE)
    {
        mce_log(LL_WARN, "Send operation failed on D-Bus sessionbus");
    }

    msg = 0;
}

/** Gesture event callback
 *
 * @param input_event struct
 */
static void onyx_gesture_trigger(gconstpointer const data)
{
    struct input_event const *const *evp;
    struct input_event const *ev;
    struct input_event *ev_mimic = &evmimic;

    DBusMessage *msg = 0;

    if( !(evp = data) )
        goto EXIT;

    if( !(ev = *evp) )
        goto EXIT;

    cover_state_t proximity_sensor_state = datapipe_get_gint(proximity_sensor_pipe);
    cover_state_t lid_cover_policy_state = datapipe_get_gint(lid_cover_policy_pipe);

    mce_log(LL_DEBUG, "Gesture event %s; proximity=%s, lid=%s",
            evdev_get_event_code_name(ev->type, ev->code),
            proximity_state_repr(proximity_sensor_state),
            proximity_state_repr(lid_cover_policy_state));

    switch( doubletap_enable_mode )
    {
        case DBLTAP_ENABLE_NEVER:
            mce_log(LL_DEVEL, "Gesture ignored due to doubletap setting=never");
            goto EXIT;

        case DBLTAP_ENABLE_ALWAYS:
            break;

        default:
        case DBLTAP_ENABLE_NO_PROXIMITY:
            if( lid_cover_policy_state == COVER_CLOSED )
            {
                mce_log(LL_DEVEL, "Gesture ignored due to lid=closed");
                goto EXIT;
            }
            if( proximity_sensor_state != COVER_OPEN )
            {
                mce_log(LL_DEVEL, "Gesture ignored due to proximity");
                goto EXIT;
            }
            break;
    }

    if ( sessionbus_connection == NULL )
        if ( !connect_sessionbus() )
            goto EXIT;

    switch (ev->code)
    {
        case KEY_GESTURE_CIRCLE:
            mce_log(LL_DEBUG, "Camera gesture");

            /* Mimic gesture to wake-up */
            ev_mimic->type  = EV_MSC;
            ev_mimic->code  = MSC_GESTURE;
            ev_mimic->value = 0x4;

            execute_datapipe(&touchscreen_pipe, &ev_mimic, USE_INDATA, DONT_CACHE_INDATA);
            
            msg = dbus_new_method_call("com.jolla.camera", "/", "com.jolla.camera.ui", "showViewfinder");
            if (dbus_connection_send(sessionbus_connection, msg, NULL) == FALSE) 
            {
                mce_log(LL_WARN, "Send operation failed on D-Bus sessionbus");
            }

            msg = 0;

            break;

        case KEY_GESTURE_LEFT_V:
            mce_log(LL_DEBUG, "Previous track gesture");
            mpris2_control("Previous");
            break;

        case KEY_GESTURE_RIGHT_V:
            mce_log(LL_DEBUG, "Next track gesture");
            mpris2_control("Next");
            break;

        case KEY_GESTURE_TWO_SWIPE:
            mce_log(LL_DEBUG, "Play/Pause gesture");
            mpris2_control("PlayPause");
            break;

        case KEY_GESTURE_V:
            mce_log(LL_DEBUG, "Flashlight gesture");

            if (!toggle_torch_brightness())
            {
                mce_log(LL_ERR, "Failed to toggle torch brightness");
            }
            break;

        default:
            break;
    }

EXIT:
    return;
}

/** GConf callback for touchscreen/keypad lock related settings
 *
 * @param gcc Unused
 * @param id Connection ID from gconf_client_notify_add()
 * @param entry The modified GConf entry
 * @param data Unused
 */
static void doubletap_gconf_cb(GConfClient *const gcc, const guint id,
                               GConfEntry *const entry, gpointer const data)
{
    (void)gcc;
    (void)data;
    (void)id;

    const GConfValue *gcv = gconf_entry_get_value(entry);

    if( id == doubletap_enable_mode_cb_id )
    {
        doubletap_enable_mode = gconf_value_get_int(gcv);
    }
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

    /** Touchscreen double tap gesture mode */
    mce_gconf_track_int(MCE_GCONF_DOUBLETAP_MODE,
                        &doubletap_enable_mode,
                        DBLTAP_ENABLE_DEFAULT,
                        doubletap_gconf_cb,
                        &doubletap_enable_mode_cb_id);
   
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

    mce_gconf_notifier_remove(doubletap_enable_mode_cb_id),
        doubletap_enable_mode_cb_id = 0;

    if (sessionbus_connection != NULL)
    {
        mce_log(LL_DEBUG, "Unreferencing D-Bus sessionbus connection");
        dbus_connection_unref(sessionbus_connection);
        sessionbus_connection = NULL;
    }

    return;
}
