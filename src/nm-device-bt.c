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
 * Copyright (C) 2009 - 2011 Red Hat, Inc.
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <net/ethernet.h>
#include <netinet/ether.h>

#include <glib/gi18n.h>

#include "nm-glib-compat.h"
#include "nm-bluez-common.h"
#include "nm-dbus-manager.h"
#include "nm-device-bt.h"
#include "nm-device-private.h"
#include "nm-logging.h"
#include "nm-marshal.h"
#include "ppp-manager/nm-ppp-manager.h"
#include "nm-properties-changed-signal.h"
#include "nm-setting-connection.h"
#include "nm-setting-bluetooth.h"
#include "nm-setting-cdma.h"
#include "nm-setting-gsm.h"
#include "nm-setting-serial.h"
#include "nm-setting-ppp.h"
#include "nm-device-bt-glue.h"
#include "NetworkManagerUtils.h"
#include "nm-enum-types.h"
#include "nm-utils.h"

#define MM_OLD_DBUS_SERVICE  "org.freedesktop.ModemManager"
#define MM_NEW_DBUS_SERVICE  "org.freedesktop.ModemManager1"
#define BLUETOOTH_DUN_UUID "dun"
#define BLUETOOTH_NAP_UUID "nap"

G_DEFINE_TYPE (NMDeviceBt, nm_device_bt, NM_TYPE_DEVICE)

#define NM_DEVICE_BT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), NM_TYPE_DEVICE_BT, NMDeviceBtPrivate))

static gboolean modem_stage1 (NMDeviceBt *self, NMModem *modem, NMDeviceStateReason *reason);

typedef struct {
	NMDBusManager *dbus_mgr;
	guint mm_watch_id;
	gboolean mm_running;

	guint8 hw_addr[ETH_ALEN];  /* binary representation of bdaddr */
	char *bdaddr;
	char *name;
	guint32 capabilities;

	gboolean connected;
	gboolean have_iface;

	DBusGProxy *type_proxy;
	DBusGProxy *dev_proxy;

	char *rfcomm_iface;
	NMModem *modem;
	guint32 timeout_id;

	guint32 bt_type;  /* BT type of the current connection */
} NMDeviceBtPrivate;

enum {
	PROP_0,
	PROP_HW_ADDRESS,
	PROP_BT_NAME,
	PROP_BT_CAPABILITIES,

	LAST_PROP
};

enum {
	PPP_STATS,
	PROPERTIES_CHANGED,

	LAST_SIGNAL
};
static guint signals[LAST_SIGNAL] = { 0 };


#define NM_BT_ERROR (nm_bt_error_quark ())

static GQuark
nm_bt_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("nm-bt-error");
	return quark;
}

guint32 nm_device_bt_get_capabilities (NMDeviceBt *self)
{
	g_return_val_if_fail (self != NULL, NM_BT_CAPABILITY_NONE);
	g_return_val_if_fail (NM_IS_DEVICE_BT (self), NM_BT_CAPABILITY_NONE);

	return NM_DEVICE_BT_GET_PRIVATE (self)->capabilities;
}

static guint32
get_connection_bt_type (NMConnection *connection)
{
	NMSettingBluetooth *s_bt;
	const char *bt_type;

	s_bt = nm_connection_get_setting_bluetooth (connection);
	if (!s_bt)
		return NM_BT_CAPABILITY_NONE;

	bt_type = nm_setting_bluetooth_get_connection_type (s_bt);
	g_assert (bt_type);

	if (!strcmp (bt_type, NM_SETTING_BLUETOOTH_TYPE_DUN))
		return NM_BT_CAPABILITY_DUN;
	else if (!strcmp (bt_type, NM_SETTING_BLUETOOTH_TYPE_PANU))
		return NM_BT_CAPABILITY_NAP;

	return NM_BT_CAPABILITY_NONE;
}

static NMConnection *
get_best_auto_connection (NMDevice *device,
                          GSList *connections,
                          char **specific_object)
{
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (device);
	GSList *iter;

	for (iter = connections; iter; iter = g_slist_next (iter)) {
		NMConnection *connection = NM_CONNECTION (iter->data);
		guint32 bt_type;

		if (!nm_connection_is_type (connection, NM_SETTING_BLUETOOTH_SETTING_NAME))
			continue;

		bt_type = get_connection_bt_type (connection);
		if (!(bt_type & priv->capabilities))
			continue;

		/* Can't auto-activate a DUN connection without ModemManager */
		if (bt_type == NM_BT_CAPABILITY_DUN && priv->mm_running == FALSE)
			continue;

		return connection;
	}
	return NULL;
}

static gboolean
check_connection_compatible (NMDevice *device,
                             NMConnection *connection,
                             GError **error)
{
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (device);
	NMSettingConnection *s_con;
	NMSettingBluetooth *s_bt;
	const GByteArray *array;
	char *str;
	int addr_match = FALSE;
	guint32 bt_type;

	s_con = nm_connection_get_setting_connection (connection);
	g_assert (s_con);

	if (strcmp (nm_setting_connection_get_connection_type (s_con), NM_SETTING_BLUETOOTH_SETTING_NAME)) {
		g_set_error (error,
		             NM_BT_ERROR, NM_BT_ERROR_CONNECTION_NOT_BT,
		             "The connection was not a Bluetooth connection.");
		return FALSE;
	}

	s_bt = nm_connection_get_setting_bluetooth (connection);
	if (!s_bt) {
		g_set_error (error,
		             NM_BT_ERROR, NM_BT_ERROR_CONNECTION_INVALID,
		             "The connection was not a valid Bluetooth connection.");
		return FALSE;
	}

	array = nm_setting_bluetooth_get_bdaddr (s_bt);
	if (!array || (array->len != ETH_ALEN)) {
		g_set_error (error,
		             NM_BT_ERROR, NM_BT_ERROR_CONNECTION_INVALID,
		             "The connection did not contain a valid Bluetooth address.");
		return FALSE;
	}

	bt_type = get_connection_bt_type (connection);
	if (!(bt_type & priv->capabilities)) {
		g_set_error (error,
		             NM_BT_ERROR, NM_BT_ERROR_CONNECTION_INCOMPATIBLE,
		             "The connection was not compatible with the device's capabilities.");
		return FALSE;
	}

	str = g_strdup_printf ("%02X:%02X:%02X:%02X:%02X:%02X",
	                       array->data[0], array->data[1], array->data[2],
	                       array->data[3], array->data[4], array->data[5]);
	addr_match = !strcmp (priv->bdaddr, str);
	g_free (str);

	return addr_match;
}

