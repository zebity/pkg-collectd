/**
 * collectd - src/mysql.c
 * Copyright (C) 2006-2009  Florian octo Forster
 * Copyright (C) 2008       Mirko Buffoni
 * Copyright (C) 2009       Doug MacEachern
 * Copyright (C) 2009       Sebastian tokkee Harl
 * Copyright (C) 2009       Rodolphe Quiédeville
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
 *   Mirko Buffoni <briareos at eswat.org>
 *   Doug MacEachern <dougm at hyperic.com>
 *   Sebastian tokkee Harl <sh at tokkee.org>
 *   Rodolphe Quiédeville <rquiedeville at bearstech.com>
 **/

#include "collectd.h"
#include "common.h"
#include "plugin.h"
#include "configfile.h"

#ifdef HAVE_MYSQL_H
#include <mysql.h>
#elif defined(HAVE_MYSQL_MYSQL_H)
#include <mysql/mysql.h>
#endif

/* TODO: Understand `Select_*' and possibly do that stuff as well.. */

struct mysql_database_s /* {{{ */
{
	/* instance == NULL  =>  legacy mode */
	char *instance;
	char *host;
	char *user;
	char *pass;
	char *database;
	char *socket;
	int   port;

	int   master_stats;
	int   slave_stats;

	int   slave_notif;
	int   slave_io_running;
	int   slave_sql_running;

	MYSQL *con;
	int    state;
};
typedef struct mysql_database_s mysql_database_t; /* }}} */

static int mysql_read (user_data_t *ud);

static void mysql_database_free (void *arg) /* {{{ */
{
	mysql_database_t *db;

	DEBUG ("mysql plugin: mysql_database_free (arg = %p);", arg);

	db = (mysql_database_t *) arg;

	if (db == NULL)
		return;

	if (db->con != NULL)
		mysql_close (db->con);

	sfree (db->host);
	sfree (db->user);
	sfree (db->pass);
	sfree (db->socket);
	sfree (db->instance);
	sfree (db->database);
	sfree (db);
} /* }}} void mysql_database_free */

/* Configuration handling functions {{{
 *
 * <Plugin mysql>
 *   <Database "plugin_instance1">
 *     Host "localhost"
 *     Port 22000
 *     ...
 *   </Database>
 * </Plugin>
 */

static int mysql_config_set_string (char **ret_string, /* {{{ */
				    oconfig_item_t *ci)
{
	char *string;

	if ((ci->values_num != 1)
	    || (ci->values[0].type != OCONFIG_TYPE_STRING))
	{
		WARNING ("mysql plugin: The `%s' config option "
			 "needs exactly one string argument.", ci->key);
		return (-1);
	}

	string = strdup (ci->values[0].value.string);
	if (string == NULL)
	{
		ERROR ("mysql plugin: strdup failed.");
		return (-1);
	}

	if (*ret_string != NULL)
		free (*ret_string);
	*ret_string = string;

	return (0);
} /* }}} int mysql_config_set_string */

static int mysql_config_set_int (int *ret_int, /* {{{ */
				 oconfig_item_t *ci)
{
	if ((ci->values_num != 1)
	    || (ci->values[0].type != OCONFIG_TYPE_NUMBER))
	{
		WARNING ("mysql plugin: The `%s' config option "
			 "needs exactly one string argument.", ci->key);
		return (-1);
	}

	*ret_int = ci->values[0].value.number;

	return (0);
} /* }}} int mysql_config_set_int */

static int mysql_config_set_boolean (int *ret_boolean, /* {{{ */
				oconfig_item_t *ci)
{
	int status = 0;

	if (ci->values_num != 1)
		status = -1;

	if (status == 0)
	{
		if (ci->values[0].type == OCONFIG_TYPE_BOOLEAN)
			*ret_boolean = ci->values[0].value.boolean;
		else if (ci->values[0].type == OCONFIG_TYPE_STRING)
		{
			if (IS_TRUE (ci->values[0].value.string))
				*ret_boolean = 1;
			else if (IS_FALSE (ci->values[0].value.string))
				*ret_boolean = 0;
			else
				status = -1;
		}
		else
			status = -1;
	}

	if (status != 0)
	{
		WARNING ("mysql plugin: The `%s' config option "
			"needs exactly one boolean argument.", ci->key);
		return (-1);
	}
	return (0);
} /* }}} mysql_config_set_boolean */

