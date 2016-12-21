/*
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This code based on
 * https://github.com/sdroege/gst-player/gtk/gtk-play.c
 * Copyright (C) 2014-2015 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015 Brijesh Singh <brijesh.ksingh@gmail.com>
 */

#include <string.h>
#include <math.h>
#include <gtk/gtk.h>
#include <gst/player/player.h>
#include <gst/gst.h>
#include <gst/tag/tag.h>
#include <mp4v2/mp4v2.h>
#include "audite_app.h"
#include "audite_app_win.h"

#define MP4V2_SECOND 1000
#define CONFIG_FILE "audite.conf"

struct _AuditeAppWindow
{
  GtkApplicationWindow parent;

  GstPlayer *player;
  gboolean   playing;
  gchar     *current_uri;
  gboolean   audiobook;
  gint       amount_of_chapters;
  gint       current_chapter_number;
  guint64    current_chapter_end;
  guint64    current_chapter_start;

  GSettings *settings;
  GtkWidget *gears;
  GtkWidget *volume_button;
  GtkWidget *previous_button;
  GtkWidget *rewind_button;
  GtkWidget *play_button;
  GtkWidget *pause_image;
  GtkWidget *play_image;
  GtkWidget *forward_button;
  GtkWidget *next_button;
  GtkWidget *chapter_count_label;
  GtkWidget *window_title_label;
  GtkWidget *remain_time_label;
  GtkWidget *elapsed_time_label;
  GtkWidget *seek_bar;
  GtkWidget *cover_art_image;
  GtkWidget *cover_image;
  GtkWidget *chapter_list_store;
  GtkWidget *chapters_tree_view;
  GtkWidget *progress;
  GtkWidget *genre_value_label;
  GtkWidget *year_value_label;
  GtkWidget *stream_properties_label;
  GtkWidget *pos_label;
  GtkWidget *total_dur_label;
  GtkWidget *status_box;
  GtkWidget *genre_box;
  GtkWidget *tree_scroll_win;


};

enum {
	ICON,
	NUMBER,
	NAME,
	DURATION,
	START,
	END
};

G_DEFINE_TYPE(AuditeAppWindow, audite_app_window, GTK_TYPE_APPLICATION_WINDOW);

static void play_button_clicked_handler(GtkButton * button, AuditeAppWindow *win);
static void forward_button_clicked_handler (GtkButton * button, AuditeAppWindow *win);
static void rewind_button_clicked_handler (GtkButton * button, AuditeAppWindow *win);
static void previous_button_clicked_handler (GtkButton * button, AuditeAppWindow *win);
static void next_button_clicked_handler (GtkButton * button, AuditeAppWindow *win);

static void gst_media_info_updated_handler (GstPlayer * player, GstPlayerMediaInfo * media_info, AuditeAppWindow *win);
static void gst_volume_changed_handler(GstPlayer * unused, AuditeAppWindow *win);
static void gst_duration_changed_handler (GstPlayer * unused, GstClockTime duration, AuditeAppWindow *win);
static void gst_position_updated_handler(GstPlayer * unused, GstClockTime position, AuditeAppWindow *win);
static void gst_state_changed_handler (GstPlayer * unused, GstPlayerState state, AuditeAppWindow *win);
static void gst_media_eos_handler (GstPlayer * unused, AuditeAppWindow *win);


static GdkPixbuf *get_cover_image (GstPlayerMediaInfo * media_info);
static void update_position_label (GtkLabel * label, guint64 seconds);
static GObject *
audite_app_window_constructor (GType type, guint n_construct_params,
    GObjectConstructParam * construct_params);
static void seek_bar_value_changed_handler(GtkRange * range, gpointer data);
static gchar * seconds_to_hhmmss (guint64 seconds);
static void seek_bar_set_range (AuditeAppWindow *win, guint64 start, guint64 end);
static void set_curent_chapter (AuditeAppWindow *win, GstClockTime position);
static void cover_art_dialog (AuditeAppWindow *win);


