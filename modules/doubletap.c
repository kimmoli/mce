/* ------------------------------------------------------------------------- *
 * Copyright (C) 2013-2014 Jolla Ltd.
 * Contact: Simo Piiroinen <simo.piiroinen@jollamobile.com>
 * License: LGPLv2
 * ------------------------------------------------------------------------- */

#include "doubletap.h"

#include "../mce.h"
#include "../mce-io.h"
#include "../mce-log.h"
#include "../mce-conf.h"
#include "../mce-gconf.h"

#include <stdbool.h>
#include <unistd.h>

#include <glib.h>
#include <gmodule.h>

/** Config value for doubletap enable mode */
static dbltap_mode_t dbltap_mode = DBLTAP_ENABLE_DEFAULT;

static guint dbltap_mode_gconf_id = 0;

/** Latest reported proximity sensor state */
static cover_state_t dbltap_ps_state = COVER_UNDEF;

/** Latest reported proximity blanking */
static bool dbltap_ps_blank = false;

/** Path to doubletap wakeup control file */
static char *dbltap_ctrl_path = 0;

/** String to write when enabling double tap wakeups */
static char *dbltap_enable_val = 0;

/** String to write when disabling double tap wakeups */
static char *dbltap_disable_val = 0;

typedef enum {
        DT_UNDEF = -1,
        DT_DISABLED,
        DT_ENABLED,
        DT_DISABLED_NOSLEEP,
} dt_state_t;

static const char * const dt_state_name[] =
{
        "disabled",
        "enabled",
        "disabled-no-sleep",
};

/** Path to touchpanel sleep blocking control file */
static char *sleep_mode_ctrl_path = 0;

/** Value to write when touch panel is allowed to enter sleep mode */
static char *sleep_mode_allow_val = 0;

/** Value to write when touch panel is not allowed to enter sleep mode */
static char *sleep_mode_deny_val  = 0;

/** Allow/deny touch panel to enter sleep mode
 *
 * @param allow true to allow touch to sleep, false to deny
 */
static void dbltap_allow_sleep_mode(bool allow)
{
        /* Initialized not to match any bool value  */
        static int allowed = -1;

        /* Whether or not this function gets called depends on the
         * availability of dbltap_ctrl_path, so we need to check that
         * sleep_mode_ctrl_path and related values are also configured
         * and available */

        if( !sleep_mode_ctrl_path )
                goto EXIT;

        if( !sleep_mode_allow_val || !sleep_mode_deny_val )
                goto EXIT;

        if( allowed == allow )
                goto EXIT;

        allowed = allow;

        mce_log(LL_DEBUG, "touch panel sleep mode %s",
                allowed ? "allowed" : "denied");

        mce_write_string_to_file(sleep_mode_ctrl_path,
                                 allowed ?
                                 sleep_mode_allow_val :
                                 sleep_mode_deny_val);

EXIT:
        return;
}

/** Enable/disable doubletap wakeups
 *
 * @param state disable/enable/disable-without-powering-off
 */
static void dbltap_set_state(dt_state_t state)
{
        static dt_state_t prev_state = DT_UNDEF;

        bool allow_sleep_mode = true;

        if( prev_state == state )
                goto EXIT;

        prev_state = state;

        mce_log(LL_DEBUG, "double tap wakeups: %s", dt_state_name[state]);

        const char *val = 0;

        switch( state ) {
        case DT_DISABLED:
                val = dbltap_disable_val;
                break;
        case DT_ENABLED:
                val = dbltap_enable_val;
                break;
        case DT_DISABLED_NOSLEEP:
                val = dbltap_disable_val;
                allow_sleep_mode = false;
                break;
        default:
                break;
        }

        if( val ) {
                dbltap_allow_sleep_mode(allow_sleep_mode);
                mce_write_string_to_file(dbltap_ctrl_path, val);
        }
EXIT:
        return;
}

/** Re-evaluate whether doubletap wakeups should be enabled or not
 */
static void dbltap_rethink(void)
{
        dt_state_t state = DT_DISABLED;

        switch( dbltap_mode ) {
        default:
        case DBLTAP_ENABLE_ALWAYS:
                state = DT_ENABLED;
                break;

        case DBLTAP_ENABLE_NEVER:
                break;

        case DBLTAP_ENABLE_NO_PROXIMITY:
                switch( dbltap_ps_state ) {
                case COVER_CLOSED:
                  if( dbltap_ps_blank )
                                state = DT_DISABLED_NOSLEEP;
                        break;

                default:
                case COVER_OPEN:
                case COVER_UNDEF:
                        state = DT_ENABLED;
                        break;
                }
                break;
        }
        dbltap_set_state(state);
}

/** Set doubletap wakeup policy
 *
 * @param mode DBLTAP_ENABLE_ALWAYS|NEVER|NO_PROXIMITY
 */
static void dbltap_mode_set(dbltap_mode_t mode)
{
        if( dbltap_mode != mode ) {
                dbltap_mode = mode;
                dbltap_rethink();
        }
}

/** Proximity state changed callback
 *
 * @param data proximity state as void pointer
 */
static void dbltap_proximity_trigger(gconstpointer data)
{
        cover_state_t state = GPOINTER_TO_INT(data);

        if( dbltap_ps_state != state ) {
                dbltap_ps_state = state;
                dbltap_rethink();
        }
}

/** Proximity blank changed callback
 *
 * @param data proximity blank as void pointer
 */
static void dbltap_proximity_blank_trigger(gconstpointer data)
{
        cover_state_t state = GPOINTER_TO_INT(data);

        if( dbltap_ps_blank != state ) {
                dbltap_ps_blank = state;
                dbltap_rethink();
        }
}

