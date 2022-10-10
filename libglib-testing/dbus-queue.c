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

#include "config.h"

#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>
#include <libglib-testing/dbus-queue.h>


/**
 * SECTION:dbus-queue
 * @short_description: D-Bus service mock implementation
 * @stability: Unstable
 * @include: libglib-testing/dbus-queue.h
 *
 * #GtDBusQueue is an object which allows a D-Bus service to be mocked and
 * implemented to return a variety of results from method calls with a high
 * degree of flexibility. The mock service is driven from within the same
 * process (and potentially the same #GMainContext) as the code under test.
 *
 * This allows a D-Bus service to be mocked without needing to generate and
 * implement a full implementation of its interfaces.
 *
 * A single #GtDBusQueue instance can be used to mock one or more D-Bus
 * services, depending on whether it’s desirable to process the queues of method
 * calls to those services in order or independently from each other. Each
 * #GtDBusQueue has a queue of method calls received by the services it is
 * mocking, which are ordered the same as they were received off the bus. It is
 * intended that the test harness which is using #GtDBusQueue should pop
 * messages off the queue, and either check they are as expected and return a
 * static reply, or construct a reply dynamically based on their contents.
 *
 * Messages can be popped off the queue using
 * gt_dbus_queue_assert_pop_message(), gt_dbus_queue_pop_message() or
 * gt_dbus_queue_try_pop_message(). The former two block until a message can be
 * popped, iterating the thread-default #GMainContext while they block. The
 * latter returns %FALSE immediately if the queue is empty.
 *
 * Popping and handling messages is typically done in the #GtDBusQueue server
 * thread using gt_dbus_queue_set_server_func(). This will work whether the code
 * under test is synchronous or asynchronous. If the code under test is
 * asynchronous, popping and handling messages can instead be done in the main
 * test thread, but this has no particular advantages.
 *
 * By default, a #GtDBusQueue will not assert that its message queue is empty
 * on destruction unless the `assert_queue_empty` argument is passed to
 * gt_dbus_queue_disconnect(). If that argument is %FALSE, it is highly
 * recommended that gt_dbus_queue_assert_no_messages() is called before a
 * #GtDBusQueue is destroyed, or after a particular unit test is completed.
 *
 * Conversely, a #GtDBusQueue will not ensure that the thread default
 * #GMainContext for the thread where it’s constructed is empty when the
 * #GtDBusQueue is finalised. That is the responsibility of the caller who
 * constructed the #GMainContext.
 *
 * ## Usage Example with GLib Testing Framework
 *
 * |[<!-- language="C" -->
 * typedef struct
 * {
 *   GtDBusQueue *queue;  // (owned)
 *   uid_t valid_uid;
 * } BusFixture;
 *
 * static void
 * bus_set_up (BusFixture    *fixture,
 *             gconstpointer  test_data)
 * {
 *   g_autoptr(GError) local_error = NULL;
 *   g_autofree gchar *object_path = NULL;
 *
 *   fixture->valid_uid = 500;  // arbitrarily chosen
 *   fixture->queue = gt_dbus_queue_new ();
 *
 *   gt_dbus_queue_connect (fixture->queue, &local_error);
 *   g_assert_no_error (local_error);
 *
 *   gt_dbus_queue_own_name (fixture->queue, "org.freedesktop.Accounts");
 *
 *   object_path = g_strdup_printf ("/org/freedesktop/Accounts/User%u", fixture->valid_uid);
 *   gt_dbus_queue_export_object (fixture->queue,
 *                                object_path,
 *                                (GDBusInterfaceInfo *) &app_filter_interface_info,
 *                                &local_error);
 *   g_assert_no_error (local_error);
 *
 *   gt_dbus_queue_export_object (fixture->queue,
 *                                "/org/freedesktop/Accounts",
 *                                (GDBusInterfaceInfo *) &accounts_interface_info,
 *                                &local_error);
 *   g_assert_no_error (local_error);
 * }
 *
 * static void
 * bus_tear_down (BusFixture    *fixture,
 *                gconstpointer  test_data)
 * {
 *   gt_dbus_queue_disconnect (fixture->queue, TRUE);
 *   g_clear_pointer (&fixture->queue, gt_dbus_queue_free);
 * }
 *
 * // Helper #GAsyncReadyCallback which returns the #GAsyncResult in its @user_data.
 * static void
 * async_result_cb (GObject      *obj,
 *                  GAsyncResult *result,
 *                  gpointer      user_data)
 * {
 *   GAsyncResult **result_out = (GAsyncResult **) user_data;
 *
 *   g_assert_null (*result_out);
 *   *result_out = g_object_ref (result);
 * }
 *
 * // Test that getting an #EpcAppFilter from the mock D-Bus service works. The
 * // @test_data is a boolean value indicating whether to do the call
 * // synchronously (%FALSE) or asynchronously (%TRUE).
 * //
 * // The mock D-Bus replies are generated in get_app_filter_server_cb(), which is
 * // used for both synchronous and asynchronous calls.
 * static void get_app_filter_server_cb (GtDBusQueue *queue,
 *                                       gpointer     user_data);
 *
 * static void
 * test_app_filter_bus_get (BusFixture    *fixture,
 *                          gconstpointer  test_data)
 * {
 *   g_autoptr(EpcAppFilter) app_filter = NULL;
 *   g_autoptr(GError) local_error = NULL;
 *   gboolean test_async = GPOINTER_TO_UINT (test_data);
 *
 *   gt_dbus_queue_set_server_func (fixture->queue, get_app_filter_server_cb, fixture);
 *
 *   if (test_async)
 *     {
 *       g_autoptr(GAsyncResult) result = NULL;
 *
 *       epc_get_app_filter_async (gt_dbus_queue_get_client_connection (fixture->queue),
 *                                 fixture->valid_uid,
 *                                 FALSE, NULL, async_result_cb, &result);
 *
 *       while (result == NULL)
 *         g_main_context_iteration (NULL, TRUE);
 *       app_filter = epc_get_app_filter_finish (result, &local_error);
 *     }
 *   else
 *     {
 *       app_filter = epc_get_app_filter (gt_dbus_queue_get_client_connection (fixture->queue),
 *                                        fixture->valid_uid,
 *                                        FALSE, NULL, &local_error);
 *     }
 *
 *   g_assert_no_error (local_error);
 *   g_assert_nonnull (app_filter);
 *
 *   // Check the app filter properties.
 *   g_assert_cmpuint (epc_app_filter_get_user_id (app_filter), ==, fixture->valid_uid);
 *   g_assert_false (epc_app_filter_is_flatpak_app_allowed (app_filter, "org.gnome.Builder"));
 *   g_assert_true (epc_app_filter_is_flatpak_app_allowed (app_filter, "org.gnome.Chess"));
 * }
 *
 * // This is run in a worker thread.
 * static void
 * get_app_filter_server_cb (GtDBusQueue *queue,
 *                           gpointer     user_data)
 * {
 *   BusFixture *fixture = user_data;
 *   g_autoptr(GDBusMethodInvocation) invocation1 = NULL;
 *   g_autoptr(GDBusMethodInvocation) invocation2 = NULL;
 *   g_autofree gchar *object_path = NULL;
 *   g_autofree gchar *reply1 = NULL;
 *
 *   // Handle the FindUserById() call.
 *   gint64 user_id;
 *   invocation1 =
 *       gt_dbus_queue_assert_pop_message (queue,
 *                                         "/org/freedesktop/Accounts",
 *                                         "org.freedesktop.Accounts",
 *                                         "FindUserById", "(x)", &user_id);
 *   g_assert_cmpint (user_id, ==, fixture->valid_uid);
 *
 *   object_path = g_strdup_printf ("/org/freedesktop/Accounts/User%u", (uid_t) user_id);
 *   reply1 = g_strdup_printf ("(@o '%s',)", object_path);
 *   g_dbus_method_invocation_return_value (invocation1, g_variant_new_parsed (reply1));
 *
 *   // Handle the Properties.GetAll() call and return some arbitrary, valid values
 *   // for the given user.
 *   const gchar *property_interface;
 *   invocation2 =
 *       gt_dbus_queue_assert_pop_message (queue,
 *                                         object_path,
 *                                         "org.freedesktop.DBus.Properties",
 *                                         "GetAll", "(&s)", &property_interface);
 *   g_assert_cmpstr (property_interface, ==, "com.endlessm.ParentalControls.AppFilter");
 *
 *   const gchar *reply2 =
 *     "({"
 *       "'allow-user-installation': <true>,"
 *       "'allow-system-installation': <false>,"
 *       "'app-filter': <(false, ['app/org.gnome.Builder/x86_64/stable'])>,"
 *       "'oars-filter': <('oars-1.1', { 'violence-bloodshed': 'mild' })>"
 *     "},)";
 *   g_dbus_method_invocation_return_value (invocation2, g_variant_new_parsed (reply2));
 * }
 * ]|
 *
 * Since: 0.1.0
 */

