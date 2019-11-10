/**
 * collectd - src/swap.c
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

#define MODULE_NAME "swap"

#if defined(KERNEL_LINUX) || defined(KERNEL_SOLARIS) || defined(HAVE_LIBSTATGRAB)
# define SWAP_HAVE_READ 1
#else
# define SWAP_HAVE_READ 0
#endif

#if HAVE_SYS_SWAP_H
# include <sys/swap.h>
#endif

#undef  MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))

static char *swap_file = "swap.rrd";

/* 1099511627776 == 1TB ought to be enough for anyone ;) */
static char *ds_def[] =
{
	"DS:used:GAUGE:"COLLECTD_HEARTBEAT":0:1099511627776",
	"DS:free:GAUGE:"COLLECTD_HEARTBEAT":0:1099511627776",
	"DS:cached:GAUGE:"COLLECTD_HEARTBEAT":0:1099511627776",
	"DS:resv:GAUGE:"COLLECTD_HEARTBEAT":0:1099511627776",
	NULL
};
static int ds_num = 4;

#ifdef KERNEL_SOLARIS
static unsigned long long pagesize;
static kstat_t *ksp;
#endif /* KERNEL_SOLARIS */

static void swap_init (void)
{
#ifdef KERNEL_SOLARIS
	/* getpagesize(3C) tells me this does not fail.. */
	pagesize = (unsigned long long) getpagesize ();
	if (get_kstat (&ksp, "unix", 0, "system_pages"))
		ksp = NULL;
#endif /* KERNEL_SOLARIS */

	return;
}

static void swap_write (char *host, char *inst, char *val)
{
	rrd_update_file (host, swap_file, val, ds_def, ds_num);
}

#if SWAP_HAVE_READ
static void swap_submit (unsigned long long swap_used,
		unsigned long long swap_free,
		unsigned long long swap_cached,
		unsigned long long swap_resv)
{
	char buffer[512];

	if (snprintf (buffer, 512, "%u:%llu:%llu:%llu:%llu", (unsigned int) curtime,
				swap_used, swap_free, swap_cached, swap_resv) >= 512)
		return;

	plugin_submit (MODULE_NAME, "-", buffer);
}

static void swap_read (void)
{
#ifdef KERNEL_LINUX
	FILE *fh;
	char buffer[1024];
	
	char *fields[8];
	int numfields;

	unsigned long long swap_used   = 0LL;
	unsigned long long swap_cached = 0LL;
	unsigned long long swap_free   = 0LL;
	unsigned long long swap_total  = 0LL;

	if ((fh = fopen ("/proc/meminfo", "r")) == NULL)
	{
		syslog (LOG_WARNING, "memory: fopen: %s", strerror (errno));
		return;
	}

	while (fgets (buffer, 1024, fh) != NULL)
	{
		unsigned long long *val = NULL;

		if (strncasecmp (buffer, "SwapTotal:", 10) == 0)
			val = &swap_total;
		else if (strncasecmp (buffer, "SwapFree:", 9) == 0)
			val = &swap_free;
		else if (strncasecmp (buffer, "SwapCached:", 11) == 0)
			val = &swap_cached;
		else
			continue;

		numfields = strsplit (buffer, fields, 8);

		if (numfields < 2)
			continue;

		*val = atoll (fields[1]) * 1024LL;
	}

	if (fclose (fh))
		syslog (LOG_WARNING, "memory: fclose: %s", strerror (errno));

	if ((swap_total == 0LL) || ((swap_free + swap_cached) > swap_total))
		return;

	swap_used = swap_total - (swap_free + swap_cached);

	swap_submit (swap_used, swap_free, swap_cached, -1LL);
/* #endif defined(KERNEL_LINUX) */

#elif defined(KERNEL_SOLARIS)
	unsigned long long swap_alloc;
	unsigned long long swap_resv;
	unsigned long long swap_avail;

	struct anoninfo ai;

	if (swapctl (SC_AINFO, &ai) == -1)
	{
		syslog (LOG_ERR, "swap plugin: swapctl failed: %s",
				strerror (errno));
		return;
	}

	/*
	 * Calculations from:
	 * http://cvs.opensolaris.org/source/xref/on/usr/src/cmd/swap/swap.c
	 * Also see:
	 * http://www.itworld.com/Comp/2377/UIR980701perf/ (outdated?)
	 * /usr/include/vm/anon.h
	 *
	 * In short, swap -s shows: allocated + reserved = used, available
	 *
	 * However, Solaris does not allow to allocated/reserved more than the
	 * available swap (physical memory + disk swap), so the pedant may
	 * prefer: allocated + unallocated = reserved, available
	 * 
	 * We map the above to: used + resv = n/a, free
	 *
	 * Does your brain hurt yet?  - Christophe Kalt
	 *
	 * Oh, and in case you wonder,
	 * swap_alloc = pagesize * ( ai.ani_max - ai.ani_free );
	 * can suffer from a 32bit overflow.
	 */
	swap_alloc  = ai.ani_max - ai.ani_free;
	swap_alloc *= pagesize;
	swap_resv   = ai.ani_resv + ai.ani_free - ai.ani_max;
	swap_resv  *= pagesize;
	swap_avail  = ai.ani_max - ai.ani_resv;
	swap_avail *= pagesize;

	swap_submit (swap_alloc, swap_avail, -1LL, swap_resv);
/* #endif defined(KERNEL_SOLARIS) */

#elif defined(HAVE_LIBSTATGRAB)
	sg_swap_stats *swap;

	if ((swap = sg_get_swap_stats ()) != NULL)
		swap_submit (swap->used, swap->free, -1LL, -1LL);
#endif /* HAVE_LIBSTATGRAB */
}
#else
# define swap_read NULL
#endif /* SWAP_HAVE_READ */

void module_register (void)
{
	plugin_register (MODULE_NAME, swap_init, swap_read, swap_write);
}

#undef MODULE_NAME
