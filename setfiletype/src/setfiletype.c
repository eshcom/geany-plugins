/*
 *      setfiletype.c
 *
 *      Copyright 2020 Egor Shinkarev <esh.eburg@gmail.com>
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
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
	#include "config.h" /* for the gettext domain */
#endif

#include <string.h>
#ifdef HAVE_LOCALE_H
	#include <locale.h>
#endif

#include <gdk/gdkkeysyms.h>

#include <geanyplugin.h>
#include <geany.h>

#include "Scintilla.h"
#include "SciLexer.h"
#include "filetypes.h"


GeanyPlugin	*geany_plugin;
GeanyData	*geany_data;


/* settings */
static gchar	*CONFIG_FILE	= NULL;
static gint		FILETYPE_1		= GEANY_FILETYPES_NONE;
static gint		FILETYPE_2		= GEANY_FILETYPES_NONE;
static gint		FILETYPE_3		= GEANY_FILETYPES_NONE;
static gint		FILETYPE_4		= GEANY_FILETYPES_NONE;
static gint		FILETYPE_5		= GEANY_FILETYPES_NONE;
static gint		FILETYPE_6		= GEANY_FILETYPES_NONE;
static gint		FILETYPE_7		= GEANY_FILETYPES_NONE;
static gint		FILETYPE_8		= GEANY_FILETYPES_NONE;
static gint		FILETYPE_9		= GEANY_FILETYPES_NONE;


static void configure_response_cb(GtkDialog *dialog, gint response,
								  gpointer user_data)
{
	if (response != GTK_RESPONSE_OK && response != GTK_RESPONSE_APPLY)
		return;
	GKeyFile *config = g_key_file_new();
	gchar    *config_dir = g_path_get_dirname(sft_info->config_file);
	
	g_key_file_load_from_file(config, sft_info->config_file, G_KEY_FILE_NONE, NULL);
	
#define SAVE_CONF_BOOL(name) G_STMT_START {                                    \
	sft_info->name = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(            \
						g_object_get_data(G_OBJECT(dialog), "check_" #name))); \
	g_key_file_set_boolean(config, "setfiletype", #name, sft_info->name);         \
} G_STMT_END
	
	SAVE_CONF_BOOL(parenthesis);
	SAVE_CONF_BOOL(abracket);
	SAVE_CONF_BOOL(abracket_htmlonly);
	SAVE_CONF_BOOL(cbracket);
	SAVE_CONF_BOOL(sbracket);
	SAVE_CONF_BOOL(dquote);
	SAVE_CONF_BOOL(squote);
	SAVE_CONF_BOOL(backquote);
	SAVE_CONF_BOOL(backquote_bashonly);
	SAVE_CONF_BOOL(comments_ac_enable);
	SAVE_CONF_BOOL(delete_pairing_brace);
	SAVE_CONF_BOOL(suppress_doubling);
	SAVE_CONF_BOOL(enclose_selections);
	SAVE_CONF_BOOL(comments_enclose);
	SAVE_CONF_BOOL(keep_selection);
	SAVE_CONF_BOOL(make_indent_for_cbracket);
	SAVE_CONF_BOOL(move_cursor_to_beginning);
	SAVE_CONF_BOOL(improved_cbracket_indent);
	SAVE_CONF_BOOL(whitesmiths_style);
	SAVE_CONF_BOOL(close_functions);
	SAVE_CONF_BOOL(bcksp_remove_pair);
	SAVE_CONF_BOOL(jump_on_tab);
	
#undef SAVE_CONF_BOOL
	
	if (!g_file_test(config_dir, G_FILE_TEST_IS_DIR) &&
		utils_mkdir(config_dir, TRUE) != 0)
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR,
			_("Plugin configuration directory could not be created."));
	}
	else
	{
		/* write config to file */
		gchar *data;
		data = g_key_file_to_data(config, NULL, NULL);
		utils_write_file(sft_info->config_file, data);
		g_free(data);
	}
	g_free(config_dir);
	g_key_file_free(config);
}

