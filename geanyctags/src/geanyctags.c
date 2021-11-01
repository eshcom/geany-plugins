/*
 *   Copyright 2010-2014 Jiri Techet <techet@gmail.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <sys/time.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <geanyplugin.h>
#include <../../utils/src/spawn.h>

#include "readtags.h"

#include <errno.h>
#include <glib/gstdio.h>

#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>


/* Pre-GTK 2.24 compatibility */
#ifndef GTK_COMBO_BOX_TEXT
#	define GTK_COMBO_BOX_TEXT GTK_COMBO_BOX
#	define gtk_combo_box_text_new gtk_combo_box_new_text
#	define gtk_combo_box_text_append_text gtk_combo_box_append_text
#endif


static GeanyPlugin *geany_plugin = NULL;
static GeanyData *geany_data = NULL;

typedef struct {
	/* settings */
	gchar *extra_options_1;
	gchar *extra_options_2;
	gchar *extra_options_3;
	/* others */
	gchar *config_file;
} GeanyctagsInfo;

static GeanyctagsInfo *gtags_info = NULL;


static GtkWidget *s_context_fdec_item, *s_context_fdef_item,
				 *s_context_sep_item, *s_gt_item, *s_sep_item, *s_ft_item;

static struct
{
	GtkWidget *widget;
	
	GtkWidget *combo;
	GtkWidget *combo_match;
	GtkWidget *case_sensitive;
	GtkWidget *declaration;
} s_ft_dialog = {NULL, NULL, NULL, NULL, NULL};


enum
{
	KB_FIND_TAG,
	KB_GENERATE_TAGS,
	KB_COUNT
};



static void set_widgets_sensitive(gboolean sensitive)
{
	gtk_widget_set_sensitive(GTK_WIDGET(s_gt_item), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(s_ft_item), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(s_context_fdec_item), sensitive);
	gtk_widget_set_sensitive(GTK_WIDGET(s_context_fdef_item), sensitive);
}

static void on_project_open(G_GNUC_UNUSED GObject *obj, GKeyFile *config,
							G_GNUC_UNUSED gpointer user_data)
{
	set_widgets_sensitive(TRUE);
}

static void on_project_save(G_GNUC_UNUSED GObject *obj, GKeyFile *config,
							G_GNUC_UNUSED gpointer user_data)
{
	set_widgets_sensitive(TRUE);
}

static void on_project_close(G_GNUC_UNUSED GObject *obj,
							 G_GNUC_UNUSED gpointer user_data)
{
	set_widgets_sensitive(FALSE);
}

static PluginCallback plugin_geanyctags_callbacks[] = {
	{"project-open", (GCallback) &on_project_open, TRUE, NULL},
	{"project-save", (GCallback) &on_project_save, TRUE, NULL},
	{"project-close", (GCallback) &on_project_close, TRUE, NULL},
	{NULL, NULL, FALSE, NULL}
};

static void plugin_geanyctags_help(G_GNUC_UNUSED GeanyPlugin *plugin,
								   G_GNUC_UNUSED gpointer pdata)
{
	utils_open_browser("https://plugins.geany.org/geanyctags.html");
}

static gboolean spawn_cmd(const gchar *locale_cmd, const gchar *locale_dir)
{
	msgwin_clear_tab(MSG_MESSAGE);
	msgwin_switch_tab(MSG_MESSAGE, TRUE);
	msgwin_msg_add(COLOR_BLUE, -1, NULL, _("%s (in directory: %s)"),
				   locale_cmd, locale_dir);
	
	SpawnResult *result = call_spawn_sync(locale_cmd, locale_dir);
	gboolean success;
	
	if (result->success)
	{
		if (!EMPTY(result->output1))
			msgwin_msg_add(COLOR_BLACK, -1, NULL, "%s", result->output1);
		success = TRUE;
	}
	else
	{
		if (!EMPTY(result->errmsg))
			msgwin_msg_add(COLOR_RED, -1, NULL,
						   _("Process execution failed (%s)"),
						   result->errmsg);
		if (!EMPTY(result->output1))
			msgwin_msg_add(COLOR_RED, -1, NULL, "%s", result->output1);
		if (!EMPTY(result->output2))
			msgwin_msg_add(COLOR_RED, -1, NULL, "%s", result->output2);
		success = FALSE;
	}
	
	free_spawn_result(result);
	return success;
}

