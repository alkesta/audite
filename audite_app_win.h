/*
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef __AUDITE_APP_WIN_H
#define __AUDITE_APP_WIN_H

#include <gtk/gtk.h>
#include "audite_app.h"


#define AUDITE_APP_WINDOW_TYPE (audite_app_window_get_type ())
G_DECLARE_FINAL_TYPE (AuditeAppWindow, audite_app_window, AUDITE, APP_WINDOW, GtkApplicationWindow)

AuditeAppWindow       *audite_app_window_new          (AuditeApp *app);
void                    audite_app_window_open         (AuditeAppWindow *win,
                                                         gchar            *uri);


#endif /* __AUDITE_APP_WIN_H */
