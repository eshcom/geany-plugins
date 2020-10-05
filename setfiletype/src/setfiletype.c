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

GeanyPlugin	*geany_plugin;
GeanyData	*geany_data;

typedef struct {
	/* settings */
	gchar *filetype_1;
	gchar *filetype_2;
	gchar *filetype_3;
	gchar *filetype_4;
	gchar *filetype_5;
	gchar *filetype_6;
	gchar *filetype_7;
	gchar *filetype_8;
	gchar *filetype_9;
	gchar *filetype_10;
	gchar *filetype_11;
	gchar *filetype_12;
	/* others */
	gchar *config_file;
} SetfiletypeInfo;

static SetfiletypeInfo *sft_info = NULL;

/* keybindings */
enum
{
	KB_filetype_1,
	KB_filetype_2,
	KB_filetype_3,
	KB_filetype_4,
	KB_filetype_5,
	KB_filetype_6,
	KB_filetype_7,
	KB_filetype_8,
	KB_filetype_9,
	KB_filetype_10,
	KB_filetype_11,
	KB_filetype_12,
	KB_COUNT
};


static void configure_response_cb(GtkDialog *dialog, gint response,
								  gpointer user_data)
{
	if (response != GTK_RESPONSE_OK && response != GTK_RESPONSE_APPLY)
		return;
	GKeyFile *config = g_key_file_new();
	gchar    *config_dir = g_path_get_dirname(sft_info->config_file);
	
	g_key_file_load_from_file(config, sft_info->config_file, G_KEY_FILE_NONE, NULL);
	
#define SAVE_CONF_TEXT(name) G_STMT_START {												\
	sft_info->name = gtk_editable_get_chars(GTK_EDITABLE(								\
						g_object_get_data(G_OBJECT(dialog), "entry_" #name)), 0, -1);	\
	g_key_file_set_string(config, "setfiletype", #name, sft_info->name);				\
} G_STMT_END
	
	SAVE_CONF_TEXT(filetype_1);
	SAVE_CONF_TEXT(filetype_2);
	SAVE_CONF_TEXT(filetype_3);
	SAVE_CONF_TEXT(filetype_4);
	SAVE_CONF_TEXT(filetype_5);
	SAVE_CONF_TEXT(filetype_6);
	SAVE_CONF_TEXT(filetype_7);
	SAVE_CONF_TEXT(filetype_8);
	SAVE_CONF_TEXT(filetype_9);
	SAVE_CONF_TEXT(filetype_10);
	SAVE_CONF_TEXT(filetype_11);
	SAVE_CONF_TEXT(filetype_12);
#undef SAVE_CONF_TEXT
	
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

/* Called when a keybinding is activated */
static void kb_activate(guint key_id)
{
	GeanyDocument *doc = document_get_current();
	if (!doc)
		return;
	switch (key_id)
	{
#define CASE_KEY_ID(name) G_STMT_START {											\
		case KB_##name:																\
			document_set_filetype(doc, filetypes_lookup_by_name(sft_info->name));	\
			return;																	\
} G_STMT_END
	CASE_KEY_ID(filetype_1);
	CASE_KEY_ID(filetype_2);
	CASE_KEY_ID(filetype_3);
	CASE_KEY_ID(filetype_4);
	CASE_KEY_ID(filetype_5);
	CASE_KEY_ID(filetype_6);
	CASE_KEY_ID(filetype_7);
	CASE_KEY_ID(filetype_8);
	CASE_KEY_ID(filetype_9);
	CASE_KEY_ID(filetype_10);
	CASE_KEY_ID(filetype_11);
	CASE_KEY_ID(filetype_12);
#undef CASE_KEY_ID
	}
}

/* Called by Geany to initialize the plugin */
static gboolean plugin_setfiletype_init(GeanyPlugin *plugin,
										G_GNUC_UNUSED gpointer pdata)
{
	geany_plugin = plugin;
	geany_data = plugin->geany_data;
	
	GKeyFile *config = g_key_file_new();
	GeanyKeyGroup *key_group;
	
	sft_info = g_new0(SetfiletypeInfo, 1);
	
	sft_info->config_file = g_strconcat(geany->app->configdir,
										G_DIR_SEPARATOR_S, "plugins",
										G_DIR_SEPARATOR_S, "setfiletype",
										G_DIR_SEPARATOR_S, "setfiletype.conf", NULL);
	
	g_key_file_load_from_file(config, sft_info->config_file, G_KEY_FILE_NONE, NULL);
	
	key_group = plugin_set_key_group(geany_plugin, "setfiletype", KB_COUNT, NULL);
	
#define GET_CONF_TEXT(name, hotkey_text) G_STMT_START {								\
	sft_info->name = utils_get_setting_string(config, "setfiletype", #name, NULL);	\
	keybindings_set_item(key_group, KB_##name, kb_activate,							\
						 0, 0, #name, hotkey_text, NULL);							\
} G_STMT_END
	
	GET_CONF_TEXT(filetype_1, _("File Type 1"));
	GET_CONF_TEXT(filetype_2, _("File Type 2"));
	GET_CONF_TEXT(filetype_3, _("File Type 3"));
	GET_CONF_TEXT(filetype_4, _("File Type 4"));
	GET_CONF_TEXT(filetype_5, _("File Type 5"));
	GET_CONF_TEXT(filetype_6, _("File Type 6"));
	GET_CONF_TEXT(filetype_7, _("File Type 7"));
	GET_CONF_TEXT(filetype_8, _("File Type 8"));
	GET_CONF_TEXT(filetype_9, _("File Type 9"));
	GET_CONF_TEXT(filetype_10, _("File Type 10"));
	GET_CONF_TEXT(filetype_11, _("File Type 11"));
	GET_CONF_TEXT(filetype_12, _("File Type 12"));
#undef GET_CONF_TEXT
	
	g_key_file_free(config);
	return TRUE;
}

static GtkWidget *plugin_setfiletype_configure(G_GNUC_UNUSED GeanyPlugin *plugin,
											   GtkDialog *dialog,
											   G_GNUC_UNUSED gpointer pdata)
{
	GtkWidget *vbox, *hbox, *label, *entry;
	
	vbox = gtk_vbox_new(FALSE, 0);
	
#define WIDGET_CONF_TEXT(name, description, tooltip) G_STMT_START {		\
	label = gtk_label_new(description);									\
	gtk_widget_set_size_request(label, 150, -1);						\
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);					\
	entry = gtk_entry_new();											\
	gtk_entry_set_text(GTK_ENTRY(entry), sft_info->name);				\
	if (tooltip) gtk_widget_set_tooltip_text(entry, tooltip);			\
	hbox = gtk_hbox_new(FALSE, 0);										\
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 6);			\
	gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);			\
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);			\
	g_object_set_data(G_OBJECT(dialog), "entry_" #name, entry);			\
} G_STMT_END
	
	WIDGET_CONF_TEXT(filetype_1, _("File Type 1"), _("XML, JSON, Erlang, ..."));
	WIDGET_CONF_TEXT(filetype_2, _("File Type 2"), _("XML, JSON, Erlang, ..."));
	WIDGET_CONF_TEXT(filetype_3, _("File Type 3"), _("XML, JSON, Erlang, ..."));
	WIDGET_CONF_TEXT(filetype_4, _("File Type 4"), _("XML, JSON, Erlang, ..."));
	WIDGET_CONF_TEXT(filetype_5, _("File Type 5"), _("XML, JSON, Erlang, ..."));
	WIDGET_CONF_TEXT(filetype_6, _("File Type 6"), _("XML, JSON, Erlang, ..."));
	WIDGET_CONF_TEXT(filetype_7, _("File Type 7"), _("XML, JSON, Erlang, ..."));
	WIDGET_CONF_TEXT(filetype_8, _("File Type 8"), _("XML, JSON, Erlang, ..."));
	WIDGET_CONF_TEXT(filetype_9, _("File Type 9"), _("XML, JSON, Erlang, ..."));
	WIDGET_CONF_TEXT(filetype_10, _("File Type 10"), _("XML, JSON, Erlang, ..."));
	WIDGET_CONF_TEXT(filetype_11, _("File Type 11"), _("XML, JSON, Erlang, ..."));
	WIDGET_CONF_TEXT(filetype_12, _("File Type 12"), _("XML, JSON, Erlang, ..."));
#undef WIDGET_CONF_TEXT
	
	g_signal_connect(dialog, "response", G_CALLBACK(configure_response_cb), NULL);
	gtk_widget_show_all(vbox);
	return vbox;
}

