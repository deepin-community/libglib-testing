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

#include <gio/gio.h>
#include <glib.h>
#include <libglib-testing/dbus-queue.h>
#include <locale.h>
#include "test-service-iface.h"

/* Test that creating and destroying a D-Bus queue works. A basic smoketest. */
static void
test_dbus_queue_construction (void)
{
  g_autoptr(GtDBusQueue) queue = NULL;
  queue = gt_dbus_queue_new ();

  /* Call a method to avoid warnings about unused variables. */
  g_assert_cmpuint (gt_dbus_queue_get_n_messages (queue), ==, 0);
}

/* Fixture for tests which interact with the com.example.Test service over
 * D-Bus.
 *
 * It exports one object (with ID 123) and an manager object. The method return
 * values from ID 123 are up to the test in question.
 */
typedef struct
{
  GtDBusQueue *queue;  /* (owned) */
  guint valid_id;
} BusFixture;

static void
bus_set_up (BusFixture    *fixture,
            gconstpointer  test_data)
{
  g_autoptr(GError) local_error = NULL;
  g_autofree gchar *object_path = NULL;

  fixture->valid_id = 123;  /* arbitrarily chosen */
  fixture->queue = gt_dbus_queue_new ();

  gt_dbus_queue_connect (fixture->queue, &local_error);
  g_assert_no_error (local_error);

  gt_dbus_queue_own_name (fixture->queue, "com.example.Test");

  object_path = g_strdup_printf ("/com/example/Test/Object%u", fixture->valid_id);
  gt_dbus_queue_export_object (fixture->queue,
                               object_path,
                               (GDBusInterfaceInfo *) &object_interface_info,
                               &local_error);
  g_assert_no_error (local_error);

  gt_dbus_queue_export_object (fixture->queue,
                               "/com/example/Test",
                               (GDBusInterfaceInfo *) &manager_interface_info,
                               &local_error);
  g_assert_no_error (local_error);
}

static void
bus_tear_down (BusFixture    *fixture,
               gconstpointer  test_data)
{
  gt_dbus_queue_disconnect (fixture->queue, TRUE);
  g_clear_pointer (&fixture->queue, gt_dbus_queue_free);
}

/* Helper #GAsyncReadyCallback which returns the #GAsyncResult in its @user_data. */
static void
async_result_cb (GObject      *obj,
                 GAsyncResult *result,
                 gpointer      user_data)
{
  GAsyncResult **result_out = (GAsyncResult **) user_data;

  g_assert_null (*result_out);
  *result_out = g_object_ref (result);
}

/* Test that making two calls in series to a mock D-Bus service works. The
 * @test_data is a boolean value indicating whether to do the calls
 * synchronously (%FALSE) or asynchronously (%TRUE).
 *
 * The mock D-Bus replies are generated in series_server_cb(), which is
 * used for both synchronous and asynchronous calls. */
static void series_server_cb (GtDBusQueue *queue,
                              gpointer     user_data);

