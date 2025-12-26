/*
 *
 *		dconfig.c
 *      
 *      Copyright 2011 Alexander Petukhov <devel(at)apetukhov.ru>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

/*
 *		Plugin panel and debug session configs
 */
 
#ifdef HAVE_CONFIG_H
	#include "config.h"		// for the gettext domain
#endif

#include <geanyplugin.h>	// includes geany.h, gtkcompat.h, etc.

#include "dconfig.h"
#include "tabs.h"
#include "breakpoints.h"
#include "debug.h"
#include "watch_model.h"
#include "wtree.h"
#include "tpage.h"
#include "bptree.h"

#include "../../utils/src/common.h"

extern GeanyData *geany_data;


/* keyfile debug group name */
#define DEBUGGER_GROUP "debugger"
/* saving interval */
#define SAVING_INTERVAL 2000000

/* check button for a configure dialog */
static GtkWidget *save_to_project_btn = NULL;

/* plugin config file path */
static gchar *plugin_config_path = NULL;

/* current debug session store */
static debug_store dstore = DEBUG_STORE_PLUGIN;

/* GKeyFile's for a project and plugin config */
static GKeyFile *plugin_config = NULL;
static GKeyFile *project_config = NULL;

/* flag that indicates that debug session is being loaded to controls
 * to prevent change state to modified from GUI callbacks */
static gboolean debug_config_loading = FALSE;

/* saving thread staff */
static GMutex change_config_mutex;
static GCond cond;
static GThread *saving_thread;

/* flags that indicate that part of a config has been changed and
 * is going to be saved on the savng thread */
static gboolean debug_config_changed = FALSE;
static gboolean panel_config_changed = FALSE;

/*
 * loads debug session from a keyfile and updates GUI 
 */
static void debug_load_from_keyfile(GKeyFile *keyfile)
{
	gchar *value;
	int i, count;
	
	debug_config_loading = TRUE;
	
	/* target */
	tpage_set_target(value = g_key_file_get_string(keyfile, DEBUGGER_GROUP, "target", NULL));
	g_free(value);
	/* debugger */
	tpage_set_debugger(value = g_key_file_get_string(keyfile, DEBUGGER_GROUP, "debugger", NULL));
	g_free(value);
	/* arguments */
	tpage_set_commandline(value = g_key_file_get_string(keyfile, DEBUGGER_GROUP, "arguments", NULL));
	g_free(value);

	/* environment */
	count = g_key_file_get_integer(keyfile, DEBUGGER_GROUP, "envvar_count", NULL);
	for (i = 0; i < count; i++)
	{
		gchar *env_name_id = g_strdup_printf("envvar_%i_name", i);
		gchar *env_value_id = g_strdup_printf("envvar_%i_value", i);
		gchar *name = g_key_file_get_string(keyfile, DEBUGGER_GROUP, env_name_id, NULL);

		value = g_key_file_get_string(keyfile, DEBUGGER_GROUP, env_value_id, NULL);

		tpage_add_environment(name, value);

		g_free(name);
		g_free(value);

		g_free(env_name_id);
		g_free(env_value_id);
	}

	/* watches */
	count = g_key_file_get_integer(keyfile, DEBUGGER_GROUP, "watches_count", NULL);
	for (i = 0; i < count; i++)
	{
		gchar *watch_id = g_strdup_printf("watch_%i", i);
		wtree_add_watch(value = g_key_file_get_string(keyfile, DEBUGGER_GROUP, watch_id, NULL));
		g_free(value);
		g_free(watch_id);
	}

	/* breakpoints */
	count = g_key_file_get_integer(keyfile, DEBUGGER_GROUP, "breaks_count", NULL);
	for (i = 0; i < count; i++)
	{
		gchar *break_file_id = g_strdup_printf("break_%i_file", i);
		gchar *break_line_id = g_strdup_printf("break_%i_line", i);
		gchar *break_condition_id = g_strdup_printf("break_%i_condition", i);
		gchar *break_hits_id = g_strdup_printf("break_%i_hits_count", i);
		gchar *break_enabled_id = g_strdup_printf("break_%i_enabled", i);
		
		gchar *file = g_key_file_get_string(keyfile, DEBUGGER_GROUP, break_file_id, NULL);
		int line = g_key_file_get_integer(keyfile, DEBUGGER_GROUP, break_line_id, NULL);
		gchar *condition = g_key_file_get_string(keyfile, DEBUGGER_GROUP, break_condition_id, NULL);
		int hits_count = g_key_file_get_integer(keyfile, DEBUGGER_GROUP, break_hits_id, NULL);
		gboolean enabled = g_key_file_get_boolean(keyfile, DEBUGGER_GROUP, break_enabled_id, NULL);
		
		breaks_add(file, line, condition, enabled, hits_count);

		g_free(break_file_id);
		g_free(break_line_id);
		g_free(break_condition_id);
		g_free(break_hits_id);
		g_free(break_enabled_id);
		
		g_free(file);
		g_free(condition);
	}
	bptree_update_file_nodes();

	debug_config_loading = FALSE;
}

