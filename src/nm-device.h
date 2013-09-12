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
 * Copyright (C) 2005 - 2012 Red Hat, Inc.
 * Copyright (C) 2006 - 2008 Novell, Inc.
 */

#ifndef NM_DEVICE_H
#define NM_DEVICE_H

#include <glib-object.h>
#include <dbus/dbus-glib.h>
#include <netinet/in.h>

#include "NetworkManager.h"
#include "nm-types.h"
#include "nm-activation-request.h"
#include "nm-ip4-config.h"
#include "nm-ip6-config.h"
#include "nm-dhcp4-config.h"
#include "nm-dhcp6-config.h"
#include "nm-connection.h"
#include "nm-rfkill.h"
#include "nm-connection-provider.h"

/* Properties */
#define NM_DEVICE_UDI              "udi"
#define NM_DEVICE_IFACE            "interface"
#define NM_DEVICE_IP_IFACE         "ip-interface"
#define NM_DEVICE_DRIVER           "driver"
#define NM_DEVICE_DRIVER_VERSION   "driver-version"
#define NM_DEVICE_FIRMWARE_VERSION "firmware-version"
#define NM_DEVICE_CAPABILITIES     "capabilities"
#define NM_DEVICE_IP4_ADDRESS      "ip4-address"
#define NM_DEVICE_IP4_CONFIG       "ip4-config"
#define NM_DEVICE_DHCP4_CONFIG     "dhcp4-config"
#define NM_DEVICE_IP6_CONFIG       "ip6-config"
#define NM_DEVICE_DHCP6_CONFIG     "dhcp6-config"
#define NM_DEVICE_STATE            "state"
#define NM_DEVICE_STATE_REASON     "state-reason"
#define NM_DEVICE_ACTIVE_CONNECTION "active-connection"
#define NM_DEVICE_DEVICE_TYPE      "device-type" /* ugh */
#define NM_DEVICE_MANAGED          "managed"
#define NM_DEVICE_AUTOCONNECT      "autoconnect"
#define NM_DEVICE_FIRMWARE_MISSING "firmware-missing"
#define NM_DEVICE_TYPE_DESC        "type-desc"    /* Internal only */
#define NM_DEVICE_RFKILL_TYPE      "rfkill-type"  /* Internal only */
#define NM_DEVICE_IFINDEX          "ifindex"      /* Internal only */
#define NM_DEVICE_IS_MASTER        "is-master"    /* Internal only */
#define NM_DEVICE_AVAILABLE_CONNECTIONS "available-connections"

/* Internal signals */
#define NM_DEVICE_AUTH_REQUEST "auth-request"
#define NM_DEVICE_IP4_CONFIG_CHANGED "ip4-config-changed"
#define NM_DEVICE_IP6_CONFIG_CHANGED "ip6-config-changed"


G_BEGIN_DECLS

#define NM_TYPE_DEVICE			(nm_device_get_type ())
#define NM_DEVICE(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), NM_TYPE_DEVICE, NMDevice))
#define NM_DEVICE_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass),  NM_TYPE_DEVICE, NMDeviceClass))
#define NM_IS_DEVICE(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), NM_TYPE_DEVICE))
#define NM_IS_DEVICE_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass),  NM_TYPE_DEVICE))
#define NM_DEVICE_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj),  NM_TYPE_DEVICE, NMDeviceClass))

typedef enum NMActStageReturn NMActStageReturn;

typedef enum {
	NM_DEVICE_ERROR_CONNECTION_ACTIVATING = 0, /*< nick=ConnectionActivating >*/
	NM_DEVICE_ERROR_CONNECTION_INVALID,        /*< nick=ConnectionInvalid >*/
	NM_DEVICE_ERROR_NOT_ACTIVE,                /*< nick=NotActive >*/
} NMDeviceError;

struct _NMDevice {
	GObject parent;
};