/**
 * GtDBusQueue:
 *
 * An object which allows a D-Bus service to be mocked and implemented to return
 * a variety of results from method calls with a high degree of flexibility. The
 * mock service is driven from within the same process (and potentially the same
 * #GMainContext) as the code under test.
 *
 * This allows a D-Bus service to be mocked without needing to generate and
 * implement a full implementation of its interfaces.
 *
 * Since: 0.1.0
 */
struct _GtDBusQueue
{
  GTestDBus *bus;  /* (owned) */

  GThread *server_thread;  /* (owned) */
  guint server_filter_id;
  GtDBusQueueServerFunc server_func;  /* (nullable) (atomic) */
  gpointer server_func_data;  /* (unowned) (nullable) (atomic) */
  gboolean quitting;  /* (atomic) */

  GMainContext *server_context;  /* (owned) */
  GDBusConnection *server_connection;  /* (owned) */

  GMutex lock;  /* (owned) */
  GArray *name_ids;  /* (owned) (element-type guint) (locked-by lock) */
  GArray *object_ids;  /* (owned) (element-type guint) (locked-by lock) */

  GAsyncQueue *server_message_queue;  /* (owned) (element-type GDBusMethodInvocation) */

  GMainContext *client_context;  /* (owned) */
  GDBusConnection *client_connection;  /* (owned) */
};

static gpointer gt_dbus_queue_server_thread_cb (gpointer user_data);
static void gt_dbus_queue_method_call (GDBusConnection       *connection,
                                       const gchar           *sender,
                                       const gchar           *object_path,
                                       const gchar           *interface_name,
                                       const gchar           *method_name,
                                       GVariant              *parameters,
                                       GDBusMethodInvocation *invocation,
                                       gpointer               user_data);

static const GDBusInterfaceVTable gt_dbus_queue_vtable =
{
  .method_call = gt_dbus_queue_method_call,
  .get_property = NULL,  /* handled manually */
  .set_property = NULL,  /* handled manually */
};

/**
 * gt_dbus_queue_new:
 *
 * Create a new #GtDBusQueue. Start it using gt_dbus_queue_connect(), own a name
 * using gt_dbus_queue_own_name() and register objects using
 * gt_dbus_queue_export_object(). Start a particular test run using
 * gt_dbus_queue_set_server_func().
 *
 * Returns: (transfer full): a new #GtDBusQueue
 * Since: 0.1.0
 */
