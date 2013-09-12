/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/*
 * Dan Williams <dcbw@redhat.com>
 * Tambet Ingo <tambet@gmail.com>
 * Sjoerd Simons <sjoerd.simons@collabora.co.uk>
 * Daniel Drake <dsd@laptop.org>
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
 * (C) Copyright 2007 - 2008 Red Hat, Inc.
 * (C) Copyright 2007 - 2008 Novell, Inc.
 * (C) Copyright 2009 One Laptop per Child
 */

#include <string.h>
#include <netinet/ether.h>
#include <dbus/dbus-glib.h>

#include "NetworkManager.h"
#include "nm-setting-olpc-mesh.h"
#include "nm-param-spec-specialized.h"
#include "nm-utils.h"
#include "nm-dbus-glib-types.h"
#include "nm-utils-private.h"
#include "nm-setting-private.h"

GQuark
nm_setting_olpc_mesh_error_quark (void)
{
	static GQuark quark;

	if (G_UNLIKELY (!quark))
		quark = g_quark_from_static_string ("nm-setting-olpc-mesh-error-quark");
	return quark;
}

static void nm_setting_olpc_mesh_init (NMSettingOlpcMesh *setting);

G_DEFINE_TYPE_WITH_CODE (NMSettingOlpcMesh, nm_setting_olpc_mesh, NM_TYPE_SETTING,
                         _nm_register_setting (NM_SETTING_OLPC_MESH_SETTING_NAME,
                                               g_define_type_id,
                                               1,
                                               NM_SETTING_OLPC_MESH_ERROR))
NM_SETTING_REGISTER_TYPE (NM_TYPE_SETTING_OLPC_MESH)

#define NM_SETTING_OLPC_MESH_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NM_TYPE_SETTING_OLPC_MESH, NMSettingOlpcMeshPrivate))

typedef struct {
	GByteArray *ssid;
	guint32 channel;
	GByteArray *dhcp_anycast_addr;
} NMSettingOlpcMeshPrivate;

enum {
	PROP_0,
	PROP_SSID,
	PROP_CHANNEL,
	PROP_DHCP_ANYCAST_ADDRESS,

	LAST_PROP
};

/**
 * nm_setting_olpc_mesh_new:
 *
 * Creates a new #NMSettingOlpcMesh object with default values.
 *
 * Returns: the new empty #NMSettingOlpcMesh object
 **/
NMSetting *nm_setting_olpc_mesh_new (void)
{
	return (NMSetting *) g_object_new (NM_TYPE_SETTING_OLPC_MESH, NULL);
}

static void
nm_setting_olpc_mesh_init (NMSettingOlpcMesh *setting)
{
	g_object_set (setting, NM_SETTING_NAME, NM_SETTING_OLPC_MESH_SETTING_NAME, NULL);
}

const GByteArray *
nm_setting_olpc_mesh_get_ssid (NMSettingOlpcMesh *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_OLPC_MESH (setting), NULL);

	return NM_SETTING_OLPC_MESH_GET_PRIVATE (setting)->ssid;
}

guint32
nm_setting_olpc_mesh_get_channel (NMSettingOlpcMesh *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_OLPC_MESH (setting), 0);

	return NM_SETTING_OLPC_MESH_GET_PRIVATE (setting)->channel;
}

const GByteArray *
nm_setting_olpc_mesh_get_dhcp_anycast_address (NMSettingOlpcMesh *setting)
{
	g_return_val_if_fail (NM_IS_SETTING_OLPC_MESH (setting), NULL);

	return NM_SETTING_OLPC_MESH_GET_PRIVATE (setting)->dhcp_anycast_addr;
}

static gboolean
verify (NMSetting *setting, GSList *all_settings, GError **error)
{
	NMSettingOlpcMeshPrivate *priv = NM_SETTING_OLPC_MESH_GET_PRIVATE (setting);

	if (!priv->ssid) {
		g_set_error (error,
		             NM_SETTING_OLPC_MESH_ERROR,
		             NM_SETTING_OLPC_MESH_ERROR_MISSING_PROPERTY,
		             NM_SETTING_OLPC_MESH_SSID);
		return FALSE;
	}

	if (!priv->ssid->len || priv->ssid->len > 32) {
		g_set_error (error,
		             NM_SETTING_OLPC_MESH_ERROR,
		             NM_SETTING_OLPC_MESH_ERROR_INVALID_PROPERTY,
		             NM_SETTING_OLPC_MESH_SSID);
		return FALSE;
	}

	if (priv->channel == 0 || priv->channel > 13) {
		g_set_error (error,
		             NM_SETTING_OLPC_MESH_ERROR,
		             NM_SETTING_OLPC_MESH_ERROR_INVALID_PROPERTY,
		             NM_SETTING_OLPC_MESH_CHANNEL);
		return FALSE;
	}

	if (priv->dhcp_anycast_addr && priv->dhcp_anycast_addr->len != ETH_ALEN) {
		g_set_error (error,
		             NM_SETTING_OLPC_MESH_ERROR,
		             NM_SETTING_OLPC_MESH_ERROR_INVALID_PROPERTY,
		             NM_SETTING_OLPC_MESH_DHCP_ANYCAST_ADDRESS);
		return FALSE;
	}

	return TRUE;
}

