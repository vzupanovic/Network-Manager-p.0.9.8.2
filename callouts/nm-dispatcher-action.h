/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager -- Network link manager
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2008 - 2012 Red Hat, Inc.
 */

#include <dbus/dbus-glib.h>

/* dbus-glib types for dispatcher call return value */
#define DISPATCHER_TYPE_RESULT       (dbus_g_type_get_struct ("GValueArray", G_TYPE_STRING, G_TYPE_UINT, G_TYPE_STRING, G_TYPE_INVALID))
#define DISPATCHER_TYPE_RESULT_ARRAY (dbus_g_type_get_collection ("GPtrArray", DISPATCHER_TYPE_RESULT))

#define NM_DISPATCHER_DBUS_SERVICE "org.freedesktop.nm_dispatcher"
#define NM_DISPATCHER_DBUS_IFACE   "org.freedesktop.nm_dispatcher"
#define NM_DISPATCHER_DBUS_PATH    "/org/freedesktop/nm_dispatcher"

#define NMD_CONNECTION_PROPS_PATH         "path"

#define NMD_DEVICE_PROPS_INTERFACE        "interface"
#define NMD_DEVICE_PROPS_IP_INTERFACE     "ip-interface"
#define NMD_DEVICE_PROPS_TYPE             "type"
#define NMD_DEVICE_PROPS_STATE            "state"
#define NMD_DEVICE_PROPS_PATH             "path"

typedef enum {
	DISPATCH_RESULT_UNKNOWN = 0,
	DISPATCH_RESULT_SUCCESS = 1,
	DISPATCH_RESULT_EXEC_FAILED = 2,
	DISPATCH_RESULT_FAILED = 3,
	DISPATCH_RESULT_TIMEOUT = 4,
} DispatchResult;