GtDBusQueue *
gt_dbus_queue_new (void)
{
  g_autoptr(GtDBusQueue) queue = g_new0 (GtDBusQueue, 1);

  queue->server_context = g_main_context_new ();
  queue->client_context = g_main_context_ref_thread_default ();

  queue->bus = g_test_dbus_new (G_TEST_DBUS_NONE);
  queue->server_message_queue = g_async_queue_new_full ((GDestroyNotify) g_object_unref);
  queue->name_ids = g_array_new (FALSE, FALSE, sizeof (guint));
  queue->object_ids = g_array_new (FALSE, FALSE, sizeof (guint));
  g_mutex_init (&queue->lock);

  return g_steal_pointer (&queue);
}

/**
 * gt_dbus_queue_free:
 * @self: (transfer full): a #GtDBusQueue
 *
 * Free a #GtDBusQueue. This will call gt_dbus_queue_disconnect() if it hasn’t
 * been called already, and will assert that there are no messages left in the
 * server message queue.
 *
 * If you wish to free the #GtDBusQueue regardless of whether there are messages
 * left in the server message queue, call gt_dbus_queue_disconnect() explicitly
 * before this function, and pass %FALSE as its second argument.
 *
 * Since: 0.1.0
 */
void
gt_dbus_queue_free (GtDBusQueue *self)
{
  g_return_if_fail (self != NULL);

  /* Typically we’d expect the test harness to call this explicitly, but we can
   * just as easily do it implicitly. Give them the strictest assertion
   * behaviour though. */
  if (self->server_thread != NULL)
    gt_dbus_queue_disconnect (self, TRUE);

  /* We can access @object_ids and @name_ids unlocked, since the thread has been
   * shut down. */
  if (self->object_ids != NULL)
    g_assert (self->object_ids->len == 0);
  g_clear_pointer (&self->object_ids, g_array_unref);

  if (self->name_ids != NULL)
    g_assert (self->name_ids->len == 0);
  g_clear_pointer (&self->name_ids, g_array_unref);

  if (self->server_message_queue != NULL)
    g_assert (g_async_queue_try_pop (self->server_message_queue) == NULL);
  g_clear_pointer (&self->server_message_queue, g_async_queue_unref);

  g_clear_object (&self->bus);

  /* Note: We can’t assert that the @client_context is empty because we didn’t
   * construct it. */
  if (self->server_context != NULL)
    g_assert (!g_main_context_iteration (self->server_context, FALSE));
  g_clear_pointer (&self->server_context, g_main_context_unref);

  g_mutex_clear (&self->lock);

  g_free (self);
}

/**
 * gt_dbus_queue_get_client_connection:
 * @self: a #GtDBusQueue
 *
 * Get the client #GDBusConnection which should be passed to the code under test
 * as its connection to a bus. This will be %NULL if gt_dbus_queue_connect() has
 * not been called yet, or if gt_dbus_queue_disconnect() has been called.
 *
 * Returns: (nullable) (transfer none): the client’s bus connection
 * Since: 0.1.0
 */
GDBusConnection *
gt_dbus_queue_get_client_connection (GtDBusQueue *self)
{
  g_return_val_if_fail (self != NULL, NULL);

  return self->client_connection;
}

/* Run on all messages seen (incoming or outgoing) by the server thread. Do some
 * debug output from it. We do this here, rather than in any of the message
 * handling functions, since it sees messages which the client might have
 * forgotten to export objects for.
 *
 * Called in a random message handling thread. */
static GDBusMessage *
gt_dbus_queue_server_filter_cb (GDBusConnection *connection,
                                GDBusMessage    *message,
                                gboolean         incoming,
                                gpointer         user_data)
{
  g_autofree gchar *formatted = g_dbus_message_print (message, 2);

  g_debug ("%s: Server %s Code Under Test\n%s",
           G_STRFUNC, incoming ? "←" : "→", formatted);

  /* We could add a debugging feature here where it detects incoming method
   * calls to object paths which are not exported and emits an obvious debug
   * message, since it’s probably an omission in the unit test (or a typo in an
   * existing object registration).
   *
   * On the other hand, to do so would be tricky (this function is called in a
   * random thread), would not catch similar problems with well-known name
   * ownership, and would only be useful in a small number of cases during
   * test development. So it’s left unimplemented for now. */

  return message;
}

/**
 * gt_dbus_queue_connect:
 * @self: a #GtDBusQueue
 * @error: return location for a #GError, or %NULL
 *
 * Create a private bus, mock D-Bus service, and a client #GDBusConnection to be
 * used by the code under test. Once this function has been called, the test
 * harness may call gt_dbus_queue_own_name() and gt_dbus_queue_export_object()
 * and then run the code under test.
 *
 * This must be called from the thread which constructed the #GtDBusQueue.
 *
 * Returns: %TRUE on success, %FALSE otherwise
 * Since: 0.1.0
 */
