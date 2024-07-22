/*
 *      runscript.c
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
	#include "config.h"		// for the gettext domain
#endif

#include <geanyplugin.h>	// includes geany.h

#include "../../utils/src/ui_plugins.h"


GeanyPlugin	*geany_plugin;
GeanyData	*geany_data;

typedef struct {
	/* settings */
	gchar *script_1;
	gchar *script_2;
	gchar *script_3;
	gchar *script_4;
	gchar *script_5;
	gchar *script_6;
	gchar *script_7;
	gchar *script_8;
	gchar *script_9;
	gchar *script_10;
	gchar *script_11;
	gchar *script_12;
	gchar *script_13;
	gchar *script_14;
	gchar *script_15;
	gchar *script_16;
	/* others */
	gchar *config_file;
} RunscriptInfo;

static RunscriptInfo *rs_info = NULL;

/* keybindings */
enum
{
	KB_script_1,
	KB_script_2,
	KB_script_3,
	KB_script_4,
	KB_script_5,
	KB_script_6,
	KB_script_7,
	KB_script_8,
	KB_script_9,
	KB_script_10,
	KB_script_11,
	KB_script_12,
	KB_script_13,
	KB_script_14,
	KB_script_15,
	KB_script_16,
	KB_COUNT
};


static void configure_response_cb(GtkDialog *dialog, gint response,
								  gpointer user_data)
{
	if (response != GTK_RESPONSE_OK && response != GTK_RESPONSE_APPLY)
		return;
	GKeyFile *config	 = g_key_file_new();
	gchar    *config_dir = g_path_get_dirname(rs_info->config_file);
	
	g_key_file_load_from_file(config, rs_info->config_file,
							  G_KEY_FILE_NONE, NULL);
	
#define SAVE_CONF_TEXT(name) G_STMT_START {									\
	rs_info->name = gtk_editable_get_chars(									\
						GTK_EDITABLE(g_object_get_data(G_OBJECT(dialog),	\
													   "entry_" #name)),	\
						0, -1);												\
	g_key_file_set_string(config, "runscript", #name, rs_info->name);		\
} G_STMT_END
	
	SAVE_CONF_TEXT(script_1);
	SAVE_CONF_TEXT(script_2);
	SAVE_CONF_TEXT(script_3);
	SAVE_CONF_TEXT(script_4);
	SAVE_CONF_TEXT(script_5);
	SAVE_CONF_TEXT(script_6);
	SAVE_CONF_TEXT(script_7);
	SAVE_CONF_TEXT(script_8);
	SAVE_CONF_TEXT(script_9);
	SAVE_CONF_TEXT(script_10);
	SAVE_CONF_TEXT(script_11);
	SAVE_CONF_TEXT(script_12);
	SAVE_CONF_TEXT(script_13);
	SAVE_CONF_TEXT(script_14);
	SAVE_CONF_TEXT(script_15);
	SAVE_CONF_TEXT(script_16);
#undef SAVE_CONF_TEXT
	
	if (!g_file_test(config_dir, G_FILE_TEST_IS_DIR) &&
		utils_mkdir(config_dir, TRUE) != 0)
	{
		dialogs_show_msgbox(GTK_MESSAGE_ERROR,
			_("Plugin configuration directory could not be created."));
	}
	else
	{	/* write config to file */
		gchar *data;
		data = g_key_file_to_data(config, NULL, NULL);
		utils_write_file(rs_info->config_file, data);
		g_free(data);
	}
	g_free(config_dir);
	g_key_file_free(config);
}

static void run_script(const gchar *script, const gchar *filepath,
					   const gchar *projpath)
{
	if (EMPTY(script))
		return;
	
	gchar *argv[3] = { (gchar *)filepath, (gchar *)projpath, NULL };
	
	GError *error = NULL;
	if (!spawn_async(NULL, script, argv, NULL, NULL, &error))
	{
		ui_set_statusbar_color(TRUE, COLOR_RED,
							   _("Unable to execute script '%s' (%s)."),
							     script, error->message);
		g_error_free(error);
	}
}

/* Called when a keybinding is activated */
static void kb_activate(guint key_id)
{
	GeanyDocument *doc = document_get_current();
	GeanyProject *project = geany->app->project;
	if (!doc && !project)
		return;
	
	const gchar *filepath = doc && !EMPTY(doc->real_path) ?
								doc->real_path : G_DIR_SEPARATOR_S;
	const gchar *projpath = project && !EMPTY(project->base_path) ?
								project->base_path : G_DIR_SEPARATOR_S;
	
	switch (key_id)
	{
#define CASE_KEY_ID(name) G_STMT_START {				\
	case KB_##name:										\
		run_script(rs_info->name, filepath, projpath);	\
		return;											\
} G_STMT_END
	CASE_KEY_ID(script_1);
	CASE_KEY_ID(script_2);
	CASE_KEY_ID(script_3);
	CASE_KEY_ID(script_4);
	CASE_KEY_ID(script_5);
	CASE_KEY_ID(script_6);
	CASE_KEY_ID(script_7);
	CASE_KEY_ID(script_8);
	CASE_KEY_ID(script_9);
	CASE_KEY_ID(script_10);
	CASE_KEY_ID(script_11);
	CASE_KEY_ID(script_12);
	CASE_KEY_ID(script_13);
	CASE_KEY_ID(script_14);
	CASE_KEY_ID(script_15);
	CASE_KEY_ID(script_16);
#undef CASE_KEY_ID
	}
}

/* Called by Geany to initialize the plugin */
static gboolean plugin_runscript_init(GeanyPlugin *plugin,
									  G_GNUC_UNUSED gpointer pdata)
{
	geany_plugin = plugin;
	geany_data = plugin->geany_data;
	
	GKeyFile *config = g_key_file_new();
	GeanyKeyGroup *key_group;
	
	key_group = plugin_set_key_group(geany_plugin, "runscript",
									 KB_COUNT, NULL);
	
	rs_info = g_new0(RunscriptInfo, 1);
	
	rs_info->config_file = g_strconcat(geany->app->configdir,
									   G_DIR_SEPARATOR_S, "plugins",
									   G_DIR_SEPARATOR_S, "runscript",
									   G_DIR_SEPARATOR_S, "runscript.conf",
									   NULL);
	
	g_key_file_load_from_file(config, rs_info->config_file,
							  G_KEY_FILE_NONE, NULL);
	
#define GET_CONF_TEXT(name, hotkey_text) G_STMT_START {				\
	rs_info->name = utils_get_setting_string(config, "runscript",	\
											 #name, NULL);			\
	keybindings_set_item(key_group, KB_##name, kb_activate,			\
						 0, 0, #name, hotkey_text, NULL);			\
} G_STMT_END
	
	GET_CONF_TEXT(script_1, _("Script 1"));
	GET_CONF_TEXT(script_2, _("Script 2"));
	GET_CONF_TEXT(script_3, _("Script 3"));
	GET_CONF_TEXT(script_4, _("Script 4"));
	GET_CONF_TEXT(script_5, _("Script 5"));
	GET_CONF_TEXT(script_6, _("Script 6"));
	GET_CONF_TEXT(script_7, _("Script 7"));
	GET_CONF_TEXT(script_8, _("Script 8"));
	GET_CONF_TEXT(script_9, _("Script 9"));
	GET_CONF_TEXT(script_10, _("Script 10"));
	GET_CONF_TEXT(script_11, _("Script 11"));
	GET_CONF_TEXT(script_12, _("Script 12"));
	GET_CONF_TEXT(script_13, _("Script 13"));
	GET_CONF_TEXT(script_14, _("Script 14"));
	GET_CONF_TEXT(script_15, _("Script 15"));
	GET_CONF_TEXT(script_16, _("Script 16"));
#undef GET_CONF_TEXT
	
	g_key_file_free(config);
	return TRUE;
}

static GtkWidget *plugin_runscript_configure(G_GNUC_UNUSED GeanyPlugin *plugin,
											 GtkDialog *dialog,
											 G_GNUC_UNUSED gpointer pdata)
{
	GtkWidget *vbox, *entry;
	
	vbox = gtk_vbox_new(FALSE, 0);
	
#define WIDGET_CONF_TEXT(name, label_text) G_STMT_START {							\
	entry = add_inputbox(vbox, label_text, rs_info->name, -1,						\
						 _("rabbitvcs-diff, rabbitvcs-log, ..."), FALSE, FALSE);	\
	g_object_set_data(G_OBJECT(dialog), "entry_" #name, entry);						\
} G_STMT_END
	
	WIDGET_CONF_TEXT(script_1, _("Script 1"));
	WIDGET_CONF_TEXT(script_2, _("Script 2"));
	WIDGET_CONF_TEXT(script_3, _("Script 3"));
	WIDGET_CONF_TEXT(script_4, _("Script 4"));
	WIDGET_CONF_TEXT(script_5, _("Script 5"));
	WIDGET_CONF_TEXT(script_6, _("Script 6"));
	WIDGET_CONF_TEXT(script_7, _("Script 7"));
	WIDGET_CONF_TEXT(script_8, _("Script 8"));
	WIDGET_CONF_TEXT(script_9, _("Script 9"));
	WIDGET_CONF_TEXT(script_10, _("Script 10"));
	WIDGET_CONF_TEXT(script_11, _("Script 11"));
	WIDGET_CONF_TEXT(script_12, _("Script 12"));
	WIDGET_CONF_TEXT(script_13, _("Script 13"));
	WIDGET_CONF_TEXT(script_14, _("Script 14"));
	WIDGET_CONF_TEXT(script_15, _("Script 15"));
	WIDGET_CONF_TEXT(script_16, _("Script 16"));
#undef WIDGET_CONF_TEXT
	
	g_signal_connect(dialog, "response",
					 G_CALLBACK(configure_response_cb), NULL);
	
	gtk_widget_show_all(vbox);
	return vbox;
}

/* Called by Geany before unloading the plugin. */
static void plugin_runscript_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin,
									 G_GNUC_UNUSED gpointer pdata)
{
	g_free(rs_info->config_file);
	g_free(rs_info->script_1);
	g_free(rs_info->script_2);
	g_free(rs_info->script_3);
	g_free(rs_info->script_4);
	g_free(rs_info->script_5);
	g_free(rs_info->script_6);
	g_free(rs_info->script_7);
	g_free(rs_info->script_8);
	g_free(rs_info->script_9);
	g_free(rs_info->script_10);
	g_free(rs_info->script_11);
	g_free(rs_info->script_12);
	g_free(rs_info->script_13);
	g_free(rs_info->script_14);
	g_free(rs_info->script_15);
	g_free(rs_info->script_16);
	g_free(rs_info);
}


/* Load module */
G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
	/* Setup translation */
	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);
	
	/* Set metadata */
	plugin->info->name = _("Run Script");
	plugin->info->description = _("Run a script (bash, python, ...) "
								  "on the current doc using a hotkey");
	plugin->info->version = "0.1";
	plugin->info->author = "Egor Shinkarev <esh.eburg@gmail.com>";
	
	/* Set functions */
	plugin->funcs->init = plugin_runscript_init;
	plugin->funcs->cleanup = plugin_runscript_cleanup;
	plugin->funcs->configure = plugin_runscript_configure;
	
	/* Register! */
	GEANY_PLUGIN_REGISTER(plugin, 226);
}