static gboolean
check_connection_available (NMDevice *device, NMConnection *connection)
{
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (device);
	guint32 bt_type;

	bt_type = get_connection_bt_type (connection);
	if (!(bt_type & priv->capabilities))
		return FALSE;

	/* DUN connections aren't available without ModemManager */
	if (bt_type == NM_BT_CAPABILITY_DUN && priv->mm_running == FALSE)
		return FALSE;

	return TRUE;
}

static gboolean
complete_connection (NMDevice *device,
                     NMConnection *connection,
                     const char *specific_object,
                     const GSList *existing_connections,
                     GError **error)
{
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (device);
	NMSettingBluetooth *s_bt;
	const GByteArray *setting_bdaddr;
	struct ether_addr *devaddr = ether_aton (priv->bdaddr);
	const char *ctype;
	gboolean is_dun = FALSE, is_pan = FALSE;
	NMSettingGsm *s_gsm;
	NMSettingCdma *s_cdma;
	NMSettingSerial *s_serial;
	NMSettingPPP *s_ppp;
	const char *format = NULL, *preferred = NULL;

	s_gsm = nm_connection_get_setting_gsm (connection);
	s_cdma = nm_connection_get_setting_cdma (connection);
	s_serial = nm_connection_get_setting_serial (connection);
	s_ppp = nm_connection_get_setting_ppp (connection);

	s_bt = nm_connection_get_setting_bluetooth (connection);
	if (!s_bt) {
		s_bt = (NMSettingBluetooth *) nm_setting_bluetooth_new ();
		nm_connection_add_setting (connection, NM_SETTING (s_bt));
	}

	ctype = nm_setting_bluetooth_get_connection_type (s_bt);
	if (ctype) {
		if (!strcmp (ctype, NM_SETTING_BLUETOOTH_TYPE_DUN))
			is_dun = TRUE;
		else if (!strcmp (ctype, NM_SETTING_BLUETOOTH_TYPE_PANU))
			is_pan = TRUE;
	} else {
		if (s_gsm || s_cdma)
			is_dun = TRUE;
		else if (priv->capabilities & NM_BT_CAPABILITY_NAP)
			is_pan = TRUE;
	}

	if (is_pan) {
		/* Make sure the device supports PAN */
		if (!(priv->capabilities & NM_BT_CAPABILITY_NAP)) {
			g_set_error_literal (error,
			                     NM_SETTING_BLUETOOTH_ERROR,
			                     NM_SETTING_BLUETOOTH_ERROR_INVALID_PROPERTY,
			                     "PAN required but Bluetooth device does not support NAP");
			return FALSE;
		}

		/* PAN can't use any DUN-related settings */
		if (s_gsm || s_cdma || s_serial || s_ppp) {
			g_set_error_literal (error,
			                     NM_SETTING_BLUETOOTH_ERROR,
			                     NM_SETTING_BLUETOOTH_ERROR_INVALID_PROPERTY,
			                     "PAN incompatible with GSM, CDMA, or serial settings");
			return FALSE;
		}

		g_object_set (G_OBJECT (s_bt),
		              NM_SETTING_BLUETOOTH_TYPE, NM_SETTING_BLUETOOTH_TYPE_PANU,
		              NULL);

		format = _("PAN connection %d");
	} else if (is_dun) {
		/* Make sure the device supports PAN */
		if (!(priv->capabilities & NM_BT_CAPABILITY_DUN)) {
			g_set_error_literal (error,
			                     NM_SETTING_BLUETOOTH_ERROR,
			                     NM_SETTING_BLUETOOTH_ERROR_INVALID_PROPERTY,
			                     "DUN required but Bluetooth device does not support DUN");
			return FALSE;
		}

		/* Need at least a GSM or a CDMA setting */
		if (!s_gsm && !s_cdma) {
			g_set_error_literal (error,
			                     NM_SETTING_BLUETOOTH_ERROR,
			                     NM_SETTING_BLUETOOTH_ERROR_INVALID_PROPERTY,
			                     "Setting requires DUN but no GSM or CDMA setting is present");
			return FALSE;
		}

		g_object_set (G_OBJECT (s_bt),
		              NM_SETTING_BLUETOOTH_TYPE, NM_SETTING_BLUETOOTH_TYPE_DUN,
		              NULL);

		if (s_gsm) {
			format = _("GSM connection %d");
			if (!nm_setting_gsm_get_number (s_gsm))
				g_object_set (G_OBJECT (s_gsm), NM_SETTING_GSM_NUMBER, "*99#", NULL);
		} else if (s_cdma) {
			format = _("CDMA connection %d");
			if (!nm_setting_cdma_get_number (s_cdma))
				g_object_set (G_OBJECT (s_cdma), NM_SETTING_GSM_NUMBER, "#777", NULL);
		} else
			format = _("DUN connection %d");
	} else {
		g_set_error_literal (error,
		                     NM_SETTING_BLUETOOTH_ERROR,
		                     NM_SETTING_BLUETOOTH_ERROR_INVALID_PROPERTY,
		                     "Unknown/unhandled Bluetooth connection type");
		return FALSE;
	}

	nm_utils_complete_generic (connection,
	                           NM_SETTING_BLUETOOTH_SETTING_NAME,
	                           existing_connections,
	                           format,
	                           preferred,
	                           is_dun ? FALSE : TRUE); /* No IPv6 yet for DUN */

	setting_bdaddr = nm_setting_bluetooth_get_bdaddr (s_bt);
	if (setting_bdaddr) {
		/* Make sure the setting BT Address (if any) matches the device's */
		if (memcmp (setting_bdaddr->data, devaddr->ether_addr_octet, ETH_ALEN)) {
			g_set_error_literal (error,
			                     NM_SETTING_BLUETOOTH_ERROR,
			                     NM_SETTING_BLUETOOTH_ERROR_INVALID_PROPERTY,
			                     NM_SETTING_BLUETOOTH_BDADDR);
			return FALSE;
		}
	} else {
		GByteArray *bdaddr;
		const guint8 null_mac[ETH_ALEN] = { 0, 0, 0, 0, 0, 0 };

		/* Lock the connection to this device by default */
		if (memcmp (devaddr->ether_addr_octet, null_mac, ETH_ALEN)) {
			bdaddr = g_byte_array_sized_new (ETH_ALEN);
			g_byte_array_append (bdaddr, devaddr->ether_addr_octet, ETH_ALEN);
			g_object_set (G_OBJECT (s_bt), NM_SETTING_BLUETOOTH_BDADDR, bdaddr, NULL);
			g_byte_array_free (bdaddr, TRUE);
		}
	}

	return TRUE;
}