static void set_chapter (AuditeAppWindow *win, gint next);

static void row_activated_handler(GtkTreeView *view, GtkTreePath *path,
                        GtkTreeViewColumn *col, AuditeAppWindow *win) {
  GtkTreeIter   iter;
  GtkTreeModel *model;

  model = gtk_tree_view_get_model(view);
  if (gtk_tree_model_get_iter(model, &iter, path)) {
    gchar *title;
    gint number;
    guint64 start, end;
    gtk_tree_model_get(model, &iter,
                        NUMBER,&number,
			NAME,  &title,
			START, &start,
			END,   &end,
			-1);
    win->current_chapter_number = number;
    win->current_chapter_start = start;
    win->current_chapter_end = end;

    gst_player_seek (win->player, gst_util_uint64_scale ( (gdouble) win->current_chapter_start, GST_SECOND, 1) );
    set_curent_chapter (win, (gint64) start * GST_SECOND);
    gst_player_play (win->player);
  }
}

G_MODULE_EXPORT void
volume_button_value_changed_handler (GtkScaleButton * button, gdouble value, AuditeAppWindow *win){

  gst_player_set_volume (win->player, value);
}

static void gst_media_info_updated_handler (GstPlayer * player,
				GstPlayerMediaInfo * media_info,
				AuditeAppWindow *win) {
	GdkPixbuf  *pixbuf;
	gchar      *title;
	gchar      *basename = NULL;
	gchar      *filename = NULL;
	GstTagList *tags =     NULL;
	gchar      *artist =   NULL;
	gchar      *album =    NULL;
	gchar      *genre =    NULL;
	GDate      *date =     NULL;
	gchar      *codec =    NULL;
	gint        bitrate =    0;
	gint        channels =   0;
	gchar      *ch =       NULL;
	gint        samplerate = 0;
	gchar       year[5] =  "";
	GList      *list, *l;
	gchar      *prop =     NULL;

	list = gst_player_media_info_get_stream_list (media_info);
	if (list) {
		for (l = list; l != NULL; l = l->next) {
			GstPlayerStreamInfo *stream = (GstPlayerStreamInfo *) l->data;
			if (GST_IS_PLAYER_AUDIO_INFO (stream)) {
				
				codec = gst_player_stream_info_get_codec ((GstPlayerStreamInfo *) stream);
				samplerate = gst_player_audio_info_get_sample_rate ((GstPlayerAudioInfo *) stream);
				channels = gst_player_audio_info_get_channels ((GstPlayerAudioInfo *) stream);
				bitrate = gst_player_audio_info_get_bitrate ((GstPlayerAudioInfo *) stream);
			}
		}
	}


	tags = gst_player_media_info_get_tags (media_info);
	if (tags) {
		gst_tag_list_get_string(tags, GST_TAG_ARTIST, &artist);
		gst_tag_list_get_string(tags, GST_TAG_ALBUM, &album);
		gst_tag_list_get_string(tags, GST_TAG_GENRE, &genre);
		gst_tag_list_get_date(tags, GST_TAG_DATE, &date);
	}

	if (date) 
		g_date_strftime (year, sizeof(year), "%Y-%M-%D", date);
	
	gtk_label_set_text (GTK_LABEL (win->year_value_label), date ? year : NULL);
	gtk_label_set_text (GTK_LABEL (win->genre_value_label), genre ? genre : NULL);

	if (channels) {
		switch (channels) {
			case 1: ch = g_strdup_printf ("Mono");	break;
			case 2: ch = g_strdup_printf ("Stereo"); break;
			default : ch = g_strdup_printf ("%d channels", channels); break;
		}
	}
	gtk_label_set_text (GTK_LABEL (win->stream_properties_label),
			g_strdup_printf ("%s | %d Hz | %d kbps | %s",
					ch ? ch : NULL,
					samplerate ? samplerate : NULL,
					bitrate ? bitrate/1000 : NULL,
					codec ? codec : NULL ) );

	title = gst_player_media_info_get_title (media_info);
	if (!title) {
		filename =
			g_filename_from_uri (gst_player_media_info_get_uri (media_info),
									NULL, NULL);
		basename = g_path_get_basename (filename);
	}
	else {
		if (artist)
			title = g_strdup_printf ("%s - %s", title, artist);
	}
	gtk_label_set_text (GTK_LABEL (win->window_title_label), title ? title : basename);

	g_free (basename);
	g_free (filename);

	pixbuf = get_cover_image (media_info);
	if (pixbuf) {
		pixbuf = gdk_pixbuf_scale_simple (pixbuf, 300, 300, GDK_INTERP_BILINEAR);
		gtk_image_set_from_pixbuf (GTK_IMAGE(win->cover_art_image),  pixbuf);
		g_object_unref (pixbuf);
	}
	if (win->audiobook) {
		update_position_label (GTK_LABEL (win->total_dur_label), gst_player_get_duration (win->player) / GST_SECOND);
		gtk_widget_show(GTK_BOX (win->status_box));
		gtk_widget_show(GTK_SCROLLED_WINDOW (win->tree_scroll_win));
		gtk_widget_show(GTK_PROGRESS_BAR (win->progress));

	}
	else {
		gtk_widget_hide(GTK_BOX (win->status_box));
		gtk_widget_hide(GTK_SCROLLED_WINDOW (win->tree_scroll_win));
		gtk_widget_hide(GTK_PROGRESS_BAR (win->progress));
		gtk_window_resize (win, 500, 1);
	}
	if (genre || date)
		gtk_widget_show(GTK_BOX (win->genre_box));
	else
		gtk_widget_hide(GTK_BOX (win->genre_box));
}