/** GConf callback for doubletap mode setting
 *
 * @param gcc   (not used)
 * @param id    Connection ID from gconf_client_notify_add()
 * @param entry The modified GConf entry
 * @param data  (not used)
 */
static void dbltap_mode_gconf_cb(GConfClient *const gcc, const guint id,
                                 GConfEntry *const entry, gpointer const data)
{
        (void)gcc; (void)data;

        const GConfValue *gcv;

        if( id != dbltap_mode_gconf_id )
                goto EXIT;

        gint mode = DBLTAP_ENABLE_DEFAULT;

        if( (gcv = gconf_entry_get_value(entry)) )
                mode = gconf_value_get_int(gcv);

        dbltap_mode_set(mode);
EXIT:
        return;
}

/** Check if touch panel sleep mode controls are available
 */
static void dbltap_probe_sleep_mode_controls(void)
{
        static const char def_ctrl[] =
                "/sys/class/i2c-adapter/i2c-3/3-0020/block_sleep_mode";
        static const char def_allow[] = "0";
        static const char def_deny[]  = "1";

        bool success = false;

        sleep_mode_ctrl_path =
                mce_conf_get_string(MCE_CONF_TPSLEEP_GROUP,
                                    MCE_CONF_TPSLEEP_CONTROL_PATH,
                                    def_ctrl);

        if( !sleep_mode_ctrl_path || access(sleep_mode_ctrl_path, F_OK) == -1 )
                goto EXIT;

        sleep_mode_allow_val =
                mce_conf_get_string(MCE_CONF_TPSLEEP_GROUP,
                                    MCE_CONF_TPSLEEP_ALLOW_VALUE,
                                    def_allow);
        sleep_mode_deny_val =
                mce_conf_get_string(MCE_CONF_TPSLEEP_GROUP,
                                    MCE_CONF_TPSLEEP_DENY_VALUE,
                                    def_deny);

        if( !sleep_mode_allow_val || !sleep_mode_deny_val )
                goto EXIT;

        /* Start from kernel boot time default */
        dbltap_allow_sleep_mode(true);

        success = true;

EXIT:

        /* All or nothing */
        if( !success ) {
                g_free(sleep_mode_ctrl_path), sleep_mode_ctrl_path = 0;
                g_free(sleep_mode_allow_val), sleep_mode_allow_val = 0;
                g_free(sleep_mode_deny_val),  sleep_mode_deny_val  = 0;;
        }
        return;
}

/** Init function for the doubletap module
 *
 * @param module (not used)
 *
 * @return NULL on success, a string with an error message on failure
 */
G_MODULE_EXPORT const gchar *g_module_check_init(GModule *module);
const gchar *g_module_check_init(GModule *module)
{
        (void)module;

        /* Config from ini-files */
        dbltap_ctrl_path = mce_conf_get_string(MCE_CONF_DOUBLETAP_GROUP,
                                               MCE_CONF_DOUBLETAP_CONTROL_PATH,
                                               NULL);

        dbltap_enable_val = mce_conf_get_string(MCE_CONF_DOUBLETAP_GROUP,
                                                MCE_CONF_DOUBLETAP_ENABLE_VALUE,
                                                "1");

        dbltap_disable_val = mce_conf_get_string(MCE_CONF_DOUBLETAP_GROUP,
                                                 MCE_CONF_DOUBLETAP_DISABLE_VALUE,
                                                 "0");

        if( !dbltap_ctrl_path || !dbltap_enable_val || !dbltap_disable_val ) {
                mce_log(LL_NOTICE, "no double tap wakeup controls defined");
                goto EXIT;
        }

        dbltap_probe_sleep_mode_controls();

        /* Runtime configuration settings */
        mce_gconf_notifier_add(MCE_GCONF_DOUBLETAP_PATH,
                               MCE_GCONF_DOUBLETAP_MODE,
                               dbltap_mode_gconf_cb,
                               &dbltap_mode_gconf_id);
        gint mode = DBLTAP_ENABLE_DEFAULT;
        mce_gconf_get_int(MCE_GCONF_DOUBLETAP_MODE, &mode);
        dbltap_mode = mode;

        /* Get initial state of datapipes */
        dbltap_ps_state = datapipe_get_gint(proximity_sensor_pipe);

        /* Append triggers/filters to datapipes */
        append_output_trigger_to_datapipe(&proximity_sensor_pipe,
                                          dbltap_proximity_trigger);

        dbltap_ps_blank = datapipe_get_gint(proximity_blank_pipe);
        append_output_trigger_to_datapipe(&proximity_blank_pipe,
                                          dbltap_proximity_blank_trigger);

        /* enable/disable double tap wakeups based on initial conditions */
        dbltap_rethink();
EXIT:
        return NULL;
}

/** Exit function for the doubletap module
 *
 * @param module (not used)
 */
G_MODULE_EXPORT void g_module_unload(GModule *module);
void g_module_unload(GModule *module)
{
        (void)module;

        /* Remove triggers/filters from datapipes */
        remove_output_trigger_from_datapipe(&proximity_sensor_pipe,
                                            dbltap_proximity_trigger);
        remove_output_trigger_from_datapipe(&proximity_blank_pipe,
                                            dbltap_proximity_blank_trigger);

        /* Free config strings */
        g_free(dbltap_ctrl_path);
        g_free(dbltap_enable_val);
        g_free(dbltap_disable_val);

        g_free(sleep_mode_ctrl_path);
        g_free(sleep_mode_allow_val);
        g_free(sleep_mode_deny_val);

        return;
}