/*
 * saves debug session to a keyfile using values from GUI 
 */
static void save_to_keyfile(GKeyFile *keyfile)
{
	g_key_file_remove_group(keyfile, DEBUGGER_GROUP, NULL);
	g_key_file_set_string(keyfile, DEBUGGER_GROUP, "target", tpage_get_target());
	g_key_file_set_string(keyfile, DEBUGGER_GROUP, "debugger", tpage_get_debugger());
	g_key_file_set_string(keyfile, DEBUGGER_GROUP, "arguments", tpage_get_commandline());
	
	/* environment */
	GList *_env = tpage_get_environment();
	g_key_file_set_integer(keyfile, DEBUGGER_GROUP, "envvar_count",
						   g_list_length(_env) / 2);
	GList *iter = _env;
	int env_index = 0;
	while(iter)
	{
		gchar *env_name_id = g_strdup_printf("envvar_%i_name", env_index);
		gchar *env_value_id = g_strdup_printf("envvar_%i_value", env_index);
		
		gchar *name = (gchar *)iter->data;
		iter = iter->next;
		gchar *value = (gchar *)iter->data;
		
		g_key_file_set_string(keyfile, DEBUGGER_GROUP, env_name_id, name);
		g_key_file_set_string(keyfile, DEBUGGER_GROUP, env_value_id, value);
		
		g_free(env_name_id);
		g_free(env_value_id);
		
		env_index++;
		iter = iter->next;
	}
	g_list_foreach(_env, (GFunc)g_free, NULL);
	g_list_free(_env);
	
	/* watches */
	GList *watches = wtree_get_watches();
	g_key_file_set_integer(keyfile, DEBUGGER_GROUP, "watches_count", g_list_length(watches));
	int watch_index = 0;
	for (iter = watches; iter; iter = iter->next)
	{
		gchar *watch = (gchar*)iter->data;
		gchar *watch_id = g_strdup_printf("watch_%i", watch_index);
		
		g_key_file_set_string(keyfile, DEBUGGER_GROUP, watch_id, watch);

		g_free(watch_id);

		watch_index++;
	}
	g_list_foreach(watches, (GFunc)g_free, NULL);
	g_list_free(watches);

	/* breakpoints */
	GList *_breaks = breaks_get_all();
	g_key_file_set_integer(keyfile, DEBUGGER_GROUP, "breaks_count", g_list_length(_breaks));
	int bp_index = 0;
	for (iter = _breaks; iter; iter = iter->next)
	{
		breakpoint *bp = (breakpoint*)iter->data;
		
		gchar *break_file_id = g_strdup_printf("break_%i_file", bp_index);
		gchar *break_line_id = g_strdup_printf("break_%i_line", bp_index);
		gchar *break_condition_id = g_strdup_printf("break_%i_condition", bp_index);
		gchar *break_hits_id = g_strdup_printf("break_%i_hits_count", bp_index);
		gchar *break_enabled_id = g_strdup_printf("break_%i_enabled", bp_index);
		
		g_key_file_set_string(keyfile, DEBUGGER_GROUP, break_file_id, bp->file);
		g_key_file_set_integer(keyfile, DEBUGGER_GROUP, break_line_id, bp->line);
		g_key_file_set_string(keyfile, DEBUGGER_GROUP, break_condition_id, bp->condition);
		g_key_file_set_integer(keyfile, DEBUGGER_GROUP, break_hits_id, bp->hitscount);
		g_key_file_set_boolean(keyfile, DEBUGGER_GROUP, break_enabled_id, bp->enabled);
		
		g_free(break_file_id);
		g_free(break_line_id);
		g_free(break_condition_id);
		g_free(break_hits_id);
		g_free(break_enabled_id);

		bp_index++;
	}
	g_list_free(_breaks);
}