static void gst_volume_changed_handler(GstPlayer * unused, AuditeAppWindow *win) {

  gdouble new_val, cur_val;

  cur_val = gtk_scale_button_get_value (GTK_SCALE_BUTTON (win->volume_button));
  new_val = gst_player_get_volume (win->player);

  if (fabs (cur_val - new_val) > 0.001) {
    g_signal_handlers_block_by_func (win->volume_button,
        volume_button_value_changed_handler, win);
    gtk_scale_button_set_value (GTK_SCALE_BUTTON (win->volume_button),
        new_val);
    g_signal_handlers_unblock_by_func (win->volume_button,
        volume_button_value_changed_handler, win);
  }
}

static void gst_duration_changed_handler (GstPlayer * unused, GstClockTime duration, AuditeAppWindow *win) {

	if (!(win->audiobook))
		seek_bar_set_range (win, 0, duration / GST_SECOND);
}

static void gst_position_updated_handler(GstPlayer * unused, GstClockTime position, AuditeAppWindow *win) {

	if (win->audiobook) {
		gtk_progress_bar_set_fraction ( (GtkProgressBar *) (win->progress),
			(gdouble) position / gst_player_get_duration (win->player));
		update_position_label (GTK_LABEL (win->pos_label), position / GST_SECOND);


		if ( (position/GST_SECOND >= win->current_chapter_end)
				|| (position/GST_SECOND < win->current_chapter_start) ) {// if out of range
			set_curent_chapter(win, position);
		}
		update_position_label (GTK_LABEL (win->elapsed_time_label),
			(position  / GST_SECOND - (gint64) win->current_chapter_start) );
		update_position_label (GTK_LABEL (win->remain_time_label),
			( (gint64) win->current_chapter_end - (gint64) win->current_chapter_start ) );
	}
	else {
		update_position_label (GTK_LABEL (win->elapsed_time_label), position / GST_SECOND);
		update_position_label (GTK_LABEL (win->remain_time_label),
		GST_CLOCK_DIFF (position, gst_player_get_duration (win->player)) / GST_SECOND);
	}
	g_signal_handlers_block_by_func (win->seek_bar,	seek_bar_value_changed_handler, win);
	gtk_range_set_value (GTK_RANGE (win->seek_bar),
		(gdouble) position / GST_SECOND);
	g_signal_handlers_unblock_by_func (win->seek_bar, seek_bar_value_changed_handler, win);
}