#ifndef G_OS_WIN32
#define ADD_EXTRA_OPTIONS(gstr, options)	\
	if (!EMPTY(options))					\
	{										\
		g_string_append_c(gstr, ' ');		\
		g_string_append(gstr, options);		\
	}

static GString *generate_find_cmd(GeanyProject *prj)
{
	GString *gstr = g_string_new("find -L . -type f -not -path '*/.*'");
	
	if (!EMPTY(prj->file_patterns))
	{
		ADD_EXTRA_OPTIONS(gstr, gtags_info->extra_options_1);
		ADD_EXTRA_OPTIONS(gstr, gtags_info->extra_options_2);
		ADD_EXTRA_OPTIONS(gstr, gtags_info->extra_options_3);
		
		g_string_append(gstr, " \\( -name \"");
		g_string_append(gstr, prj->file_patterns[0]);
		g_string_append_c(gstr, '\"');
		
		for (guint i = 1; prj->file_patterns[i]; i++)
		{
			g_string_append(gstr, " -o -name \"");
			g_string_append(gstr, prj->file_patterns[i]);
			g_string_append_c(gstr, '\"');
		}
		g_string_append(gstr, " \\)");
	}
	return gstr;
}
#endif

static void on_generate_tags(GtkMenuItem *menuitem, gpointer user_data)
{
	GeanyProject *prj = geany_data->app->project;
	if (!prj)
		return;
	
	GString *cmd;
	gchar *tag_filename = project_get_tags_file();
	
#ifndef G_OS_WIN32
	cmd = generate_find_cmd(prj);
	
	g_string_append(cmd, " | ctags --totals --fields=fKsSt --extra=-fq"
						 " --c-kinds=+p --sort=foldcase --excmd=number -L - -f '");
	g_string_append(cmd, tag_filename);
	g_string_append_c(cmd, '\'');
#else
	/* We don't have find and | on windows, generate tags
	 * for all files in the project (-R recursively) */
	
	/* Unfortunately, there's a bug in ctags - when run with -R,
	 * the first line is empty, ctags doesn't recognize the tags
	 * file as a valid ctags file and refuses to overwrite it.
	 * Therefore, we need to delete the tags file manually. */
	g_unlink(tag_filename);
	
	cmd = g_string_new("ctags.exe -R --totals --fields=fKsSt --extra=-fq"
					   " --c-kinds=+p --sort=foldcase --excmd=number -f \"");
	g_string_append(cmd, tag_filename);
	g_string_append_c(cmd, '"');
#endif
	
	gchar *base_path = project_get_base_path();
	gchar *locale_path = utils_get_locale_from_utf8(base_path);
	
	if (spawn_cmd(cmd->str, locale_path))
		project_load_tags_file(tag_filename, base_path);
	
	g_string_free(cmd, TRUE);
	g_free(tag_filename);
	g_free(locale_path);
	g_free(base_path);
}