gboolean
gt_dbus_queue_connect (GtDBusQueue  *self,
                       GError      **error)
{
  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (self->server_thread == NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  g_main_context_push_thread_default (self->client_context);
  g_test_dbus_up (self->bus);

  self->client_connection =
      g_dbus_connection_new_for_address_sync (g_test_dbus_get_bus_address (self->bus),
                                              G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                              G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                              NULL,
                                              NULL,
                                              error);
  g_main_context_pop_thread_default (self->client_context);

  if (self->client_connection == NULL)
    return FALSE;

  g_main_context_push_thread_default (self->server_context);
  self->server_connection =
      g_dbus_connection_new_for_address_sync (g_test_dbus_get_bus_address (self->bus),
                                              G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                                              G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
                                              NULL,
                                              NULL,
                                              error);
  g_main_context_pop_thread_default (self->server_context);

  if (self->server_connection == NULL)
    return FALSE;

  self->server_filter_id = g_dbus_connection_add_filter (self->server_connection,
                                                         gt_dbus_queue_server_filter_cb,
                                                         NULL, NULL);

  self->server_thread = g_thread_new ("GtDBusQueue server",
                                      gt_dbus_queue_server_thread_cb,
                                      self);

  return TRUE;
}

/**
 * gt_dbus_queue_disconnect:
 * @self: a #GtDBusQueue
 * @assert_queue_empty: %TRUE to assert that the server message queue is empty
 *    before disconnecting, %FALSE to do nothing
 *
 * Disconnect the mock D-Bus service and client #GDBusConnection, and shut down
 * the private bus.
 *
 * This must be called from the thread which constructed the #GtDBusQueue.
 *
 * Since: 0.1.0
 */
void
gt_dbus_queue_disconnect (GtDBusQueue *self,
                          gboolean     assert_queue_empty)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->server_thread != NULL);

  if (assert_queue_empty)
    gt_dbus_queue_assert_no_messages (self);

  if (self->client_connection != NULL)
    g_dbus_connection_close_sync (self->client_connection, NULL, NULL);
  g_clear_object (&self->client_connection);

  g_mutex_lock (&self->lock);

  for (gsize i = 0; i < self->name_ids->len; i++)
    {
      guint id = g_array_index (self->name_ids, guint, i);
      g_bus_unown_name (id);
    }
  g_array_set_size (self->name_ids, 0);

  for (gsize i = 0; i < self->object_ids->len; i++)
    {
      guint id = g_array_index (self->object_ids, guint, i);
      g_dbus_connection_unregister_object (self->server_connection, id);
    }
  g_array_set_size (self->object_ids, 0);

  g_mutex_unlock (&self->lock);

  if (self->server_filter_id != 0)
    {
      g_dbus_connection_remove_filter (self->server_connection, self->server_filter_id);
      self->server_filter_id = 0;
    }

  if (self->server_connection != NULL)
    g_dbus_connection_close_sync (self->server_connection, NULL, NULL);
  g_clear_object (&self->server_connection);

  g_test_dbus_down (self->bus);

  /* Pack up the server thread. */
  g_atomic_int_set (&self->quitting, TRUE);
  g_main_context_wakeup (self->server_context);
  g_thread_join (g_steal_pointer (&self->server_thread));
}

typedef struct
{
  GtDBusQueue *queue;  /* (unowned) */
  const gchar *name;  /* (unowned) */
  gint id;  /* (atomic) */
} OwnNameData;

static gboolean
own_name_cb (gpointer user_data)
{
  OwnNameData *data = user_data;
  GtDBusQueue *queue = data->queue;
  guint id;

  g_assert (g_main_context_get_thread_default () == queue->server_context);

  g_debug ("%s: Owning ‘%s’", G_STRFUNC, data->name);
  id = g_bus_own_name_on_connection (queue->server_connection,
                                     data->name,
                                     G_BUS_NAME_OWNER_FLAGS_NONE,
                                     NULL, NULL, NULL, NULL);

  g_atomic_int_set (&data->id, id);

  return G_SOURCE_REMOVE;
}

/**
 * gt_dbus_queue_own_name:
 * @self: a #GtDBusQueue
 * @name: the well-known D-Bus name to own
 *
 * Make the mock D-Bus service acquire the given @name on the private bus, so
 * that code under test can address the mock service using @name. This behaves
 * similarly to g_bus_own_name().
 *
 * This may be called from any thread after gt_dbus_queue_connect() has been
 * called.
 *
 * Returns: ID for the name ownership, which may be passed to
 *    gt_dbus_queue_unown_name() to release it in future; guaranteed to be
 *    non-zero
 * Since: 0.1.0
 */
guint
gt_dbus_queue_own_name (GtDBusQueue *self,
                        const gchar *name)
{
  OwnNameData data = { NULL, };
  guint id;

  g_return_val_if_fail (self != NULL, 0);
  g_return_val_if_fail (self->server_thread != NULL, 0);
  g_return_val_if_fail (g_dbus_is_name (name) && !g_dbus_is_unique_name (name), 0);

  /* The name has to be acquired from the server thread, so invoke a callback
   * there to do that, and block on a result. No need for locking: @id is
   * accessed atomically, and the other members are not written after they’re
   * initially set. */
  data.queue = self;
  data.name = name;
  data.id = 0;

  g_main_context_invoke_full (self->server_context,
                              G_PRIORITY_DEFAULT,
                              own_name_cb,
                              &data,
                              NULL);

  while ((id = g_atomic_int_get (&data.id)) == 0);

  g_mutex_lock (&self->lock);
  g_array_append_val (self->name_ids, id);
  g_mutex_unlock (&self->lock);

  return id;
}

/**
 * gt_dbus_queue_unown_name:
 * @self: a #GtDBusQueue
 * @id: the name ID returned by gt_dbus_queue_own_name()
 *
 * Make the mock D-Bus service release a name on the private bus previously
 * acquired using gt_dbus_queue_own_name(). This behaves similarly to
 * g_bus_unown_name().
 *
 * This may be called from any thread after gt_dbus_queue_connect() has been
 * called.
 *
 * Since: 0.1.0
 */