/* Called by Geany to initialize the plugin */
static gboolean plugin_setfiletype_init(GeanyPlugin *plugin,
										G_GNUC_UNUSED gpointer pdata)
{
	guint i = 0;
	
	geany_plugin = plugin;
	geany_data = plugin->geany_data;
	
	foreach_document(i)
	{
		on_document_open(NULL, documents[i], NULL);
	}
	GKeyFile *config = g_key_file_new();
	
	sft_info = g_new0(SetfiletypeInfo, 1);
	
	sft_info->config_file = g_strconcat(geany->app->configdir,
										G_DIR_SEPARATOR_S, "plugins",
										G_DIR_SEPARATOR_S, "setfiletype",
										G_DIR_SEPARATOR_S, "setfiletype.conf", NULL);
	
	g_key_file_load_from_file(config, sft_info->config_file, G_KEY_FILE_NONE, NULL);
	
#define GET_CONF_BOOL(name, def) sft_info->name = utils_get_setting_boolean(config, "setfiletype", #name, def)
	
	GET_CONF_BOOL(parenthesis, TRUE);
	/* Angular bracket conflicts with conditional statements, enable only for HTML by default */
	GET_CONF_BOOL(abracket, TRUE);
	GET_CONF_BOOL(abracket_htmlonly, TRUE);
	GET_CONF_BOOL(cbracket, TRUE);
	GET_CONF_BOOL(sbracket, TRUE);
	GET_CONF_BOOL(dquote, TRUE);
	GET_CONF_BOOL(squote, TRUE);
	GET_CONF_BOOL(backquote, TRUE);
	GET_CONF_BOOL(backquote_bashonly, TRUE);
	GET_CONF_BOOL(comments_ac_enable, FALSE);
	GET_CONF_BOOL(delete_pairing_brace, TRUE);
	GET_CONF_BOOL(suppress_doubling, TRUE);
	GET_CONF_BOOL(enclose_selections, TRUE);
	GET_CONF_BOOL(comments_enclose, FALSE);
	GET_CONF_BOOL(keep_selection, TRUE);
	GET_CONF_BOOL(make_indent_for_cbracket, TRUE);
	GET_CONF_BOOL(move_cursor_to_beginning, TRUE);
	GET_CONF_BOOL(improved_cbracket_indent, TRUE);
	GET_CONF_BOOL(whitesmiths_style, FALSE);
	GET_CONF_BOOL(close_functions, TRUE);
	GET_CONF_BOOL(bcksp_remove_pair, FALSE);
	GET_CONF_BOOL(jump_on_tab, TRUE);
	
#undef GET_CONF_BOOL
	
	g_key_file_free(config);
	return TRUE;
}

static GtkWidget *plugin_setfiletype_configure(G_GNUC_UNUSED GeanyPlugin *plugin,
											   GtkDialog *dialog,
											   G_GNUC_UNUSED gpointer pdata)
{
	GtkWidget *widget, *vbox, *frame, *container, *scrollbox;
	vbox = gtk_vbox_new(FALSE, 0);
	scrollbox = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_set_size_request(GTK_WIDGET(scrollbox), -1, 400);
#if GTK_CHECK_VERSION(3, 8, 0)
	gtk_container_add(GTK_CONTAINER(scrollbox), vbox);
#else
	gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrollbox), vbox);
#endif
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollbox),
		GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	
#define WIDGET_CONF_TEXT(name, description, tooltip) G_STMT_START {            \
	widget = gtk_check_button_new_with_label(description);                     \
	if (tooltip) gtk_widget_set_tooltip_text(widget, tooltip);                 \
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), sft_info->name);    \
	gtk_box_pack_start(GTK_BOX(vbox), widget, FALSE, FALSE, 3);           \
	g_object_set_data(G_OBJECT(dialog), "check_" #name, widget);               \
} G_STMT_END
	
	WIDGET_CONF_TEXT(jump_on_tab, _("File Type 1"), _("XML, JSON, Erlang, etc"));
	
#undef WIDGET_CONF_TEXT
	
	gtk_widget_show_all(scrollbox);
	return scrollbox;
}

/* Called by Geany before unloading the plugin. */
static void plugin_setfiletype_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin,
									   G_GNUC_UNUSED gpointer pdata)
{
	g_free(CONFIG_FILE);
}


/* Load module */
G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
	/* Setup translation */
	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);
	
	/* Set metadata */
	plugin->info->name = _("SetFileType");
	plugin->info->description = _("SetFileType plugin sets file type by hotkey");
	plugin->info->version = "0.1";
	plugin->info->author = "Egor Shinkarev <esh.eburg@gmail.com>";
	
	/* Set functions */
	plugin->funcs->init = plugin_setfiletype_init;
	plugin->funcs->cleanup = plugin_setfiletype_cleanup;
	plugin->funcs->configure = plugin_setfiletype_configure;
	
	/* Register! */
	GEANY_PLUGIN_REGISTER(plugin, 226);
}
