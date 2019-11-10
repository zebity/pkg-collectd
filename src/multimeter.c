/**
 * collectd - src/multimeter.c
 * Copyright (C) 2005,2006  Peter Holik
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
 *   Peter Holik <peter at holik.at>
 *
 * Used multimeter: Metex M-4650CR
 *
 **/

#include <termios.h>
#include <sys/ioctl.h>
#include <math.h>
#include "collectd.h"
#include "common.h"
#include "plugin.h"

#define MODULE_NAME "multimeter"

static char *multimeter_file = "multimeter.rrd";

static char *ds_def[] =
{
	"DS:value:GAUGE:"COLLECTD_HEARTBEAT":U:U",
	NULL
};
static int ds_num = 1;

static int fd = -1;

static int multimeter_timeval_sub (struct timeval *tv1, struct timeval *tv2,
                struct timeval *res)
{
        if ((tv1->tv_sec < tv2->tv_sec) ||
	    ((tv1->tv_sec == tv2->tv_sec) && (tv1->tv_usec < tv2->tv_usec)))
    	        return (-1);

        res->tv_sec  = tv1->tv_sec  - tv2->tv_sec;
        res->tv_usec = tv1->tv_usec - tv2->tv_usec;

        assert ((res->tv_sec > 0) || ((res->tv_sec == 0) && (res->tv_usec > 0)));

        while (res->tv_usec < 0)
        {
	        res->tv_usec += 1000000;
                res->tv_sec--;
        }
	return (0);
}
#define LINE_LENGTH 14
static int multimeter_read_value(double *value)
{
	int retry = 3; /* sometimes we receive garbadge */

	do
	{
		struct timeval time_end;

		tcflush(fd, TCIFLUSH);

		if (gettimeofday (&time_end, NULL) < 0)
	        {
	                syslog (LOG_ERR, MODULE_NAME": gettimeofday failed: %s",
                                strerror (errno));
	                return (-1);
	        }
	        time_end.tv_sec++;	

		while (1)
		{
		        char buf[LINE_LENGTH];
			char *range;
			int status;
			fd_set rfds;
    			struct timeval timeout;
    			struct timeval time_now;

			write(fd, "D", 1);

			FD_ZERO(&rfds);
			FD_SET(fd, &rfds);

			if (gettimeofday (&time_now, NULL) < 0)
	                {
		                syslog (LOG_ERR, MODULE_NAME": gettimeofday failed: %s",
                                        strerror (errno));
	                        return (-1);
	                }
			if (multimeter_timeval_sub (&time_end, &time_now, &timeout) == -1)
	                        break;

			status = select(fd+1, &rfds, NULL, NULL, &timeout);

			if (status > 0) /* usually we succeed */
			{
				status = read(fd, buf, LINE_LENGTH);

				if ((status < 0) && ((errno == EAGAIN) || (errno == EINTR)))
				        continue;

				/* Format: "DC 00.000mV  \r" */
				if (status > 0 && status == LINE_LENGTH)
				{
					*value = strtod(buf + 2, &range);

					if ( range > (buf + 6) )
					{
			    			range = buf + 9;

						switch ( *range )
						{
			    				case 'p': *value *= 1.0E-12; break;
					    		case 'n': *value *= 1.0E-9; break;
							case 'u': *value *= 1.0E-6; break;
			    				case 'm': *value *= 1.0E-3; break;
							case 'k': *value *= 1.0E3; break;
							case 'M': *value *= 1.0E6; break;
							case 'G': *value *= 1.0E9; break;
						}
					}
					else
						return (-1); /* Overflow */

					return (0); /* value received */
				}
				else break;
			}
			else if (!status) /* Timeout */
            		{
	                        break;
			}
			else if ((status == -1) && ((errno == EAGAIN) || (errno == EINTR)))
			{
			        continue;
			}
			else /* status == -1 */
            		{
		                syslog (LOG_ERR, MODULE_NAME": select failed: %s",
                                        strerror (errno));
	                        break;
			}
		}
	} while (--retry);

	return (-2);  /* no value received */
}

static void multimeter_init (void)
{
	int i;
	char device[] = "/dev/ttyS ";

	for (i = 0; i < 10; i++)
	{
		device[strlen(device)-1] = i + '0'; 

		if ((fd = open(device, O_RDWR | O_NOCTTY)) > 0)
		{
			struct termios tios;
			int rts = TIOCM_RTS;
			double value;

			tios.c_cflag = B1200 | CS7 | CSTOPB | CREAD | CLOCAL;
			tios.c_iflag = IGNBRK | IGNPAR;
	    		tios.c_oflag = 0;
			tios.c_lflag = 0;
			tios.c_cc[VTIME] = 3;
			tios.c_cc[VMIN]  = LINE_LENGTH;

			tcflush(fd, TCIFLUSH);
			tcsetattr(fd, TCSANOW, &tios);
			ioctl(fd, TIOCMBIC, &rts);
			
    			if (multimeter_read_value(&value) < -1)
			{
				close(fd);
				fd = -1;
			}
			else
			{
				syslog (LOG_INFO, MODULE_NAME" found (%s)", device);
				return;
			}
		}
	}
	syslog (LOG_ERR, MODULE_NAME" not found");
}
#undef LINE_LENGTH

static void multimeter_write (char *host, char *inst, char *val)
{
	rrd_update_file (host, multimeter_file, val, ds_def, ds_num);
}
#define BUFSIZE 128
static void multimeter_submit (double *value)
{
	char buf[BUFSIZE];

	if (snprintf (buf, BUFSIZE, "%u:%f", (unsigned int) curtime, *value) >= BUFSIZE)
		return;

	plugin_submit (MODULE_NAME, NULL, buf);
}
#undef BUFSIZE

static void multimeter_read (void)
{
	double value;

	if (fd > -1 && !(multimeter_read_value(&value)))
		multimeter_submit (&value);
}

void module_register (void)
{
	plugin_register (MODULE_NAME, multimeter_init, multimeter_read, multimeter_write);
}

#undef MODULE_NAME