static guint32
get_generic_capabilities (NMDevice *dev)
{
	return NM_DEVICE_CAP_NM_SUPPORTED;
}

static const guint8 *
get_hw_address (NMDevice *device, guint *out_len)
{
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (device);

	*out_len = sizeof (priv->hw_addr);
	return priv->hw_addr;
}

static gboolean
hwaddr_matches (NMDevice *device,
                NMConnection *connection,
                const guint8 *other_hwaddr,
                guint other_hwaddr_len,
                gboolean fail_if_no_hwaddr)
{
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (device);
	NMSettingBluetooth *s_bt;
	const GByteArray *mac = NULL;
	gboolean matches = FALSE;
	GByteArray *devmac;

	s_bt = nm_connection_get_setting_bluetooth (connection);
	if (s_bt)
		mac = nm_setting_bluetooth_get_bdaddr (s_bt);

	if (mac) {
		devmac = nm_utils_hwaddr_atoba (priv->bdaddr, ARPHRD_ETHER);
		g_return_val_if_fail (devmac != NULL, FALSE);
		g_return_val_if_fail (devmac->len == mac->len, FALSE);

		if (other_hwaddr) {
			g_return_val_if_fail (other_hwaddr_len == devmac->len, FALSE);
			matches = (memcmp (mac->data, other_hwaddr, mac->len) == 0) ? TRUE : FALSE;
		} else
			matches = (memcmp (mac->data, devmac->data, mac->len) == 0) ? TRUE : FALSE;

		g_byte_array_free (devmac, TRUE);
		return matches;
	} else if (fail_if_no_hwaddr == FALSE)
		return TRUE;

	return FALSE;
}

/*****************************************************************************/
/* IP method PPP */

static void
ppp_stats (NMModem *modem,
		   guint32 in_bytes,
		   guint32 out_bytes,
		   gpointer user_data)
{
	g_signal_emit (NM_DEVICE_BT (user_data), signals[PPP_STATS], 0, in_bytes, out_bytes);
}

static void
ppp_failed (NMModem *modem, NMDeviceStateReason reason, gpointer user_data)
{
	NMDevice *device = NM_DEVICE (user_data);

	switch (nm_device_get_state (device)) {
	case NM_DEVICE_STATE_PREPARE:
	case NM_DEVICE_STATE_CONFIG:
	case NM_DEVICE_STATE_NEED_AUTH:
		nm_device_state_changed (device, NM_DEVICE_STATE_FAILED, reason);
		break;
	case NM_DEVICE_STATE_IP_CONFIG:
	case NM_DEVICE_STATE_IP_CHECK:
	case NM_DEVICE_STATE_SECONDARIES:
	case NM_DEVICE_STATE_ACTIVATED:
		if (nm_device_activate_ip4_state_in_conf (device))
			nm_device_activate_schedule_ip4_config_timeout (device);
		else {
			nm_device_state_changed (device,
			                         NM_DEVICE_STATE_FAILED,
			                         NM_DEVICE_STATE_REASON_IP_CONFIG_UNAVAILABLE);
		}
		break;
	default:
		break;
	}
}

static void
modem_auth_requested (NMModem *modem, gpointer user_data)
{
	nm_device_state_changed (NM_DEVICE (user_data),
	                         NM_DEVICE_STATE_NEED_AUTH,
	                         NM_DEVICE_STATE_REASON_NONE);
}

static void
modem_auth_result (NMModem *modem, GError *error, gpointer user_data)
{
	NMDevice *device = NM_DEVICE (user_data);
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (device);
	NMDeviceStateReason reason = NM_DEVICE_STATE_REASON_NONE;

	if (error) {
		nm_device_state_changed (device,
		                         NM_DEVICE_STATE_FAILED,
		                         NM_DEVICE_STATE_REASON_NO_SECRETS);
	} else {
		/* Otherwise, on success for GSM/CDMA secrets we need to schedule modem stage1 again */
		g_return_if_fail (nm_device_get_state (device) == NM_DEVICE_STATE_NEED_AUTH);
		if (!modem_stage1 (NM_DEVICE_BT (device), priv->modem, &reason))
			nm_device_state_changed (device, NM_DEVICE_STATE_FAILED, reason);
	}
}

static void
modem_prepare_result (NMModem *modem,
                      gboolean success,
                      NMDeviceStateReason reason,
                      gpointer user_data)
{
	NMDevice *device = NM_DEVICE (user_data);
	NMDeviceState state;

	state = nm_device_get_state (device);
	g_return_if_fail (state == NM_DEVICE_STATE_CONFIG || state == NM_DEVICE_STATE_NEED_AUTH);

	if (success) {
		NMActRequest *req;
		NMActStageReturn ret;
		NMDeviceStateReason stage2_reason = NM_DEVICE_STATE_REASON_NONE;

		req = nm_device_get_act_request (device);
		g_assert (req);

		ret = nm_modem_act_stage2_config (modem, req, &stage2_reason);
		switch (ret) {
		case NM_ACT_STAGE_RETURN_POSTPONE:
			break;
		case NM_ACT_STAGE_RETURN_SUCCESS:
			nm_device_activate_schedule_stage3_ip_config_start (device);
			break;
		case NM_ACT_STAGE_RETURN_FAILURE:
		default:
			nm_device_state_changed (device, NM_DEVICE_STATE_FAILED, stage2_reason);
			break;
		}
	} else
		nm_device_state_changed (device, NM_DEVICE_STATE_FAILED, reason);
}

