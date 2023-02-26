/*
 * jsonprettifier.c - a Geany plugin to format not formatted JSON files
 *
 *  Copyright 2018 zhgzhg @ github.com
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
	#include "config.h"			// for the gettext domain
#endif

#ifdef HAVE_LOCALE_H
	#include <locale.h>
#endif

#include <sys/stat.h>
#include <gdk/gdkkeysyms.h>		// for the key bindings

#include <geanyplugin.h>		// includes geany.h

#include <yajl/yajl_parse.h>	// "lloyd-yajl-66cb08c/src/api/yajl_parse.h"
#include <yajl/yajl_gen.h>		// "lloyd-yajl-66cb08c/src/api/yajl_gen.h"

#include "../../utils/src/ui_plugins.h"


GeanyPlugin	*geany_plugin;
GeanyData	*geany_data;

struct GeanyKeyGroup *geany_key_group;

static gchar *plugin_config_path = NULL;
static GKeyFile *keyfile_plugin = NULL;

PLUGIN_VERSION_CHECK(235)

PLUGIN_SET_TRANSLATABLE_INFO(LOCALEDIR, GETTEXT_PACKAGE,
	_("JSON Prettifier"),
	_("JSON file format prettifier, minifier and validator.\n"
	  "https://github.com/zhgzhg/Geany-JSON-Prettifier"),
	"1.6.0",
	"zhgzhg @@ github.com\nhttps://github.com/zhgzhg/Geany-JSON-Prettifier"
);

static const gchar *fcfg_g_settings = "settings";
static const gchar *fcfg_e_escForwardSlashed = "escape_forward_slashes";
static const gchar *fcfg_e_allowInvalUtf8TextStr =
									"allow_invalid_utf8_text_strings";
static const gchar *fcfg_e_reformatMultJsonEntities =
									"reformat_multiple_json_entities";
static const gchar *fcfg_e_showErrsInWindow = "show_errors_in_window";
static const gchar *fcfg_e_logMsgOnFormattingSuccess =
									"log_formatting_success_messages";
static const gchar *fcfg_e_allowComments = "allow_comments";
static const gchar *fcfg_e_indentChar = "indent_char";
static const gchar *fcfg_e_indentWidth = "indent_width";

static GtkWidget *menu_item_minify = NULL;
static GtkWidget *menu_item_prettify = NULL;
static GtkWidget *escape_forward_slashes_btn = NULL;
static GtkWidget *allow_invalid_utf8_text_strings_btn = NULL;
static GtkWidget *reformat_multiple_json_entities_btn = NULL;
static GtkWidget *show_errors_in_window_btn = NULL;
static GtkWidget *log_formatting_success_messages_btn = NULL;
static GtkWidget *allow_comments_btn = NULL;
static GtkWidget *indent_char_combo = NULL;
static GtkWidget *indent_width_spin = NULL;

static gboolean escapeForwardSlashes = FALSE;
static gboolean allowInvalidStringsInUtf8 = TRUE;
static gboolean reformatMultipleJsonEntities = FALSE;
static gboolean showErrorsInPopupWindow = TRUE;
static gboolean logFormattingSuccessMessages = TRUE;
static gboolean allowComments = TRUE;
static gchar indentChar = '\t';
static guint indentWidth = 4;

/* JSON Prettifier Code - yajl example used as a basis */

#define GEN_AND_RETURN(func)						\
{													\
	yajl_gen_status __stat = func;					\
	if (__stat == yajl_gen_generation_complete &&	\
		reformatMultipleJsonEntities)				\
	{												\
		yajl_gen_reset(g, "\n");					\
		__stat = func;								\
	}												\
	return __stat == yajl_gen_status_ok;			\
}

static int reformat_null(void *ctx)
{
	yajl_gen g = (yajl_gen)ctx;
	GEN_AND_RETURN(yajl_gen_null(g));
}

static int reformat_boolean(void *ctx, int boolean)
{
	yajl_gen g = (yajl_gen)ctx;
	GEN_AND_RETURN(yajl_gen_bool(g, boolean));
}

static int reformat_number(void *ctx, const char *s, size_t l)
{
	yajl_gen g = (yajl_gen)ctx;
	GEN_AND_RETURN(yajl_gen_number(g, s, l));
}

