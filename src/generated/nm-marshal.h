
#ifndef ___nm_marshal_MARSHAL_H__
#define ___nm_marshal_MARSHAL_H__

#include	<glib-object.h>

G_BEGIN_DECLS

/* BOOLEAN:VOID (nm-marshal.list:1) */
extern void _nm_marshal_BOOLEAN__VOID (GClosure     *closure,
                                       GValue       *return_value,
                                       guint         n_param_values,
                                       const GValue *param_values,
                                       gpointer      invocation_hint,
                                       gpointer      marshal_data);

/* VOID:BOOLEAN,UINT (nm-marshal.list:2) */
extern void _nm_marshal_VOID__BOOLEAN_UINT (GClosure     *closure,
                                            GValue       *return_value,
                                            guint         n_param_values,
                                            const GValue *param_values,
                                            gpointer      invocation_hint,
                                            gpointer      marshal_data);

/* VOID:INT,UINT,BOOLEAN (nm-marshal.list:3) */
extern void _nm_marshal_VOID__INT_UINT_BOOLEAN (GClosure     *closure,
                                                GValue       *return_value,
                                                guint         n_param_values,
                                                const GValue *param_values,
                                                gpointer      invocation_hint,
                                                gpointer      marshal_data);

/* VOID:OBJECT,OBJECT (nm-marshal.list:4) */
extern void _nm_marshal_VOID__OBJECT_OBJECT (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);

/* VOID:OBJECT,POINTER (nm-marshal.list:5) */
extern void _nm_marshal_VOID__OBJECT_POINTER (GClosure     *closure,
                                              GValue       *return_value,
                                              guint         n_param_values,
                                              const GValue *param_values,
                                              gpointer      invocation_hint,
                                              gpointer      marshal_data);

/* VOID:OBJECT,STRING (nm-marshal.list:6) */
extern void _nm_marshal_VOID__OBJECT_STRING (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);

/* VOID:POINTER (nm-marshal.list:7) */
#define _nm_marshal_VOID__POINTER	g_cclosure_marshal_VOID__POINTER

/* VOID:POINTER,POINTER,POINTER,POINTER,INT (nm-marshal.list:8) */
extern void _nm_marshal_VOID__POINTER_POINTER_POINTER_POINTER_INT (GClosure     *closure,
                                                                   GValue       *return_value,
                                                                   guint         n_param_values,
                                                                   const GValue *param_values,
                                                                   gpointer      invocation_hint,
                                                                   gpointer      marshal_data);

/* VOID:POINTER,STRING (nm-marshal.list:9) */
extern void _nm_marshal_VOID__POINTER_STRING (GClosure     *closure,
                                              GValue       *return_value,
                                              guint         n_param_values,
                                              const GValue *param_values,
                                              gpointer      invocation_hint,
                                              gpointer      marshal_data);

/* VOID:POINTER,STRING,BOOLEAN,POINTER,POINTER (nm-marshal.list:10) */
extern void _nm_marshal_VOID__POINTER_STRING_BOOLEAN_POINTER_POINTER (GClosure     *closure,
                                                                      GValue       *return_value,
                                                                      guint         n_param_values,
                                                                      const GValue *param_values,
                                                                      gpointer      invocation_hint,
                                                                      gpointer      marshal_data);

/* VOID:STRING,BOXED (nm-marshal.list:11) */
extern void _nm_marshal_VOID__STRING_BOXED (GClosure     *closure,
                                            GValue       *return_value,
                                            guint         n_param_values,
                                            const GValue *param_values,
                                            gpointer      invocation_hint,
                                            gpointer      marshal_data);

/* VOID:STRING,BOXED,BOXED (nm-marshal.list:12) */
extern void _nm_marshal_VOID__STRING_BOXED_BOXED (GClosure     *closure,
                                                  GValue       *return_value,
                                                  guint         n_param_values,
                                                  const GValue *param_values,
                                                  gpointer      invocation_hint,
                                                  gpointer      marshal_data);

/* VOID:STRING,OBJECT (nm-marshal.list:13) */
extern void _nm_marshal_VOID__STRING_OBJECT (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);

/* VOID:STRING,POINTER (nm-marshal.list:14) */
extern void _nm_marshal_VOID__STRING_POINTER (GClosure     *closure,
                                              GValue       *return_value,
                                              guint         n_param_values,
                                              const GValue *param_values,
                                              gpointer      invocation_hint,
                                              gpointer      marshal_data);

/* VOID:STRING,STRING (nm-marshal.list:15) */
extern void _nm_marshal_VOID__STRING_STRING (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);

/* VOID:STRING,STRING,STRING (nm-marshal.list:16) */
extern void _nm_marshal_VOID__STRING_STRING_STRING (GClosure     *closure,
                                                    GValue       *return_value,
                                                    guint         n_param_values,
                                                    const GValue *param_values,
                                                    gpointer      invocation_hint,
                                                    gpointer      marshal_data);

/* VOID:STRING,STRING,STRING,UINT (nm-marshal.list:17) */
extern void _nm_marshal_VOID__STRING_STRING_STRING_UINT (GClosure     *closure,
                                                         GValue       *return_value,
                                                         guint         n_param_values,
                                                         const GValue *param_values,
                                                         gpointer      invocation_hint,
                                                         gpointer      marshal_data);

/* VOID:UINT,UINT (nm-marshal.list:18) */
extern void _nm_marshal_VOID__UINT_UINT (GClosure     *closure,
                                         GValue       *return_value,
                                         guint         n_param_values,
                                         const GValue *param_values,
                                         gpointer      invocation_hint,
                                         gpointer      marshal_data);

/* VOID:UINT,UINT,INT (nm-marshal.list:19) */
extern void _nm_marshal_VOID__UINT_UINT_INT (GClosure     *closure,
                                             GValue       *return_value,
                                             guint         n_param_values,
                                             const GValue *param_values,
                                             gpointer      invocation_hint,
                                             gpointer      marshal_data);

/* VOID:UINT,UINT,UINT (nm-marshal.list:20) */
extern void _nm_marshal_VOID__UINT_UINT_UINT (GClosure     *closure,
                                              GValue       *return_value,
                                              guint         n_param_values,
                                              const GValue *param_values,
                                              gpointer      invocation_hint,
                                              gpointer      marshal_data);

G_END_DECLS

#endif /* ___nm_marshal_MARSHAL_H__ */