static void
device_state_changed (NMDevice *device,
                      NMDeviceState new_state,
                      NMDeviceState old_state,
                      NMDeviceStateReason reason)
{
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (device);

	if (priv->modem)
		nm_modem_device_state_changed (priv->modem, new_state, old_state, reason);
}

static void
modem_ip4_config_result (NMModem *self,
                         NMIP4Config *config,
                         GError *error,
                         gpointer user_data)
{
	NMDevice *device = NM_DEVICE (user_data);

	g_return_if_fail (nm_device_activate_ip4_state_in_conf (device) == TRUE);

	if (error) {
		nm_log_warn (LOGD_MB | LOGD_IP4 | LOGD_BT,
		             "(%s): retrieving IP4 configuration failed: (%d) %s",
		             nm_device_get_ip_iface (device),
		             error ? error->code : -1,
		             error && error->message ? error->message : "(unknown)");

		nm_device_state_changed (device, NM_DEVICE_STATE_FAILED, NM_DEVICE_STATE_REASON_IP_CONFIG_UNAVAILABLE);
	} else
		nm_device_activate_schedule_ip4_config_result (device, config);
}

static void
data_port_changed_cb (NMModem *modem, GParamSpec *pspec, gpointer user_data)
{
	NMDevice *self = NM_DEVICE (user_data);

	nm_device_set_ip_iface (self, nm_modem_get_data_port (modem));
}

static gboolean
modem_stage1 (NMDeviceBt *self, NMModem *modem, NMDeviceStateReason *reason)
{
	NMActRequest *req;
	NMActStageReturn ret;

	g_return_val_if_fail (reason != NULL, FALSE);

	req = nm_device_get_act_request (NM_DEVICE (self));
	g_assert (req);

	ret = nm_modem_act_stage1_prepare (modem, req, reason);
	switch (ret) {
	case NM_ACT_STAGE_RETURN_POSTPONE:
	case NM_ACT_STAGE_RETURN_SUCCESS:
		/* Success, wait for the 'prepare-result' signal */
		return TRUE;
	case NM_ACT_STAGE_RETURN_FAILURE:
	default:
		break;
	}

	return FALSE;
}

/*****************************************************************************/

gboolean
nm_device_bt_modem_added (NMDeviceBt *self,
                          NMModem *modem,
                          const char *driver)
{
	NMDeviceBtPrivate *priv;
	const gchar *modem_data_port;
	const gchar *modem_control_port;
	char *base;
	NMDeviceState state;
	NMDeviceStateReason reason = NM_DEVICE_STATE_REASON_NONE;

	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (NM_IS_DEVICE_BT (self), FALSE);
	g_return_val_if_fail (modem != NULL, FALSE);
	g_return_val_if_fail (NM_IS_MODEM (modem), FALSE);

	priv = NM_DEVICE_BT_GET_PRIVATE (self);
	modem_data_port = nm_modem_get_data_port (modem);
	modem_control_port = nm_modem_get_control_port (modem);
	g_return_val_if_fail (modem_data_port != NULL || modem_control_port != NULL, FALSE);

	if (!priv->rfcomm_iface)
		return FALSE;

	base = g_path_get_basename (priv->rfcomm_iface);
	if (g_strcmp0 (base, modem_data_port) && g_strcmp0 (base, modem_control_port)) {
		g_free (base);
		return FALSE;
	}
	g_free (base);

	/* Got the modem */
	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	/* Can only accept the modem in stage2, but since the interface matched
	 * what we were expecting, don't let anything else claim the modem either.
	 */
	state = nm_device_get_state (NM_DEVICE (self));
	if (state != NM_DEVICE_STATE_CONFIG) {
		nm_log_warn (LOGD_BT | LOGD_MB,
		             "(%s): modem found but device not in correct state (%d)",
		             nm_device_get_iface (NM_DEVICE (self)),
		             nm_device_get_state (NM_DEVICE (self)));
		return TRUE;
	}

	nm_log_info (LOGD_BT | LOGD_MB,
	             "Activation (%s/bluetooth) Stage 2 of 5 (Device Configure) modem found.",
	             nm_device_get_iface (NM_DEVICE (self)));

	if (priv->modem) {
		g_warn_if_reached ();
		g_object_unref (priv->modem);
	}

	priv->modem = g_object_ref (modem);
	g_signal_connect (modem, NM_MODEM_PPP_STATS, G_CALLBACK (ppp_stats), self);
	g_signal_connect (modem, NM_MODEM_PPP_FAILED, G_CALLBACK (ppp_failed), self);
	g_signal_connect (modem, NM_MODEM_PREPARE_RESULT, G_CALLBACK (modem_prepare_result), self);
	g_signal_connect (modem, NM_MODEM_IP4_CONFIG_RESULT, G_CALLBACK (modem_ip4_config_result), self);
	g_signal_connect (modem, NM_MODEM_AUTH_REQUESTED, G_CALLBACK (modem_auth_requested), self);
	g_signal_connect (modem, NM_MODEM_AUTH_RESULT, G_CALLBACK (modem_auth_result), self);

	/* In the old ModemManager the data port is known from the very beginning;
	 * while in the new ModemManager the data port is set afterwards when the bearer gets
	 * created */
	if (modem_data_port)
		nm_device_set_ip_iface (NM_DEVICE (self), modem_data_port);
	g_signal_connect (modem, "notify::" NM_MODEM_DATA_PORT, G_CALLBACK (data_port_changed_cb), self);

	/* Kick off the modem connection */
	if (!modem_stage1 (self, modem, &reason))
		nm_device_state_changed (NM_DEVICE (self), NM_DEVICE_STATE_FAILED, reason);

	return TRUE;
}