static void gst_state_changed_handler (GstPlayer * unused, GstPlayerState state, AuditeAppWindow *win) {

	gint rc = strcmp(gst_player_state_get_name (state), "playing");
	if (rc == 0) {
		win->playing = TRUE;
		gtk_button_set_image (GTK_BUTTON (win->play_button), win->pause_image);
	}
	else {
		win->playing = FALSE;
		gtk_button_set_image (GTK_BUTTON (win->play_button), win->play_image);
	}
}

static void gst_media_eos_handler (GstPlayer * unused, AuditeAppWindow *win) {

	gst_player_seek (win->player, gst_util_uint64_scale ( (gdouble) 0, GST_SECOND, 1) );
	gst_player_pause (win->player);
	gtk_button_set_image (GTK_BUTTON (win->play_button), win->play_image);
}

static inline void seekbar_add_delta (AuditeAppWindow *win, gint delta_sec) {

  gdouble value = gtk_range_get_value (GTK_RANGE (win->seek_bar));
  gtk_range_set_value (GTK_RANGE (win->seek_bar), value + delta_sec);
}

static void mp4v2_get_chapters (AuditeAppWindow *win) {

    gchar *f = g_filename_from_uri (win->current_uri, NULL, NULL);
    gchar *dur_hh_mm_ss;
    gint index = 0, chapterCount = 0;
    guint64 dur = 0, startpos = 0, endpos = 0;
    GtkTreeIter iter;
    GError *error = NULL;

    MP4FileHandle file = MP4Read( f );
    if( file == MP4_INVALID_FILE_HANDLE ) {
        printf( "MP4Read failed\n" );
        return;
    }
    MP4Chapter_t *chapterList;
    MP4ChapterType chapters = MP4GetChapters( file, &chapterList, &chapterCount, MP4ChapterTypeQt );
    if (chapterCount == 0) {
		MP4Close( file, 0 );
		printf( "Chapters not found\n" );
		return;
    }
    win->audiobook = TRUE;
    win->amount_of_chapters = chapterCount;
    win->current_chapter_number = 0;
	/* fill chapter list */
    for(index; index < chapterCount; index++) {

	dur = chapterList[index].duration;
	dur_hh_mm_ss = seconds_to_hhmmss (dur/1000);
	endpos = endpos + dur;

	gtk_list_store_append(GTK_LIST_STORE( win->chapter_list_store ), &iter);
	gtk_list_store_set(GTK_LIST_STORE(win->chapter_list_store), &iter, 
			ICON, (index == 0) ? "►" : NULL,  //set icon to first row only
			NUMBER,   (index+1),
			NAME,      chapterList[index].title,
			DURATION,  dur_hh_mm_ss,
			START,    (startpos == 0) ? 0 : startpos/MP4V2_SECOND,
			END,      (endpos/MP4V2_SECOND),
			-1 );
	startpos = endpos;
	if (index == 0)
	        win->current_chapter_end = endpos/MP4V2_SECOND;
    }
    set_curent_chapter (win, GST_SECOND);
    MP4Close( file, 0 );
    g_free(f);
    g_free(dur_hh_mm_ss);
    if (error != NULL)
	    g_error_free (error);
}

static void audite_app_window_init (AuditeAppWindow *win) {

  GtkBuilder *builder;
  GMenuModel *menu;
  GAction *action;
 
  gtk_widget_init_template (GTK_WIDGET (win));
  win->settings = g_settings_new ("com.github.alkesta.audite");

  builder = gtk_builder_new_from_resource ("/com/github/alkesta/audite/gears-menu.ui");
  menu = G_MENU_MODEL (gtk_builder_get_object (builder, "menu"));
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (win->gears), menu);
  g_object_unref (builder);

  action = g_settings_create_action (win->settings, "show-words");
  g_action_map_add_action (G_ACTION_MAP (win), action);
  g_object_unref (action);

