/**
 * collectd - src/common.h
 * Copyright (C) 2005-2008  Florian octo Forster
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
 *   Niki W. Waibel <niki.waibel@gmx.net>
**/

#ifndef COMMON_H
#define COMMON_H

#include "collectd.h"
#include "plugin.h"

#if HAVE_PWD_H
# include <pwd.h>
#endif

#define sfree(ptr) \
	if((ptr) != NULL) { \
		free(ptr); \
	} \
	(ptr) = NULL

#define STATIC_ARRAY_SIZE(a) (sizeof (a) / sizeof (*(a)))

char *sstrncpy (char *dest, const char *src, size_t n);
char *sstrdup(const char *s);
void *smalloc(size_t size);
char *sstrerror (int errnum, char *buf, size_t buflen);

/*
 * NAME
 *   sread
 *
 * DESCRIPTION
 *   Reads exactly `n' bytes or fails. Syntax and other behavior is analogous
 *   to `read(2)'. If EOF is received the file descriptor is closed and an
 *   error is returned.
 *
 * PARAMETERS
 *   `fd'          File descriptor to write to.
 *   `buf'         Buffer that is to be written.
 *   `count'       Number of bytes in the buffer.
 *
 * RETURN VALUE
 *   Zero upon success or non-zero if an error occurred. `errno' is set in this
 *   case.
 */
ssize_t sread (int fd, void *buf, size_t count);

/*
 * NAME
 *   swrite
 *
 * DESCRIPTION
 *   Writes exactly `n' bytes or fails. Syntax and other behavior is analogous
 *   to `write(2)'.
 *
 * PARAMETERS
 *   `fd'          File descriptor to write to.
 *   `buf'         Buffer that is to be written.
 *   `count'       Number of bytes in the buffer.
 *
 * RETURN VALUE
 *   Zero upon success or non-zero if an error occurred. `errno' is set in this
 *   case.
 */
ssize_t swrite (int fd, const void *buf, size_t count);

/*
 * NAME
 *   strsplit
 *
 * DESCRIPTION
 *   Splits a string into parts and stores pointers to the parts in `fields'.
 *   The characters split at are ` ' (space) and "\t" (tab).
 *
 * PARAMETERS
 *   `string'      String to split. This string will be modified. `fields' will
 *                 contain pointers to parts of this string, so free'ing it
 *                 will destroy `fields' as well.
 *   `fields'      Array of strings where pointers to the parts will be stored.
 *   `size'        Number of elements in the array. No more than `size'
 *                 pointers will be stored in `fields'.
 *
 * RETURN VALUE
 *    Returns the number of parts stored in `fields'.
 */
int strsplit (char *string, char **fields, size_t size);

/*
 * NAME
 *   strjoin
 *
 * DESCRIPTION
 *   Joins together several parts of a string using `sep' as a separator. This
 *   is equivalent to the Perl built-in `join'.
 *
 * PARAMETERS
 *   `dst'         Buffer where the result is stored.
 *   `dst_len'     Length of the destination buffer. No more than this many
 *                 bytes will be written to the memory pointed to by `dst',
 *                 including the trailing null-byte.
 *   `fields'      Array of strings to be joined.
 *   `fields_num'  Number of elements in the `fields' array.
 *   `sep'         String to be inserted between any two elements of `fields'.
 *                 This string is neither prepended nor appended to the result.
 *                 Instead of passing "" (empty string) one can pass NULL.
 *
 * RETURN VALUE
 *   Returns the number of characters in `dst', NOT including the trailing
 *   null-byte. If an error occurred (empty array or `dst' too small) a value
 *   smaller than zero will be returned.
 */
int strjoin (char *dst, size_t dst_len, char **fields, size_t fields_num, const char *sep);

/*
 * NAME
 *   escape_slashes
 *
 * DESCRIPTION
 *   Removes slashes from the string `buf' and substitutes them with something
 *   appropriate. This function should be used whenever a path is to be used as
 *   (part of) an instance.
 *
 * PARAMETERS
 *   `buf'         String to be escaped.
 *   `buf_len'     Length of the buffer. No more then this many bytes will be
 *   written to `buf', including the trailing null-byte.
 *
 * RETURN VALUE
 *   Returns zero upon success and a value smaller than zero upon failure.
 */
int escape_slashes (char *buf, int buf_len);

int strsubstitute (char *str, char c_from, char c_to);

/* FIXME: `timeval_sub_timespec' needs a description */
int timeval_sub_timespec (struct timeval *tv0, struct timeval *tv1, struct timespec *ret);

int check_create_dir (const char *file_orig);

#ifdef HAVE_LIBKSTAT
int get_kstat (kstat_t **ksp_ptr, char *module, int instance, char *name);
long long get_kstat_value (kstat_t *ksp, char *name);
#endif

unsigned long long ntohll (unsigned long long n);
unsigned long long htonll (unsigned long long n);

#if FP_LAYOUT_NEED_NOTHING
# define ntohd(d) (d)
# define htond(d) (d)
#elif FP_LAYOUT_NEED_ENDIANFLIP || FP_LAYOUT_NEED_INTSWAP
double ntohd (double d);
double htond (double d);
#else
# error "Don't know how to convert between host and network representation of doubles."
#endif

int format_name (char *ret, int ret_len,
		const char *hostname,
		const char *plugin, const char *plugin_instance,
		const char *type, const char *type_instance);
#define FORMAT_VL(ret, ret_len, vl, ds) \
	format_name (ret, ret_len, (vl)->host, (vl)->plugin, (vl)->plugin_instance, \
			(ds)->type, (vl)->type_instance)

int parse_identifier (char *str, char **ret_host,
		char **ret_plugin, char **ret_plugin_instance,
		char **ret_type, char **ret_type_instance);
int parse_values (char *buffer, value_list_t *vl, const data_set_t *ds);

#if !HAVE_GETPWNAM_R
int getpwnam_r (const char *name, struct passwd *pwbuf, char *buf,
		size_t buflen, struct passwd **pwbufp);
#endif

int notification_init (notification_t *n, int severity, const char *message,
		const char *host,
		const char *plugin, const char *plugin_instance,
		const char *type, const char *type_instance);
#define NOTIFICATION_INIT_VL(n, vl, ds) \
	notification_init (n, NOTIF_FAILURE, NULL, \
			(vl)->host, (vl)->plugin, (vl)->plugin_instance, \
			(ds)->type, (vl)->type_instance)
#endif /* COMMON_H */
