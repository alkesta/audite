/*
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <gtk/gtk.h>

#include "audite_app.h"

int
main (int argc, char *argv[]) {

  g_setenv ("GSETTINGS_SCHEMA_DIR", ".", FALSE);
  return g_application_run (G_APPLICATION (audite_app_new ()), argc, argv);
}