//  action = (GAction*) g_property_action_new.........

  g_object_set (gtk_settings_get_default (), "gtk-shell-shows-app-menu", FALSE, NULL);
  gtk_application_window_set_show_menubar (GTK_APPLICATION_WINDOW (win), TRUE);
}

static GObject *
audite_app_window_constructor (GType type, guint n_construct_params,
    GObjectConstructParam * construct_params) {

  AuditeAppWindow *win;

  win = (AuditeAppWindow *) G_OBJECT_CLASS (audite_app_window_parent_class)->constructor (type,
 					     n_construct_params, construct_params);
  g_clear_object (&win->settings);

  win->player = gst_player_new (NULL,
		gst_player_g_main_context_signal_dispatcher_new (NULL));

  g_signal_connect (GST_PLAYER(win->player),
			"position-updated",
			G_CALLBACK (gst_position_updated_handler),
			win);
  g_signal_connect (GST_PLAYER(win->player),
			"duration-changed",
			G_CALLBACK (gst_duration_changed_handler),
			win);
  g_signal_connect (GST_PLAYER(win->player),
			"end-of-stream",
			G_CALLBACK (gst_media_eos_handler),
			win);
  g_signal_connect (GST_PLAYER(win->player),
			"media-info-updated",
			G_CALLBACK (gst_media_info_updated_handler),
			win);
  g_signal_connect (GST_PLAYER(win->player),
			"volume-changed",
			G_CALLBACK (gst_volume_changed_handler),
			win);
  g_signal_connect (GST_PLAYER(win->player),
			"state-changed",
			G_CALLBACK (gst_state_changed_handler),
			win);

  return G_OBJECT (win);
}

static void
audite_app_window_class_init (AuditeAppWindowClass *class)
{
 // G_OBJECT_CLASS (class)->dispose = audite_app_window_dispose;
  G_OBJECT_CLASS (class)->constructor = audite_app_window_constructor;

  gtk_widget_class_set_template_from_resource (GTK_WIDGET_CLASS (class),
                                               "/com/github/alkesta/audite/window.ui");

  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, gears);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, volume_button);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, previous_button);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, rewind_button);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, play_button);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, pause_image);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, play_image);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, forward_button);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, next_button);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, chapter_count_label);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, window_title_label);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, elapsed_time_label);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, remain_time_label);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, seek_bar);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, cover_art_image);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, chapter_list_store);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, chapters_tree_view);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, progress);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, genre_value_label);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, year_value_label);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, stream_properties_label);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, pos_label);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, total_dur_label);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, status_box);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, genre_box);
  gtk_widget_class_bind_template_child (GTK_WIDGET_CLASS (class), AuditeAppWindow, tree_scroll_win);


  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), seek_bar_value_changed_handler);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), play_button_clicked_handler);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), volume_button_value_changed_handler);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), forward_button_clicked_handler);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), rewind_button_clicked_handler);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), row_activated_handler);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), previous_button_clicked_handler);
  gtk_widget_class_bind_template_callback (GTK_WIDGET_CLASS (class), next_button_clicked_handler);
}

AuditeAppWindow *
audite_app_window_new (AuditeApp *app) {

  return g_object_new (AUDITE_APP_WINDOW_TYPE, "application", app, NULL);
}

void audite_app_window_open (AuditeAppWindow *win, gchar *uri) {

	gchar *filename;
	win->current_uri = uri;
	win->current_chapter_number = -1;
	win->amount_of_chapters = -1;
	win->current_chapter_number = -1;
	win->current_chapter_end = 0;
	win->current_chapter_start = 0;

	/* restore ui */
	gtk_list_store_clear (GTK_LIST_STORE(win->chapter_list_store));
	gtk_image_clear (GTK_IMAGE(win->cover_art_image));
	gtk_label_set_text (GTK_LABEL (win->window_title_label), "m4b Player");
	gtk_label_set_text (GTK_LABEL (win->chapter_count_label), NULL);
	gtk_progress_bar_set_fraction ( (GtkProgressBar *) (win->progress), 0);
	win->audiobook = FALSE;

	gst_player_set_uri (win->player,  uri);
	seek_bar_set_range (win, 0, 10);
	gst_player_play (win->player);
	
	if ( is_mp4_container (uri) )
			mp4v2_get_chapters (win);
}