static int reformat_string(void *ctx, const unsigned char *stringVal,
						   size_t stringLen)
{
	yajl_gen g = (yajl_gen)ctx;
	GEN_AND_RETURN(yajl_gen_string(g, stringVal, stringLen));
}

static int reformat_map_key(void *ctx, const unsigned char *stringVal,
							size_t stringLen)
{
	yajl_gen g = (yajl_gen)ctx;
	GEN_AND_RETURN(yajl_gen_string(g, stringVal, stringLen));
}

static int reformat_start_map(void *ctx)
{
	yajl_gen g = (yajl_gen)ctx;
	GEN_AND_RETURN(yajl_gen_map_open(g));
}

static int reformat_end_map(void *ctx)
{
	yajl_gen g = (yajl_gen)ctx;
	GEN_AND_RETURN(yajl_gen_map_close(g));
}

static int reformat_start_array(void *ctx)
{
	yajl_gen g = (yajl_gen)ctx;
	GEN_AND_RETURN(yajl_gen_array_open(g));
}

static int reformat_end_array(void *ctx)
{
	yajl_gen g = (yajl_gen)ctx;
	GEN_AND_RETURN(yajl_gen_array_close(g));
}

static yajl_callbacks callbacks = {
	reformat_null,
	reformat_boolean,
	NULL,
	NULL,
	reformat_number,
	reformat_string,
	reformat_start_map,
	reformat_map_key,
	reformat_end_map,
	reformat_start_array,
	reformat_end_array
};

static void my_json_prettify(GeanyDocument *doc, gboolean beautify)
{
	if (!doc) return;
	
	GeanyEditor *editor = doc->editor;
	ScintillaObject *sci = editor->sci;
	
	gint text_len = 0;
	gchar *text_string = NULL;
	
	/* load the text and prepares it */
	
	gboolean workWithTextSelection = FALSE;
	
	/* first try to work only with a text selection (if any) */
	if (sci_has_selection(sci))
	{
		text_string = sci_get_selection_contents(sci);
		if (text_string != NULL)
		{
			workWithTextSelection = TRUE;
			text_len = sci_get_selected_text_length(sci) + 1;
		}
	}
	else
	{	/* Work with the entire file */
		text_len = sci_get_length(sci);
		if (text_len == 0) return;
		++text_len;
		text_string = sci_get_contents(sci, -1);
	}
	
	if (text_string == NULL) return;
	
	/* filter characters that may break the formatting */
	
	utils_str_remove_chars(text_string, (const gchar *)"\r\n");
	
	for (text_len = 0; text_string[text_len]; ++text_len);
	++text_len;
	
	/* begin the prettifying process */
	
	const gchar *chosenActionString = beautify ? "Prettifying" : "Minifying";
	gchar timeNow[11];
	{
		time_t rawtime;
		struct tm *timeinfo;
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		if (strftime(timeNow, sizeof(timeNow), "%T", timeinfo) == 0)
			timeNow[0] = '\0';
	}
	
	gchar *textIndentString = g_strnfill(indentChar == '\t' && indentWidth > 0 ?
										 1 : indentWidth, indentChar);
	
	/* yajl generator config */
	
	yajl_gen g = yajl_gen_alloc(NULL);
	yajl_gen_config(g, yajl_gen_beautify, beautify);
	yajl_gen_config(g, yajl_gen_validate_utf8, 1);
	yajl_gen_config(g, yajl_gen_escape_solidus, escapeForwardSlashes);
	yajl_gen_config(g, yajl_gen_indent_string, textIndentString);
	
	yajl_handle hand = yajl_alloc(&callbacks, NULL, (void *)g);
	yajl_config(hand, yajl_dont_validate_strings, allowInvalidStringsInUtf8);
	yajl_config(hand, yajl_allow_multiple_values, reformatMultipleJsonEntities);
	yajl_config(hand, yajl_allow_comments, allowComments);
	
	yajl_status stat;
	stat = yajl_parse(hand, (unsigned char *)text_string, (size_t)text_len - 1);
	stat = yajl_complete_parse(hand);
	
	if (stat == yajl_status_ok)
	{
		const unsigned char *buf;
		size_t len;
		yajl_gen_get_buf(g, &buf, &len);
		
		if (!workWithTextSelection)
			sci_set_text(sci, (const gchar *)buf);
		else
		{
			gint spos = sci_get_selection_start(sci);
			
			sci_replace_sel(sci, (const gchar *)buf);
			
			/* Insert additional new lines for user's ease */
			if (beautify && spos > 0)
				sci_insert_text(sci, spos, editor_get_eol_char(editor));
		}
		
		// Change the cursor position to the start of the line
		// and scroll to there
		gint cursPos = sci_get_current_position(sci);
		gint colPos = sci_get_col_from_position(sci, cursPos);
		sci_set_current_position(sci, cursPos - colPos, TRUE);
		
		yajl_gen_clear(g);
		
		if (logFormattingSuccessMessages)
			msgwin_msg_add(COLOR_BLUE, -1, doc,
						   "[%s] %s of %s succeeded! (%s)",
						   timeNow, chosenActionString,
						   document_get_basename_for_display(doc, -1),
						   DOC_FILENAME(doc));
		
		if (!workWithTextSelection)
		{
			GeanyIndentType indentType = indentChar == '\t' ? GEANY_INDENT_TYPE_TABS
															: GEANY_INDENT_TYPE_SPACES;
			document_set_filetype_and_indent(doc, filetypes_lookup_by_name("JSON"),
											 indentType, indentWidth);
		}
	}
	else
	{
		unsigned char *err_str = yajl_get_error(hand, 1,
												(unsigned char *)text_string,
												(size_t)text_len - 1);
		msgwin_msg_add(COLOR_RED, -1, doc,
					   "[%s] %s of %s failed!\n%s\n"
					   "Probably improper format or odd symbols! (%s)",
					   timeNow, chosenActionString,
					   document_get_basename_for_display(doc, -1),
					   err_str, DOC_FILENAME(doc));
		
		if (showErrorsInPopupWindow)
			dialogs_show_msgbox(GTK_MESSAGE_ERROR, "%s", err_str);
		
		yajl_free_error(hand, err_str);
	}
	
	yajl_gen_free(g);
	yajl_free(hand);
	
	g_free(textIndentString);
	g_free(text_string);
}