static int mysql_config (oconfig_item_t *ci) /* {{{ */
{
	mysql_database_t *db;
	int plugin_block;
	int status = 0;
	int i;

	if ((ci->values_num != 1)
	    || (ci->values[0].type != OCONFIG_TYPE_STRING))
	{
		WARNING ("mysql plugin: The `Database' block "
			 "needs exactly one string argument.");
		return (-1);
	}

	db = (mysql_database_t *) malloc (sizeof (*db));
	if (db == NULL)
	{
		ERROR ("mysql plugin: malloc failed.");
		return (-1);
	}
	memset (db, 0, sizeof (*db));

	/* initialize all the pointers */
	db->host     = NULL;
	db->user     = NULL;
	db->pass     = NULL;
	db->database = NULL;
	db->socket   = NULL;
	db->con      = NULL;

	/* trigger a notification, if it's not running */
	db->slave_io_running  = 1;
	db->slave_sql_running = 1;

	plugin_block = 1;
	if (strcasecmp ("Plugin", ci->key) == 0)
	{
		db->instance = NULL;
	}
	else if (strcasecmp ("Database", ci->key) == 0)
	{
		plugin_block = 0;
		status = mysql_config_set_string (&db->instance, ci);
		if (status != 0)
		{
			sfree (db);
			return (status);
		}
		assert (db->instance != NULL);
	}
	else
	{
		ERROR ("mysql plugin: mysql_config: "
				"Invalid key: %s", ci->key);
		return (-1);
	}

	/* Fill the `mysql_database_t' structure.. */
	for (i = 0; i < ci->children_num; i++)
	{
		oconfig_item_t *child = ci->children + i;

		if (strcasecmp ("Host", child->key) == 0)
			status = mysql_config_set_string (&db->host, child);
		else if (strcasecmp ("User", child->key) == 0)
			status = mysql_config_set_string (&db->user, child);
		else if (strcasecmp ("Password", child->key) == 0)
			status = mysql_config_set_string (&db->pass, child);
		else if (strcasecmp ("Port", child->key) == 0)
			status = mysql_config_set_int (&db->port, child);
		else if (strcasecmp ("Socket", child->key) == 0)
			status = mysql_config_set_string (&db->socket, child);
		/* Check if we're currently handling the `Plugin' block. If so,
		 * handle `Database' _blocks_, too. */
		else if ((plugin_block != 0)
				&& (strcasecmp ("Database", child->key) == 0)
				&& (child->children != NULL))
		{
			/* If `plugin_block > 1', there has been at least one
			 * `Database' block */
			plugin_block++;
			status = mysql_config (child);
		}
		/* Now handle ordinary `Database' options (without children) */
		else if ((strcasecmp ("Database", child->key) == 0)
				&& (child->children == NULL))
			status = mysql_config_set_string (&db->database, child);
		else if (strcasecmp ("MasterStats", child->key) == 0)
			status = mysql_config_set_boolean (&db->master_stats, child);
		else if (strcasecmp ("SlaveStats", child->key) == 0)
			status = mysql_config_set_boolean (&db->slave_stats, child);
		else if (strcasecmp ("SlaveNotifications", child->key) == 0)
			status = mysql_config_set_boolean (&db->slave_notif, child);
		else
		{
			WARNING ("mysql plugin: Option `%s' not allowed here.", child->key);
			status = -1;
		}

		if (status != 0)
			break;
	}

	/* Check if there were any `Database' blocks. */
	if (plugin_block > 1)
	{
		/* There were connection blocks. Don't use any legacy stuff. */
		if ((db->host != NULL)
			|| (db->user != NULL)
			|| (db->pass != NULL)
			|| (db->database != NULL)
			|| (db->socket != NULL)
			|| (db->port != 0))
		{
			WARNING ("mysql plugin: At least one <Database> "
					"block has been found. The legacy "
					"configuration will be ignored.");
		}
		mysql_database_free (db);
		return (0);
	}
	else if (plugin_block != 0)
	{
		WARNING ("mysql plugin: You're using the legacy "
				"configuration options. Please consider "
				"updating your configuration!");
	}

	/* Check that all necessary options have been given. */
	while (status == 0)
	{
		/* Zero is allowed and automatically handled by
		 * `mysql_real_connect'. */
		if ((db->port < 0) || (db->port > 65535))
		{
			ERROR ("mysql plugin: Database %s: Port number out "
					"of range: %i",
					(db->instance != NULL)
					? db->instance
					: "<legacy>",
					db->port);
			status = -1;
		}
		break;
	} /* while (status == 0) */

	/* If all went well, register this database for reading */
	if (status == 0)
	{
		user_data_t ud;
		char cb_name[DATA_MAX_NAME_LEN];

		DEBUG ("mysql plugin: Registering new read callback: %s",
				(db->database != NULL) ? db->database : "<default>");

		memset (&ud, 0, sizeof (ud));
		ud.data = (void *) db;
		ud.free_func = mysql_database_free;

		if (db->database != NULL)
			ssnprintf (cb_name, sizeof (cb_name), "mysql-%s",
					db->database);
		else
			sstrncpy (cb_name, "mysql", sizeof (cb_name));

		plugin_register_complex_read (cb_name, mysql_read,
					      /* interval = */ NULL, &ud);
	}
	else
	{
		mysql_database_free (db);
		return (-1);
	}

	return (0);
} /* }}} int mysql_config */