void
gt_dbus_queue_unown_name (GtDBusQueue *self,
                          guint        id)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->server_thread != NULL);
  g_return_if_fail (id != 0);

  g_mutex_lock (&self->lock);
  for (gsize i = 0; i < self->name_ids->len; i++)
    {
      guint found_id = g_array_index (self->name_ids, guint, i);

      if (found_id == id)
        {
          g_array_remove_index_fast (self->name_ids, i);
          g_mutex_unlock (&self->lock);

          /* This is thread safe by itself. */
          g_bus_unown_name (id);

          return;
        }
    }

  g_mutex_unlock (&self->lock);

  /* @id wasn’t found. */
  g_assert_not_reached ();
}

typedef struct
{
  /* Protects everything in this struct. */
  GMutex lock;
  GCond cond;

  GtDBusQueue *queue;  /* (unowned) */

  const gchar *object_path;  /* (unowned) */
  const GDBusInterfaceInfo *interface_info;  /* (unowned) */

  guint id;
  GError *error;  /* (nullable) (owned) */
} ExportObjectData;

static gboolean
export_object_cb (gpointer user_data)
{
  ExportObjectData *data = user_data;
  GtDBusQueue *queue = data->queue;

  g_assert (g_main_context_get_thread_default () == queue->server_context);

  g_mutex_lock (&data->lock);

  g_debug ("%s: Exporting ‘%s’", G_STRFUNC, data->object_path);
  data->id = g_dbus_connection_register_object (queue->server_connection,
                                                data->object_path,
                                                data->interface_info,
                                                &gt_dbus_queue_vtable,
                                                queue,
                                                NULL,
                                                &data->error);

  g_cond_signal (&data->cond);
  g_mutex_unlock (&data->lock);

  return G_SOURCE_REMOVE;
}

/**
 * gt_dbus_queue_export_object:
 * @self: a #GtDBusQueue
 * @object_path: the path to export an object on
 * @interface_info: (transfer none): definition of the interface to export
 * @error: return location for a #GError, or %NULL
 *
 * Make the mock D-Bus service export an interface matching @interface_info at
 * the given @object_path, so that code under test can call methods at that
 * @object_path. This behaves similarly to g_dbus_connection_register_object().
 *
 * This may be called from any thread after gt_dbus_queue_connect() has been
 * called.
 *
 * Returns: ID for the exported object, which may be passed to
 *    gt_dbus_queue_unexport_object() to release it in future; guaranteed to be
 *    non-zero
 * Since: 0.1.0
 */
guint
gt_dbus_queue_export_object (GtDBusQueue         *self,
                             const gchar         *object_path,
                             GDBusInterfaceInfo  *interface_info,
                             GError             **error)
{
  ExportObjectData data = { NULL, };
  g_autoptr(GError) local_error = NULL;
  guint id;

  g_return_val_if_fail (self != NULL, 0);
  g_return_val_if_fail (self->server_thread != NULL, 0);
  g_return_val_if_fail (object_path != NULL && g_variant_is_object_path (object_path), 0);
  g_return_val_if_fail (interface_info != NULL, 0);
  g_return_val_if_fail (error == NULL || *error == NULL, 0);

  /* The object has to be exported from the server thread, so invoke a callback
   * there to do that, and block on a result. */
  g_mutex_init (&data.lock);
  g_cond_init (&data.cond);
  data.queue = self;
  data.object_path = object_path;
  data.interface_info = interface_info;
  data.id = 0;
  data.error = NULL;

  g_main_context_invoke_full (self->server_context,
                              G_PRIORITY_DEFAULT,
                              export_object_cb,
                              &data,
                              NULL);

  g_mutex_lock (&data.lock);

  while (data.id == 0 && data.error == NULL)
    g_cond_wait (&data.cond, &data.lock);

  local_error = g_steal_pointer (&data.error);
  id = data.id;

  g_mutex_unlock (&data.lock);

  if (local_error != NULL)
    {
      g_propagate_error (error, g_steal_pointer (&local_error));
      return 0;
    }

  g_assert (id != 0);

  g_mutex_lock (&self->lock);
  g_array_append_val (self->object_ids, id);
  g_mutex_unlock (&self->lock);

  return id;
}

/**
 * gt_dbus_queue_unexport_object:
 * @self: a #GtDBusQueue
 * @id: the name ID returned by gt_dbus_queue_export_object()
 *
 * Make the mock D-Bus service unexport an object on the private bus previously
 * exported using gt_dbus_queue_export_object(). This behaves similarly to
 * g_dbus_connection_unregister_object().
 *
 * This may be called from any thread after gt_dbus_queue_connect() has been
 * called.
 *
 * Since: 0.1.0
 */
void
gt_dbus_queue_unexport_object (GtDBusQueue *self,
                               guint        id)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->server_thread != NULL);
  g_return_if_fail (id != 0);

  g_mutex_lock (&self->lock);

  for (gsize i = 0; i < self->object_ids->len; i++)
    {
      guint found_id = g_array_index (self->object_ids, guint, i);

      if (found_id == id)
        {
          gboolean was_registered;

          g_array_remove_index_fast (self->object_ids, i);
          g_mutex_unlock (&self->lock);

          /* This is inherently thread safe. */
          was_registered = g_dbus_connection_unregister_object (self->server_connection,
                                                                id);
          g_assert (was_registered);

          return;
        }
    }

  g_mutex_unlock (&self->lock);

  /* @id wasn’t found. */
  g_assert_not_reached ();
}