/* Plugin settings */

static void config_save_setting(GKeyFile *keyfile, const gchar *filePath)
{
	if (keyfile && filePath)
		g_key_file_save_to_file(keyfile, filePath, NULL);
}

static gboolean config_get_bool(GKeyFile *keyfile, const gchar *name)
{
	gboolean value = FALSE;
	
	if (keyfile)
	{
		GError *error = NULL;
		value = g_key_file_get_boolean(keyfile, fcfg_g_settings,
									   name, &error);
		if (error != NULL)
			g_error_free(error);
	}
	return value;
}

static gint config_get_uint(GKeyFile *keyfile, const gchar *name, guint maxVal)
{
	gint value = 0;
	
	if (keyfile)
	{
		GError *error = NULL;
		value = g_key_file_get_integer(keyfile, fcfg_g_settings,
									   name, &error);
		if (error != NULL)
			g_error_free(error);
		else
		{
			if (value < 0)
				value = 0;
			else if (value > maxVal)
				value = maxVal;
		}
	}
	return value;
}

static void config_set_bool(GKeyFile *keyfile, const gchar *name,
							gboolean value)
{
	if (!keyfile) return;
	
	g_key_file_set_boolean(keyfile, fcfg_g_settings, name, value);
}

static void config_set_uint(GKeyFile *keyfile, const gchar *name,
							gint value, guint maxVal)
{
	if (!keyfile) return;
	
	if (value < 0)
		value = 0;
	else if (value > maxVal)
		value = maxVal;
	
	g_key_file_set_integer(keyfile, fcfg_g_settings, name, value);
}

