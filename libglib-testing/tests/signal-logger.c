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

#include <glib.h>
#include <libglib-testing/signal-logger.h>
#include <locale.h>


/* Test that creating and destroying a signal logger works. A basic smoketest. */
static void
test_signal_logger_construction (void)
{
  g_autoptr(GtSignalLogger) logger = NULL;
  logger = gt_signal_logger_new ();

  /* Call a method to avoid warnings about unused variables. */
  g_assert_cmpuint (gt_signal_logger_get_n_emissions (logger), ==, 0);
}

int
main (int   argc,
      char *argv[])
{
  setlocale (LC_ALL, "");

  g_test_init (&argc, &argv, NULL);

  g_test_add_func ("/signal-logger/construction",
                   test_signal_logger_construction);

  return g_test_run ();
}