/**
 * gt_dbus_queue_set_server_func:
 * @self: a #GtDBusQueue
 * @func: (not nullable): a #GtDBusQueueServerFunc to run in the server thread
 * @user_data: data to pass to @func
 *
 * Set a function to run in the server thread to handle incoming method calls.
 * This is a requirement when testing code which makes synchronous function
 * calls, as they will block the test thread’s main context until they return.
 * This can also be used when testing asynchronous code, which allows reuse of
 * the same mock service implementation when testing synchronous and
 * asynchronous versions of the same code under test functionality.
 *
 * @func will be executed in the server thread, so must only call thread safe
 * methods of the #GtDBusQueue, and must use thread safe access to @user_data
 * if it’s used in any other threads.
 *
 * Since: 0.1.0
 */
void
gt_dbus_queue_set_server_func (GtDBusQueue           *self,
                               GtDBusQueueServerFunc  func,
                               gpointer               user_data)
{
  gboolean swapped;

  g_return_if_fail (self != NULL);
  g_return_if_fail (func != NULL);

  /* Set the data first so it’s gated by the server func. */
  g_atomic_pointer_set (&self->server_func_data, user_data);
  swapped = g_atomic_pointer_compare_and_exchange (&self->server_func, NULL, func);

  /* This function should not be called twice. */
  g_assert (swapped);

  g_main_context_wakeup (self->server_context);
}

/* The main thread function for the server thread. This will run until
 * #GtDBusQueue.quitting is set. It will wait for a #GtDBusQueueServerFunc to
 * be set, call it, and then continue to run the #GtDBusQueue.server_context
 * until instructed to quit.
 *
 * Acquires #GtDBusQueue.server_context. */
static gpointer
gt_dbus_queue_server_thread_cb (gpointer user_data)
{
  GtDBusQueue *self = user_data;
  GtDBusQueueServerFunc server_func;
  gpointer server_func_data;

  g_main_context_push_thread_default (self->server_context);

  /* Wait for the client to provide message handling. */
  while (!g_atomic_int_get (&self->quitting) &&
         g_atomic_pointer_get (&self->server_func) == NULL)
    g_main_context_iteration (self->server_context, TRUE);

  server_func = g_atomic_pointer_get (&self->server_func);
  server_func_data = g_atomic_pointer_get (&self->server_func_data);

  if (server_func != NULL)
    server_func (self, server_func_data);

  /* Wait for the process to signal to quit. This is also the main message
   * handling loop if no @server_func has been provided. */
  while (!g_atomic_int_get (&self->quitting))
    g_main_context_iteration (self->server_context, TRUE);

  /* Process any remaining sources while quitting, without blocking. */
  while (g_main_context_iteration (self->server_context, FALSE));

  g_main_context_pop_thread_default (self->server_context);

  return NULL;
}

/* Handle an incoming method call to the mock service. This is run in the server
 * thread, under #GtDBusQueue.server_context. It pushes the received message
 * onto the server’s message queue and wakes up any #GMainContext which is
 * potentially blocking on a gt_dbus_queue_pop_message() call. */
static void
gt_dbus_queue_method_call (GDBusConnection       *connection,
                           const gchar           *sender,
                           const gchar           *object_path,
                           const gchar           *interface_name,
                           const gchar           *method_name,
                           GVariant              *parameters,
                           GDBusMethodInvocation *invocation,
                           gpointer               user_data)
{
  GtDBusQueue *self = user_data;
  GDBusMessage *message = g_dbus_method_invocation_get_message (invocation);

  g_debug ("%s: Server pushing message serial %u",
           G_STRFUNC, g_dbus_message_get_serial (message));
  g_async_queue_push (self->server_message_queue, g_object_ref (invocation));

  /* Either of these could be listening for the message, depending on whether
   * gt_dbus_queue_pop_message() is being called in the #GtDBusQueueServerFunc,
   * or in the thread where the #GtDBusQueue was constructed (typically the main
   * thread of the test program). */
  g_main_context_wakeup (self->client_context);
  g_main_context_wakeup (self->server_context);
}

/**
 * gt_dbus_queue_get_n_messages:
 * @self: a #GtDBusQueue
 *
 * Get the number of messages waiting in the server queue to be popped by
 * gt_dbus_queue_pop_message() and processed.
 *
 * If asserting that the queue is empty, gt_dbus_queue_assert_no_messages() is
 * more appropriate.
 *
 * This may be called from any thread.
 *
 * Returns: number of messages waiting to be popped and processed
 * Since: 0.1.0
 */
gsize
gt_dbus_queue_get_n_messages (GtDBusQueue *self)
{
  gint n_messages;

  g_return_val_if_fail (self != NULL, 0);

  n_messages = g_async_queue_length (self->server_message_queue);

  return MAX (n_messages, 0);
}

/*
 * gt_dbus_queue_pop_message_internal:
 * @self: a #GtDBusQueue
 * @wait: %TRUE to block until a message can be popped, %FALSE to return
 *    immediately if no message is queued
 * @out_invocation: (out) (transfer full) (optional) (nullable): return location
 *    for the popped #GDBusMethodInvocation, which may be %NULL; pass %NULL to
 *    not receive the #GDBusMethodInvocation
 *
 * Internal helper which implements gt_dbus_queue_pop_message() and
 * gt_dbus_queue_try_pop_message().
 *
 * This may be called from any thread after gt_dbus_queue_connect() has been
 * called.
 *
 * Returns: %TRUE if a message was popped (and returned in @out_invocation if
 *    @out_invocation was non-%NULL), %FALSE otherwise
 * Since: 0.1.0
 */
