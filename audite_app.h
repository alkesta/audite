/*
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef __AUDITE_APP_H
#define __AUDITE_APP_H

#include <gtk/gtk.h>


#define AUDITE_APP_TYPE (audite_app_get_type ())
G_DECLARE_FINAL_TYPE (AuditeApp, audite_app, AUDITE, APP, GtkApplication)


AuditeApp     *audite_app_new         (void);


#endif /* __AUDITE_APP_H */