static void seek_bar_value_changed_handler (GtkRange * range, gpointer data) {
	AuditeAppWindow *win = data;

	gdouble value = gtk_range_get_value (GTK_RANGE (win->seek_bar));
	gst_player_seek (win->player, gst_util_uint64_scale (value, GST_SECOND, 1));
}

static void play_button_clicked_handler (GtkButton * button, AuditeAppWindow *win) {

	if (win->playing)
		gst_player_pause(GST_PLAYER(win->player));
	else
		gst_player_play(GST_PLAYER(win->player));
}

static void forward_button_clicked_handler (GtkButton * button, AuditeAppWindow *win) {
	seekbar_add_delta (win, 10);
}

static void rewind_button_clicked_handler (GtkButton * button, AuditeAppWindow *win) {
	seekbar_add_delta (win, -10);
}

static void previous_button_clicked_handler (GtkButton * button, AuditeAppWindow *win) {

	if (win->current_chapter_number != 0)
		set_chapter (win, -1);
}

static void next_button_clicked_handler (GtkButton * button, AuditeAppWindow *win) {

	if (win->current_chapter_number != win->amount_of_chapters)
		set_chapter (win, 1);
}

static GdkPixbuf * get_cover_image (GstPlayerMediaInfo * media_info) {
  GstSample *sample;
  GstMapInfo info;
  GstBuffer *buffer;
  GError *err = NULL;
  GdkPixbufLoader *loader;
  GdkPixbuf *pixbuf = NULL;
  const GstStructure *caps_struct;
  GstTagImageType type = GST_TAG_IMAGE_TYPE_UNDEFINED;
  /* get image sample buffer from media */

  sample = gst_player_media_info_get_image_sample (media_info);
  if (!sample)
	return NULL;
  buffer = gst_sample_get_buffer (sample);
  caps_struct = gst_sample_get_info (sample);
  /* if sample is retrieved from preview-image tag then caps struct
   * will not be defined. */
  if (caps_struct)
    gst_structure_get_enum (caps_struct, "image-type",
        GST_TYPE_TAG_IMAGE_TYPE, &type);
  /* FIXME: Should we check more type ?? */
  if ((type != GST_TAG_IMAGE_TYPE_FRONT_COVER) &&
      (type != GST_TAG_IMAGE_TYPE_UNDEFINED) &&
      (type != GST_TAG_IMAGE_TYPE_NONE)) {
    g_print ("unsupport type ... %d \n", type);
    return NULL;
  }
  if (!gst_buffer_map (buffer, &info, GST_MAP_READ)) {
    g_print ("failed to map gst buffer \n");
    return NULL;
  }
  loader = gdk_pixbuf_loader_new ();
  if (gdk_pixbuf_loader_write (loader, info.data, info.size, &err) &&
      gdk_pixbuf_loader_close (loader, &err)) {
    pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
    if (pixbuf) {
      g_object_ref (pixbuf);
    } else {
      g_print ("failed to convert gst buffer to pixbuf %s \n", err->message);
      g_error_free (err);
    }
  }
  g_object_unref (loader);
  gst_buffer_unmap (buffer, &info);
  return pixbuf;
}

static gchar * seconds_to_hhmmss (guint64 seconds) {

	gchar *hhmmss;
	guint hrs, mins;

	hrs = seconds / 3600;
	seconds -= hrs * 3600;
	mins = seconds / 60;
	seconds -= mins * 60;

	if (hrs)
		hhmmss = g_strdup_printf ("%d:%02d:%02" G_GUINT64_FORMAT, hrs, mins, seconds);
	else
		hhmmss = g_strdup_printf ("%02d:%02" G_GUINT64_FORMAT, mins, seconds);

	return hhmmss;
}

