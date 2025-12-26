/*
 *      ao_copyfilename.c - this file is part of Addons, a Geany plugin
 *
 *      Copyright 2015 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
	#include "config.h"		// for the gettext domain
#endif

#include <geanyplugin.h>	// includes geany.h, gtkcompat.h, etc.

#include "addons.h"
#include "ao_copyfilename.h"


typedef struct _AoCopyFileNamePrivate AoCopyFileNamePrivate;

struct _AoCopyFileName
{
	GObject parent;
};

struct _AoCopyFileNameClass
{
	GObjectClass parent_class;
};

struct _AoCopyFileNamePrivate
{
	GtkWidget *tools_menu_item;
};


static void ao_copy_file_name_finalize(GObject *object);

G_DEFINE_TYPE_WITH_PRIVATE(AoCopyFileName, ao_copy_file_name, G_TYPE_OBJECT)


static void ao_copy_file_name_class_init(AoCopyFileNameClass *klass)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = ao_copy_file_name_finalize;
}


static void ao_copy_file_name_finalize(GObject *object)
{
	AoCopyFileNamePrivate *priv =
			ao_copy_file_name_get_instance_private((AoCopyFileName *)object);
	
	gtk_widget_destroy(priv->tools_menu_item);
	G_OBJECT_CLASS(ao_copy_file_name_parent_class)->finalize(object);
}


GtkWidget *ao_copy_file_name_get_menu_item(AoCopyFileName *self)
{
	AoCopyFileNamePrivate *priv = ao_copy_file_name_get_instance_private(self);
	
	return priv->tools_menu_item;
}


void ao_copy_file_name_copy(AoCopyFileName *self)
{
	GeanyDocument *doc = document_get_current();
	if (!doc) return;
	
	gchar *file_name = g_path_get_basename(DOC_FILENAME(doc));
	GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	GtkClipboard *primary = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text(clipboard, file_name, -1);
	gtk_clipboard_set_text(primary, file_name, -1);
	
	ui_set_statusbar(TRUE, _("File name \"%s\" copied to clipboard."), file_name);
	g_free(file_name);
}


static void copy_file_name_activated_cb(GtkMenuItem *item, AoCopyFileName *self)
{
	ao_copy_file_name_copy(self);
}


static void ao_copy_file_name_init(AoCopyFileName *self)
{
	AoCopyFileNamePrivate *priv = ao_copy_file_name_get_instance_private(self);
	
	priv->tools_menu_item = ui_image_menu_item_new(GTK_STOCK_COPY, _("Copy File Name"));
	
	gtk_widget_set_tooltip_text(priv->tools_menu_item, _("Copy the file name of the "
														 "current document to the clipboard"));
	gtk_widget_show(priv->tools_menu_item);
	
	g_signal_connect(priv->tools_menu_item, "activate",
					 G_CALLBACK(copy_file_name_activated_cb), self);
	
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu),
					  priv->tools_menu_item);
	
	ui_add_document_sensitive(priv->tools_menu_item);
}


AoCopyFileName *ao_copy_file_name_new(void)
{
	return g_object_new(AO_COPY_FILE_NAME_TYPE, NULL);
}