/*
 * function for config files background saving
 */
static gpointer saving_thread_func(gpointer data)
{
	gint64 interval;
	g_mutex_lock(&change_config_mutex);
	do
	{
		if (panel_config_changed ||
			(debug_config_changed && DEBUG_STORE_PLUGIN == dstore))
		{
			/* if all saving is going to be done to a plugin keyfile */
			if (debug_config_changed)
			{
				save_to_keyfile(plugin_config);
				debug_config_changed = FALSE;
			}
			write_config_to_file(plugin_config, plugin_config_path, SYSLOG);
			panel_config_changed = FALSE;
		}
		
		if (debug_config_changed && DEBUG_STORE_PROJECT == dstore)
		{
			/* if debug is saved into a project and has been changed */
			save_to_keyfile(project_config);
			write_config_to_file(project_config, geany->app->project->file_name, SYSLOG);
			debug_config_changed = FALSE;
		}
		
		interval = g_get_monotonic_time() + SAVING_INTERVAL;
	}
	while (!g_cond_wait_until(&cond, &change_config_mutex, interval));
	g_mutex_unlock(&change_config_mutex);
	
	return NULL;
}

/*
 * set "debug changed" flag to save it on "saving_thread" thread
 */
void config_set_debug_changed(void)
{
	if (!debug_config_loading)
	{
		g_mutex_lock(&change_config_mutex);
		debug_config_changed = TRUE;
		g_mutex_unlock(&change_config_mutex);
	}
}

/*
 *	set one or several panel config values
 */
void config_set_panel(int config_part, gpointer config_value, ...)
{
	va_list ap;
	
	g_mutex_lock(&change_config_mutex);
	
	va_start(ap, config_value);
	
	while(config_part)
	{
		switch (config_part)
		{
			case CP_TABBED_MODE:
			{
				g_key_file_set_boolean(plugin_config, "tabbed_mode", "enabled", *((gboolean*)config_value));
				break;
			}
			case CP_OT_TABS:
			{
				int *array = (int*)config_value;
				g_key_file_set_integer_list(plugin_config, "one_panel_mode", "tabs", array + 1, array[0]);
				break;
			}
			case CP_OT_SELECTED:
			{
				g_key_file_set_integer(plugin_config, "one_panel_mode", "selected_tab_index", *((int*)config_value));
				break;
			}
			case CP_TT_LTABS:
			{
				int *array = (int*)config_value;
				g_key_file_set_integer_list(plugin_config, "two_panels_mode", "left_tabs", array + 1, array[0]);
				break;
			}
			case CP_TT_LSELECTED:
			{
				g_key_file_set_integer(plugin_config, "two_panels_mode", "left_selected_tab_index", *((int*)config_value));
				break;
			}
			case CP_TT_RTABS:
			{
				int *array = (int*)config_value;
				g_key_file_set_integer_list(plugin_config, "two_panels_mode", "right_tabs", array + 1, array[0]);
				break;
			}
			case CP_TT_RSELECTED:
			{
				g_key_file_set_integer(plugin_config, "two_panels_mode", "right_selected_tab_index", *((int*)config_value));
				break;
			}
		}
		
		config_part = va_arg(ap, int);
		if (config_part)
		{
			config_value = va_arg(ap, gpointer);
		}
	}
	
	va_end(ap);
	
	panel_config_changed = TRUE;
	g_mutex_unlock(&change_config_mutex);
}

/*
 *	set default debug session values to a keyfile
 */