static void seek_bar_set_range (AuditeAppWindow *win, guint64 start, guint64 end) {

	g_signal_handlers_block_by_func (win->seek_bar,
   				seek_bar_value_changed_handler, win);
	gtk_range_set_range (GTK_RANGE (win->seek_bar), (gdouble) start, (gdouble) end);
	g_signal_handlers_unblock_by_func (win->seek_bar,
				seek_bar_value_changed_handler, win);
}

static void set_curent_chapter (AuditeAppWindow *win, GstClockTime position) {

	GtkTreePath *path;
	GtkTreeIter iter;
	gint index = 0, chapter_amount, number;
	gchar *title, *i;
	GtkTreeModel *model;
	guint64 start_c, end_c;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(win->chapters_tree_view));
	chapter_amount = win->amount_of_chapters;
	for(index; index < chapter_amount; index++) { // find valid range
 		path = gtk_tree_path_new_from_indices (index, -1);
  		gtk_tree_model_get_iter (model, &iter, path);
    		gtk_tree_model_get(model, &iter,
        				NUMBER, &number,
					NAME,   &title,
					START,  &start_c,
					END,    &end_c,
					-1);
		gtk_list_store_set (GTK_LIST_STORE(win->chapter_list_store), &iter,
					ICON, NULL,
		                        -1);

		if ( (position/GST_SECOND < end_c) && (position/GST_SECOND >= start_c) ) { // if valid range found
			win->current_chapter_start = start_c;
			win->current_chapter_end = end_c;
			win->current_chapter_number = number;
			gtk_label_set_text (GTK_LABEL (win->chapter_count_label),
				g_strdup_printf ("%u / %u", win->current_chapter_number, win->amount_of_chapters));
			seek_bar_set_range (win, start_c, end_c);
			gtk_list_store_set (GTK_LIST_STORE(win->chapter_list_store), &iter,
						ICON, "►",
			                        -1);
gtk_tree_view_set_cursor (GTK_TREE_VIEW(win->chapters_tree_view),
                          path,
                          NULL,
                          FALSE);
		}
	}
}

static void update_position_label (GtkLabel * label, guint64 seconds) {

	gchar *data = seconds_to_hhmmss (seconds);
	gtk_label_set_text (label, data);
	g_free (data);
}

static void set_chapter (AuditeAppWindow *win, gint next) {

	GtkTreePath *path;
	GtkTreeIter iter;
	gint index = 0, chapter_amount, current_chapter, number;
	gchar *title, *i;
	GtkTreeModel *model;
	guint64 start, end;
	chapter_amount = win->amount_of_chapters;
	current_chapter = win->current_chapter_number;
	model = gtk_tree_view_get_model(GTK_TREE_VIEW (win->chapters_tree_view));
	for (index; index < chapter_amount; index++) { 
 		path = gtk_tree_path_new_from_indices (index, -1);
  		gtk_tree_model_get_iter (model, &iter, path);
    		gtk_tree_model_get(model, &iter,
        				NUMBER, &number,
					NAME,   &title,
					START,  &start,
					END,    &end,
					-1);
		if (number == (current_chapter + next) ) {
			gst_player_seek ( win->player, gst_util_uint64_scale ( (gdouble) start, GST_SECOND, 1) );
			break;
		}
	}
}

gboolean is_mp4_container (gchar* uri) {

	const gchar mp4ftyp[4] = {0x66, 0x74, 0x79, 0x70};
	gchar typ[4];
	gshort i = 0;
	FILE *file = NULL;

	file = fopen (
		g_filename_from_uri (uri, NULL, NULL),
		"rb" );

	if ( file ) {
		fseek ( file, 4, SEEK_SET );
		for ( i = 0; i < 4; i++ ) {
			fread ( &typ[i], 1, 1, file );
			if ( mp4ftyp[i] != typ[i] ) {
				fclose ( file );
				return FALSE;
			}
		}
		fclose ( file );
		return TRUE;
	}
	return FALSE;
}