static void
finalize (GObject *object)
{
	NMSettingOlpcMeshPrivate *priv = NM_SETTING_OLPC_MESH_GET_PRIVATE (object);

	if (priv->ssid)
		g_byte_array_free (priv->ssid, TRUE);
	if (priv->dhcp_anycast_addr)
		g_byte_array_free (priv->dhcp_anycast_addr, TRUE);

	G_OBJECT_CLASS (nm_setting_olpc_mesh_parent_class)->finalize (object);
}

static void
set_property (GObject *object, guint prop_id,
		    const GValue *value, GParamSpec *pspec)
{
	NMSettingOlpcMeshPrivate *priv = NM_SETTING_OLPC_MESH_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_SSID:
		if (priv->ssid)
			g_byte_array_free (priv->ssid, TRUE);
		priv->ssid = g_value_dup_boxed (value);
		break;
	case PROP_CHANNEL:
		priv->channel = g_value_get_uint (value);
		break;
	case PROP_DHCP_ANYCAST_ADDRESS:
		if (priv->dhcp_anycast_addr)
			g_byte_array_free (priv->dhcp_anycast_addr, TRUE);
		priv->dhcp_anycast_addr = g_value_dup_boxed (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
get_property (GObject *object, guint prop_id,
		    GValue *value, GParamSpec *pspec)
{
	NMSettingOlpcMesh *setting = NM_SETTING_OLPC_MESH (object);

	switch (prop_id) {
	case PROP_SSID:
		g_value_set_boxed (value, nm_setting_olpc_mesh_get_ssid (setting));
		break;
	case PROP_CHANNEL:
		g_value_set_uint (value, nm_setting_olpc_mesh_get_channel (setting));
		break;
	case PROP_DHCP_ANYCAST_ADDRESS:
		g_value_set_boxed (value, nm_setting_olpc_mesh_get_dhcp_anycast_address (setting));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nm_setting_olpc_mesh_class_init (NMSettingOlpcMeshClass *setting_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (setting_class);
	NMSettingClass *parent_class = NM_SETTING_CLASS (setting_class);

	g_type_class_add_private (setting_class, sizeof (NMSettingOlpcMeshPrivate));

	/* virtual methods */
	object_class->set_property = set_property;
	object_class->get_property = get_property;
	object_class->finalize     = finalize;
	parent_class->verify       = verify;

	/* Properties */
	/**
	 * NMSettingOlpcMesh:ssid:
	 *
	 * SSID of the mesh network to join.
	 **/
	g_object_class_install_property
		(object_class, PROP_SSID,
		 _nm_param_spec_specialized (NM_SETTING_OLPC_MESH_SSID,
		                             "SSID",
		                             "SSID of the mesh network to join.",
		                             DBUS_TYPE_G_UCHAR_ARRAY,
		                             G_PARAM_READWRITE | NM_SETTING_PARAM_SERIALIZE));

	/**
	 * NMSettingOlpcMesh:channel:
	 *
	 * Channel on which the mesh network to join is located.
	 **/
	g_object_class_install_property
		(object_class, PROP_CHANNEL,
		 g_param_spec_uint (NM_SETTING_OLPC_MESH_CHANNEL,
		                    "Channel",
		                    "Channel on which the mesh network to join is located.",
		                    0, G_MAXUINT32, 0,
		                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT | NM_SETTING_PARAM_SERIALIZE));

	/**
	 * NMSettingOlpcMesh:dhcp-anycast-address:
	 *
	 * Anycast DHCP address used when requesting an IP address via DHCP.  The
	 * specific anycast address used determines which DHCP server class answers
	 * the request.
	 **/
	g_object_class_install_property
		(object_class, PROP_DHCP_ANYCAST_ADDRESS,
		 _nm_param_spec_specialized (NM_SETTING_OLPC_MESH_DHCP_ANYCAST_ADDRESS,
		                             "Anycast DHCP MAC address",
		                             "Anycast DHCP MAC address used when "
		                             "requesting an IP address via DHCP.  The "
		                             "specific anycast address used determines "
		                             "which DHCP server class answers the "
		                             "the request.",
		                             DBUS_TYPE_G_UCHAR_ARRAY,
		                             G_PARAM_READWRITE | NM_SETTING_PARAM_SERIALIZE));

}