/* }}} End of configuration handling functions */

static MYSQL *getconnection (mysql_database_t *db)
{
	if (db->state != 0)
	{
		int err;
		if ((err = mysql_ping (db->con)) != 0)
		{
			WARNING ("mysql_ping failed: %s", mysql_error (db->con));
			db->state = 0;
		}
		else
		{
			db->state = 1;
			return (db->con);
		}
	}

	if ((db->con = mysql_init (db->con)) == NULL)
	{
		ERROR ("mysql_init failed: %s", mysql_error (db->con));
		db->state = 0;
		return (NULL);
	}

	if (mysql_real_connect (db->con, db->host, db->user, db->pass,
				db->database, db->port, db->socket, 0) == NULL)
	{
		ERROR ("mysql plugin: Failed to connect to database %s "
				"at server %s: %s",
				(db->database != NULL) ? db->database : "<none>",
				(db->host != NULL) ? db->host : "localhost",
				mysql_error (db->con));
		db->state = 0;
		return (NULL);
	}
	else
	{
		INFO ("mysql plugin: Sucessfully connected to database %s "
				"at server %s (server version: %s, protocol version: %d)",
				(db->database != NULL) ? db->database : "<none>",
				mysql_get_host_info (db->con),
				mysql_get_server_info (db->con),
				mysql_get_proto_info (db->con));
		db->state = 1;
		return (db->con);
	}
} /* static MYSQL *getconnection (mysql_database_t *db) */

static void set_host (mysql_database_t *db, char *buf, size_t buflen)
{
	/* XXX legacy mode - use hostname_g */
	if (db->instance == NULL)
		sstrncpy (buf, hostname_g, buflen);
	else
	{
		if ((db->host == NULL)
				|| (strcmp ("", db->host) == 0)
				|| (strcmp ("localhost", db->host) == 0))
			sstrncpy (buf, hostname_g, buflen);
		else
			sstrncpy (buf, db->host, buflen);
	}
}

static void set_plugin_instance (mysql_database_t *db,
		char *buf, size_t buflen)
{
	/* XXX legacy mode - no plugin_instance */
	if (db->instance == NULL)
		sstrncpy (buf, "", buflen);
	else
		sstrncpy (buf, db->instance, buflen);
}

static void submit (const char *type, const char *type_instance,
		value_t *values, size_t values_len, mysql_database_t *db)
{
	value_list_t vl = VALUE_LIST_INIT;

	vl.values     = values;
	vl.values_len = values_len;

	set_host (db, vl.host, sizeof (vl.host));

	sstrncpy (vl.plugin, "mysql", sizeof (vl.plugin));
	set_plugin_instance (db, vl.plugin_instance, sizeof (vl.plugin_instance));

	sstrncpy (vl.type, type, sizeof (vl.type));
	if (type_instance != NULL)
		sstrncpy (vl.type_instance, type_instance, sizeof (vl.type_instance));

	plugin_dispatch_values (&vl);
} /* submit */

