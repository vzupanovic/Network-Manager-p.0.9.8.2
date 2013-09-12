/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * libnm_glib -- Access network status & information from glib applications
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 * Copyright (C) 2012 Red Hat, Inc.
 */

#include <config.h>
#include <string.h>
#include <netinet/ether.h>

#include "nm-glib-compat.h"

#include <nm-setting-connection.h>
#include <nm-setting-vlan.h>
#include <nm-utils.h>

#include "nm-device-vlan.h"
#include "nm-device-private.h"
#include "nm-object-private.h"

G_DEFINE_TYPE (NMDeviceVlan, nm_device_vlan, NM_TYPE_DEVICE)

#define NM_DEVICE_VLAN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NM_TYPE_DEVICE_VLAN, NMDeviceVlanPrivate))

typedef struct {
	DBusGProxy *proxy;

	char *hw_address;
	gboolean carrier;
	guint vlan_id;
} NMDeviceVlanPrivate;

enum {
	PROP_0,
	PROP_HW_ADDRESS,
	PROP_CARRIER,
	PROP_VLAN_ID,

	LAST_PROP
};

#define DBUS_PROP_HW_ADDRESS "HwAddress"
#define DBUS_PROP_CARRIER "Carrier"
#define DBUS_PROP_VLAN_ID "VlanId"

/**
 * nm_device_vlan_error_quark:
 *
 * Registers an error quark for #NMDeviceVlan if necessary.
 *
 * Returns: the error quark used for #NMDeviceVlan errors.
 **/
GQuark
nm_device_vlan_error_quark (void)
{
	static GQuark quark = 0;

	if (G_UNLIKELY (quark == 0))
		quark = g_quark_from_static_string ("nm-device-vlan-error-quark");
	return quark;
}

/**
 * nm_device_vlan_new:
 * @connection: the #DBusGConnection
 * @path: the DBus object path of the device
 *
 * Creates a new #NMDeviceVlan.
 *
 * Returns: (transfer full): a new device
 **/
GObject *
nm_device_vlan_new (DBusGConnection *connection, const char *path)
{
	GObject *device;

	g_return_val_if_fail (connection != NULL, NULL);
	g_return_val_if_fail (path != NULL, NULL);

	device = g_object_new (NM_TYPE_DEVICE_VLAN,
	                       NM_OBJECT_DBUS_CONNECTION, connection,
	                       NM_OBJECT_DBUS_PATH, path,
	                       NULL);
	_nm_object_ensure_inited (NM_OBJECT (device));
	return device;
}

/**
 * nm_device_vlan_get_hw_address:
 * @device: a #NMDeviceVlan
 *
 * Gets the hardware (MAC) address of the #NMDeviceVlan
 *
 * Returns: the hardware address. This is the internal string used by the
 * device, and must not be modified.
 **/
const char *
nm_device_vlan_get_hw_address (NMDeviceVlan *device)
{
	g_return_val_if_fail (NM_IS_DEVICE_VLAN (device), NULL);

	_nm_object_ensure_inited (NM_OBJECT (device));
	return NM_DEVICE_VLAN_GET_PRIVATE (device)->hw_address;
}

/**
 * nm_device_vlan_get_carrier:
 * @device: a #NMDeviceVlan
 *
 * Whether the device has carrier.
 *
 * Returns: %TRUE if the device has carrier
 **/
gboolean
nm_device_vlan_get_carrier (NMDeviceVlan *device)
{
	g_return_val_if_fail (NM_IS_DEVICE_VLAN (device), FALSE);

	_nm_object_ensure_inited (NM_OBJECT (device));
	return NM_DEVICE_VLAN_GET_PRIVATE (device)->carrier;
}

/**
 * nm_device_vlan_get_vlan_id:
 * @device: a #NMDeviceVlan
 *
 * Returns: the device's VLAN ID
 **/
guint
nm_device_vlan_get_vlan_id (NMDeviceVlan *device)
{
	g_return_val_if_fail (NM_IS_DEVICE_VLAN (device), FALSE);

	_nm_object_ensure_inited (NM_OBJECT (device));
	return NM_DEVICE_VLAN_GET_PRIVATE (device)->vlan_id;
}

static gboolean
connection_compatible (NMDevice *device, NMConnection *connection, GError **error)
{
	NMSettingConnection *s_con;
	NMSettingVlan *s_vlan;
	const char *ctype, *dev_iface_name, *vlan_iface_name;

	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	s_con = nm_connection_get_setting_connection (connection);
	g_assert (s_con);

	ctype = nm_setting_connection_get_connection_type (s_con);
	if (strcmp (ctype, NM_SETTING_VLAN_SETTING_NAME) != 0) {
		g_set_error (error, NM_DEVICE_VLAN_ERROR, NM_DEVICE_VLAN_ERROR_NOT_VLAN_CONNECTION,
		             "The connection was not a VLAN connection.");
		return FALSE;
	}

	s_vlan = nm_connection_get_setting_vlan (connection);
	if (!s_vlan) {
		g_set_error (error, NM_DEVICE_VLAN_ERROR, NM_DEVICE_VLAN_ERROR_INVALID_VLAN_CONNECTION,
		             "The connection was not a valid VLAN connection.");
		return FALSE;
	}

	if (nm_setting_vlan_get_id (s_vlan) != nm_device_vlan_get_vlan_id (NM_DEVICE_VLAN (device))) {
		g_set_error (error, NM_DEVICE_VLAN_ERROR, NM_DEVICE_VLAN_ERROR_ID_MISMATCH,
		             "The VLAN identifiers of the device and the connection didn't match.");
		return FALSE;
	}

	dev_iface_name = nm_device_get_iface (device);
	vlan_iface_name = nm_setting_vlan_get_interface_name (s_vlan);
	if (vlan_iface_name && g_strcmp0 (dev_iface_name, vlan_iface_name) != 0) {
		g_set_error (error, NM_DEVICE_VLAN_ERROR, NM_DEVICE_VLAN_ERROR_INTERFACE_MISMATCH,
		             "The interfaces of the device and the connection didn't match.");
		return FALSE;
	}

	return TRUE;
}