static void on_configure_response(GtkDialog* dialog, gint response,
									gpointer user_data)
{
	if (keyfile_plugin &&
		(response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY))
	{
		escapeForwardSlashes = gtk_toggle_button_get_active(
						GTK_TOGGLE_BUTTON(escape_forward_slashes_btn));
		config_set_bool(keyfile_plugin, fcfg_e_escForwardSlashed,
						escapeForwardSlashes);
		
		allowInvalidStringsInUtf8 = gtk_toggle_button_get_active(
						GTK_TOGGLE_BUTTON(allow_invalid_utf8_text_strings_btn));
		config_set_bool(keyfile_plugin, fcfg_e_allowInvalUtf8TextStr,
						allowInvalidStringsInUtf8);
		
		reformatMultipleJsonEntities = gtk_toggle_button_get_active(
						GTK_TOGGLE_BUTTON(reformat_multiple_json_entities_btn));
		config_set_bool(keyfile_plugin, fcfg_e_reformatMultJsonEntities,
						reformatMultipleJsonEntities);
		
		showErrorsInPopupWindow = gtk_toggle_button_get_active(
						GTK_TOGGLE_BUTTON(show_errors_in_window_btn));
		config_set_bool(keyfile_plugin, fcfg_e_showErrsInWindow,
						showErrorsInPopupWindow);
		
		logFormattingSuccessMessages = gtk_toggle_button_get_active(
						GTK_TOGGLE_BUTTON(log_formatting_success_messages_btn));
		config_set_bool(keyfile_plugin, fcfg_e_logMsgOnFormattingSuccess,
						logFormattingSuccessMessages);
		
		allowComments = gtk_toggle_button_get_active(
						GTK_TOGGLE_BUTTON(allow_comments_btn));
		config_set_bool(keyfile_plugin, fcfg_e_allowComments, allowComments);
		
		indentChar = gtk_combo_box_get_active(
						GTK_COMBO_BOX(indent_char_combo)) == 0 ? '\t' : ' ';
		config_set_uint(keyfile_plugin, fcfg_e_indentChar, indentChar, 255);
		
		indentWidth = gtk_spin_button_get_value_as_int(
						GTK_SPIN_BUTTON(indent_width_spin));
		config_set_uint(keyfile_plugin, fcfg_e_indentWidth, indentWidth, 10);
		
		config_save_setting(keyfile_plugin, plugin_config_path);
	}
}

static void config_set_defaults(GKeyFile *keyfile)
{
	if (!keyfile) return;
	
	config_set_bool(keyfile, fcfg_e_escForwardSlashed,
					escapeForwardSlashes = FALSE);
	
	config_set_bool(keyfile, fcfg_e_allowInvalUtf8TextStr,
					allowInvalidStringsInUtf8 = TRUE);
	
	config_set_bool(keyfile, fcfg_e_reformatMultJsonEntities,
					reformatMultipleJsonEntities = FALSE);
	
	config_set_bool(keyfile, fcfg_e_showErrsInWindow,
					showErrorsInPopupWindow = TRUE);
	
	config_set_bool(keyfile, fcfg_e_logMsgOnFormattingSuccess,
					logFormattingSuccessMessages = TRUE);
	
	config_set_bool(keyfile, fcfg_e_allowComments, allowComments = TRUE);
	
	config_set_uint(keyfile, fcfg_e_indentChar, indentChar = '\t', 255);
	
	config_set_uint(keyfile, fcfg_e_indentWidth, indentWidth = 4, 10);
}

GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
	
	//----------------------------------------------------------------
	escape_forward_slashes_btn = add_checkbox(vbox,
		_("Escape any forward slashes"),
		escapeForwardSlashes, NULL, FALSE);
	
	//----------------------------------------------------------------
	allow_invalid_utf8_text_strings_btn = add_checkbox(vbox,
		_("Allow invalid UTF-8 in the strings"),
		allowInvalidStringsInUtf8, NULL, FALSE);
	
	//----------------------------------------------------------------
	reformat_multiple_json_entities_btn = add_checkbox(vbox,
		_("Reformat multiple JSON entities at once"),
		reformatMultipleJsonEntities, NULL, FALSE);
	
	//----------------------------------------------------------------
	allow_comments_btn = add_checkbox(vbox,
		_("Allow (effectively strip and ignore) multiline "
		  "comments like for e.g. /* comment */"),
		allowComments, NULL, FALSE);
	
	//----------------------------------------------------------------
	GtkWidget *container = add_unnamed_hbox(vbox);
	
	const gchar *INDENT_TEXTS[] = {_("Tab"), _("Space")};
	indent_char_combo = add_combobox(container, _("Indentation:"), INDENT_TEXTS, 2,
									 indentChar == '\t' ? 0 : 1, NULL, FALSE);
	
	indent_width_spin = add_spinbox(container, _("Indent width:"), 0, 10, 1,
									indentWidth, NULL, TRUE);
	
	//----------------------------------------------------------------
	show_errors_in_window_btn = add_checkbox(vbox,
		_("Show errors in a message window"),
		showErrorsInPopupWindow, NULL, FALSE);
	
	//----------------------------------------------------------------
	log_formatting_success_messages_btn = add_checkbox(vbox,
		_("Log successful formatting messages"),
		logFormattingSuccessMessages, NULL, FALSE);
	
	//----------------------------------------------------------------
	g_signal_connect(dialog, "response",
					 G_CALLBACK(on_configure_response), NULL);
	
	gtk_widget_show_all(vbox);
	return vbox;
}