static void
test_dbus_queue_series (BusFixture    *fixture,
                        gconstpointer  test_data)
{
  g_autoptr(GVariant) reply = NULL;
  g_autoptr(GError) local_error = NULL;
  gboolean test_async = GPOINTER_TO_UINT (test_data);
  GDBusConnection *client_connection = gt_dbus_queue_get_client_connection (fixture->queue);
  g_autofree gchar *object_path = NULL;
  g_autoptr(GVariant) properties = NULL;
  const gchar *some_str;
  guint some_int;

  gt_dbus_queue_set_server_func (fixture->queue, series_server_cb, fixture);

  if (test_async)
    {
      g_autoptr(GAsyncResult) result = NULL;

      g_dbus_connection_call (client_connection,
                              "com.example.Test",
                              "/com/example/Test",
                              "com.example.Test.Manager",
                              "GetObjectPath",
                              g_variant_new ("(u)", 123),
                              G_VARIANT_TYPE ("(o)"),
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,  /* timeout (ms) */
                              NULL,  /* cancellable */
                              async_result_cb,
                              &result);

      while (result == NULL)
        g_main_context_iteration (NULL, TRUE);
      reply = g_dbus_connection_call_finish (client_connection, result, &local_error);
    }
  else
    {
      reply = g_dbus_connection_call_sync (client_connection,
                                           "com.example.Test",
                                           "/com/example/Test",
                                           "com.example.Test.Manager",
                                           "GetObjectPath",
                                           g_variant_new ("(u)", 123),
                                           G_VARIANT_TYPE ("(o)"),
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1,  /* timeout (ms) */
                                           NULL,  /* cancellable */
                                           &local_error);
    }

  g_assert_no_error (local_error);
  g_assert_nonnull (reply);

  g_variant_get (reply, "(o)", &object_path);

  g_clear_pointer (&reply, g_variant_unref);

  /* Get and check the object properties. */
  if (test_async)
    {
      g_autoptr(GAsyncResult) result = NULL;

      g_dbus_connection_call (client_connection,
                              "com.example.Test",
                              object_path,
                              "org.freedesktop.DBus.Properties",
                              "GetAll",
                              g_variant_new ("(s)", "com.example.Test.Object"),
                              G_VARIANT_TYPE ("(a{sv})"),
                              G_DBUS_CALL_FLAGS_NONE,
                              -1,  /* timeout (ms) */
                              NULL,  /* cancellable */
                              async_result_cb,
                              &result);

      while (result == NULL)
        g_main_context_iteration (NULL, TRUE);
      reply = g_dbus_connection_call_finish (client_connection, result, &local_error);
    }
  else
    {
      reply = g_dbus_connection_call_sync (client_connection,
                                           "com.example.Test",
                                           object_path,
                                           "org.freedesktop.DBus.Properties",
                                           "GetAll",
                                           g_variant_new ("(s)", "com.example.Test.Object"),
                                           G_VARIANT_TYPE ("(a{sv})"),
                                           G_DBUS_CALL_FLAGS_NONE,
                                           -1,  /* timeout (ms) */
                                           NULL,  /* cancellable */
                                           &local_error);
    }

  g_assert_no_error (local_error);
  g_assert_nonnull (reply);

  properties = g_variant_get_child_value (reply, 0);
  g_variant_lookup (properties, "some-str", "&s", &some_str);
  g_assert_cmpstr (some_str, ==, "hello");
  g_variant_lookup (properties, "some-int", "u", &some_int);
  g_assert_cmpuint (some_int, ==, 11);
}

/* This is run in a worker thread. */
static void
series_server_cb (GtDBusQueue *queue,
                  gpointer     user_data)
{
  BusFixture *fixture = user_data;
  g_autoptr(GDBusMethodInvocation) invocation1 = NULL;
  g_autoptr(GDBusMethodInvocation) invocation2 = NULL;
  g_autofree gchar *object_path = NULL;
  g_autofree gchar *reply1 = NULL;

  /* Handle the GetObjectPath() call. */
  guint object_id;
  invocation1 =
      gt_dbus_queue_assert_pop_message (queue,
                                        "/com/example/Test",
                                        "com.example.Test.Manager",
                                        "GetObjectPath", "(u)", &object_id);
  g_assert_cmpint (object_id, ==, fixture->valid_id);

  object_path = g_strdup_printf ("/com/example/Test/Object%u", object_id);
  reply1 = g_strdup_printf ("(@o '%s',)", object_path);
  g_dbus_method_invocation_return_value (invocation1, g_variant_new_parsed (reply1));

  /* Handle the Properties.GetAll() call and return some arbitrary values for
   * the given object. */
  const gchar *property_interface;
  invocation2 =
      gt_dbus_queue_assert_pop_message (queue,
                                        object_path,
                                        "org.freedesktop.DBus.Properties",
                                        "GetAll", "(&s)", &property_interface);
  g_assert_cmpstr (property_interface, ==, "com.example.Test.Object");

  const gchar *reply2 =
    "({"
      "'some-str': <'hello'>,"
      "'some-int': <@u 11>"
    "},)";
  g_dbus_method_invocation_return_value (invocation2, g_variant_new_parsed (reply2));
}

int
main (int   argc,
      char *argv[])
{
  setlocale (LC_ALL, "");

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/dbus-queue/construction",
                   test_dbus_queue_construction);

  g_test_add ("/dbus-queue/series-async", BusFixture, GUINT_TO_POINTER (TRUE),
              bus_set_up, test_dbus_queue_series, bus_tear_down);
  g_test_add ("/dbus-queue/series-sync", BusFixture, GUINT_TO_POINTER (FALSE),
              bus_set_up, test_dbus_queue_series, bus_tear_down);

  return g_test_run ();
}