static void show_entry(tagEntry *entry)
{
	const gchar *file = entry->file;
	if (!file)
		file = "";
	
	const gchar *name = entry->name;
	if (!name)
		name = "";
	
	const gchar *signature = tagsField(entry, "signature");
	if (!signature)
		signature = "";
	
	const gchar *scope = tagsField(entry, "class");
	const gchar *scope_sep = "::";
	if (!scope)
		scope = tagsField(entry, "struct");
	if (!scope)
		scope = tagsField(entry, "union");
	if (!scope)
		scope = tagsField(entry, "enum");
	//~ esh: for erlang the module name (scope) is the same as the file name,
	//		 show scope only if file name is empty
	if (!scope && EMPTY(file))
	{
		scope = tagsField(entry, "module"); // esh: used in Erlang
		scope_sep = ":";
	}
	gchar *scope_str = scope ? g_strconcat(scope, scope_sep, NULL)
							 : g_strdup("");
	
	const gchar *kind = entry->kind;
	gchar *kind_str;
	if (kind)
	{
		kind_str = g_strconcat(kind, ":  ", NULL);
		SETPTR(kind_str, g_strdup_printf("%-14s", kind_str));
	}
	else
		kind_str = g_strdup("");
	
	msgwin_msg_add(COLOR_BLACK, -1, NULL, "%s:%lu:\n    %s%s%s%s", file,
				   entry->address.lineNumber, kind_str, scope_str, name,
				   signature);
	g_free(scope_str);
	g_free(kind_str);
}


static gchar *get_selection(void)
{
	GeanyDocument *doc = document_get_current();
	if (!doc)
		return NULL;
	
	GeanyEditor *editor = doc->editor;
	gchar *ret = NULL;
	
	if (sci_has_selection(editor->sci))
		ret = sci_get_selection_contents(editor->sci);
	else
		ret = editor_get_word_at_pos(editor, -1, GEANY_WORDCHARS);
	
	return ret;
}

/* TODO: Not possible to do it the way below because some of the API is private
 * in Geany. This means the cursor has to be placed at the symbol first and
 * then right-clicked (right-clicking without having the cursor at the symbol
 * doesn't work) */
 
/*
static gchar *get_selection()
{
	GeanyDocument *doc = document_get_current();
	if (!doc)
		return NULL;
	
	if (!sci_has_selection(doc->editor->sci))
		sci_set_current_position(doc->editor->sci,
								 editor_info.click_pos, FALSE);
	
	if (sci_has_selection(doc->editor->sci))
		return sci_get_selection_contents(doc->editor->sci);
	
	gint len = sci_get_selected_text_length(doc->editor->sci);
	
	gchar *ret = g_malloc(len + 1);
	sci_get_selected_text(doc->editor->sci, ret);
	
	editor_find_current_word(doc->editor, -1, editor_info.current_word,
							 GEANY_MAX_WORD_LENGTH, NULL);
	
	return editor_info.current_word != 0 ? g_strdup(editor_info.current_word)
										 : NULL;
}
*/

typedef enum
{
	MATCH_FULL,
	MATCH_PREFIX,
	MATCH_PATTERN
} MatchType;

static gboolean find_first(tagFile *tf, tagEntry *entry,
						   const gchar *name,
						   MatchType match_type)
{
	gboolean ret;
	
	if (match_type == MATCH_PATTERN)
		ret = tagsFirst(tf, entry) == TagSuccess;
	else
	{
		int options = TAG_IGNORECASE;
		
		options |= match_type == MATCH_PREFIX ? TAG_PARTIALMATCH
											  : TAG_FULLMATCH;
		ret = tagsFind(tf, entry, name, options) == TagSuccess;
	}
	return ret;
}

static gboolean find_next(tagFile *tf, tagEntry *entry,
						  MatchType match_type)
{
	gboolean ret;
	
	if (match_type == MATCH_PATTERN)
		ret = tagsNext(tf, entry) == TagSuccess;
	else
		ret = tagsFindNext(tf, entry) == TagSuccess;
	return ret;
}

static gboolean filter_tag(tagEntry *entry, GPatternSpec *name,
						   gboolean declaration, gboolean case_sensitive)
{
	gboolean filter = TRUE;
	
	if (!EMPTY(entry->kind))
	{
		gboolean is_prototype;
		
		is_prototype = g_strcmp0(entry->kind, "prototype") == 0;
		filter = (declaration && !is_prototype) ||
				 (!declaration && is_prototype);
		if (filter)
			return TRUE;
	}
	gchar *entry_name = case_sensitive ? g_strdup(entry->name)
									   : g_utf8_strdown(entry->name, -1);
	
	filter = !g_pattern_match_string(name, entry_name);
	g_free(entry_name);
	
	return filter;
}

