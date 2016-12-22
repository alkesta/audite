/*
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include <gtk/gtk.h>

#include "audite_app.h"
#include "audite_app_win.h"
#include "audite_app_prefs.h"

struct _AuditeApp
{
  GtkApplication parent;
};

G_DEFINE_TYPE(AuditeApp, audite_app, GTK_TYPE_APPLICATION);

static void
audite_app_init (AuditeApp *app)
{
}

static void
open_activated (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       app)
{
  GtkWidget *dialog = g_object_get_data(app, "dialog");
  GtkFileFilter *filter_m4b, *filter_mime_audio, *filter_any_files;
  AuditeAppWindow *win;

  win = gtk_application_get_active_window (GTK_APPLICATION (app));
//  prefs = audite_app_prefs_new (AUDITE_APP_WINDOW (win));
 // gtk_window_present (GTK_WINDOW (prefs));

  filter_m4b = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter_m4b, "m4b books");
  gtk_file_filter_add_pattern (filter_m4b, "*.m4b");
  gtk_file_filter_add_pattern (filter_m4b, "*.M4B");

  filter_mime_audio = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter_mime_audio, "Any audio files");
  gtk_file_filter_add_mime_type (filter_mime_audio, "audio/*");

  filter_any_files = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter_any_files, "Any files");
  gtk_file_filter_add_pattern (filter_any_files, "*");

  dialog = gtk_file_chooser_dialog_new ("Open File",
                                      GTK_WINDOW(win),
                                      GTK_FILE_CHOOSER_ACTION_OPEN,
                                      "_Cancel",
                                      GTK_RESPONSE_CANCEL,
                                      "_Open",
                                      GTK_RESPONSE_ACCEPT,
                                      NULL);

  gtk_file_chooser_add_filter ( (GtkFileChooser *) dialog, filter_m4b);
  gtk_file_chooser_add_filter ( (GtkFileChooser *) dialog, filter_mime_audio);
  gtk_file_chooser_add_filter ( (GtkFileChooser *) dialog, filter_any_files);


  gint res = gtk_dialog_run (GTK_DIALOG (dialog));
  if (res == GTK_RESPONSE_ACCEPT)  {
    gchar *file, *uri;
    file = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(dialog));
    g_print("Choosed: %s\n ", file);
   uri = g_file_get_uri(g_file_new_for_commandline_arg (file));
   audite_app_window_open (win, uri);
    g_free (file);
    g_free (uri); 
  }
  gtk_widget_destroy ( (GtkWidget *) (dialog));
  gtk_window_present (GTK_WINDOW (win));
}

static void
preferences_activated (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       app)
{
  AuditeAppPrefs *prefs;
  GtkWindow *win;

  win = gtk_application_get_active_window (GTK_APPLICATION (app));
  prefs = audite_app_prefs_new (AUDITE_APP_WINDOW (win));
  gtk_window_present (GTK_WINDOW (prefs));
}

static void
quit_activated (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       app)
{
  AuditeAppWindow *win;
  win = gtk_application_get_active_window (GTK_APPLICATION (app));
  g_application_quit (G_APPLICATION (app));
}

static GActionEntry app_entries[] =
{
  { "open", open_activated, NULL, NULL, NULL },
  { "quit", quit_activated, NULL, NULL, NULL }
};

static void
audite_app_startup (GApplication *app)
{
  GtkBuilder *builder;
  GMenuModel *app_menu = g_malloc (sizeof(*app_menu));
  const gchar *quit_accels[2] = { "<Ctrl>Q", NULL };
  const gchar *open_accels[2] = { "<Ctrl>O", NULL };

  G_APPLICATION_CLASS (audite_app_parent_class)->startup (app);

  g_action_map_add_action_entries (G_ACTION_MAP (app),
                                   app_entries, G_N_ELEMENTS (app_entries),
                                   app);

  gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                         "app.open",
                                         open_accels);

  gtk_application_set_accels_for_action (GTK_APPLICATION (app),
                                         "app.quit",
                                         quit_accels);

  builder = gtk_builder_new_from_resource ("/com/github/alkesta/audite/app-menu.ui");
  app_menu = G_MENU_MODEL (gtk_builder_get_object (builder, "appmenu"));
  gtk_application_set_app_menu (GTK_APPLICATION (app), app_menu);
  g_object_unref (builder);
}

static void
audite_app_activate (GApplication *app)
{
  AuditeAppWindow *win;

  win = audite_app_window_new (AUDITE_APP (app));
  gtk_window_present (GTK_WINDOW (win));
}

static void
audite_app_open (GApplication  *app,
                  GFile        **files,
                  gint           n_files,
                  const gchar   *hint)
{
  GList *windows;
  AuditeAppWindow *win;
  int i = 0;
  gchar **uris;
  windows = gtk_application_get_windows (GTK_APPLICATION (app));
  if (windows)
    win = AUDITE_APP_WINDOW (windows->data);
  else 
    win = audite_app_window_new (AUDITE_APP (app));

  uris[i] = g_file_get_uri(((GFile **)files)[i]);
  audite_app_window_open (win, uris[i]);
  gtk_window_present (GTK_WINDOW (win));
}


static void
audite_app_class_init (AuditeAppClass *class)
{
  G_APPLICATION_CLASS (class)->startup = audite_app_startup;
  G_APPLICATION_CLASS (class)->activate = audite_app_activate;
  G_APPLICATION_CLASS (class)->open = audite_app_open;
}

AuditeApp *
audite_app_new (void)
{
  return g_object_new (AUDITE_APP_TYPE,
                       "application-id", "org.gtk.audite",
                       "flags", G_APPLICATION_HANDLES_OPEN,
                       NULL);
}
