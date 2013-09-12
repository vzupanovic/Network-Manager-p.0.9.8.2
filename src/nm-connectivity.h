
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
 * Copyright (C) 2011 Thomas Bechtold <thomasbechtold@jpberlin.de>
 */

#ifndef NM_CONNECTIVITY_H
#define NM_CONNECTIVITY_H

#include <glib.h>
#include <glib-object.h>

#include "NetworkManager.h"

#define NM_TYPE_CONNECTIVITY            (nm_connectivity_get_type ())
#define NM_CONNECTIVITY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), NM_TYPE_CONNECTIVITY, NMConnectivity))
#define NM_CONNECTIVITY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), NM_TYPE_CONNECTIVITY, NMConnectivityClass))
#define NM_IS_CONNECTIVITY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), NM_TYPE_CONNECTIVITY))
#define NM_IS_CONNECTIVITY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), NM_TYPE_CONNECTIVITY))
#define NM_CONNECTIVITY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), NM_TYPE_CONNECTIVITY, NMConnectivityClass))

/* Properties */
#define NM_CONNECTIVITY_RUNNING   "running"
#define NM_CONNECTIVITY_URI       "uri"
#define NM_CONNECTIVITY_INTERVAL  "interval"
#define NM_CONNECTIVITY_RESPONSE  "response"
#define NM_CONNECTIVITY_CONNECTED "connected"


typedef struct {
	GObject parent;
} NMConnectivity;

typedef struct {
	GObjectClass parent;
} NMConnectivityClass;

GType nm_connectivity_get_type (void);


NMConnectivity *nm_connectivity_new           (const gchar *check_uri,
                                               guint check_interval,
                                               const gchar *check_response);

void            nm_connectivity_start_check   (NMConnectivity *connectivity);

void            nm_connectivity_stop_check    (NMConnectivity *connectivity);

gboolean        nm_connectivity_get_connected (NMConnectivity *connectivity);

#endif /* NM_CONNECTIVITY_H */
