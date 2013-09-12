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
 * Copyright (C) 2008 Red Hat, Inc.
 */

#ifndef NM_OBJECT_CACHE_H
#define NM_OBJECT_CACHE_H

#include <glib.h>
#include <glib-object.h>
#include "nm-object.h"

G_BEGIN_DECLS

/* Returns referenced object from the cache */
NMObject *_nm_object_cache_get (const char *path);
void _nm_object_cache_add (NMObject *object);
void _nm_object_cache_clear (NMObject *except);

G_END_DECLS

#endif /* NM_OBJECT_CACHE_H */