static void counter_submit (const char *type, const char *type_instance,
		counter_t value, mysql_database_t *db)
{
	value_t values[1];

	values[0].counter = value;
	submit (type, type_instance, values, STATIC_ARRAY_SIZE (values), db);
} /* void counter_submit */

static void gauge_submit (const char *type, const char *type_instance,
		gauge_t value, mysql_database_t *db)
{
	value_t values[1];

	values[0].gauge = value;
	submit (type, type_instance, values, STATIC_ARRAY_SIZE (values), db);
} /* void gauge_submit */

static void qcache_submit (counter_t hits, counter_t inserts,
		counter_t not_cached, counter_t lowmem_prunes,
		gauge_t queries_in_cache, mysql_database_t *db)
{
	value_t values[5];

	values[0].counter = hits;
	values[1].counter = inserts;
	values[2].counter = not_cached;
	values[3].counter = lowmem_prunes;
	values[4].gauge   = queries_in_cache;

	submit ("mysql_qcache", NULL, values, STATIC_ARRAY_SIZE (values), db);
} /* void qcache_submit */

static void threads_submit (gauge_t running, gauge_t connected, gauge_t cached,
		counter_t created, mysql_database_t *db)
{
	value_t values[4];

	values[0].gauge   = running;
	values[1].gauge   = connected;
	values[2].gauge   = cached;
	values[3].counter = created;

	submit ("mysql_threads", NULL, values, STATIC_ARRAY_SIZE (values), db);
} /* void threads_submit */

static void traffic_submit (counter_t rx, counter_t tx, mysql_database_t *db)
{
	value_t values[2];

	values[0].counter = rx;
	values[1].counter = tx;

	submit ("mysql_octets", NULL, values, STATIC_ARRAY_SIZE (values), db);
} /* void traffic_submit */

static MYSQL_RES *exec_query (MYSQL *con, const char *query)
{
	MYSQL_RES *res;

	int query_len = strlen (query);

	if (mysql_real_query (con, query, query_len))
	{
		ERROR ("mysql plugin: Failed to execute query: %s",
				mysql_error (con));
		INFO ("mysql plugin: SQL query was: %s", query);
		return (NULL);
	}

	res = mysql_store_result (con);
	if (res == NULL)
	{
		ERROR ("mysql plugin: Failed to store query result: %s",
				mysql_error (con));
		INFO ("mysql plugin: SQL query was: %s", query);
		return (NULL);
	}

	return (res);
} /* exec_query */

static int mysql_read_master_stats (mysql_database_t *db, MYSQL *con)
{
	MYSQL_RES *res;
	MYSQL_ROW  row;

	char *query;
	int   field_num;
	unsigned long long position;

	query = "SHOW MASTER STATUS";

	res = exec_query (con, query);
	if (res == NULL)
		return (-1);

	row = mysql_fetch_row (res);
	if (row == NULL)
	{
		ERROR ("mysql plugin: Failed to get master statistics: "
				"`%s' did not return any rows.", query);
		return (-1);
	}

	field_num = mysql_num_fields (res);
	if (field_num < 2)
	{
		ERROR ("mysql plugin: Failed to get master statistics: "
				"`%s' returned less than two columns.", query);
		return (-1);
	}

	position = atoll (row[1]);
	counter_submit ("mysql_log_position", "master-bin", position, db);

	row = mysql_fetch_row (res);
	if (row != NULL)
		WARNING ("mysql plugin: `%s' returned more than one row - "
				"ignoring further results.", query);

	mysql_free_result (res);

	return (0);
} /* mysql_read_master_stats */