static gboolean
gt_dbus_queue_pop_message_internal (GtDBusQueue            *self,
                                    gboolean                wait,
                                    GDBusMethodInvocation **out_invocation)
{
  g_autoptr(GDBusMethodInvocation) invocation = NULL;
  gboolean message_popped = FALSE;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (self->server_thread != NULL, FALSE);

  while (wait && (invocation = g_async_queue_try_pop (self->server_message_queue)) == NULL)
    {
      /* This could be the client or server context, depending on whether we’re
       * executing in a #GtDBusQueueServerFunc or not. */
      g_main_context_iteration (g_main_context_get_thread_default (), TRUE);
    }

  if (invocation != NULL)
    {
      GDBusMessage *message = g_dbus_method_invocation_get_message (invocation);
      g_debug ("%s: Client popping message serial %u",
               G_STRFUNC, g_dbus_message_get_serial (message));
      message_popped = TRUE;
    }

  if (out_invocation != NULL)
    *out_invocation = g_steal_pointer (&invocation);

  return message_popped;
}

/**
 * gt_dbus_queue_try_pop_message:
 * @self: a #GtDBusQueue
 * @out_invocation: (out) (transfer full) (optional) (nullable): return location
 *    for the popped #GDBusMethodInvocation, which may be %NULL; pass %NULL to
 *    not receive the #GDBusMethodInvocation
 *
 * Pop a message off the server’s message queue, if one is ready to be popped.
 * Otherwise, immediately return %NULL.
 *
 * This may be called from any thread after gt_dbus_queue_connect() has been
 * called.
 *
 * Returns: %TRUE if a message was popped (and returned in @out_invocation if
 *    @out_invocation was non-%NULL), %FALSE otherwise
 * Since: 0.1.0
 */
gboolean
gt_dbus_queue_try_pop_message (GtDBusQueue            *self,
                               GDBusMethodInvocation **out_invocation)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return gt_dbus_queue_pop_message_internal (self, FALSE, out_invocation);
}

/**
 * gt_dbus_queue_pop_message:
 * @self: a #GtDBusQueue
 * @out_invocation: (out) (transfer full) (optional) (nullable): return location
 *    for the popped #GDBusMethodInvocation, which may be %NULL; pass %NULL to
 *    not receive the #GDBusMethodInvocation
 *
 * Pop a message off the server’s message queue, if one is ready to be popped.
 * Otherwise, block indefinitely until one is.
 *
 * This may be called from any thread after gt_dbus_queue_connect() has been
 * called.
 *
 * Returns: %TRUE if a message was popped (and returned in @out_invocation if
 *    @out_invocation was non-%NULL), %FALSE if the pop timed out
 * Since: 0.1.0
 */
gboolean
gt_dbus_queue_pop_message (GtDBusQueue            *self,
                           GDBusMethodInvocation **out_invocation)
{
  g_return_val_if_fail (self != NULL, FALSE);

  return gt_dbus_queue_pop_message_internal (self, TRUE, out_invocation);
}

/**
 * gt_dbus_queue_match_client_message:
 * @self: a #GtDBusQueue
 * @invocation: (transfer none): invocation to match against
 * @expected_object_path: object path the invocation is expected to be calling
 * @expected_interface_name: interface name the invocation is expected to be calling
 * @expected_method_name: method name the invocation is expected to be calling
 * @expected_parameters_string: (nullable): expected parameters for the
 *    invocation, or %NULL to not match its parameters
 *
 * Check whether @invocation matches the given expected object path, interface
 * name, method name and (optionally) parameters, and was sent by the client
 * connection of the #GtDBusQueue.
 *
 * This may be called from any thread after gt_dbus_queue_connect() has been
 * called.
 *
 * @expected_parameters_string is optional, and will be matched against only if
 * it is non-%NULL. The other arguments are not optional. If non-%NULL,
 * @expected_parameters_string will be parsed using g_variant_new_parsed(). It
 * is a programmer error to provide a string which doesn’t parse correctly.
 *
 * Returns: %TRUE if @invocation matches the expected arguments,
 *    %FALSE otherwise
 * Since: 0.1.0
 */
gboolean
gt_dbus_queue_match_client_message (GtDBusQueue           *self,
                                    GDBusMethodInvocation *invocation,
                                    const gchar           *expected_object_path,
                                    const gchar           *expected_interface_name,
                                    const gchar           *expected_method_name,
                                    const gchar           *expected_parameters_string)
{
  g_autoptr(GVariant) expected_parameters = NULL;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (G_IS_DBUS_METHOD_INVOCATION (invocation), FALSE);
  g_return_val_if_fail (g_variant_is_object_path (expected_object_path), FALSE);
  g_return_val_if_fail (g_dbus_is_interface_name (expected_interface_name), FALSE);
  g_return_val_if_fail (g_dbus_is_member_name (expected_method_name), FALSE);

  if (expected_parameters_string != NULL)
    expected_parameters = g_variant_new_parsed (expected_parameters_string);

  return (g_str_equal (g_dbus_method_invocation_get_sender (invocation),
                       g_dbus_connection_get_unique_name (self->client_connection)) &&
          g_str_equal (g_dbus_method_invocation_get_object_path (invocation),
                       expected_object_path) &&
          g_str_equal (g_dbus_method_invocation_get_interface_name (invocation),
                       expected_interface_name) &&
          g_str_equal (g_dbus_method_invocation_get_method_name (invocation),
                       expected_method_name) &&
          (expected_parameters == NULL ||
           g_variant_equal (g_dbus_method_invocation_get_parameters (invocation),
                            expected_parameters)));
}

/**
 * gt_dbus_queue_format_message:
 * @invocation: a #GDBusMethodInvocation to format
 *
 * Format a #GDBusMethodInvocation in a human readable way. This format is not
 * intended to be stable or machine parsable.
 *
 * Returns: (transfer full): human readable version of @invocation
 * Since: 0.1.0
 */
gchar *
gt_dbus_queue_format_message (GDBusMethodInvocation *invocation)
{
  g_return_val_if_fail (G_IS_DBUS_METHOD_INVOCATION (invocation), NULL);

  return g_dbus_message_print (g_dbus_method_invocation_get_message (invocation), 0);
}