/* Called by Geany before unloading the plugin. */
static void plugin_setfiletype_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin,
									   G_GNUC_UNUSED gpointer pdata)
{
	g_free(sft_info->config_file);
	g_free(sft_info->filetype_1);
	g_free(sft_info->filetype_2);
	g_free(sft_info->filetype_3);
	g_free(sft_info->filetype_4);
	g_free(sft_info->filetype_5);
	g_free(sft_info->filetype_6);
	g_free(sft_info->filetype_7);
	g_free(sft_info->filetype_8);
	g_free(sft_info->filetype_9);
	g_free(sft_info->filetype_10);
	g_free(sft_info->filetype_11);
	g_free(sft_info->filetype_12);
	g_free(sft_info);
}


/* Load module */
G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
	/* Setup translation */
	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);
	
	/* Set metadata */
	plugin->info->name = _("Set FileType");
	plugin->info->description = _("Set file type (XML, JSON, Erlang, ...) by hotkey");
	plugin->info->version = "0.1";
	plugin->info->author = "Egor Shinkarev <esh.eburg@gmail.com>";
	
	/* Set functions */
	plugin->funcs->init = plugin_setfiletype_init;
	plugin->funcs->cleanup = plugin_setfiletype_cleanup;
	plugin->funcs->configure = plugin_setfiletype_configure;
	
	/* Register! */
	GEANY_PLUGIN_REGISTER(plugin, 226);
}
