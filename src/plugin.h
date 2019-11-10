#ifndef PLUGIN_H
#define PLUGIN_H
/**
 * collectd - src/plugin.h
 * Copyright (C) 2005-2007  Florian octo Forster
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; only version 2 of the License is applicable.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 * Authors:
 *   Florian octo Forster <octo at verplant.org>
 **/

#include "collectd.h"
#include "configfile.h"

#define DATA_MAX_NAME_LEN 64

#define DS_TYPE_COUNTER 0
#define DS_TYPE_GAUGE   1

#ifndef LOG_ERR
# define LOG_ERR 3
#endif
#ifndef LOG_WARNING
# define LOG_WARNING 4
#endif
#ifndef LOG_NOTICE
# define LOG_NOTICE 5
#endif
#ifndef LOG_INFO
# define LOG_INFO 6
#endif
#ifndef LOG_DEBUG
# define LOG_DEBUG 7
#endif

/*
 * Public data types
 */
typedef unsigned long long counter_t;
typedef double gauge_t;

union value_u
{
	counter_t counter;
	gauge_t   gauge;
};
typedef union value_u value_t;

struct value_list_s
{
	value_t *values;
	int      values_len;
	time_t   time;
	int      interval;
	char     host[DATA_MAX_NAME_LEN];
	char     plugin[DATA_MAX_NAME_LEN];
	char     plugin_instance[DATA_MAX_NAME_LEN];
	char     type_instance[DATA_MAX_NAME_LEN];
};
typedef struct value_list_s value_list_t;

#define VALUE_LIST_INIT { NULL, 0, 0, interval_g, "localhost", "", "", "" }
#define VALUE_LIST_STATIC { NULL, 0, 0, 0, "localhost", "", "", "" }

struct data_source_s
{
	char   name[DATA_MAX_NAME_LEN];
	int    type;
	double min;
	double max;
};
typedef struct data_source_s data_source_t;

struct data_set_s
{
	char           type[DATA_MAX_NAME_LEN];
	int            ds_num;
	data_source_t *ds;
};
typedef struct data_set_s data_set_t;

typedef struct complain_s
{
	unsigned int interval; /* how long we wait for reporting this error again */
	unsigned int delay;    /* how many more iterations we still need to wait */
} complain_t;

/*
 * NAME
 *  plugin_set_dir
 *
 * DESCRIPTION
 *  Sets the current `plugindir'
 *
 * ARGUMENTS
 *  `dir'       Path to the plugin directory
 *
 * NOTES
 *  If `dir' is NULL the compiled in default `PLUGINDIR' is used.
 */
void plugin_set_dir (const char *dir);

/*
 * NAME
 *  plugin_load
 *
 * DESCRIPTION
 *  Searches the current `plugindir' (see `plugin_set_dir') for the plugin
 *  named $type and loads it. Afterwards the plugin's `module_register'
 *  function is called, which then calls `plugin_register' to register callback
 *  functions.
 *
 * ARGUMENTS
 *  `name'      Name of the plugin to load.
 *  `mr'        Types of functions to request from the plugin.
 *
 * RETURN VALUE
 *  Returns zero upon success, a value greater than zero if no plugin was found
 *  and a value below zero if an error occurs.
 *
 * NOTES
 *  No attempt is made to re-load an already loaded module.
 */
int plugin_load (const char *name);

void plugin_init_all (void);
void plugin_read_all (const int *loop);
void plugin_shutdown_all (void);

/*
 * The `plugin_register_*' functions are used to make `config', `init',
 * `read', `write' and `shutdown' functions known to the plugin
 * infrastructure. Also, the data-formats are made public like this.
 */
int plugin_register_config (const char *name,
		int (*callback) (const char *key, const char *val),
		const char **keys, int keys_num);
int plugin_register_complex_config (const char *type,
		int (*callback) (oconfig_item_t *));
int plugin_register_init (const char *name,
		int (*callback) (void));
int plugin_register_read (const char *name,
		int (*callback) (void));
int plugin_register_write (const char *name,
		int (*callback) (const data_set_t *ds, const value_list_t *vl));
int plugin_register_shutdown (char *name,
		int (*callback) (void));
int plugin_register_data_set (const data_set_t *ds);
int plugin_register_log (char *name,
		void (*callback) (int, const char *));

int plugin_unregister_config (const char *name);
int plugin_unregister_complex_config (const char *name);
int plugin_unregister_init (const char *name);
int plugin_unregister_read (const char *name);
int plugin_unregister_write (const char *name);
int plugin_unregister_shutdown (const char *name);
int plugin_unregister_data_set (const char *name);
int plugin_unregister_log (const char *name);


/*
 * NAME
 *  plugin_dispatch_values
 *
 * DESCRIPTION
 *  This function is called by reading processes with the values they've
 *  aquired. The function fetches the data-set definition (that has been
 *  registered using `plugin_register_data_set') and calls _all_ registered
 *  write-functions.
 *
 * ARGUMENTS
 *  `name'      Name/type of the data-set that describe the values in `vl'.
 *  `vl'        Value list of the values that have been read by a `read'
 *              function.
 */
int plugin_dispatch_values (const char *name, value_list_t *vl);

void plugin_log (int level, const char *format, ...);
#define ERROR(...)   plugin_log (LOG_ERR,     __VA_ARGS__)
#define WARNING(...) plugin_log (LOG_WARNING, __VA_ARGS__)
#define NOTICE(...)  plugin_log (LOG_NOTICE,  __VA_ARGS__)
#define INFO(...)    plugin_log (LOG_INFO,    __VA_ARGS__)
#if COLLECT_DEBUG
# define DEBUG(...)  plugin_log (LOG_DEBUG,   __VA_ARGS__)
#else /* COLLECT_DEBUG */
# define DEBUG(...)  /* noop */
#endif /* ! COLLECT_DEBUG */

/* TODO: Move plugin_{complain,relief} into `utils_complain.[ch]'. -octo */
void plugin_complain (int level, complain_t *c, const char *format, ...);
void plugin_relief (int level, complain_t *c, const char *format, ...);

const data_set_t *plugin_get_ds (const char *name);

#endif /* PLUGIN_H */
