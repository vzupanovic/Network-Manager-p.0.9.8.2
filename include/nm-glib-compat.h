/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* NetworkManager -- Network link manager
 *
 * Dan Williams <dcbw@redhat.com>
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
 * (C) Copyright 2008 - 2011 Red Hat, Inc.
 */

#ifndef NM_GLIB_COMPAT_H
#define NM_GLIB_COMPAT_H


#include <glib.h>
#include <glib-object.h>

#if !GLIB_CHECK_VERSION(2,31,0)
#define g_value_set_schar g_value_set_char
#define g_value_get_schar g_value_get_char
#endif

#if !GLIB_CHECK_VERSION(2,30,0)
#define G_VALUE_INIT  { 0, { { 0 } } }
#endif

#if !GLIB_CHECK_VERSION(2,28,0)
#define g_simple_async_result_take_error(result, error) \
	G_STMT_START { \
		GError *__error = error; \
		g_simple_async_result_set_from_error (result, __error); \
		g_error_free (__error); \
	} G_STMT_END

#define g_clear_object(object_ptr) \
	G_STMT_START { \
		GObject **__obj_p = (gpointer) (object_ptr); \
		if (*__obj_p) { \
			g_object_unref (*__obj_p); \
			*__obj_p = NULL; \
		} \
	} G_STMT_END

#endif

#ifndef G_DEFINE_BOXED_TYPE
#define G_DEFINE_BOXED_TYPE(t,p,d,f) \
GType \
p##_get_type (void) \
{ \
    static volatile gsize g_define_type_id__volatile = 0; \
 \
    if (g_once_init_enter (&g_define_type_id__volatile)) { \
        GType g_define_type_id = \
            g_boxed_type_register_static( \
                g_intern_static_string(#t), \
                (GBoxedCopyFunc) d, \
                (GBoxedFreeFunc) f); \
        g_once_init_leave (&g_define_type_id__volatile, g_define_type_id); \
    } \
    return g_define_type_id__volatile; \
}
#endif

#if !GLIB_CHECK_VERSION(2,34,0)
static inline void
g_type_ensure (GType type)
{
  if (G_UNLIKELY (type == (GType)-1))
    g_error ("can't happen");
}
#endif

#endif  /* NM_GLIB_COMPAT_H */