static int mysql_read_slave_stats (mysql_database_t *db, MYSQL *con)
{
	MYSQL_RES *res;
	MYSQL_ROW  row;

	char *query;
	int   field_num;

	/* WTF? libmysqlclient does not seem to provide any means to
	 * translate a column name to a column index ... :-/ */
	const int READ_MASTER_LOG_POS_IDX   = 6;
	const int SLAVE_IO_RUNNING_IDX      = 10;
	const int SLAVE_SQL_RUNNING_IDX     = 11;
	const int EXEC_MASTER_LOG_POS_IDX   = 21;
	const int SECONDS_BEHIND_MASTER_IDX = 32;

	query = "SHOW SLAVE STATUS";

	res = exec_query (con, query);
	if (res == NULL)
		return (-1);

	row = mysql_fetch_row (res);
	if (row == NULL)
	{
		ERROR ("mysql plugin: Failed to get slave statistics: "
				"`%s' did not return any rows.", query);
		return (-1);
	}

	field_num = mysql_num_fields (res);
	if (field_num < 33)
	{
		ERROR ("mysql plugin: Failed to get slave statistics: "
				"`%s' returned less than 33 columns.", query);
		return (-1);
	}

	if (db->slave_stats)
	{
		unsigned long long counter;
		double gauge;

		counter = atoll (row[READ_MASTER_LOG_POS_IDX]);
		counter_submit ("mysql_log_position", "slave-read", counter, db);

		counter = atoll (row[EXEC_MASTER_LOG_POS_IDX]);
		counter_submit ("mysql_log_position", "slave-exec", counter, db);

		if (row[SECONDS_BEHIND_MASTER_IDX] != NULL)
		{
			gauge = atof (row[SECONDS_BEHIND_MASTER_IDX]);
			gauge_submit ("time_offset", NULL, gauge, db);
		}
	}

	if (db->slave_notif)
	{
		notification_t n = { 0, time (NULL), "", "",
			"mysql", "", "time_offset", "", NULL };

		char *io, *sql;

		io  = row[SLAVE_IO_RUNNING_IDX];
		sql = row[SLAVE_SQL_RUNNING_IDX];

		set_host (db, n.host, sizeof (n.host));
		set_plugin_instance (db,
				n.plugin_instance, sizeof (n.plugin_instance));

		if (((io == NULL) || (strcasecmp (io, "yes") != 0))
				&& (db->slave_io_running))
		{
			n.severity = NOTIF_WARNING;
			ssnprintf (n.message, sizeof (n.message),
					"slave I/O thread not started or not connected to master");
			plugin_dispatch_notification (&n);
			db->slave_io_running = 0;
		}
		else if (((io != NULL) && (strcasecmp (io, "yes") == 0))
				&& (! db->slave_io_running))
		{
			n.severity = NOTIF_OKAY;
			ssnprintf (n.message, sizeof (n.message),
					"slave I/O thread started and connected to master");
			plugin_dispatch_notification (&n);
			db->slave_io_running = 1;
		}

		if (((sql == NULL) || (strcasecmp (sql, "yes") != 0))
				&& (db->slave_sql_running))
		{
			n.severity = NOTIF_WARNING;
			ssnprintf (n.message, sizeof (n.message),
					"slave SQL thread not started");
			plugin_dispatch_notification (&n);
			db->slave_sql_running = 0;
		}
		else if (((sql != NULL) && (strcasecmp (sql, "yes") == 0))
				&& (! db->slave_sql_running))
		{
			n.severity = NOTIF_OKAY;
			ssnprintf (n.message, sizeof (n.message),
					"slave SQL thread started");
			plugin_dispatch_notification (&n);
			db->slave_sql_running = 0;
		}
	}

	row = mysql_fetch_row (res);
	if (row != NULL)
		WARNING ("mysql plugin: `%s' returned more than one row - "
				"ignoring further results.", query);

	mysql_free_result (res);

	return (0);
} /* mysql_read_slave_stats */