/***********************************************************/

static void
nm_device_vlan_init (NMDeviceVlan *device)
{
	_nm_device_set_device_type (NM_DEVICE (device), NM_DEVICE_TYPE_VLAN);
}

static void
register_properties (NMDeviceVlan *device)
{
	NMDeviceVlanPrivate *priv = NM_DEVICE_VLAN_GET_PRIVATE (device);
	const NMPropertiesInfo property_info[] = {
		{ NM_DEVICE_VLAN_HW_ADDRESS, &priv->hw_address },
		{ NM_DEVICE_VLAN_CARRIER,    &priv->carrier },
		{ NM_DEVICE_VLAN_VLAN_ID,    &priv->vlan_id },
		{ NULL },
	};

	_nm_object_register_properties (NM_OBJECT (device),
	                                priv->proxy,
	                                property_info);
}

static void
constructed (GObject *object)
{
	NMDeviceVlanPrivate *priv;

	G_OBJECT_CLASS (nm_device_vlan_parent_class)->constructed (object);

	priv = NM_DEVICE_VLAN_GET_PRIVATE (object);

	priv->proxy = dbus_g_proxy_new_for_name (nm_object_get_connection (NM_OBJECT (object)),
	                                         NM_DBUS_SERVICE,
	                                         nm_object_get_path (NM_OBJECT (object)),
	                                         NM_DBUS_INTERFACE_DEVICE_VLAN);

	register_properties (NM_DEVICE_VLAN (object));
}

static void
dispose (GObject *object)
{
	NMDeviceVlanPrivate *priv = NM_DEVICE_VLAN_GET_PRIVATE (object);

	g_clear_object (&priv->proxy);

	G_OBJECT_CLASS (nm_device_vlan_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
	NMDeviceVlanPrivate *priv = NM_DEVICE_VLAN_GET_PRIVATE (object);

	g_free (priv->hw_address);

	G_OBJECT_CLASS (nm_device_vlan_parent_class)->finalize (object);
}

static void
get_property (GObject *object,
              guint prop_id,
              GValue *value,
              GParamSpec *pspec)
{
	NMDeviceVlan *device = NM_DEVICE_VLAN (object);

	_nm_object_ensure_inited (NM_OBJECT (object));

	switch (prop_id) {
	case PROP_HW_ADDRESS:
		g_value_set_string (value, nm_device_vlan_get_hw_address (device));
		break;
	case PROP_CARRIER:
		g_value_set_boolean (value, nm_device_vlan_get_carrier (device));
		break;
	case PROP_VLAN_ID:
		g_value_set_uint (value, nm_device_vlan_get_vlan_id (device));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nm_device_vlan_class_init (NMDeviceVlanClass *eth_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (eth_class);
	NMDeviceClass *device_class = NM_DEVICE_CLASS (eth_class);

	g_type_class_add_private (eth_class, sizeof (NMDeviceVlanPrivate));

	/* virtual methods */
	object_class->constructed = constructed;
	object_class->dispose = dispose;
	object_class->finalize = finalize;
	object_class->get_property = get_property;
	device_class->connection_compatible = connection_compatible;

	/* properties */

	/**
	 * NMDeviceVlan:hw-address:
	 *
	 * The hardware (MAC) address of the device.
	 **/
	g_object_class_install_property
		(object_class, PROP_HW_ADDRESS,
		 g_param_spec_string (NM_DEVICE_VLAN_HW_ADDRESS,
		                      "Active MAC Address",
		                      "Currently set hardware MAC address",
		                      NULL,
		                      G_PARAM_READABLE));

	/**
	 * NMDeviceVlan:carrier:
	 *
	 * Whether the device has carrier.
	 **/
	g_object_class_install_property
		(object_class, PROP_CARRIER,
		 g_param_spec_boolean (NM_DEVICE_VLAN_CARRIER,
		                       "Carrier",
		                       "Carrier",
		                       FALSE,
		                       G_PARAM_READABLE));

	/**
	 * NMDeviceVlan:vlan-id:
	 *
	 * The device's VLAN ID.
	 **/
	g_object_class_install_property
		(object_class, PROP_VLAN_ID,
		 g_param_spec_uint (NM_DEVICE_VLAN_VLAN_ID,
		                    "VLAN ID",
		                    "VLAN ID",
		                    0, 4095, 0,
		                    G_PARAM_READABLE));
}
