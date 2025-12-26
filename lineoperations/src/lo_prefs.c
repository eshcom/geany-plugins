/*
 *      lo_prefs.c - Line operations, remove duplicate lines, empty lines,
 *                 lines with only whitespace, sort lines.
 *
 *      Copyright 2015 Sylvan Mostert <smostert.dev@gmail.com>
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
 *      You should have received a copy of the GNU General Public License along
 *      with this program; if not, write to the Free Software Foundation, Inc.,
 *      51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#ifdef HAVE_CONFIG_H
	#include "config.h"		// for the gettext domain
#endif

#include <geanyplugin.h>	// includes geany.h, gtkcompat.h, etc.

#include "lo_prefs.h"

#include "../../utils/src/common.h"
#include "../../utils/src/ui.h"


static struct
{
	GtkWidget *collation_cb;
} config_widgets;


LineOpsInfo *lo_info = NULL;


/* handle button presses in the preferences dialog box */
void lo_configure_response_cb(GtkDialog *dialog, gint response,
							  gpointer user_data)
{
	if (!ok_apply(response)) return;
	
	/* Grabbing options that has been set */
	lo_info->use_collation_compare = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
													config_widgets.collation_cb));
	
	/* Write preference to file */
	GKeyFile *config = load_config_from_file(lo_info->config_file, NULL);
	
	g_key_file_set_boolean(config, CONFIG_SECTION, "use_collation_compare",
						   lo_info->use_collation_compare);
	
	write_config_to_file(config, lo_info->config_file, MSGBOX);
	g_key_file_free(config);
}


/* Configure the preferences GUI and callbacks */
GtkWidget *lo_configure(G_GNUC_UNUSED GeanyPlugin *plugin, GtkDialog *dialog,
						G_GNUC_UNUSED gpointer pdata)
{
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
	
	config_widgets.collation_cb = add_checkbox(vbox,
		_("Use collation based string compare"), lo_info->use_collation_compare,
		_("If selected, g_utf8_collate will be used to compare strings, "
		  "otherwise g_strcmp0"), FALSE);
	
	g_signal_connect(dialog, "response",
					 G_CALLBACK(lo_configure_response_cb), NULL);
	
	gtk_widget_show_all(vbox);
	return vbox;
}


/* Initialize preferences */
void lo_init_prefs(GeanyPlugin *plugin)
{
	/* load preferences from file into lo_info */
	lo_info = g_new0(LineOpsInfo, 1);
	lo_info->config_file = get_config_filepath(PLUGIN, NULL);
	
	GKeyFile *config = load_config_from_file(lo_info->config_file, NULL);
	
	lo_info->use_collation_compare = utils_get_setting_boolean(config, CONFIG_SECTION,
															   "use_collation_compare",
															   FALSE);
	printf("VALUE: %d\n", lo_info->use_collation_compare);
	
	g_key_file_free(config);
}


/* Free config */
void lo_free_info()
{
	g_free(lo_info->config_file);
	g_free(lo_info);
}