/* Geany plugin EP code */

static void item_activate_cb(GtkMenuItem *menuitem, gpointer gdata)
{
	my_json_prettify(document_get_current(), GPOINTER_TO_INT(gdata));
}

static void kb_activate(G_GNUC_UNUSED guint key_id)
{
	my_json_prettify(document_get_current(), key_id);
}

void plugin_init(GeanyData *data)
{
	/* read & prepare configuration */
	gchar *config_dir = g_build_path(G_DIR_SEPARATOR_S,
									 geany_data->app->configdir,
									 "plugins", "jsonconverter", NULL);
	
	plugin_config_path = g_build_path(G_DIR_SEPARATOR_S, config_dir,
									  "jsonconverter.conf", NULL);
	
	g_mkdir_with_parents(config_dir, S_IRUSR | S_IWUSR | S_IXUSR);
	g_free(config_dir);
	
	keyfile_plugin = g_key_file_new();
	
	if (!g_key_file_load_from_file(keyfile_plugin, plugin_config_path,
								   G_KEY_FILE_NONE, NULL))
	{
		config_set_defaults(keyfile_plugin);
		config_save_setting(keyfile_plugin, plugin_config_path);
	}
	else
	{
		escapeForwardSlashes = config_get_bool(keyfile_plugin,
											fcfg_e_escForwardSlashed);
		
		allowInvalidStringsInUtf8 = config_get_bool(keyfile_plugin,
											fcfg_e_allowInvalUtf8TextStr);
		
		reformatMultipleJsonEntities = config_get_bool(keyfile_plugin,
											fcfg_e_reformatMultJsonEntities);
		
		showErrorsInPopupWindow = config_get_bool(keyfile_plugin,
											fcfg_e_showErrsInWindow);
		
		logFormattingSuccessMessages = config_get_bool(keyfile_plugin,
											fcfg_e_logMsgOnFormattingSuccess);
		
		allowComments = config_get_bool(keyfile_plugin, fcfg_e_allowComments);
		
		indentChar = config_get_uint(keyfile_plugin, fcfg_e_indentChar, 255);
		
		indentWidth = config_get_uint(keyfile_plugin, fcfg_e_indentWidth, 10);
	}
	
	/* ---------------------------- */
	
	menu_item_minify = gtk_menu_item_new_with_mnemonic(_("JSON Minify"));
	gtk_widget_show(menu_item_minify);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu),
					  menu_item_minify);
	g_signal_connect(menu_item_minify, "activate",
					 G_CALLBACK(item_activate_cb), GINT_TO_POINTER(0));
	
	menu_item_prettify = gtk_menu_item_new_with_mnemonic(_("JSON Prettify"));
	gtk_widget_show(menu_item_prettify);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu),
					  menu_item_prettify);
	g_signal_connect(menu_item_prettify, "activate",
					 G_CALLBACK(item_activate_cb), GINT_TO_POINTER(1));
	
	/* do not activate if there are do documents opened */
	ui_add_document_sensitive(menu_item_minify);
	ui_add_document_sensitive(menu_item_prettify);
	
	/* Register shortcut key group */
	geany_key_group = plugin_set_key_group(geany_plugin, "json_prettifier", 2, NULL);
	
	/* Ctrl + Alt + m to minify */
	keybindings_set_item(geany_key_group, 0, kb_activate,
						 GDK_m, GDK_CONTROL_MASK | GDK_MOD1_MASK,
						 "run_json_minifier", _("Run the JSON Minifier"),
						 menu_item_minify);
	
	/* Ctrl + Alt + j to pretify */
	keybindings_set_item(geany_key_group, 1, kb_activate,
						 GDK_j, GDK_CONTROL_MASK | GDK_MOD1_MASK,
						 "run_json_prettifier", _("Run the JSON Prettifier"),
						 menu_item_prettify);
}


void plugin_cleanup(void)
{
	g_free(plugin_config_path);
	g_key_file_free(keyfile_plugin);
	gtk_widget_destroy(menu_item_minify);
	gtk_widget_destroy(menu_item_prettify);
}
