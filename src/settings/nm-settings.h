/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager system settings service
 *
 * Søren Sandmann <sandmann@daimi.au.dk>
 * Dan Williams <dcbw@redhat.com>
 * Tambet Ingo <tambet@gmail.com>
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
 * (C) Copyright 2007 - 2011 Red Hat, Inc.
 * (C) Copyright 2008 Novell, Inc.
 */

#ifndef __NM_SETTINGS_H__
#define __NM_SETTINGS_H__

#include <nm-connection.h>

#include "nm-settings-connection.h"
#include "nm-system-config-interface.h"
#include "nm-device.h"
#include "nm-secret-agent.h"

#define NM_TYPE_SETTINGS            (nm_settings_get_type ())
#define NM_SETTINGS(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NM_TYPE_SETTINGS, NMSettings))
#define NM_SETTINGS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass),  NM_TYPE_SETTINGS, NMSettingsClass))
#define NM_IS_SETTINGS(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NM_TYPE_SETTINGS))
#define NM_IS_SETTINGS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass),  NM_TYPE_SETTINGS))
#define NM_SETTINGS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj),  NM_TYPE_SETTINGS, NMSettingsClass))

#define NM_SETTINGS_UNMANAGED_SPECS "unmanaged-specs"
#define NM_SETTINGS_HOSTNAME        "hostname"
#define NM_SETTINGS_CAN_MODIFY      "can-modify"

#define NM_SETTINGS_SIGNAL_CONNECTION_ADDED              "connection-added"
#define NM_SETTINGS_SIGNAL_CONNECTION_UPDATED            "connection-updated"
#define NM_SETTINGS_SIGNAL_CONNECTION_REMOVED            "connection-removed"
#define NM_SETTINGS_SIGNAL_CONNECTION_VISIBILITY_CHANGED "connection-visibility-changed"
#define NM_SETTINGS_SIGNAL_CONNECTIONS_LOADED            "connections-loaded"
#define NM_SETTINGS_SIGNAL_AGENT_REGISTERED              "agent-registered"

typedef struct {
	GObject parent_instance;
} NMSettings;

typedef struct {
	GObjectClass parent_class;

	/* Signals */
	void (*properties_changed) (NMSettings *self, GHashTable *properties);

	void (*connection_added)   (NMSettings *self, NMSettingsConnection *connection);

	void (*connection_updated) (NMSettings *self, NMSettingsConnection *connection);

	void (*connection_removed) (NMSettings *self, NMSettingsConnection *connection);

	void (*connection_visibility_changed) (NMSettings *self, NMSettingsConnection *connection);

	void (*connections_loaded) (NMSettings *self);

	void (*agent_registered) (NMSettings *self, NMSecretAgent *agent);
} NMSettingsClass;

GType nm_settings_get_type (void);

NMSettings *nm_settings_new (const char *config_file,
                             const char **plugins,
                             GError **error);

typedef void (*NMSettingsForEachFunc) (NMSettings *settings,
                                       NMSettingsConnection *connection,
                                       gpointer user_data);

void nm_settings_for_each_connection (NMSettings *settings,
                                      NMSettingsForEachFunc for_each_func,
                                      gpointer user_data);

typedef void (*NMSettingsAddCallback) (NMSettings *settings,
                                       NMSettingsConnection *connection,
                                       GError *error,
                                       DBusGMethodInvocation *context,
                                       gpointer user_data);

void nm_settings_add_connection (NMSettings *self,
                                 NMConnection *connection,
                                 DBusGMethodInvocation *context,
                                 NMSettingsAddCallback callback,
                                 gpointer user_data);

/* Returns a list of NMSettingsConnections.  Caller must free the list with
 * g_slist_free().
 */
GSList *nm_settings_get_connections (NMSettings *settings);

NMSettingsConnection *nm_settings_get_connection_by_path (NMSettings *settings,
                                                          const char *path);

NMSettingsConnection *nm_settings_get_connection_by_uuid (NMSettings *settings,
                                                          const char *uuid);

const GSList *nm_settings_get_unmanaged_specs (NMSettings *self);

char *nm_settings_get_hostname (NMSettings *self);

void nm_settings_device_added (NMSettings *self, NMDevice *device);

void nm_settings_device_removed (NMSettings *self, NMDevice *device);

#endif  /* __NM_SETTINGS_H__ */
