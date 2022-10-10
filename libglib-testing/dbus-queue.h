/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright © 2018 Endless Mobile, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * Authors:
 *  - Philip Withnall <withnall@endlessm.com>
 */

#pragma once

#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _GtDBusQueue GtDBusQueue;

GtDBusQueue *gt_dbus_queue_new  (void);
void         gt_dbus_queue_free (GtDBusQueue *self);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtDBusQueue, gt_dbus_queue_free)

GDBusConnection *gt_dbus_queue_get_client_connection (GtDBusQueue *self);

gboolean gt_dbus_queue_connect         (GtDBusQueue         *self,
                                        GError             **error);
void     gt_dbus_queue_disconnect      (GtDBusQueue         *self,
                                        gboolean             assert_queue_empty);

guint    gt_dbus_queue_own_name        (GtDBusQueue         *self,
                                        const gchar         *name);
void     gt_dbus_queue_unown_name      (GtDBusQueue         *self,
                                        guint                id);

guint    gt_dbus_queue_export_object   (GtDBusQueue         *self,
                                        const gchar         *object_path,
                                        GDBusInterfaceInfo  *interface_info,
                                        GError             **error);
void     gt_dbus_queue_unexport_object (GtDBusQueue         *self,
                                        guint                id);

/**
 * GtDBusQueueServerFunc:
 * @queue: a #GtDBusQueue
 * @user_data: user data passed to gt_dbus_queue_set_server_func()
 *
 * Function called in the server thread to handle incoming method calls. See
 * gt_dbus_queue_set_server_func() for details.
 *
 * Since: 0.1.0
 */
typedef void (*GtDBusQueueServerFunc) (GtDBusQueue *queue,
                                       gpointer     user_data);

void     gt_dbus_queue_set_server_func (GtDBusQueue           *self,
                                        GtDBusQueueServerFunc  func,
                                        gpointer               user_data);

gsize    gt_dbus_queue_get_n_messages   (GtDBusQueue            *self);
gboolean gt_dbus_queue_try_pop_message  (GtDBusQueue            *self,
                                         GDBusMethodInvocation **out_invocation);
gboolean gt_dbus_queue_pop_message      (GtDBusQueue            *self,
                                         GDBusMethodInvocation **out_invocation);

gboolean gt_dbus_queue_match_client_message (GtDBusQueue           *self,
                                             GDBusMethodInvocation *invocation,
                                             const gchar           *expected_object_path,
                                             const gchar           *expected_interface_name,
                                             const gchar           *expected_method_name,
                                             const gchar           *expected_parameters_string);

gchar   *gt_dbus_queue_format_message       (GDBusMethodInvocation *invocation);
gchar   *gt_dbus_queue_format_messages      (GtDBusQueue           *self);

/**
 * gt_dbus_queue_assert_no_messages:
 * @self: a #GtDBusQueue
 *
 * Assert that there are no messages currently in the mock service’s message
 * queue.
 *
 * If there are, an assertion fails and some debug output is printed.
 *
 * Since: 0.1.0
 */
#define gt_dbus_queue_assert_no_messages(self) \
  G_STMT_START { \
    if (gt_dbus_queue_get_n_messages (self) > 0) \
      { \
        g_autofree gchar *anm_list = gt_dbus_queue_format_messages (self); \
        g_autofree gchar *anm_message = \
            g_strdup_printf ("Expected no messages, but saw %" G_GSIZE_FORMAT ":\n%s", \
                             gt_dbus_queue_get_n_messages (self), \
                             anm_list); \
        g_assertion_message (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                             anm_message); \
      } \
  } G_STMT_END

/**
 * gt_dbus_queue_assert_pop_message:
 * @self: a #GtDBusQueue
 * @expected_object_path: object path the invocation is expected to be calling
 * @expected_interface_name: interface name the invocation is expected to be calling
 * @expected_method_name: method name the invocation is expected to be calling
 * @parameters_format: g_variant_get() format string to extract the parameters
 *    from the popped #GDBusMethodInvocation into the return locations provided
 *    in @...
 * @...: return locations for the parameter placeholders given in @parameters_format
 *
 * Assert that a message can be popped off the mock service’s message queue
 * (using gt_dbus_queue_pop_message(), which will block) and that it is a method
 * call from the #GtDBusQueue’s client connection to the mock service, calling
 * @expected_method_name on @expected_interface_name at @expected_object_path
 * (as determined using gt_dbus_queue_match_client_message() with a %NULL
 * parameters argument). The parameters in the method call will be returned in
 * the return locations given in the varargs, according to the
 * @parameters_format, using g_variant_get_va().
 *
 * If a timeout occurs when popping a message, or if the popped message doesn’t
 * match the expected object path, interface name or method name, an assertion
 * fails and some debug output is printed.
 *
 * Returns: (transfer full): the popped #GDBusMethodInvocation
 * Since: 0.1.0
 */
#define gt_dbus_queue_assert_pop_message(self, expected_object_path, expected_interface_name, expected_method_name, parameters_format, ...) \
  gt_dbus_queue_assert_pop_message_impl (self, G_LOG_DOMAIN, __FILE__, __LINE__, \
                                         G_STRFUNC, expected_object_path, \
                                         expected_interface_name, \
                                         expected_method_name, \
                                         parameters_format, __VA_ARGS__)

/* Private implementations of the assertion functions above. */

/*< private >*/
GDBusMethodInvocation *gt_dbus_queue_assert_pop_message_impl (GtDBusQueue *self,
                                                              const gchar *macro_log_domain,
                                                              const gchar *macro_file,
                                                              gint         macro_line,
                                                              const gchar *macro_function,
                                                              const gchar *object_path,
                                                              const gchar *interface_name,
                                                              const gchar *method_name,
                                                              const gchar *parameters_format,
                                                              ...);

G_END_DECLS
