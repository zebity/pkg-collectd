/**
 * collectd - src/network.h
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

#ifndef NETWORK_H
#define NETWORK_H

/*
 * From RFC2365: Administratively Scoped IP Multicast
 *
 * The IPv4 Organization Local Scope -- 239.192.0.0/14
 *
 * 239.192.0.0/14 is defined to be the IPv4 Organization Local Scope, and is
 * the space from which an organization should allocate sub-ranges when
 * defining scopes for private use.
 *
 * Port 25826 is not assigned as of 2005-09-12
 */

/*
 * From RFC2373: IP Version 6 Addressing Architecture
 *
 * 2.7 Multicast Addresses
 *
 *  |   8    |  4 |  4 |          80 bits          |     32 bits     |
 *  +--------+----+----+---------------------------+-----------------+
 *  |11111111|flgs|scop|   reserved must be zero   |    group ID     |
 *  +--------+----+----+---------------------------+-----------------+
 *
 * flgs = 1 => non-permanently-assigned ("transient") multicast address.
 * scop = 8 => organization-local scope
 *
 * group = efc0:4a42 = 239.192.74.66
 */

#define NET_DEFAULT_V4_ADDR "239.192.74.66"
#define NET_DEFAULT_V6_ADDR "ff18::efc0:4a42"
#define NET_DEFAULT_PORT    "25826"

int network_create_socket (const char *node, const char *service);
int network_receive (char **host, char **type, char **instance, char **value);
int network_send (char *type, char *instance, char *value);

#endif /* NETWORK_H */