typedef struct {
	GObjectClass parent;

	void (*state_changed) (NMDevice *device,
	                       NMDeviceState new_state,
	                       NMDeviceState old_state,
	                       NMDeviceStateReason reason);

	/* Hardware state, ie IFF_UP */
	gboolean        (*hw_is_up)      (NMDevice *self);
	gboolean        (*hw_bring_up)   (NMDevice *self, gboolean *no_firmware);
	void            (*hw_take_down)  (NMDevice *self);

	/* Additional stuff required to operate the device, like a 
	 * connection to the supplicant, Bluez, etc
	 */
	gboolean        (*is_up)         (NMDevice *self);
	gboolean        (*bring_up)      (NMDevice *self);
	void            (*take_down)     (NMDevice *self);

	void        (* update_hw_address) (NMDevice *self);
	void        (* update_permanent_hw_address) (NMDevice *self);
	void        (* update_initial_hw_address) (NMDevice *self);
	const guint8 * (* get_hw_address) (NMDevice *self, guint *out_len);

	guint32		(* get_type_capabilities)	(NMDevice *self);
	guint32		(* get_generic_capabilities)	(NMDevice *self);

	gboolean	(* is_available) (NMDevice *self);

	gboolean    (* get_enabled) (NMDevice *self);

	void        (* set_enabled) (NMDevice *self, gboolean enabled);

	NMConnection * (* get_best_auto_connection) (NMDevice *self,
	                                             GSList *connections,
	                                             char **specific_object);

	/* Checks whether the connection is compatible with the device using
	 * only the devices type and characteristics.  Does not use any live
	 * network information like WiFi/WiMAX scan lists etc.
	 */
	gboolean    (* check_connection_compatible) (NMDevice *self,
	                                             NMConnection *connection,
	                                             GError **error);

	/* Checks whether the connection is likely available to be activated,
	 * including any live network information like scan lists.  Returns
	 * TRUE if the connection is available; FALSE if not.
	 */
	gboolean    (* check_connection_available) (NMDevice *self,
	                                            NMConnection *connection);

	gboolean    (* complete_connection)         (NMDevice *self,
	                                             NMConnection *connection,
	                                             const char *specific_object,
	                                             const GSList *existing_connections,
	                                             GError **error);

	NMActStageReturn	(* act_stage1_prepare)	(NMDevice *self,
	                                             NMDeviceStateReason *reason);
	NMActStageReturn	(* act_stage2_config)	(NMDevice *self,
	                                             NMDeviceStateReason *reason);
	NMActStageReturn	(* act_stage3_ip4_config_start) (NMDevice *self,
														 NMIP4Config **out_config,
														 NMDeviceStateReason *reason);
	NMActStageReturn	(* act_stage3_ip6_config_start) (NMDevice *self,
														 NMIP6Config **out_config,
														 NMDeviceStateReason *reason);
	NMActStageReturn	(* act_stage4_ip4_config_timeout)	(NMDevice *self,
	                                                         NMDeviceStateReason *reason);
	NMActStageReturn	(* act_stage4_ip6_config_timeout)	(NMDevice *self,
	                                                         NMDeviceStateReason *reason);

	/* Called right before IP config is set; use for setting MTU etc */
	void                (* ip4_config_pre_commit) (NMDevice *self, NMIP4Config *config);
	void                (* ip6_config_pre_commit) (NMDevice *self, NMIP6Config *config);

	void			(* deactivate)			(NMDevice *self);

	gboolean		(* can_interrupt_activation)		(NMDevice *self);

	gboolean        (* spec_match_list)     (NMDevice *self, const GSList *specs);

	NMConnection *  (* connection_match_config) (NMDevice *self, const GSList *connections);

	gboolean        (* hwaddr_matches) (NMDevice *self,
	                                    NMConnection *connection,
	                                    const guint8 *other_hwaddr,
	                                    guint other_hwaddr_len,
	                                    gboolean fail_if_no_hwaddr);

	gboolean        (* enslave_slave) (NMDevice *self,
	                                   NMDevice *slave,
	                                   NMConnection *connection);

	gboolean        (* release_slave) (NMDevice *self,
	                                   NMDevice *slave);

	gboolean        (* have_any_ready_slaves) (NMDevice *self,
	                                           const GSList *slaves);
} NMDeviceClass;


typedef void (*NMDeviceAuthRequestFunc) (NMDevice *device,
                                         DBusGMethodInvocation *context,
                                         GError *error,
                                         gpointer user_data);

GType nm_device_get_type (void);

