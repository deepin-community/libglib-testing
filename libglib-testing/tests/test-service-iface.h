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

/* Static definition of some test D-Bus interfaces.
 * FIXME: Once we can depend on a new enough version of GLib, generate this
 * from introspection XML using `gdbus-codegen --interface-info-{header,body}`. */
static const GDBusPropertyInfo object_property_some_string =
{
  .ref_count = -1,  /* static */
  .name = "some-string",
  .signature = "s",
  .flags = G_DBUS_PROPERTY_INFO_FLAGS_READABLE | G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE,
  .annotations = NULL,
};

static const GDBusPropertyInfo object_property_some_int =
{
  .ref_count = -1,  /* static */
  .name = "some-int",
  .signature = "u",
  .flags = G_DBUS_PROPERTY_INFO_FLAGS_READABLE | G_DBUS_PROPERTY_INFO_FLAGS_WRITABLE,
  .annotations = NULL,
};

static const GDBusPropertyInfo *object_properties[] =
{
  (GDBusPropertyInfo *) &object_property_some_string,
  (GDBusPropertyInfo *) &object_property_some_int,
  NULL,
};

static const GDBusInterfaceInfo object_interface_info =
{
  .ref_count = -1,  /* static */
  .name = (gchar *) "com.example.Test.Object",
  .methods = NULL,
  .signals = NULL,
  .properties = (GDBusPropertyInfo **) &object_properties,
  .annotations = NULL,
};

static const GDBusArgInfo manager_method_get_object_path_arg_object_id =
{
  .ref_count = -1,  /* static */
  .name = (gchar *) "ObjectId",
  .signature = (gchar *) "u",
  .annotations = NULL,
};
static const GDBusArgInfo manager_method_get_object_path_arg_object_path =
{
  .ref_count = -1,  /* static */
  .name = (gchar *) "ObjectPath",
  .signature = (gchar *) "o",
  .annotations = NULL,
};
static const GDBusArgInfo *manager_method_get_object_path_in_args[] =
{
  (GDBusArgInfo *) &manager_method_get_object_path_arg_object_id,
  NULL,
};
static const GDBusArgInfo *manager_method_get_object_path_out_args[] =
{
  (GDBusArgInfo *) &manager_method_get_object_path_arg_object_path,
  NULL,
};
static const GDBusMethodInfo manager_method_get_object_path =
{
  .ref_count = -1,  /* static */
  .name = (gchar *) "GetObjectPath",
  .in_args = (GDBusArgInfo **) &manager_method_get_object_path_in_args,
  .out_args = (GDBusArgInfo **) &manager_method_get_object_path_out_args,
  .annotations = NULL,
};

static const GDBusMethodInfo *manager_methods[] =
{
  (GDBusMethodInfo *) &manager_method_get_object_path,
  NULL,
};

static const GDBusInterfaceInfo manager_interface_info =
{
  .ref_count = -1,  /* static */
  .name = (gchar *) "com.example.Test.Manager",
  .methods = (GDBusMethodInfo **) &manager_methods,
  .signals = NULL,
  .properties = NULL,
  .annotations = NULL,
};

G_END_DECLS
