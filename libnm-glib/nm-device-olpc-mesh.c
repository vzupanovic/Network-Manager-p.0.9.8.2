/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* vim: set ft=c ts=4 sts=4 sw=4 noexpandtab smartindent: */
/*
 * libnm-glib -- Access network status & information from glib applications
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

#include "nm-glib-compat.h"

#include <nm-setting-connection.h>
#include <nm-setting-olpc-mesh.h>

#include "nm-device-olpc-mesh.h"
#include "nm-device-private.h"
#include "nm-object-private.h"
#include "nm-device-wifi.h"

G_DEFINE_TYPE (NMDeviceOlpcMesh, nm_device_olpc_mesh, NM_TYPE_DEVICE)

#define NM_DEVICE_OLPC_MESH_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NM_TYPE_DEVICE_OLPC_MESH, NMDeviceOlpcMeshPrivate))

typedef struct {
	DBusGProxy *proxy;

	char *hw_address;
	NMDeviceWifi *companion;
	guint32 active_channel;
} NMDeviceOlpcMeshPrivate;

enum {
	PROP_0,
	PROP_HW_ADDRESS,
	PROP_COMPANION,
	PROP_ACTIVE_CHANNEL,

	LAST_PROP
};

#define DBUS_PROP_HW_ADDRESS      "HwAddress"
#define DBUS_PROP_COMPANION       "Companion"
#define DBUS_PROP_ACTIVE_CHANNEL  "ActiveChannel"

/**
 * nm_device_olpc_mesh_error_quark:
 *
 * Registers an error quark for #NMDeviceOlpcMesh if necessary.
 *
 * Returns: the error quark used for #NMDeviceOlpcMesh errors.
 **/
GQuark
nm_device_olpc_mesh_error_quark (void)
{
	static GQuark quark = 0;

	if (G_UNLIKELY (quark == 0))
		quark = g_quark_from_static_string ("nm-device-olpc-mesh-error-quark");
	return quark;
}

/**
 * nm_device_olpc_mesh_new:
 * @connection: the #DBusGConnection
 * @path: the DBus object path of the device
 *
 * Creates a new #NMDeviceOlpcMesh.
 *
 * Returns: (transfer full): a new OlpcMesh device
 **/
GObject *
nm_device_olpc_mesh_new (DBusGConnection *connection, const char *path)
{
	GObject *device;

	g_return_val_if_fail (connection != NULL, NULL);
	g_return_val_if_fail (path != NULL, NULL);

	device = g_object_new (NM_TYPE_DEVICE_OLPC_MESH,
	                     NM_OBJECT_DBUS_CONNECTION, connection,
	                     NM_OBJECT_DBUS_PATH, path,
	                     NULL);

	_nm_object_ensure_inited (NM_OBJECT (device));
	return device;
}

/**
 * nm_device_olpc_mesh_get_hw_address:
 * @device: a #NMDeviceOlpcMesh
 *
 * Gets the hardware (MAC) address of the #NMDeviceOlpcMesh
 *
 * Returns: the hardware address. This is the internal string used by the
 * device, and must not be modified.
 **/
const char *
nm_device_olpc_mesh_get_hw_address (NMDeviceOlpcMesh *device)
{
	g_return_val_if_fail (NM_IS_DEVICE_OLPC_MESH (device), NULL);

	_nm_object_ensure_inited (NM_OBJECT (device));
	return NM_DEVICE_OLPC_MESH_GET_PRIVATE (device)->hw_address;
}

/**
 * nm_device_olpc_mesh_get_companion:
 * @device: a #NMDeviceOlpcMesh
 *
 * Gets the companion device of the #NMDeviceOlpcMesh.
 *
 * Returns: (transfer none): the companion of the device of %NULL
 **/
NMDeviceWifi *
nm_device_olpc_mesh_get_companion (NMDeviceOlpcMesh *device)
{
	g_return_val_if_fail (NM_IS_DEVICE_OLPC_MESH (device), NULL);

	_nm_object_ensure_inited (NM_OBJECT (device));
	return NM_DEVICE_OLPC_MESH_GET_PRIVATE (device)->companion;
}

/**
 * nm_device_olpc_mesh_get_active_channel:
 * @device: a #NMDeviceOlpcMesh
 *
 * Returns the active channel of the #NMDeviceOlpcMesh device.
 *
 * Returns: active channel of the device
 **/
guint32
nm_device_olpc_mesh_get_active_channel (NMDeviceOlpcMesh *device)
{
	g_return_val_if_fail (NM_IS_DEVICE_OLPC_MESH (device), 0);

	_nm_object_ensure_inited (NM_OBJECT (device));
	return NM_DEVICE_OLPC_MESH_GET_PRIVATE (device)->active_channel;
}

static gboolean
connection_compatible (NMDevice *device, NMConnection *connection, GError **error)
{
	NMSettingConnection *s_con;
	NMSettingOlpcMesh *s_olpc_mesh;
	const char *ctype;

	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	s_con = nm_connection_get_setting_connection (connection);
	g_assert (s_con);

	ctype = nm_setting_connection_get_connection_type (s_con);
	if (strcmp (ctype, NM_SETTING_OLPC_MESH_SETTING_NAME) != 0) {
		g_set_error (error, NM_DEVICE_OLPC_MESH_ERROR, NM_DEVICE_OLPC_MESH_ERROR_NOT_OLPC_MESH_CONNECTION,
		             "The connection was not a Olpc Mesh connection.");
		return FALSE;
	}

	s_olpc_mesh = nm_connection_get_setting_olpc_mesh (connection);
	if (!s_olpc_mesh) {
		g_set_error (error, NM_DEVICE_OLPC_MESH_ERROR, NM_DEVICE_OLPC_MESH_ERROR_INVALID_OLPC_MESH_CONNECTION,
		             "The connection was not a valid Olpc Mesh connection.");
		return FALSE;
	}

	return TRUE;
}