static void find_tags(const gchar *name, gboolean declaration,
					  gboolean case_sensitive, MatchType match_type)
{
	GeanyProject *prj = geany_data->app->project;
	if (!prj)
		return;
	
	gchar *base_path = project_get_base_path();
	msgwin_clear_tab(MSG_MESSAGE);
	msgwin_set_messages_dir(base_path);
	
	gchar *tag_filename = project_get_tags_file();
	tagFileInfo info;
	tagFile *tf = tagsOpen(tag_filename, &info);
	
	if (tf)
	{
		tagEntry entry;
		if (find_first(tf, &entry, name, match_type))
		{
			gint num = 0;
			gchar *path = NULL; 
			gchar *name_case = case_sensitive ? g_strdup(name)
											  : g_utf8_strdown(name, -1);
			
			SETPTR(name_case, g_strconcat("*", name_case, "*", NULL));
			GPatternSpec *name_pat = g_pattern_spec_new(name_case);
			
			int last_line_number = 0;
			
			if (!filter_tag(&entry, name_pat, declaration, case_sensitive))
			{
				path = g_build_filename(base_path, entry.file, NULL);
				show_entry(&entry);
				last_line_number = entry.address.lineNumber;
				num++;
			}
			
			while (find_next(tf, &entry, match_type))
			{
				if (!filter_tag(&entry, name_pat, declaration, case_sensitive))
				{
					if (!path)
						path = g_build_filename(base_path, entry.file, NULL);
					show_entry(&entry);
					last_line_number = entry.address.lineNumber;
					num++;
				}
			}
			if (num == 1)
			{
				GeanyDocument *doc = document_open_file(path, FALSE,
														NULL, NULL);
				if (doc != NULL)
				{
					navqueue_goto_line(document_get_current(), doc,
									   last_line_number);
					gtk_widget_grab_focus(GTK_WIDGET(doc->editor->sci));
				}
			}
			g_pattern_spec_free(name_pat);
			g_free(name_case);
			g_free(path);
		}
		tagsClose(tf);
	}
	msgwin_switch_tab(MSG_MESSAGE, TRUE);
	
	g_free(tag_filename);
	g_free(base_path);
}

static void on_find_declaration(GtkMenuItem *menuitem, gpointer user_data)
{
	gchar *name = get_selection();
	if (name)
		find_tags(name, TRUE, TRUE, MATCH_FULL);
	g_free(name);
}

static void on_find_definition(GtkMenuItem *menuitem, gpointer user_data)
{
	gchar *name = get_selection();
	if (name)
		find_tags(name, FALSE, TRUE, MATCH_FULL);
	g_free(name);
}

