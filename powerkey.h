/**
 * @file powerkey.h
 * Headers for the power key logic for the Mode Control Entity
 * <p>
 * Copyright © 2004-2010 Nokia Corporation and/or its subsidiary(-ies).
 * <p>
 * @author David Weinehall <david.weinehall@nokia.com>
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
#ifndef _POWERKEY_H_
#define _POWERKEY_H_

#include <glib.h>

/** Path to the GConf settings for the powerkey module */
# define MCE_GCONF_POWERKEY_PATH       "/system/osso/dsm/powerkey"

/** Powerkey mode setting */
# define MCE_GCONF_POWERKEY_MODE                 MCE_GCONF_POWERKEY_PATH "/mode"

/** Powerkey blanking mode setting */
# define MCE_GCONF_POWERKEY_BLANKING_MODE        MCE_GCONF_POWERKEY_PATH "/blanking_mode"

/** Powerkey press count for proximity sensor override */
# define MCE_GCONF_POWERKEY_PS_OVERRIDE_COUNT    MCE_GCONF_POWERKEY_PATH "/ps_override_count"

/** Maximum delay between powerkey presses for ps override */
# define MCE_GCONF_POWERKEY_PS_OVERRIDE_TIMEOUT  MCE_GCONF_POWERKEY_PATH "/ps_override_timeout"

/** Long press timeout setting */
# define MCE_GCONF_POWERKEY_LONG_PRESS_DELAY     MCE_GCONF_POWERKEY_PATH "/long_press_delay"

/** Double press timeout setting */
# define MCE_GCONF_POWERKEY_DOUBLE_PRESS_DELAY   MCE_GCONF_POWERKEY_PATH "/double_press_delay"

/** Setting for single press actions from display on */
# define MCE_GCONF_POWERKEY_ACTIONS_SINGLE_ON    MCE_GCONF_POWERKEY_PATH "/actions_single_on"

/** Setting for double press actions from display on */
# define MCE_GCONF_POWERKEY_ACTIONS_DOUBLE_ON    MCE_GCONF_POWERKEY_PATH "/actions_double_on"

/** Setting for long press actions from display on */
# define MCE_GCONF_POWERKEY_ACTIONS_LONG_ON      MCE_GCONF_POWERKEY_PATH "/actions_long_on"

/** Setting for single press actions from display off */
# define MCE_GCONF_POWERKEY_ACTIONS_SINGLE_OFF   MCE_GCONF_POWERKEY_PATH "/actions_single_off"

/** Setting for double press actions from display off */
# define MCE_GCONF_POWERKEY_ACTIONS_DOUBLE_OFF   MCE_GCONF_POWERKEY_PATH "/actions_double_off"

/** Setting for long press actions from display off */
# define MCE_GCONF_POWERKEY_ACTIONS_LONG_OFF     MCE_GCONF_POWERKEY_PATH "/actions_long_off"

/** Setting for D-Bus action #1 */
# define MCE_GCONF_POWERKEY_DBUS_ACTION1         MCE_GCONF_POWERKEY_PATH "/dbus_action1"

/** Setting for D-Bus action #2 */
# define MCE_GCONF_POWERKEY_DBUS_ACTION2         MCE_GCONF_POWERKEY_PATH "/dbus_action2"

/** Setting for D-Bus action #3 */
# define MCE_GCONF_POWERKEY_DBUS_ACTION3         MCE_GCONF_POWERKEY_PATH "/dbus_action3"

/** Setting for D-Bus action #4 */
# define MCE_GCONF_POWERKEY_DBUS_ACTION4         MCE_GCONF_POWERKEY_PATH "/dbus_action4"

/** Setting for D-Bus action #5 */
# define MCE_GCONF_POWERKEY_DBUS_ACTION5         MCE_GCONF_POWERKEY_PATH "/dbus_action5"

/** Setting for D-Bus action #6 */
# define MCE_GCONF_POWERKEY_DBUS_ACTION6         MCE_GCONF_POWERKEY_PATH "/dbus_action6"

/** Power key action enable modes */
typedef enum
{
    /** Power key actions disabled */
    PWRKEY_ENABLE_NEVER,

    /** Power key actions always enabled */
    PWRKEY_ENABLE_ALWAYS,

    /** Power key actions enabled if PS is not covered */
    PWRKEY_ENABLE_NO_PROXIMITY,

    /** Power key actions enabled if PS is not covered or display is on */
    PWRKEY_ENABLE_NO_PROXIMITY2,

    PWRKEY_ENABLE_DEFAULT = PWRKEY_ENABLE_ALWAYS,
} pwrkey_enable_mode_t;

typedef enum
{
    /** Pressing power key turns display off */
    PWRKEY_BLANK_TO_OFF,

    /** Pressing power key puts display to lpm state */
    PWRKEY_BLANK_TO_LPM,
} pwrkey_blanking_mode_t;

/** Long delay for the [power] button in milliseconds */
#define DEFAULT_POWERKEY_LONG_DELAY     1500

/** Double press timeout for the [power] button in milliseconds */
#define DEFAULT_POWERKEY_DOUBLE_DELAY   400

/** Default actions for single press while display is on */
#define DEFAULT_POWERKEY_ACTIONS_SINGLE_ON  "blank,tklock"

/** Default actions for double press while display is on */
#define DEFAULT_POWERKEY_ACTIONS_DOUBLE_ON  "blank,tklock,devlock,vibrate"

/** Default actions for long press while display is on */
#define DEFAULT_POWERKEY_ACTIONS_LONG_ON    "shutdown"

/** Default actions for single press while display is off */
#define DEFAULT_POWERKEY_ACTIONS_SINGLE_OFF "unblank"

/** Default actions for double press while display is off */
#define DEFAULT_POWERKEY_ACTIONS_DOUBLE_OFF "unblank,tkunlock"

/** Default actions for long press while display is off
 *
 * Note: If kernel side reports immediately power key press + release
 *       when device is suspended, detecting long presses might not
 *       work when display is off -> leave unset by default. */
#define DEFAULT_POWERKEY_ACTIONS_LONG_OFF   ""

/** Default argument for signal sent due to dbus1 action */
#define DEFAULT_POWERKEY_DBUS_ACTION1       "event1"

/** Default argument for signal sent due to dbus2 action */
#define DEFAULT_POWERKEY_DBUS_ACTION2       "event2"

/** Default argument for signal sent due to dbus3 action */
#define DEFAULT_POWERKEY_DBUS_ACTION3       "event3"

/** Default argument for signal sent due to dbus4 action */
#define DEFAULT_POWERKEY_DBUS_ACTION4       "event4"

/** Default argument for signal sent due to dbus5 action */
#define DEFAULT_POWERKEY_DBUS_ACTION5       "event5"

/** Default argument for signal sent due to dbus6 action */
#define DEFAULT_POWERKEY_DBUS_ACTION6       "event6"

/* When MCE is made modular, this will be handled differently */
gboolean mce_powerkey_init(void);
void mce_powerkey_exit(void);

#endif /* _POWERKEY_H_ */
