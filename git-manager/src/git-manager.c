/*
 *      git-manager.c
 *
 *      Copyright 2025 Egor Shinkarev <esheburg@gmail.com>
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

#include <git2.h>
#include <geanyplugin.h>	// includes geany.h

#include "../../utils/src/common.h"
#include "../../utils/src/ui.h"

GeanyPlugin	*geany_plugin;
GeanyData	*geany_data;


#ifdef LIBGIT2_VER_MINOR
# define CHECK_LIBGIT2_VERSION(MAJOR, MINOR) \
  ((LIBGIT2_VER_MAJOR == (MAJOR) && LIBGIT2_VER_MINOR >= (MINOR)) || \
   LIBGIT2_VER_MAJOR > (MAJOR))
#else /* ! defined(LIBGIT2_VER_MINOR) */
# define CHECK_LIBGIT2_VERSION(MAJOR, MINOR) 0
#endif

#if ! CHECK_LIBGIT2_VERSION(0, 22)
# define git_libgit2_init     git_threads_init
# define git_libgit2_shutdown git_threads_shutdown
#endif
#if ! CHECK_LIBGIT2_VERSION(0, 23)
/* 0.23 added @p binary_cb */
# define git_diff_buffers(old_buffer, old_len, old_as_path, \
                          new_buffer, new_len, new_as_path, options, \
                          file_cb, binary_cb, hunk_cb, line_cb, payload) \
  git_diff_buffers (old_buffer, old_len, old_as_path, \
                    new_buffer, new_len, new_as_path, options, \
                    file_cb, hunk_cb, line_cb, payload)
#endif
#if ! CHECK_LIBGIT2_VERSION(0, 28)
# define git_buf_dispose  git_buf_free
# define git_error_last   giterr_last
#endif
#if ! CHECK_LIBGIT2_VERSION(0, 99)
# define git_diff_options_init git_diff_init_options
#endif


#define PLUGIN_NAME _("Git Manager")
#define GENERAL_SECTION "general"
#define DEFAULT_DIFF_UTIL "meld"