static void create_dialog_find_file(void)
{
	GtkWidget *label, *vbox, *ebox, *entry;
	GtkSizeGroup *size_group;
	
	if (s_ft_dialog.widget)
		return;
	
	s_ft_dialog.widget = gtk_dialog_new_with_buttons(
							_("Find Tag"),
							GTK_WINDOW(geany->main_widgets->window),
							GTK_DIALOG_DESTROY_WITH_PARENT,
							GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
	
	gtk_dialog_add_button(GTK_DIALOG(s_ft_dialog.widget), "gtk-find",
						  GTK_RESPONSE_ACCEPT);
	gtk_dialog_set_default_response(GTK_DIALOG(s_ft_dialog.widget),
									GTK_RESPONSE_ACCEPT);
	
	vbox = ui_dialog_vbox_new(GTK_DIALOG(s_ft_dialog.widget));
	gtk_box_set_spacing(GTK_BOX(vbox), 9);
	
	size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	
	label = gtk_label_new_with_mnemonic(_("_Search for:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(size_group, label);
	
	s_ft_dialog.combo = gtk_combo_box_text_new_with_entry();
	entry = gtk_bin_get_child(GTK_BIN(s_ft_dialog.combo));
	
	ui_entry_add_clear_icon(GTK_ENTRY(entry));
	gtk_entry_set_width_chars(GTK_ENTRY(entry), 40);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
	ui_entry_add_clear_icon(GTK_ENTRY(entry));
	gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
	
	ebox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), s_ft_dialog.combo, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), ebox, TRUE, FALSE, 0);
	
	label = gtk_label_new_with_mnemonic(_("_Match type:"));
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_size_group_add_widget(size_group, label);
	
	s_ft_dialog.combo_match = gtk_combo_box_text_new();
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(s_ft_dialog.combo_match),
								   _("exact"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(s_ft_dialog.combo_match),
								   _("prefix"));
	gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(s_ft_dialog.combo_match),
								   _("pattern"));
	gtk_combo_box_set_active(GTK_COMBO_BOX(s_ft_dialog.combo_match), 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), s_ft_dialog.combo_match);
	
	ebox = gtk_hbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(ebox), label, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(ebox), s_ft_dialog.combo_match, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), ebox, TRUE, FALSE, 0);
	
	s_ft_dialog.case_sensitive =
			gtk_check_button_new_with_mnemonic(_("C_ase sensitive"));
	gtk_button_set_focus_on_click(GTK_BUTTON(s_ft_dialog.case_sensitive),
								  FALSE);
	
	s_ft_dialog.declaration =
			gtk_check_button_new_with_mnemonic(_("_Declaration"));
	gtk_button_set_focus_on_click(GTK_BUTTON(s_ft_dialog.declaration),
								  FALSE);
	
	g_object_unref(G_OBJECT(size_group)); /* auto destroy the size group */
	
	gtk_container_add(GTK_CONTAINER(vbox), s_ft_dialog.case_sensitive);
	gtk_container_add(GTK_CONTAINER(vbox), s_ft_dialog.declaration);
	gtk_widget_show_all(vbox);
}

static void on_find_tag(GtkMenuItem *menuitem, gpointer user_data)
{
	create_dialog_find_file();
	
	GtkWidget *entry = gtk_bin_get_child(GTK_BIN(s_ft_dialog.combo));
	
	gchar *selection = get_selection();
	if (selection)
		gtk_entry_set_text(GTK_ENTRY(entry), selection);
	g_free(selection);
	
	gtk_widget_grab_focus(entry);
	
	if (gtk_dialog_run(GTK_DIALOG(s_ft_dialog.widget)) == GTK_RESPONSE_ACCEPT)
	{
		gboolean case_sensitive = gtk_toggle_button_get_active(
									GTK_TOGGLE_BUTTON(s_ft_dialog.case_sensitive));
		gboolean declaration = gtk_toggle_button_get_active(
									GTK_TOGGLE_BUTTON(s_ft_dialog.declaration));
		MatchType match_type = gtk_combo_box_get_active(
									GTK_COMBO_BOX(s_ft_dialog.combo_match));
		
		const gchar *name = gtk_entry_get_text(GTK_ENTRY(entry));
		
		ui_combo_box_add_to_history(GTK_COMBO_BOX_TEXT(s_ft_dialog.combo),
									name, 0);
		
		find_tags(name, declaration, case_sensitive, match_type);
	}
	gtk_widget_hide(s_ft_dialog.widget);
}

static gboolean kb_callback(guint key_id)
{
	switch (key_id)
	{
		case KB_FIND_TAG:
			// esh: reassigned hotkeys: on_find_tag -> on_find_definition
			//~ on_find_tag(NULL, NULL);
			on_find_definition(NULL, NULL);
			return TRUE;
		case KB_GENERATE_TAGS:
			on_generate_tags(NULL, NULL);
			return TRUE;
	}
	return FALSE;
}