const char *    nm_device_get_path (NMDevice *dev);
void            nm_device_set_path (NMDevice *dev, const char *path);

const char *	nm_device_get_udi		(NMDevice *dev);
const char *	nm_device_get_iface		(NMDevice *dev);
int             nm_device_get_ifindex	(NMDevice *dev);
const char *	nm_device_get_ip_iface	(NMDevice *dev);
int             nm_device_get_ip_ifindex(NMDevice *dev);
const char *	nm_device_get_driver	(NMDevice *dev);
const char *	nm_device_get_driver_version	(NMDevice *dev);
const char *	nm_device_get_firmware_version	(NMDevice *dev);
const char *	nm_device_get_type_desc (NMDevice *dev);
NMDeviceType	nm_device_get_device_type	(NMDevice *dev);

int			nm_device_get_priority (NMDevice *dev);

const guint8 *  nm_device_get_hw_address (NMDevice *dev, guint *out_len);

NMDHCP4Config * nm_device_get_dhcp4_config (NMDevice *dev);
NMDHCP6Config * nm_device_get_dhcp6_config (NMDevice *dev);

NMIP4Config *	nm_device_get_ip4_config	(NMDevice *dev);
NMIP6Config *	nm_device_get_ip6_config	(NMDevice *dev);

/* Master */
gboolean        nm_device_master_add_slave  (NMDevice *dev, NMDevice *slave);
GSList *        nm_device_master_get_slaves (NMDevice *dev);
gboolean        nm_device_is_master         (NMDevice *dev);

/* Slave */
void            nm_device_slave_notify_enslaved (NMDevice *dev,
                                                 gboolean enslaved,
                                                 gboolean master_failed);

NMActRequest *	nm_device_get_act_request	(NMDevice *dev);
NMConnection *  nm_device_get_connection	(NMDevice *dev);

gboolean		nm_device_is_available (NMDevice *dev);

NMConnection * nm_device_get_best_auto_connection (NMDevice *dev,
                                                   GSList *connections,
                                                   char **specific_object);

gboolean nm_device_complete_connection (NMDevice *device,
                                        NMConnection *connection,
                                        const char *specific_object,
                                        const GSList *existing_connection,
                                        GError **error);

gboolean nm_device_check_connection_compatible (NMDevice *device,
                                                NMConnection *connection,
                                                GError **error);

gboolean nm_device_can_assume_connections (NMDevice *device);

NMConnection * nm_device_connection_match_config (NMDevice *device,
                                                  const GSList *connections);

gboolean nm_device_hwaddr_matches (NMDevice *device,
                                   NMConnection *connection,
                                   const guint8 *other_hwaddr,
                                   guint other_hwaddr_len,
                                   gboolean fail_if_no_hwaddr);

gboolean nm_device_spec_match_list (NMDevice *device, const GSList *specs);

gboolean		nm_device_is_activating		(NMDevice *dev);
gboolean		nm_device_can_interrupt_activation		(NMDevice *self);
gboolean		nm_device_autoconnect_allowed	(NMDevice *self);

NMDeviceState nm_device_get_state (NMDevice *device);

gboolean nm_device_get_enabled (NMDevice *device);

void nm_device_set_enabled (NMDevice *device, gboolean enabled);

RfKillType nm_device_get_rfkill_type (NMDevice *device);

gboolean nm_device_get_managed (NMDevice *device);
void nm_device_set_managed (NMDevice *device,
                            gboolean managed,
                            NMDeviceStateReason reason);

gboolean nm_device_get_autoconnect (NMDevice *device);

void nm_device_handle_autoip4_event (NMDevice *self,
                                     const char *event,
                                     const char *address);

void nm_device_state_changed (NMDevice *device,
                              NMDeviceState state,
                              NMDeviceStateReason reason);

void nm_device_queue_state   (NMDevice *self,
                              NMDeviceState state,
                              NMDeviceStateReason reason);

gboolean nm_device_get_firmware_missing (NMDevice *self);

void nm_device_activate (NMDevice *device, NMActRequest *req);

void nm_device_set_connection_provider (NMDevice *device, NMConnectionProvider *provider);

gboolean nm_device_supports_vlans (NMDevice *device);

G_END_DECLS

#endif	/* NM_DEVICE_H */