/**
 * gt_dbus_queue_format_messages:
 * @self: a #GtDBusQueue
 *
 * Format all the messages currently pending in the mock service’s message queue
 * in a human readable way, with the head of the queue first in the formatted
 * list. This format is not intended to be stable or machine parsable.
 *
 * If no messages are in the queue, an empty string will be returned.
 *
 * Returns: (transfer full): human readable version of the pending message queue
 * Since: 0.1.0
 */
gchar *
gt_dbus_queue_format_messages (GtDBusQueue *self)
{
  g_autoptr(GString) output = NULL;
  g_autoptr(GPtrArray) local_queue = NULL;
  g_autoptr(GDBusMethodInvocation) message = NULL;
  gsize i;

  g_return_val_if_fail (self != NULL, NULL);

  /* Cyclically pop all the elements off the head of the queue and push them
   * onto a local queue, printing each one as we go. Then push that lot back
   * onto the original queue so it’s unchanged overall. Since there are no
   * accessors to inspect the inner elements of a #GAsyncQueue, this is the best
   * we can do. */
  g_async_queue_lock (self->server_message_queue);

  local_queue = g_ptr_array_new_with_free_func (g_object_unref);
  output = g_string_new ("");

  while ((message = g_async_queue_try_pop_unlocked (self->server_message_queue)) != NULL)
    {
      g_autofree gchar *formatted = gt_dbus_queue_format_message (message);
      g_string_append (output, formatted);

      g_ptr_array_add (local_queue, g_steal_pointer (&message));
    }

  /* Reassemble the queue. */
  for (i = 0; i < local_queue->len; i++)
    {
      /* FIXME: Use g_ptr_array_steal() here once we can depend on a new enough
       * GLib version. */
      message = g_steal_pointer (&g_ptr_array_index (local_queue, i));
      g_async_queue_push_unlocked (self->server_message_queue, g_steal_pointer (&message));
    }

  /* We’ve stolen all the elements. */
  g_ptr_array_set_free_func (local_queue, NULL);

  g_async_queue_unlock (self->server_message_queue);

  return g_string_free (g_steal_pointer (&output), FALSE);
}

/*< private >*/
/*
 * gt_dbus_queue_assert_pop_message_impl:
 * @self: a #GtDBusQueue
 * @macro_log_domain: #G_LOG_DOMAIN from the call site
 * @macro_file: C file containing the call site
 * @macro_line: line containing the call site
 * @macro_function: function containing the call site
 * @expected_object_path: object path the invocation is expected to be calling
 * @expected_interface_name: interface name the invocation is expected to be calling
 * @expected_method_name: method name the invocation is expected to be calling
 * @parameters_format: g_variant_get() format string to extract the parameters
 *    from the popped #GDBusMethodInvocation into the return locations provided
 *    in @...
 * @...: return locations for the parameter placeholders given in @parameters_format
 *
 * Internal function which implements the gt_dbus_queue_assert_pop_message()
 * macro.
 *
 * An assertion failure message will be printed if a #GDBusMethodInvocation
 * can’t be popped from the queue.
 *
 * Returns: (transfer full): the popped #GDBusMethodInvocation
 * Since: 0.1.0
 */
GDBusMethodInvocation *
gt_dbus_queue_assert_pop_message_impl (GtDBusQueue *self,
                                       const gchar *macro_log_domain,
                                       const gchar *macro_file,
                                       gint         macro_line,
                                       const gchar *macro_function,
                                       const gchar *expected_object_path,
                                       const gchar *expected_interface_name,
                                       const gchar *expected_method_name,
                                       const gchar *parameters_format,
                                       ...)
{
  g_autoptr(GDBusMethodInvocation) invocation = NULL;

  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (macro_file != NULL, NULL);
  g_return_val_if_fail (macro_line >= 0, NULL);
  g_return_val_if_fail (macro_function != NULL, NULL);
  g_return_val_if_fail (g_variant_is_object_path (expected_object_path), NULL);
  g_return_val_if_fail (g_dbus_is_interface_name (expected_interface_name), NULL);
  g_return_val_if_fail (g_dbus_is_member_name (expected_method_name), NULL);
  g_return_val_if_fail (parameters_format != NULL, NULL);

  if (!gt_dbus_queue_pop_message (self, &invocation))
    {
      g_autofree gchar *message =
          g_strdup_printf ("Expected message %s.%s from %s, but saw no messages",
                           expected_interface_name, expected_method_name,
                           expected_object_path);
      g_assertion_message (macro_log_domain, macro_file, macro_line,
                           macro_function, message);
      return NULL;
    }

  if (!gt_dbus_queue_match_client_message (self, invocation,
                                           expected_object_path,
                                           expected_interface_name,
                                           expected_method_name,
                                           NULL))
    {
      g_autofree gchar *invocation_formatted =
          gt_dbus_queue_format_message (invocation);
      g_autofree gchar *message =
          g_strdup_printf ("Expected message %s.%s from %s, but saw: %s",
                           expected_interface_name, expected_method_name,
                           expected_object_path, invocation_formatted);
      g_assertion_message (macro_log_domain, macro_file, macro_line,
                           macro_function, message);
      return NULL;
    }

  /* Passed the test! */
  va_list parameters_args;
  GVariant *parameters = g_dbus_method_invocation_get_parameters (invocation);

  va_start (parameters_args, parameters_format);
  g_variant_get_va (parameters, parameters_format, NULL, &parameters_args);
  va_end (parameters_args);

  return g_steal_pointer (&invocation);
}