/**************************************************************/

static void
nm_device_olpc_mesh_init (NMDeviceOlpcMesh *device)
{
	_nm_device_set_device_type (NM_DEVICE (device), NM_DEVICE_TYPE_OLPC_MESH);
}

static void
register_properties (NMDeviceOlpcMesh *device)
{
	NMDeviceOlpcMeshPrivate *priv = NM_DEVICE_OLPC_MESH_GET_PRIVATE (device);
	const NMPropertiesInfo property_info[] = {
		{ NM_DEVICE_OLPC_MESH_HW_ADDRESS,     &priv->hw_address },
		{ NM_DEVICE_OLPC_MESH_COMPANION,      &priv->companion, NULL, NM_TYPE_DEVICE_WIFI },
		{ NM_DEVICE_OLPC_MESH_ACTIVE_CHANNEL, &priv->active_channel },
		{ NULL },
	};

	_nm_object_register_properties (NM_OBJECT (device),
	                                priv->proxy,
	                                property_info);
}

static void
constructed (GObject *object)
{
	NMDeviceOlpcMeshPrivate *priv;

	G_OBJECT_CLASS (nm_device_olpc_mesh_parent_class)->constructed (object);

	priv = NM_DEVICE_OLPC_MESH_GET_PRIVATE (object);

	priv->proxy = dbus_g_proxy_new_for_name (nm_object_get_connection (NM_OBJECT (object)),
	                                         NM_DBUS_SERVICE,
	                                         nm_object_get_path (NM_OBJECT (object)),
	                                         NM_DBUS_INTERFACE_DEVICE_OLPC_MESH);

	register_properties (NM_DEVICE_OLPC_MESH (object));
}

static void
dispose (GObject *object)
{
	NMDeviceOlpcMeshPrivate *priv = NM_DEVICE_OLPC_MESH_GET_PRIVATE (object);

	g_clear_object (&priv->companion);
	g_clear_object (&priv->proxy);

	G_OBJECT_CLASS (nm_device_olpc_mesh_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
	NMDeviceOlpcMeshPrivate *priv = NM_DEVICE_OLPC_MESH_GET_PRIVATE (object);

	g_free (priv->hw_address);

	G_OBJECT_CLASS (nm_device_olpc_mesh_parent_class)->finalize (object);
}

static void
get_property (GObject *object,
              guint prop_id,
              GValue *value,
              GParamSpec *pspec)
{
	NMDeviceOlpcMesh *device = NM_DEVICE_OLPC_MESH (object);

	_nm_object_ensure_inited (NM_OBJECT (object));

	switch (prop_id) {
	case PROP_HW_ADDRESS:
		g_value_set_string (value, nm_device_olpc_mesh_get_hw_address (device));
		break;
	case PROP_COMPANION:
		g_value_set_object (value, nm_device_olpc_mesh_get_companion (device));
		break;
	case PROP_ACTIVE_CHANNEL:
		g_value_set_uint (value, nm_device_olpc_mesh_get_active_channel (device));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nm_device_olpc_mesh_class_init (NMDeviceOlpcMeshClass *olpc_mesh_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (olpc_mesh_class);
	NMDeviceClass *device_class = NM_DEVICE_CLASS (olpc_mesh_class);

	g_type_class_add_private (olpc_mesh_class, sizeof (NMDeviceOlpcMeshPrivate));

	/* virtual methods */
	object_class->constructed = constructed;
	object_class->dispose = dispose;
	object_class->finalize = finalize;
	object_class->get_property = get_property;
	device_class->connection_compatible = connection_compatible;

	/* properties */

	/**
	 * NMDeviceOlpcMesh:hw-address:
	 *
	 * The hardware (MAC) address of the device.
	 **/
	g_object_class_install_property
		(object_class, PROP_HW_ADDRESS,
		 g_param_spec_string (NM_DEVICE_OLPC_MESH_HW_ADDRESS,
		                      "MAC Address",
		                      "Hardware MAC address",
		                      NULL,
		                      G_PARAM_READABLE));

	/**
	 * NMDeviceOlpcMesh:companion:
	 *
	 * The companion device.
	 **/
	g_object_class_install_property
		(object_class, PROP_COMPANION,
		 g_param_spec_object (NM_DEVICE_OLPC_MESH_COMPANION,
		                     "Companion device",
		                     "Companion device",
		                     NM_TYPE_DEVICE_WIFI,
		                     G_PARAM_READABLE));

	/**
	 * NMDeviceOlpcMesh:active-channel:
	 *
	 * The device's active channel.
	 **/
	g_object_class_install_property
		(object_class, PROP_ACTIVE_CHANNEL,
		 g_param_spec_uint (NM_DEVICE_OLPC_MESH_ACTIVE_CHANNEL,
		                    "Active channel",
		                    "Active channel",
		                    0, G_MAXUINT32, 0,
		                    G_PARAM_READABLE));

}