#define WIDGET_CONF_TEXT(name, label_text, tooltip_text)					\
	entry = add_inputbox(vbox, label_text, gm_info->name, 400,				\
						 tooltip_text, FALSE, FALSE);						\
	g_object_set_data(G_OBJECT(dialog), "entry_" #name, entry);				\


#define GET_CONF_TEXT(section, name, default_value)							\
	gm_info->name = utils_get_setting_string(config, section, #name,		\
											 default_value);

#define SAVE_CONF_TEXT(section, name)										\
	gm_info->name = gtk_editable_get_chars(									\
						GTK_EDITABLE(g_object_get_data(G_OBJECT(dialog),	\
													   "entry_" #name)),	\
						0, -1);												\
	g_key_file_set_string(config, section, #name, gm_info->name);


static GtkWidget *main_menu_item = NULL;

static struct
{
	GtkWidget *log;
	GtkWidget *pull;
	GtkWidget *push;
	GtkWidget *commit;
} s_dialog = {NULL, NULL, NULL, NULL};

typedef struct {
	/* settings */
	gchar *diff_util;
	/* others */
	gchar *config_file;
} GitManagerInfo;

static GitManagerInfo *gm_info = NULL;

/* represents a menu item and key binding */
struct MenuItemInfo
{
	guint key_id;
	const gchar *key_name;
	const gchar *label;
};

/* keybindings */
enum
{
	KB_FILE_LOG,
	KB_FILE_DIR_LOG,
	KB_FILE_DIFF_LAST,
	KB_FILE_DIFF_PREV,
	KB_REPO_LOG,
	KB_REPO_PULL,
	KB_REPO_PUSH,
	KB_REPO_TAGS,
	KB_REPO_COMMIT,
	KB_REPO_BRANCHES,
};

static struct MenuItemInfo menu_items[] = {
	{ KB_REPO_TAGS, "repo_tags", N_("Repo Tags") },
	{ KB_REPO_BRANCHES, "repo_branches", N_("Repo Branches") },
	{ -1, NULL, NULL },
	{ KB_REPO_PULL, "repo_pull", N_("Pull Repo") },
	{ KB_REPO_PUSH, "repo_push", N_("Push Repo") },
	{ KB_REPO_COMMIT, "repo_commit", N_("Commit Repo") },
	{ -1, NULL, NULL },
	{ KB_REPO_LOG, "repo_log", N_("Show Repo Log") },
	{ KB_FILE_LOG, "file_log", N_("Show File Log") },
	{ KB_FILE_DIR_LOG, "file_dir_log", N_("Show File Dir Log") },
	{ -1, NULL, NULL },
	{ KB_FILE_DIFF_LAST, "file_diff_last", N_("Show File Last Diff") },
	{ KB_FILE_DIFF_PREV, "file_diff_prev", N_("Show File Prev Diff") },
};


static void create_log_dialog(const gchar *title)
{
	if (!s_dialog.log)
	{
		s_dialog.log = gtk_dialog_new_with_buttons(
							title,
							GTK_WINDOW(geany->main_widgets->window),
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);
	}
	gtk_dialog_run(GTK_DIALOG(s_dialog.log));
	gtk_widget_hide(s_dialog.log);
}

static void configure_response_cb(GtkDialog *dialog, gint response,
								  gpointer user_data)
{
	if (!ok_apply(response)) return;
	
	GKeyFile *config = load_config_from_file(gm_info->config_file, NULL);
	
	SAVE_CONF_TEXT(GENERAL_SECTION, diff_util);
	
	write_config_to_file(config, gm_info->config_file, MSGBOX);
	g_key_file_free(config);
}

/* Called when a keybinding is activated */
static void kb_activate(guint key_id)
{
	GeanyDocument *doc = document_get_current();
	if (!doc) return;
	
	switch (key_id)
	{
		case KB_FILE_LOG:
		case KB_FILE_DIR_LOG:
		case KB_FILE_DIFF_LAST:
		case KB_FILE_DIFF_PREV:
		case KB_REPO_LOG:
		case KB_REPO_PULL:
		case KB_REPO_PUSH:
		case KB_REPO_TAGS:
		case KB_REPO_COMMIT:
		case KB_REPO_BRANCHES:
			create_log_dialog(_("Log"));
			break;
	}
}

static void menu_item_activate(GtkMenuItem *menuitem, gpointer pdata)
{
	kb_activate(GPOINTER_TO_INT(pdata));
}

/* Called by Geany to initialize the plugin */
static gboolean plugin_gitmanager_init(GeanyPlugin *plugin,
									   G_GNUC_UNUSED gpointer pdata)
{
	geany_plugin = plugin;
	geany_data = plugin->geany_data;
	
	GeanyKeyGroup *key_group = plugin_set_key_group(geany_plugin, PLUGIN,
													G_N_ELEMENTS(menu_items), NULL);
	GtkWidget *submenu = gtk_menu_new();
	gtk_widget_show(submenu);
	
	main_menu_item = gtk_menu_item_new_with_mnemonic(PLUGIN_NAME);
	gtk_widget_show(main_menu_item);
	
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(main_menu_item), submenu);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), main_menu_item);
	
	for (guint i = 0; i < G_N_ELEMENTS(menu_items); i++)
	{
		GtkWidget *item;
		
		if (menu_items[i].label)
		{
			/* add menu item */
			item = gtk_menu_item_new_with_mnemonic(_(menu_items[i].label));
			g_signal_connect(item, "activate", G_CALLBACK(menu_item_activate),
							 GINT_TO_POINTER(menu_items[i].key_id));
			ui_add_document_sensitive(item);
			
			/* setup keybindings */
			keybindings_set_item(key_group, menu_items[i].key_id, kb_activate, 0, 0,
								 menu_items[i].key_name, _(menu_items[i].label), NULL);
		}
		else /* separator */
			item = gtk_separator_menu_item_new();
		
		gtk_widget_show(item);
		gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
	}
	
	gm_info = g_new0(GitManagerInfo, 1);
	gm_info->config_file = get_config_filepath(PLUGIN, NULL);
	
	GKeyFile *config = load_config_from_file(gm_info->config_file, NULL);
	
	GET_CONF_TEXT(GENERAL_SECTION, diff_util, DEFAULT_DIFF_UTIL);
	
	g_key_file_free(config);
	return TRUE;
}

static GtkWidget *plugin_gitmanager_configure(G_GNUC_UNUSED GeanyPlugin *plugin,
											  GtkDialog *dialog,
											  G_GNUC_UNUSED gpointer pdata)
{
	GtkWidget *entry, *vbox = gtk_vbox_new(FALSE, 0);
	
	WIDGET_CONF_TEXT(diff_util, "Diff util:", _("Path to the Diff Util"));
	
	g_signal_connect(dialog, "response", G_CALLBACK(configure_response_cb), NULL);
	
	gtk_widget_show_all(vbox);
	return vbox;
}

/* Called by Geany before unloading the plugin. */
static void plugin_gitmanager_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin,
									  G_GNUC_UNUSED gpointer pdata)
{
	g_free(gm_info->config_file);
	g_free(gm_info->diff_util);
	g_free(gm_info);
	
	gtk_widget_destroy(main_menu_item);
	
	if (s_dialog.log) gtk_widget_destroy(s_dialog.log);
	s_dialog.log = NULL;
}


/* Load module */
G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
	/* Setup translation */
	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);
	
	/* Set metadata */
	plugin->info->name = PLUGIN_NAME;
	plugin->info->description = _("This plugin provides a GUI for Git commands "
								  "(log/commit/push/pull/etc.)");
	plugin->info->version = "0.1";
	plugin->info->author = "Egor Shinkarev <esheburg@gmail.com>";
	
	/* Set functions */
	plugin->funcs->init = plugin_gitmanager_init;
	plugin->funcs->cleanup = plugin_gitmanager_cleanup;
	plugin->funcs->configure = plugin_gitmanager_configure;
	
	/* Register! */
	GEANY_PLUGIN_REGISTER(plugin, 226);
}
