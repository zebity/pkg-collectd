/**
 * collectd - src/sensors.c
 * Copyright (C) 2005,2006  Florian octo Forster
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
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
#include "common.h"
#include "plugin.h"

#define MODULE_NAME "sensors"

#if defined(HAVE_SENSORS_SENSORS_H)
# include <sensors/sensors.h>
#else
# undef HAVE_LIBSENSORS
#endif

#if defined(HAVE_LIBSENSORS)
# define SENSORS_HAVE_READ 1
#else
# define SENSORS_HAVE_READ 0
#endif

#define BUFSIZE 512

static char *filename_format = "sensors-%s.rrd";

static char *ds_def[] =
{
	"DS:value:GAUGE:"COLLECTD_HEARTBEAT":U:U",
	NULL
};
static int ds_num = 1;

#ifdef HAVE_LIBSENSORS
typedef struct featurelist
{
	const sensors_chip_name    *chip;
	const sensors_feature_data *data;
	struct featurelist         *next;
} featurelist_t;

featurelist_t *first_feature = NULL;
#endif /* defined (HAVE_LIBSENSORS) */

static void collectd_sensors_init (void)
{
#ifdef HAVE_LIBSENSORS
	FILE *fh;
	featurelist_t *last_feature = NULL;
	featurelist_t *new_feature;
	
	const sensors_chip_name *chip;
	int chip_num;

	const sensors_feature_data *data;
	int data_num0, data_num1;
	
	new_feature = first_feature;
	while (new_feature != NULL)
	{
		last_feature = new_feature->next;
		free (new_feature);
		new_feature = last_feature;
	}

#ifdef assert
	assert (new_feature == NULL);
	assert (last_feature == NULL);
#endif

	if ((fh = fopen ("/etc/sensors.conf", "r")) == NULL)
		return;

	if (sensors_init (fh))
	{
		fclose (fh);
		syslog (LOG_ERR, "sensors: Cannot initialize sensors. Data will not be collected.");
		return;
	}

	fclose (fh);

	chip_num = 0;
	while ((chip = sensors_get_detected_chips (&chip_num)) != NULL)
	{
		data = NULL;
		data_num0 = data_num1 = 0;

		while ((data = sensors_get_all_features (*chip, &data_num0, &data_num1)) != NULL)
		{
			/* "master features" only */
			if (data->mapping != SENSORS_NO_MAPPING)
				continue;

			/* Only temperature for now.. */
			if (strncmp (data->name, "temp", 4)
					&& strncmp (data->name, "fan", 3))
				continue;

			if ((new_feature = (featurelist_t *) malloc (sizeof (featurelist_t))) == NULL)
			{
				perror ("malloc");
				continue;
			}

			/*
			syslog (LOG_INFO, "sensors: Adding feature: %s/%s", chip->prefix, data->name);
			*/

			new_feature->chip = chip;
			new_feature->data = data;
			new_feature->next = NULL;

			if (first_feature == NULL)
			{
				first_feature = new_feature;
				last_feature  = new_feature;
			}
			else
			{
				last_feature->next = new_feature;
				last_feature = new_feature;
			}
		}
	}

	if (first_feature == NULL)
		sensors_cleanup ();
#endif /* defined(HAVE_LIBSENSORS) */

	return;
}

static void sensors_write (char *host, char *inst, char *val)
{
	char file[BUFSIZE];
	int status;

	status = snprintf (file, BUFSIZE, filename_format, inst);
	if (status < 1)
		return;
	else if (status >= BUFSIZE)
		return;

	rrd_update_file (host, file, val, ds_def, ds_num);
}

#if SENSORS_HAVE_READ
static void sensors_submit (const char *feat_name, const char *chip_prefix, double value)
{
	char buf[BUFSIZE];
	char inst[BUFSIZE];

	if (snprintf (buf, BUFSIZE, "%u:%.3f", (unsigned int) curtime, value) >= BUFSIZE)
		return;

	if (snprintf (inst, BUFSIZE, "%s-%s", chip_prefix, feat_name) >= BUFSIZE)
		return;

	plugin_submit (MODULE_NAME, inst, buf);
}

static void sensors_read (void)
{
	featurelist_t *feature;
	double value;

	for (feature = first_feature; feature != NULL; feature = feature->next)
	{
		if (sensors_get_feature (*feature->chip, feature->data->number, &value) < 0)
			continue;

		sensors_submit (feature->data->name, feature->chip->prefix, value);
	}
}
#else
# define sensors_read NULL
#endif /* SENSORS_HAVE_READ */

void module_register (void)
{
	plugin_register (MODULE_NAME, collectd_sensors_init, sensors_read, sensors_write);
}

#undef BUFSIZE
#undef MODULE_NAME