gboolean
nm_device_bt_modem_removed (NMDeviceBt *self, NMModem *modem)
{
	NMDeviceBtPrivate *priv;
	NMDeviceState state;

	g_return_val_if_fail (self != NULL, FALSE);
	g_return_val_if_fail (NM_IS_DEVICE_BT (self), FALSE);
	g_return_val_if_fail (modem != NULL, FALSE);
	g_return_val_if_fail (NM_IS_MODEM (modem), FALSE);

	priv = NM_DEVICE_BT_GET_PRIVATE (self);

	if (modem != priv->modem)
		return FALSE;

	/* Fail the device if the modem was removed while active */
	state = nm_device_get_state (NM_DEVICE (self));
	if (   state == NM_DEVICE_STATE_ACTIVATED
	    || nm_device_is_activating (NM_DEVICE (self))) {
		nm_device_state_changed (NM_DEVICE (self),
		                         NM_DEVICE_STATE_FAILED,
		                         NM_DEVICE_STATE_REASON_BT_FAILED);
	} else {
		g_object_unref (priv->modem);
		priv->modem = NULL;
	}

	return TRUE;
}

static gboolean
modem_find_timeout (gpointer user_data)
{
	NMDeviceBt *self = NM_DEVICE_BT (user_data);

	NM_DEVICE_BT_GET_PRIVATE (self)->timeout_id = 0;
	nm_device_state_changed (NM_DEVICE (self),
	                         NM_DEVICE_STATE_FAILED,
	                         NM_DEVICE_STATE_REASON_MODEM_NOT_FOUND);
	return FALSE;
}

static void
check_connect_continue (NMDeviceBt *self)
{
	NMDevice *device = NM_DEVICE (self);
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (self);
	gboolean pan = (priv->bt_type == NM_BT_CAPABILITY_NAP);
	gboolean dun = (priv->bt_type == NM_BT_CAPABILITY_DUN);

	if (!priv->connected || !priv->have_iface)
		return;

	nm_log_info (LOGD_BT, "Activation (%s %s/bluetooth) Stage 2 of 5 (Device Configure) "
	             "successful.  Will connect via %s.",
	             nm_device_get_iface (device),
	             nm_device_get_ip_iface (device),
	             dun ? "DUN" : (pan ? "PAN" : "unknown"));

	/* Kill the connect timeout since we're connected now */
	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	if (pan) {
		/* Bluez says we're connected now.  Start IP config. */
		nm_device_activate_schedule_stage3_ip_config_start (device);
	} else if (dun) {
		/* Wait for ModemManager to find the modem */
		priv->timeout_id = g_timeout_add_seconds (30, modem_find_timeout, self);

		nm_log_info (LOGD_BT | LOGD_MB, "Activation (%s/bluetooth) Stage 2 of 5 (Device Configure) "
		             "waiting for modem to appear.",
		             nm_device_get_iface (device));
	} else
		g_assert_not_reached ();
}

static void
bluez_connect_cb (DBusGProxy *proxy,
                  DBusGProxyCall *call_id,
                  void *user_data)
{
	NMDeviceBt *self = NM_DEVICE_BT (user_data);
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (self);
	GError *error = NULL;
	char *device;

	if (dbus_g_proxy_end_call (proxy, call_id, &error,
	                           G_TYPE_STRING, &device,
	                           G_TYPE_INVALID) == FALSE) {
		nm_log_warn (LOGD_BT, "Error connecting with bluez: %s",
		             error && error->message ? error->message : "(unknown)");
		g_clear_error (&error);

		nm_device_state_changed (NM_DEVICE (self),
		                         NM_DEVICE_STATE_FAILED,
		                         NM_DEVICE_STATE_REASON_BT_FAILED);
		return;
	}

	if (!device || !strlen (device)) {
		nm_log_warn (LOGD_BT, "Invalid network device returned by bluez");

		nm_device_state_changed (NM_DEVICE (self),
		                         NM_DEVICE_STATE_FAILED,
		                         NM_DEVICE_STATE_REASON_BT_FAILED);
	}

	if (priv->bt_type == NM_BT_CAPABILITY_DUN) {
		g_free (priv->rfcomm_iface);
		priv->rfcomm_iface = device;
	} else if (priv->bt_type == NM_BT_CAPABILITY_NAP) {
		nm_device_set_ip_iface (NM_DEVICE (self), device);
		g_free (device);
	}

	nm_log_dbg (LOGD_BT, "(%s): connect request successful",
	            nm_device_get_iface (NM_DEVICE (self)));

	/* Stage 3 gets scheduled when Bluez says we're connected */
	priv->have_iface = TRUE;
	check_connect_continue (self);
}

static void
bluez_property_changed (DBusGProxy *proxy,
                        const char *property,
                        GValue *value,
                        gpointer user_data)
{
	NMDevice *device = NM_DEVICE (user_data);
	NMDeviceBt *self = NM_DEVICE_BT (user_data);
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (self);
	gboolean connected;
	NMDeviceState state;
	const char *prop_str = "(unknown)";

	if (G_VALUE_HOLDS_STRING (value))
		prop_str = g_value_get_string (value);
	else if (G_VALUE_HOLDS_BOOLEAN (value))
		prop_str = g_value_get_boolean (value) ? "true" : "false";

	nm_log_dbg (LOGD_BT, "(%s): bluez property '%s' changed to '%s'",
	            nm_device_get_iface (device),
	            property,
	            prop_str);

	if (strcmp (property, "Connected"))
		return;

	state = nm_device_get_state (device);
	connected = g_value_get_boolean (value);
	if (connected) {
		if (state == NM_DEVICE_STATE_CONFIG) {
			nm_log_dbg (LOGD_BT, "(%s): connected to the device",
			            nm_device_get_iface (device));

			priv->connected = TRUE;
			check_connect_continue (self);
		}
	} else {
		gboolean fail = FALSE;

		/* Bluez says we're disconnected from the device.  Suck. */

		if (nm_device_is_activating (device)) {
			nm_log_info (LOGD_BT,
			             "Activation (%s/bluetooth): bluetooth link disconnected.",
			             nm_device_get_iface (device));
			fail = TRUE;
		} else if (state == NM_DEVICE_STATE_ACTIVATED) {
			nm_log_info (LOGD_BT, "(%s): bluetooth link disconnected.",
			             nm_device_get_iface (device));
			fail = TRUE;
		}

		if (fail) {
			nm_device_state_changed (device, NM_DEVICE_STATE_FAILED, NM_DEVICE_STATE_REASON_CARRIER);
			priv->connected = FALSE;
		}
	}
}