static gboolean plugin_geanyctags_init(GeanyPlugin *plugin,
									   G_GNUC_UNUSED gpointer pdata)
{
	geany_plugin = plugin;
	geany_data = plugin->geany_data;
	
	GKeyFile *config = g_key_file_new();
	GeanyKeyGroup *key_group;
	
	key_group = plugin_set_key_group(geany_plugin, "GeanyCtags",
									 KB_COUNT, kb_callback);
	
	gtags_info = g_new0(GeanyctagsInfo, 1);
	
	gtags_info->config_file = g_strconcat(geany->app->configdir,
										  G_DIR_SEPARATOR_S, "plugins",
										  G_DIR_SEPARATOR_S, "geanyctags",
										  G_DIR_SEPARATOR_S, "geanyctags.conf",
										  NULL);
	
	g_key_file_load_from_file(config, gtags_info->config_file,
							  G_KEY_FILE_NONE, NULL);
	
#define GET_CONF_TEXT(name) G_STMT_START {								\
	gtags_info->name = utils_get_setting_string(config, "geanyctags",	\
												#name, NULL);			\
} G_STMT_END
	
	GET_CONF_TEXT(extra_options_1);
	GET_CONF_TEXT(extra_options_2);
	GET_CONF_TEXT(extra_options_3);
#undef GET_CONF_TEXT
	
	g_key_file_free(config);
	
	
	s_context_sep_item = gtk_separator_menu_item_new();
	gtk_widget_show(s_context_sep_item);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu),
						   s_context_sep_item);
	
	s_context_fdec_item = gtk_menu_item_new_with_mnemonic(
									_("Find Tag Declaration (geanyctags)"));
	gtk_widget_show(s_context_fdec_item);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu),
						   s_context_fdec_item);
	g_signal_connect((gpointer) s_context_fdec_item, "activate",
					 G_CALLBACK(on_find_declaration), NULL);
	
	s_context_fdef_item = gtk_menu_item_new_with_mnemonic(
									_("Find Tag Definition (geanyctags)"));
	gtk_widget_show(s_context_fdef_item);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(geany->main_widgets->editor_menu),
						   s_context_fdef_item);
	g_signal_connect((gpointer) s_context_fdef_item, "activate",
					 G_CALLBACK(on_find_definition), NULL);
	// esh: reassigned hotkeys: on_find_tag -> on_find_definition
	keybindings_set_item(key_group, KB_FIND_TAG, NULL, 0, 0,
						 "find_tag", _("Find tag"), s_context_fdef_item);
	
	s_sep_item = gtk_separator_menu_item_new();
	gtk_widget_show(s_sep_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->project_menu),
					  s_sep_item);
	
	s_gt_item = gtk_menu_item_new_with_mnemonic(_("Generate tags"));
	gtk_widget_show(s_gt_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->project_menu),
					  s_gt_item);
	g_signal_connect((gpointer) s_gt_item, "activate",
					 G_CALLBACK(on_generate_tags), NULL);
	keybindings_set_item(key_group, KB_GENERATE_TAGS, NULL, 0, 0,
						 "generate_tags", _("Generate tags"), s_gt_item);
	
	s_ft_item = gtk_menu_item_new_with_mnemonic(_("Find tag..."));
	gtk_widget_show(s_ft_item);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->project_menu),
					  s_ft_item);
	g_signal_connect((gpointer) s_ft_item, "activate",
					 G_CALLBACK(on_find_tag), NULL);
	// esh: reassigned hotkeys: on_find_tag -> on_find_definition
	//~ keybindings_set_item(key_group, KB_FIND_TAG, NULL, 0, 0,
						 //~ "find_tag", _("Find tag"), s_ft_item);
	
	set_widgets_sensitive(geany_data->app->project != NULL);
	return TRUE;
}