static void config_set_debug_defaults(GKeyFile *keyfile)
{
	g_key_file_set_string(keyfile, DEBUGGER_GROUP, "target", "");
	g_key_file_set_string(keyfile, DEBUGGER_GROUP, "debugger", "");
	g_key_file_set_string(keyfile, DEBUGGER_GROUP, "arguments", "");

	g_key_file_set_integer(keyfile, DEBUGGER_GROUP, "envvar_count", 0);
	g_key_file_set_integer(keyfile, DEBUGGER_GROUP, "watches_count", 0);
	g_key_file_set_integer(keyfile, DEBUGGER_GROUP, "breaks_count", 0);
}

/*
 *	set default panel config values in a GKeyFile
 */
static void config_set_panel_defaults(GKeyFile *keyfile)
{
	int all_tabs[] = { TID_TARGET, TID_BREAKS, TID_AUTOS, TID_WATCH, TID_STACK, TID_TERMINAL, TID_MESSAGES };
	int left_tabs[] = { TID_TARGET, TID_BREAKS, TID_AUTOS, TID_WATCH };
	int right_tabs[] = { TID_STACK, TID_TERMINAL, TID_MESSAGES };

	g_key_file_set_boolean(plugin_config, "tabbed_mode", "enabled", FALSE);
	/* all tabs */
	g_key_file_set_integer_list(keyfile, "one_panel_mode", "tabs", all_tabs, sizeof(all_tabs) / sizeof(int));
	g_key_file_set_integer(keyfile, "one_panel_mode", "selected_tab_index", 0);
	/* left tabs */
	g_key_file_set_integer_list(keyfile, "two_panels_mode", "left_tabs", left_tabs, sizeof(left_tabs) / sizeof(int));
	g_key_file_set_integer(keyfile, "two_panels_mode", "left_selected_tab_index", 0);
	/* right tabs */
	g_key_file_set_integer_list(keyfile, "two_panels_mode", "right_tabs", right_tabs, sizeof(right_tabs) / sizeof(int));
	g_key_file_set_integer(keyfile, "two_panels_mode", "right_selected_tab_index", 0);

	g_key_file_set_boolean(keyfile, "saving_settings", "save_to_project", FALSE);
}

/*
 *	initialize
 */
void config_init(void)
{
	/* read config */
	plugin_config_path = get_config_filepath(PLUGIN, NULL);
	
	gboolean result = FALSE;
	plugin_config = load_config_from_file(plugin_config_path, &result);
	if (!result)
	{
		config_set_panel_defaults(plugin_config);
		write_config_to_file(plugin_config, plugin_config_path, SYSLOG);
	}
	
	g_mutex_init(&change_config_mutex);
	g_cond_init(&cond);
	saving_thread = g_thread_new(NULL, saving_thread_func, NULL);
}	

/*
 *	destroy
 */
void config_destroy(void)
{
	g_cond_signal(&cond);
	g_thread_join(saving_thread);
	
	g_mutex_clear(&change_config_mutex);
	g_cond_clear(&cond);

	g_free(plugin_config_path);
	
	g_key_file_free(plugin_config);
	if(project_config)
	{
		g_key_file_free(project_config);
		project_config = NULL;
	}
}

/*
 *	config parts getters
 */
/* saving option */
gboolean config_get_save_to_project(void)
{
	return g_key_file_get_boolean(plugin_config, "saving_settings", "save_to_project", NULL);
}
/* panel config */
gboolean config_get_tabbed(void)
{
	return g_key_file_get_boolean(plugin_config, "tabbed_mode", "enabled", NULL);
}
int* config_get_tabs(gsize *length)
{
	return g_key_file_get_integer_list(plugin_config, "one_panel_mode", "tabs", length, NULL);
}
int config_get_selected_tab_index(void)
{
	return g_key_file_get_integer(plugin_config, "one_panel_mode", "selected_tab_index", NULL);
}
int* config_get_left_tabs(gsize *length)
{
	return g_key_file_get_integer_list(plugin_config, "two_panels_mode", "left_tabs", length, NULL);
}
int config_get_left_selected_tab_index(void)
{
	return g_key_file_get_integer(plugin_config, "two_panels_mode", "left_selected_tab_index", NULL);
}
int* config_get_right_tabs(gsize *length)
{
	return g_key_file_get_integer_list(plugin_config, "two_panels_mode", "right_tabs", length, NULL);
}
int	config_get_right_selected_tab_index(void)
{
	return g_key_file_get_integer(plugin_config, "two_panels_mode", "right_selected_tab_index", NULL);
}