static gboolean
bt_connect_timeout (gpointer user_data)
{
	NMDeviceBt *self = NM_DEVICE_BT (user_data);

	nm_log_dbg (LOGD_BT, "(%s): initial connection timed out",
	            nm_device_get_iface (NM_DEVICE (self)));

	NM_DEVICE_BT_GET_PRIVATE (self)->timeout_id = 0;
	nm_device_state_changed (NM_DEVICE (self),
	                         NM_DEVICE_STATE_FAILED,
	                         NM_DEVICE_STATE_REASON_BT_FAILED);
	return FALSE;
}

static NMActStageReturn
act_stage2_config (NMDevice *device, NMDeviceStateReason *reason)
{
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (device);
	DBusGConnection *bus;
	gboolean dun = FALSE;
	NMConnection *connection;

	connection = nm_device_get_connection (device);
	g_assert (connection);
	priv->bt_type = get_connection_bt_type (connection);
	if (priv->bt_type == NM_BT_CAPABILITY_NONE) {
		// FIXME: set a reason code
		return NM_ACT_STAGE_RETURN_FAILURE;
	}

	if (priv->bt_type == NM_BT_CAPABILITY_DUN && !priv->mm_running) {
		*reason = NM_DEVICE_STATE_REASON_MODEM_MANAGER_UNAVAILABLE;
		return NM_ACT_STAGE_RETURN_FAILURE;
	}

	if (priv->bt_type == NM_BT_CAPABILITY_DUN)
		dun = TRUE;
	else if (priv->bt_type == NM_BT_CAPABILITY_NAP)
		dun = FALSE;
	else
		g_assert_not_reached ();

	bus = nm_dbus_manager_get_connection (priv->dbus_mgr);
	priv->dev_proxy = dbus_g_proxy_new_for_name (bus,
	                                             BLUEZ_SERVICE,
	                                             nm_device_get_udi (device),
	                                             BLUEZ_DEVICE_INTERFACE);
	if (!priv->dev_proxy) {
		// FIXME: set a reason code
		return NM_ACT_STAGE_RETURN_FAILURE;
	}

	/* Watch for BT device property changes */
	dbus_g_object_register_marshaller (_nm_marshal_VOID__STRING_BOXED,
	                                   G_TYPE_NONE,
	                                   G_TYPE_STRING, G_TYPE_VALUE,
	                                   G_TYPE_INVALID);
	dbus_g_proxy_add_signal (priv->dev_proxy, "PropertyChanged",
	                         G_TYPE_STRING, G_TYPE_VALUE, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (priv->dev_proxy, "PropertyChanged",
	                             G_CALLBACK (bluez_property_changed), device, NULL);

	priv->type_proxy = dbus_g_proxy_new_for_name (bus,
	                                              BLUEZ_SERVICE,
	                                              nm_device_get_udi (device),
	                                              dun ? BLUEZ_SERIAL_INTERFACE : BLUEZ_NETWORK_INTERFACE);
	if (!priv->type_proxy) {
		// FIXME: set a reason code
		return NM_ACT_STAGE_RETURN_FAILURE;
	}

	nm_log_dbg (LOGD_BT, "(%s): requesting connection to the device",
	            nm_device_get_iface (device));

	/* Connect to the BT device */
	dbus_g_proxy_begin_call_with_timeout (priv->type_proxy, "Connect",
	                                      bluez_connect_cb,
	                                      device,
	                                      NULL,
	                                      20000,
	                                      G_TYPE_STRING, dun ? BLUETOOTH_DUN_UUID : BLUETOOTH_NAP_UUID,
	                                      G_TYPE_INVALID);

	if (priv->timeout_id)
		g_source_remove (priv->timeout_id);
	priv->timeout_id = g_timeout_add_seconds (30, bt_connect_timeout, device);

	return NM_ACT_STAGE_RETURN_POSTPONE;
}

static NMActStageReturn
act_stage3_ip4_config_start (NMDevice *device,
                             NMIP4Config **out_config,
                             NMDeviceStateReason *reason)
{
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (device);
	NMActStageReturn ret;

	if (priv->bt_type == NM_BT_CAPABILITY_DUN) {
		ret = nm_modem_stage3_ip4_config_start (NM_DEVICE_BT_GET_PRIVATE (device)->modem,
		                                        device,
		                                        NM_DEVICE_CLASS (nm_device_bt_parent_class),
		                                        reason);
	} else
		ret = NM_DEVICE_CLASS (nm_device_bt_parent_class)->act_stage3_ip4_config_start (device, out_config, reason);

	return ret;
}

static NMActStageReturn
act_stage3_ip6_config_start (NMDevice *device,
                             NMIP6Config **out_config,
                             NMDeviceStateReason *reason)
{
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (device);
	NMActStageReturn ret;

	if (priv->bt_type == NM_BT_CAPABILITY_DUN) {
		ret = nm_modem_stage3_ip6_config_start (NM_DEVICE_BT_GET_PRIVATE (device)->modem,
		                                        device,
		                                        NM_DEVICE_CLASS (nm_device_bt_parent_class),
		                                        reason);
	} else
		ret = NM_DEVICE_CLASS (nm_device_bt_parent_class)->act_stage3_ip6_config_start (device, out_config, reason);

	return ret;
}

static void
deactivate (NMDevice *device)
{
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (device);

	priv->have_iface = FALSE;
	priv->connected = FALSE;

	if (priv->bt_type == NM_BT_CAPABILITY_DUN) {

		if (priv->modem) {
			nm_modem_deactivate (priv->modem, device);

			/* Since we're killing the Modem object before it'll get the
			 * state change signal, simulate the state change here.
			 */
			nm_modem_device_state_changed (priv->modem,
			                               NM_DEVICE_STATE_DISCONNECTED,
			                               NM_DEVICE_STATE_ACTIVATED,
			                               NM_DEVICE_STATE_REASON_USER_REQUESTED);
			g_object_unref (priv->modem);
			priv->modem = NULL;
		}

		if (priv->type_proxy) {
			/* Don't ever pass NULL through dbus; rfcomm_iface
			 * might happen to be NULL for some reason.
			 */
			if (priv->rfcomm_iface) {
				dbus_g_proxy_call_no_reply (priv->type_proxy, "Disconnect",
				                            G_TYPE_STRING, priv->rfcomm_iface,
				                            G_TYPE_INVALID);
			}
			g_object_unref (priv->type_proxy);
			priv->type_proxy = NULL;
		}
	} else if (priv->bt_type == NM_BT_CAPABILITY_NAP) {
		if (priv->type_proxy) {
			dbus_g_proxy_call_no_reply (priv->type_proxy, "Disconnect",
			                            G_TYPE_INVALID);
			g_object_unref (priv->type_proxy);
			priv->type_proxy = NULL;
		}
	}

	if (priv->dev_proxy) {
		g_object_unref (priv->dev_proxy);
		priv->dev_proxy = NULL;
	}

	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	priv->bt_type = NM_BT_CAPABILITY_NONE;

	g_free (priv->rfcomm_iface);
	priv->rfcomm_iface = NULL;

	if (NM_DEVICE_CLASS (nm_device_bt_parent_class)->deactivate)
		NM_DEVICE_CLASS (nm_device_bt_parent_class)->deactivate (device);
}

/*****************************************************************************/

static gboolean
is_available (NMDevice *dev)
{
	NMDeviceBt *self = NM_DEVICE_BT (dev);
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (self);

	/* PAN doesn't need ModemManager, so devices that support it are always available */
	if (priv->capabilities & NM_BT_CAPABILITY_NAP)
		return TRUE;

	/* DUN requires ModemManager */
	return priv->mm_running;
}

static void
handle_availability_change (NMDeviceBt *self,
                            gboolean old_available,
                            NMDeviceStateReason unavailable_reason)
{
	NMDevice *device = NM_DEVICE (self);
	NMDeviceState state;
	gboolean available;

	state = nm_device_get_state (device);
	if (state < NM_DEVICE_STATE_UNAVAILABLE) {
		nm_log_dbg (LOGD_BT, "(%s): availability blocked by UNMANAGED state",
		            nm_device_get_iface (device));
		return;
	}

	available = nm_device_is_available (device);
	if (available == old_available)
		return;

	if (available) {
		if (state != NM_DEVICE_STATE_UNAVAILABLE)
			nm_log_warn (LOGD_CORE | LOGD_BT, "not in expected unavailable state!");

		nm_device_state_changed (device,
		                         NM_DEVICE_STATE_DISCONNECTED,
		                         NM_DEVICE_STATE_REASON_NONE);
	} else {
		nm_device_state_changed (device,
		                         NM_DEVICE_STATE_UNAVAILABLE,
		                         unavailable_reason);
	}
}

static void
set_mm_running (NMDeviceBt *self, gboolean running)
{
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (self);
	gboolean old_available;

	if (priv->mm_running == running)
		return;

	nm_log_dbg (LOGD_BT, "(%s): ModemManager now %s",
	            nm_device_get_iface (NM_DEVICE (self)),
	            running ? "available" : "unavailable");

	old_available = nm_device_is_available (NM_DEVICE (self));
	priv->mm_running = running;
	handle_availability_change (self, old_available, NM_DEVICE_STATE_REASON_MODEM_MANAGER_UNAVAILABLE);

	/* Need to recheck available connections whenever MM appears or disappears,
	 * since the device could be both DUN and NAP capable and thus may not
	 * change state (which rechecks available connections) when MM comes and goes.
	 */
	if (priv->capabilities & NM_BT_CAPABILITY_DUN)
	    nm_device_recheck_available_connections (NM_DEVICE (self));
}

static void
mm_name_owner_changed (NMDBusManager *dbus_mgr,
                       const char *name,
                       const char *old_owner,
                       const char *new_owner,
                       NMDeviceBt *self)
{
	gboolean old_owner_good;
	gboolean new_owner_good;

	/* Can't handle the signal if its not from the modem service */
	if (   strcmp (MM_OLD_DBUS_SERVICE, name) != 0
#if WITH_MODEM_MANAGER_1
	    && strcmp (MM_NEW_DBUS_SERVICE, name) != 0
#endif
	    )
		return;

	old_owner_good = (old_owner && strlen (old_owner));
	new_owner_good = (new_owner && strlen (new_owner));

	if (!old_owner_good && new_owner_good)
		set_mm_running (self, TRUE);
	else if (old_owner_good && !new_owner_good)
		set_mm_running (self, FALSE);
}

/*****************************************************************************/

NMDevice *
nm_device_bt_new (const char *udi,
                  const char *bdaddr,
                  const char *name,
                  guint32 capabilities,
                  gboolean managed)
{
	g_return_val_if_fail (udi != NULL, NULL);
	g_return_val_if_fail (bdaddr != NULL, NULL);
	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (capabilities != NM_BT_CAPABILITY_NONE, NULL);

	return (NMDevice *) g_object_new (NM_TYPE_DEVICE_BT,
	                                  NM_DEVICE_UDI, udi,
	                                  NM_DEVICE_IFACE, bdaddr,
	                                  NM_DEVICE_DRIVER, "bluez",
	                                  NM_DEVICE_BT_HW_ADDRESS, bdaddr,
	                                  NM_DEVICE_BT_NAME, name,
	                                  NM_DEVICE_BT_CAPABILITIES, capabilities,
	                                  NM_DEVICE_MANAGED, managed,
	                                  NM_DEVICE_TYPE_DESC, "Bluetooth",
	                                  NM_DEVICE_DEVICE_TYPE, NM_DEVICE_TYPE_BT,
	                                  NULL);
}

static void
nm_device_bt_init (NMDeviceBt *self)
{
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (self);
	gboolean mm_running;

	priv->dbus_mgr = nm_dbus_manager_get ();

	priv->mm_watch_id = g_signal_connect (priv->dbus_mgr,
	                                      NM_DBUS_MANAGER_NAME_OWNER_CHANGED,
	                                      G_CALLBACK (mm_name_owner_changed),
	                                      self);

	/* Initial check to see if ModemManager is running */
	mm_running = nm_dbus_manager_name_has_owner (priv->dbus_mgr, MM_OLD_DBUS_SERVICE);
#if WITH_MODEM_MANAGER_1
	if (!mm_running)
		mm_running = nm_dbus_manager_name_has_owner (priv->dbus_mgr, MM_NEW_DBUS_SERVICE);
#endif
	set_mm_running (self, mm_running);
}

static void
set_property (GObject *object, guint prop_id,
              const GValue *value, GParamSpec *pspec)
{
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_HW_ADDRESS:
		/* Construct only */
		priv->bdaddr = g_ascii_strup (g_value_get_string (value), -1);
		if (!nm_utils_hwaddr_aton (priv->bdaddr, ARPHRD_ETHER, &priv->hw_addr))
			nm_log_err (LOGD_HW, "Failed to convert BT address '%s'", priv->bdaddr);
		break;
	case PROP_BT_NAME:
		/* Construct only */
		priv->name = g_value_dup_string (value);
		break;
	case PROP_BT_CAPABILITIES:
		/* Construct only */
		priv->capabilities = g_value_get_uint (value);
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
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (object);

	switch (prop_id) {
	case PROP_HW_ADDRESS:
		g_value_set_string (value, priv->bdaddr);
		break;
	case PROP_BT_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_BT_CAPABILITIES:
		g_value_set_uint (value, priv->capabilities);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
dispose (GObject *object)
{
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (object);

	if (priv->timeout_id) {
		g_source_remove (priv->timeout_id);
		priv->timeout_id = 0;
	}

	if (priv->dbus_mgr && priv->mm_watch_id) {
		g_signal_handler_disconnect (priv->dbus_mgr, priv->mm_watch_id);
		priv->mm_watch_id = 0;
	}
	g_clear_object (&priv->dbus_mgr);

	g_clear_object (&priv->type_proxy);
	g_clear_object (&priv->dev_proxy);
	g_clear_object (&priv->modem);

	G_OBJECT_CLASS (nm_device_bt_parent_class)->dispose (object);
}

static void
finalize (GObject *object)
{
	NMDeviceBtPrivate *priv = NM_DEVICE_BT_GET_PRIVATE (object);

	g_free (priv->rfcomm_iface);
	g_free (priv->bdaddr);
	g_free (priv->name);

	G_OBJECT_CLASS (nm_device_bt_parent_class)->finalize (object);
}

static void
nm_device_bt_class_init (NMDeviceBtClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	NMDeviceClass *device_class = NM_DEVICE_CLASS (klass);

	g_type_class_add_private (object_class, sizeof (NMDeviceBtPrivate));

	object_class->get_property = get_property;
	object_class->set_property = set_property;
	object_class->dispose = dispose;
	object_class->finalize = finalize;

	device_class->get_best_auto_connection = get_best_auto_connection;
	device_class->get_generic_capabilities = get_generic_capabilities;
	device_class->deactivate = deactivate;
	device_class->act_stage2_config = act_stage2_config;
	device_class->act_stage3_ip4_config_start = act_stage3_ip4_config_start;
	device_class->act_stage3_ip6_config_start = act_stage3_ip6_config_start;
	device_class->check_connection_compatible = check_connection_compatible;
	device_class->check_connection_available = check_connection_available;
	device_class->complete_connection = complete_connection;
	device_class->hwaddr_matches = hwaddr_matches;
	device_class->get_hw_address = get_hw_address;
	device_class->is_available = is_available;

	device_class->state_changed = device_state_changed;

	/* Properties */
	g_object_class_install_property
		(object_class, PROP_HW_ADDRESS,
		 g_param_spec_string (NM_DEVICE_BT_HW_ADDRESS,
		                      "Bluetooth address",
		                      "Bluetooth address",
		                      NULL,
		                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property
		(object_class, PROP_BT_NAME,
		 g_param_spec_string (NM_DEVICE_BT_NAME,
		                      "Bluetooth device name",
		                      "Bluetooth device name",
		                      NULL,
		                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property
		(object_class, PROP_BT_CAPABILITIES,
		 g_param_spec_uint (NM_DEVICE_BT_CAPABILITIES,
		                    "Bluetooth device capabilities",
		                    "Bluetooth device capabilities",
		                    NM_BT_CAPABILITY_NONE, G_MAXUINT, NM_BT_CAPABILITY_NONE,
		                    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	/* Signals */
	signals[PPP_STATS] =
		g_signal_new ("ppp-stats",
		              G_OBJECT_CLASS_TYPE (object_class),
		              G_SIGNAL_RUN_FIRST,
		              G_STRUCT_OFFSET (NMDeviceBtClass, ppp_stats),
		              NULL, NULL,
		              _nm_marshal_VOID__UINT_UINT,
		              G_TYPE_NONE, 2,
		              G_TYPE_UINT, G_TYPE_UINT);

	signals[PROPERTIES_CHANGED] =
		nm_properties_changed_signal_new (object_class,
		                                  G_STRUCT_OFFSET (NMDeviceBtClass, properties_changed));

	dbus_g_object_type_install_info (G_TYPE_FROM_CLASS (klass),
	                                 &dbus_glib_nm_device_bt_object_info);

	dbus_g_error_domain_register (NM_BT_ERROR, NULL, NM_TYPE_BT_ERROR);
}