static void configure_response_cb(GtkDialog *dialog, gint response,
								  gpointer user_data)
{
	if (response != GTK_RESPONSE_OK && response != GTK_RESPONSE_APPLY)
		return;
	
	GKeyFile *config = g_key_file_new();
	gchar *config_dir = g_path_get_dirname(gtags_info->config_file);
	
	g_key_file_load_from_file(config, gtags_info->config_file,
							  G_KEY_FILE_NONE, NULL);
	
#define SAVE_CONF_TEXT(name) G_STMT_START {										\
	gtags_info->name = gtk_editable_get_chars(									\
							GTK_EDITABLE(g_object_get_data(G_OBJECT(dialog),	\
														   "entry_" #name)),	\
							0, -1);												\
	g_strstrip(gtags_info->name);												\
	g_key_file_set_string(config, "geanyctags", #name, gtags_info->name);		\
} G_STMT_END
	
	SAVE_CONF_TEXT(extra_options_1);
	SAVE_CONF_TEXT(extra_options_2);
	SAVE_CONF_TEXT(extra_options_3);
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
		utils_write_file(gtags_info->config_file, data);
		g_free(data);
	}
	g_free(config_dir);
	g_key_file_free(config);
}

static GtkWidget *plugin_geanyctags_configure(G_GNUC_UNUSED GeanyPlugin *plugin,
											  GtkDialog *dialog,
											  G_GNUC_UNUSED gpointer pdata)
{
	GtkWidget *vbox, *hbox, *label, *entry;
	
	vbox = gtk_vbox_new(FALSE, 0);
	
#define WIDGET_CONF_TEXT(name, description) G_STMT_START {				\
	label = gtk_label_new(description);									\
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);					\
	entry = gtk_entry_new();											\
	gtk_entry_set_text(GTK_ENTRY(entry), gtags_info->name);				\
	gtk_widget_set_tooltip_text(entry,									\
								_("Other options to pass to Find"));	\
	hbox = gtk_hbox_new(FALSE, 0);										\
	gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 6);			\
	gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);			\
	gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 6);			\
	g_object_set_data(G_OBJECT(dialog), "entry_" #name, entry);			\
} G_STMT_END
	
	WIDGET_CONF_TEXT(extra_options_1, _("Extra options 1:"));
	WIDGET_CONF_TEXT(extra_options_2, _("Extra options 2:"));
	WIDGET_CONF_TEXT(extra_options_3, _("Extra options 3:"));
#undef WIDGET_CONF_TEXT
	
	g_signal_connect(dialog, "response",
					 G_CALLBACK(configure_response_cb), NULL);
	gtk_widget_show_all(vbox);
	return vbox;
}

static void plugin_geanyctags_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin,
									  G_GNUC_UNUSED gpointer pdata)
{
	g_free(gtags_info->config_file);
	g_free(gtags_info->extra_options_1);
	g_free(gtags_info->extra_options_2);
	g_free(gtags_info->extra_options_3);
	g_free(gtags_info);
	
	gtk_widget_destroy(s_context_fdec_item);
	gtk_widget_destroy(s_context_fdef_item);
	gtk_widget_destroy(s_context_sep_item);
	
	gtk_widget_destroy(s_ft_item);
	gtk_widget_destroy(s_gt_item);
	gtk_widget_destroy(s_sep_item);
	
	if (s_ft_dialog.widget)
		gtk_widget_destroy(s_ft_dialog.widget);
	s_ft_dialog.widget = NULL;
}


/* Load module */
G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
	/* Setup translation */
	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);
	
	/* Set metadata */
	plugin->info->name = _("Geany Ctags");
	plugin->info->description = _("Ctags generation and search plugin "
								  "for geany projects");
	plugin->info->version = VERSION;
	plugin->info->author = "Jiri Techet <techet@gmail.com>";
	
	/* Set functions */
	plugin->funcs->init = plugin_geanyctags_init;
	plugin->funcs->help = plugin_geanyctags_help;
	plugin->funcs->cleanup = plugin_geanyctags_cleanup;
	plugin->funcs->callbacks = plugin_geanyctags_callbacks;
	plugin->funcs->configure = plugin_geanyctags_configure;
	
	/* Register! */
	GEANY_PLUGIN_REGISTER(plugin, 226);
}
