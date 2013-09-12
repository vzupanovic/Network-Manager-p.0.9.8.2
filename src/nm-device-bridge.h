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
 * Copyright 2012 Red Hat, Inc.
 */

#ifndef NM_DEVICE_BRIDGE_H
#define NM_DEVICE_BRIDGE_H

#include <glib-object.h>

#include "nm-device-wired.h"

G_BEGIN_DECLS

#define NM_TYPE_DEVICE_BRIDGE            (nm_device_bridge_get_type ())
#define NM_DEVICE_BRIDGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NM_TYPE_DEVICE_BRIDGE, NMDeviceBridge))
#define NM_DEVICE_BRIDGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  NM_TYPE_DEVICE_BRIDGE, NMDeviceBridgeClass))
#define NM_IS_DEVICE_BRIDGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NM_TYPE_DEVICE_BRIDGE))
#define NM_IS_DEVICE_BRIDGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  NM_TYPE_DEVICE_BRIDGE))
#define NM_DEVICE_BRIDGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  NM_TYPE_DEVICE_BRIDGE, NMDeviceBridgeClass))

typedef enum {
	NM_BRIDGE_ERROR_CONNECTION_NOT_BRIDGE = 0, /*< nick=ConnectionNotBridge >*/
	NM_BRIDGE_ERROR_CONNECTION_INVALID,      /*< nick=ConnectionInvalid >*/
	NM_BRIDGE_ERROR_CONNECTION_INCOMPATIBLE, /*< nick=ConnectionIncompatible >*/
} NMBridgeError;

#define NM_DEVICE_BRIDGE_HW_ADDRESS "hw-address"
#define NM_DEVICE_BRIDGE_CARRIER "carrier"
#define NM_DEVICE_BRIDGE_SLAVES "slaves"

typedef struct {
	NMDeviceWired parent;
} NMDeviceBridge;

typedef struct {
	NMDeviceWiredClass parent;

	/* Signals */
	void (*properties_changed) (NMDeviceBridge *device, GHashTable *properties);
} NMDeviceBridgeClass;


GType nm_device_bridge_get_type (void);

NMDevice *nm_device_bridge_new (const char *udi,
                                const char *iface);

G_END_DECLS

#endif	/* NM_DEVICE_BRIDGE_H */