static int mysql_read (user_data_t *ud)
{
	mysql_database_t *db;
	MYSQL     *con;
	MYSQL_RES *res;
	MYSQL_ROW  row;
	char      *query;
	int        field_num;

	unsigned long long qcache_hits          = 0ULL;
	unsigned long long qcache_inserts       = 0ULL;
	unsigned long long qcache_not_cached    = 0ULL;
	unsigned long long qcache_lowmem_prunes = 0ULL;
	int qcache_queries_in_cache = -1;

	int threads_running   = -1;
	int threads_connected = -1;
	int threads_cached    = -1;
	unsigned long long threads_created = 0ULL;

	unsigned long long traffic_incoming = 0ULL;
	unsigned long long traffic_outgoing = 0ULL;

	if ((ud == NULL) || (ud->data == NULL))
	{
		ERROR ("mysql plugin: mysql_database_read: Invalid user data.");
		return (-1);
	}

	db = (mysql_database_t *) ud->data;

	/* An error message will have been printed in this case */
	if ((con = getconnection (db)) == NULL)
		return (-1);

	query = "SHOW STATUS";
	if (mysql_get_server_version (con) >= 50002)
		query = "SHOW GLOBAL STATUS";

	res = exec_query (con, query);
	if (res == NULL)
		return (-1);

	field_num = mysql_num_fields (res);
	while ((row = mysql_fetch_row (res)))
	{
		char *key;
		unsigned long long val;

		key = row[0];
		val = atoll (row[1]);

		if (strncmp (key, "Com_", 
			          strlen ("Com_")) == 0)
		{
			if (val == 0ULL)
				continue;

			/* Ignore `prepared statements' */
			if (strncmp (key, "Com_stmt_", strlen ("Com_stmt_")) != 0)
				counter_submit ("mysql_commands", 
						key + strlen ("Com_"), 
						val, db);
		}
		else if (strncmp (key, "Handler_", 
				        strlen ("Handler_")) == 0)
		{
			if (val == 0ULL)
				continue;

			counter_submit ("mysql_handler", 
					key + strlen ("Handler_"), 
					val, db);
		}
		else if (strncmp (key, "Qcache_",
       				        strlen ("Qcache_")) == 0)
		{
			if (strcmp (key, "Qcache_hits") == 0)
				qcache_hits = val;
			else if (strcmp (key, "Qcache_inserts") == 0)
				qcache_inserts = val;
			else if (strcmp (key, "Qcache_not_cached") == 0)
				qcache_not_cached = val;
			else if (strcmp (key, "Qcache_lowmem_prunes") == 0)
				qcache_lowmem_prunes = val;
			else if (strcmp (key, "Qcache_queries_in_cache") == 0)
				qcache_queries_in_cache = (int) val;
		}
		else if (strncmp (key, "Bytes_", 
				        strlen ("Bytes_")) == 0)
		{
			if (strcmp (key, "Bytes_received") == 0)
				traffic_incoming += val;
			else if (strcmp (key, "Bytes_sent") == 0)
				traffic_outgoing += val;
		}
		else if (strncmp (key, "Threads_", 
       				        strlen ("Threads_")) == 0)
		{
			if (strcmp (key, "Threads_running") == 0)
				threads_running = (int) val;
			else if (strcmp (key, "Threads_connected") == 0)
				threads_connected = (int) val;
			else if (strcmp (key, "Threads_cached") == 0)
				threads_cached = (int) val;
			else if (strcmp (key, "Threads_created") == 0)
				threads_created = val;
		}
		else if (strncmp (key, "Table_locks_",
					strlen ("Table_locks_")) == 0)
		{
			counter_submit ("mysql_locks",
					key + strlen ("Table_locks_"),
					val, db);
		}
	}
	mysql_free_result (res); res = NULL;

	if ((qcache_hits != 0ULL)
			|| (qcache_inserts != 0ULL)
			|| (qcache_not_cached != 0ULL)
			|| (qcache_lowmem_prunes != 0ULL))
		qcache_submit (qcache_hits, qcache_inserts, qcache_not_cached,
			       qcache_lowmem_prunes, qcache_queries_in_cache, db);

	if (threads_created != 0ULL)
		threads_submit (threads_running, threads_connected,
				threads_cached, threads_created, db);

	traffic_submit  (traffic_incoming, traffic_outgoing, db);

	if (db->master_stats)
		mysql_read_master_stats (db, con);

	if ((db->slave_stats) || (db->slave_notif))
		mysql_read_slave_stats (db, con);

	return (0);
} /* int mysql_read */

void module_register (void)
{
	plugin_register_complex_config ("mysql", mysql_config);
} /* void module_register */