/*
 *	update GUI fron the store specified
 *  also handles default values insertion in a keyfile if debug section doesn't exist 
 */
void config_set_debug_store(debug_store store)
{
	dstore = store;
	
	tpage_clear();
	wtree_remove_all();
	breaks_remove_all();
	
	GKeyFile *keyfile = DEBUG_STORE_PROJECT == dstore ? project_config
													  : plugin_config;
	if (!g_key_file_has_group(keyfile, DEBUGGER_GROUP))
	{
		gchar *file = DEBUG_STORE_PROJECT == dstore ? geany->app->project->file_name
													: plugin_config_path;
		config_set_debug_defaults(keyfile);
		write_config_to_file(keyfile, file, SYSLOG);
	}
	debug_load_from_keyfile(keyfile);
}

/*
 *	updates project_config from a current geany project path
 */
void config_update_project_keyfile(void)
{
	if (project_config)
		g_key_file_free(project_config);
	
	project_config = load_config_from_file(geany->app->project->file_name, NULL);
}

/*
 *	project open handler
 */
void config_on_project_open(GObject *obj, GKeyFile *config, gpointer user_data)
{
	config_update_project_keyfile();
	
	if (config_get_save_to_project())
		config_set_debug_store(DEBUG_STORE_PROJECT);
}

/*
 *	project close handler
 */
void config_on_project_close(GObject *obj, gpointer user_data)
{
	if (config_get_save_to_project())
	{
		if (DBS_IDLE != debug_get_state())
		{
			/* stop a debugger and ait for it to be stopped */
			debug_stop();
			
			while (DBS_IDLE != debug_get_state())
				g_main_context_iteration(NULL, FALSE);
		}
		config_set_debug_store(DEBUG_STORE_PLUGIN);
	}
}

/*
 *	project save handler
 * 	handles ne project creation and updatng a project using project properties dialog 
 */
void config_on_project_save(GObject *obj, GKeyFile *config, gpointer user_data)
{
	if (config_get_save_to_project())
	{
		if (!g_key_file_has_group(config, DEBUGGER_GROUP))
		{
			/* no debug group, creating a new project */
			dstore = DEBUG_STORE_PROJECT;

			/* clear values taken from a plugin */
			tpage_clear();
			wtree_remove_all();
			breaks_remove_all();

			/* set default debug values */
			config_set_debug_defaults(config);
		}

		/* update local keyfile */
		if (project_config)
			g_key_file_free(project_config);
		
		project_config = create_copy_config(config);
	}
}

/*
 *	a configure dialog has been closed
 */
static void on_configure_response(GtkDialog* dialog, gint response, gpointer user_data)
{
	gboolean newvalue = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(save_to_project_btn));
	if (newvalue ^ config_get_save_to_project())
	{
		g_key_file_set_boolean(plugin_config, "saving_settings", "save_to_project", newvalue);

		g_mutex_lock(&change_config_mutex);
		panel_config_changed = TRUE;
		g_mutex_unlock(&change_config_mutex);

		if (geany->app->project)
		{
			if (DBS_IDLE != debug_get_state())
			{
				debug_stop();
				
				while (DBS_IDLE != debug_get_state())
					g_main_context_iteration(NULL, FALSE);
			}
			config_set_debug_store(newvalue ? DEBUG_STORE_PROJECT : DEBUG_STORE_PLUGIN);
		}
	}
}

/*
 *	"plugin_configure" signal handler
 */
GtkWidget *config_plugin_configure(GtkDialog *dialog)
{
#if GTK_CHECK_VERSION(3, 0, 0)
	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
	GtkWidget *_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
#else
	GtkWidget *vbox = gtk_vbox_new(FALSE, 6);
	GtkWidget *_hbox = gtk_hbox_new(FALSE, 6);
#endif
	
	save_to_project_btn = gtk_check_button_new_with_label(_("Save debug session data to a project"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(save_to_project_btn), config_get_save_to_project());

	gtk_box_pack_start(GTK_BOX(_hbox), save_to_project_btn, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), _hbox, FALSE, FALSE, 0);

	gtk_widget_show_all(vbox);

	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);

	return vbox;
}
