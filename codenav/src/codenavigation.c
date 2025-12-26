/*
 *      codenavigation.c - this file is part of "codenavigation", which is
 *      part of the "geany-plugins" project.
 *
 *      Copyright 2009 Lionel Fuentes <funto66(at)gmail(dot)com>
 * 		Copyright 2014 Federico Reghenzani <federico(dot)dev(at)reghe(dot)net>
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

#ifdef HAVE_CONFIG_H
	#include "config.h" // for the gettext domain
#endif

#include "codenavigation.h"
#include "switch_head_impl.h"
#include "goto_file.h"

#include "../../utils/src/common.h"


/********************* Data types for the feature *********************/
/* Column for the configuration widget */
typedef enum
{
	COLUMN_IMPL = 0,
	COLUMN_HEAD,
	NB_COLUMNS
} Column;

/************************* Global variables ***************************/

GeanyKeyGroup *plugin_key_group;

static GtkListStore *list_store;	/* for settings dialog */

/**************************** Prototypes ******************************/

GtkWidget *config_widget(void);

static void load_configuration(void);

static void on_configure_add_language(GtkWidget *widget, gpointer data);

static void on_configure_remove_language(GtkWidget *widget, gpointer data);

static void on_configure_reset_to_default(GtkWidget *widget, gpointer data);

static void on_configure_cell_edited(GtkCellRendererText *text,
								     gchar *arg1, gchar *arg2, gpointer data);

static void on_configure_response(GtkDialog *dialog, gint response,
								  gpointer user_data);

/***************************** Functions ******************************/

/**
 * @brief Called by Geany to initialize the plugin.
 * @note data is the same as geany_data.
 */
static gboolean plugin_codenav_init(GeanyPlugin *plugin, G_GNUC_UNUSED gpointer pdata)
{
	geany_data = plugin->geany_data;
	
	log_func();
	
	plugin_key_group = plugin_set_key_group(plugin, PLUGIN, NB_KEY_IDS, NULL);
	
	/* Load configuration */
	load_configuration();
	/* Initialize the features */
	switch_head_impl_init();
	goto_file_init();
	return TRUE;
}

/**
 * @brief load plugin's configuration or set default values
 * @param void
 * 
 * @return voids
 */
static void load_configuration(void)
{
	gchar **impl_list  = NULL, **head_list = NULL;
	gsize head_list_len, impl_list_len;
	
	/* Load user configuration */ 
	gchar *config_filename = get_config_filepath(PLUGIN, NULL);
	
	gboolean result = FALSE;
	GKeyFile *config = load_config_from_file(config_filename, &result);
	if (result) {
		log_debug("Loading user configuration");
		
		impl_list = g_key_file_get_string_list(config, "switch_head_impl",
											   "implementations_list",
											   &impl_list_len, NULL);
		head_list = g_key_file_get_string_list(config, "switch_head_impl",
											   "headers_list", &head_list_len, NULL);
		
		// Wrong lists
		if (head_list_len != impl_list_len) {
			dialogs_show_msgbox(GTK_MESSAGE_WARNING,
				_("Codenav head/impl lists should have been same length. "
				  "Geany will use the default configuration."));
			fill_default_languages_list();
		}
		else
			fill_languages_list((const gchar **)impl_list, (const gchar **)head_list,
								head_list_len);
	}
	else {
		log_debug("Fresh configuration");
		fill_default_languages_list();
	}
	
	/* Freeing memory */
	g_key_file_free(config);
	g_free(config_filename);
	
	if (impl_list) {
		for (gsize i = 0; i < impl_list_len; i++)
			g_free(impl_list[i]);
		g_free(impl_list);
	}
	if (head_list) {
		for (gsize i = 0; i < head_list_len; i++)
			g_free(head_list[i]);
		g_free(head_list);
	}
}

/**
 * @brief Called by Geany to show the plugin's configure dialog. This function
 * 		  is always called after plugin_init() is called.
 * @param the plugin GtkWidget*
 * 
 * @return the GtkDialog from Geany
 */
GtkWidget *plugin_codenav_configure(G_GNUC_UNUSED GeanyPlugin *plugin,
									GtkDialog *dialog, G_GNUC_UNUSED gpointer pdata)
{
	log_func();
	
	GtkWidget *vbox = gtk_vbox_new(FALSE, 6);
	
	/* Switch header/implementation widget */
	gtk_box_pack_start(GTK_BOX(vbox), config_widget(), TRUE, TRUE, 0);
	
	gtk_widget_show_all(vbox);
	
	/* Connect a callback for when the user clicks a dialog button */
	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);
	
	return vbox;
}

/**
 * @brief Called by Geany before unloading the plugin
 * @param void 
 * 
 * @return void
 */
static void plugin_codenav_cleanup(G_GNUC_UNUSED GeanyPlugin *plugin,
								   G_GNUC_UNUSED gpointer pdata)
{
	log_func();
	
	/* Cleanup the features */
	goto_file_cleanup();
	switch_head_impl_cleanup();
}

/**
 * @brief	Callback called when validating the configuration of the plug-in
 * @param	dialog		the parent dialog, not very interesting here
 * @param	response	OK/Cancel/Apply user action
 * @param	user_data	NULL
 * 
 * @return void
 */
static void on_configure_response(GtkDialog *dialog, gint response,
								  gpointer user_data)
{
	log_func();
	
	if (!ok_apply(response)) return;
	
	/* Write the settings into a file, using GLib's GKeyFile API.
	 * File name :
	 * geany->app->configdir G_DIR_SEPARATOR_S "plugins" G_DIR_SEPARATOR_S 
	 * PLUGIN G_DIR_SEPARATOR_S PLUGIN".conf"
	 * e.g. this could be: ~/.config/geany/plugins/codenav/codenav.conf
	 */
	
	/* Allocate the list */
	gsize list_len = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(list_store), NULL);
	gchar **impl_list = g_malloc0(sizeof(gchar **) * list_len);
	gchar **head_list = g_malloc0(sizeof(gchar **) * list_len);
	
	gsize empty_lines = 0;
	gsize i = 0;
	
	if (list_len > 0) {
		// Get the first item
		GtkTreeIter iter;
		gtk_tree_model_iter_children(GTK_TREE_MODEL(list_store), &iter, NULL);
		
		do { /* forall elements in list... */
			gtk_tree_model_get(GTK_TREE_MODEL(list_store), &iter,
							   COLUMN_IMPL, &impl_list[i], -1);
			gtk_tree_model_get(GTK_TREE_MODEL(list_store), &iter,
							   COLUMN_HEAD, &head_list[i], -1);
			
			/* If one field is empty, ignore this line (it will be replaces
			   at next execution) */ 
			if (strlen(impl_list[i]) == 0 || strlen(head_list[i]) == 0)
				empty_lines++;
			else
				i++;
		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(list_store), &iter));
	}
	
	GKeyFile *config = g_key_file_new();
	
	/* write lists */
	g_key_file_set_string_list(config, "switch_head_impl", "implementations_list", 
							   (const gchar * const *)impl_list, list_len - empty_lines);
	g_key_file_set_string_list(config, "switch_head_impl", "headers_list", 
							   (const gchar * const *)head_list, list_len - empty_lines);
	
	/* Save config */
	gchar *filepath = get_config_filepath(PLUGIN, NULL);
	write_config_to_file(config, filepath, MSGBOX);
	g_free(filepath);
	g_key_file_free(config);
	
	/* Replace the current (runtime) languages list */
	fill_languages_list((const gchar **)impl_list, (const gchar **)head_list,
						list_len - empty_lines);
	
	/* Freeing memory */
	for (i = 0; i < list_len; i++) {
		g_free(impl_list[i]);
		g_free(head_list[i]);
	}
	g_free(impl_list);
	g_free(head_list);
	
	g_key_file_free(config);
}

 /**
 * @brief	Utility function to concatenate the extensions of a language,
 * 			separated with a comma, like PHP-implode function.
 * @param	extensions	a list of (string) extensions
 * 
 * @return	concatenated string.
 */
static gchar *concatenate_extensions(GSList *extensions)
{
	gchar *p_str = NULL;
	
	for (GSList *iter_ext = extensions; iter_ext; iter_ext = iter_ext->next)
	{
		gchar *temp = p_str;
		p_str = g_strjoin(",", (const gchar *)(iter_ext->data), p_str, NULL);
		g_free(temp);
	}
	
	return p_str;
}

/**
 * @brief	Utility function to add a language to a GtkListStore
 * @param	list	the list where to add lang
 * @param	lang	the item to add
 * 
 * @return	void
 */
static void add_language(GtkListStore *list, Language *lang)
{
	gchar *p_str = NULL;
	GtkTreeIter tree_iter;
	
	if (lang->head_extensions == NULL || lang->impl_extensions == NULL)
		return;
	
	/* Append an empty row */
	gtk_list_store_append(list, &tree_iter);
	
	/* Header extensions */
	p_str = concatenate_extensions(lang->head_extensions);
	gtk_list_store_set(list, &tree_iter, COLUMN_HEAD, p_str, -1);
	g_free(p_str);
	
	/* Implementation extensions */
	p_str = concatenate_extensions(lang->impl_extensions);
	gtk_list_store_set(list, &tree_iter, COLUMN_IMPL, p_str, -1);
	g_free(p_str);
}

/**
 * @brief	The configuration widget
 * 
 * @return	The configuration widget
 */
GtkWidget*
config_widget(void)
{
	GtkWidget *help_label;
	GtkWidget *frame, *vbox, *tree_view;
	GtkWidget *hbox_buttons, *add_button, *remove_button, *reset_button;
	GtkTreeViewColumn *column;
	GtkCellRenderer *cell_renderer;

	GSList *iter_lang;

	log_func();

	/* Frame, which is the returned widget */
	frame = gtk_frame_new(_("Switch header/implementation"));

	/* Main VBox */
	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(frame), vbox);

	/* Help label */
	help_label = gtk_label_new(_("You can specify multiple extensions by "
								 "separating them by commas."));
	gtk_box_pack_start(GTK_BOX(vbox), help_label, FALSE, FALSE, 6);

	/* ======= Extensions list ======= */

	/* Add a list containing the extensions for each language (headers / implementations) */
	/* - create the GtkListStore */
	list_store = gtk_list_store_new(NB_COLUMNS, G_TYPE_STRING, G_TYPE_STRING);

	/* - fill the GtkListStore with the extensions of the languages */
	for (iter_lang = switch_head_impl_get_languages(); 
			iter_lang != NULL ; iter_lang = iter_lang->next)
		add_language(list_store, (Language*)(iter_lang->data));

	/* - create the GtkTreeView */
	tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));
 
	/* - add the columns */
	/* -> implementations : */
	cell_renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(cell_renderer), "editable", TRUE, NULL);
	g_signal_connect_after(G_OBJECT(cell_renderer), "edited", G_CALLBACK(on_configure_cell_edited), GINT_TO_POINTER(COLUMN_IMPL));
	column = gtk_tree_view_column_new_with_attributes(	_("Implementations extensions"), cell_renderer,
														"text", COLUMN_IMPL,
														NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);

	/* -> headers : */
	cell_renderer = gtk_cell_renderer_text_new();
	g_object_set(G_OBJECT(cell_renderer), "editable", TRUE, NULL);
	g_signal_connect_after(G_OBJECT(cell_renderer), "edited", G_CALLBACK(on_configure_cell_edited), GINT_TO_POINTER(COLUMN_HEAD));
	column = gtk_tree_view_column_new_with_attributes(	_("Headers extensions"), cell_renderer,
														"text", COLUMN_HEAD,
														NULL);
	gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), column);


	/* - finally add the GtkTreeView to the frame's vbox */
	gtk_box_pack_start(GTK_BOX(vbox), tree_view, TRUE, TRUE, 6);


	/* ========= Buttons ======== */

	/* HBox */
	hbox_buttons = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_buttons, FALSE, FALSE, 0);

	/* Add the "add" button to the frame's hbox */
	add_button = gtk_button_new_from_stock(GTK_STOCK_ADD);
	g_signal_connect(G_OBJECT(add_button), "clicked", G_CALLBACK(on_configure_add_language), tree_view);
	gtk_box_pack_start(GTK_BOX(hbox_buttons), add_button, FALSE, FALSE, 0);

	/* Add the "remove" button to the frame's hbox */
	remove_button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
	g_signal_connect(G_OBJECT(remove_button), "clicked", G_CALLBACK(on_configure_remove_language), tree_view);
	gtk_box_pack_start(GTK_BOX(hbox_buttons), remove_button, FALSE, FALSE, 0);

	/* Add the "reset to default" button to the frame's hbox */
	reset_button = gtk_button_new_with_label(_("Reset to Default"));
	g_signal_connect(G_OBJECT(reset_button), "clicked", G_CALLBACK(on_configure_reset_to_default), NULL);
	gtk_box_pack_start(GTK_BOX(hbox_buttons), reset_button, FALSE, FALSE, 0);
	gtk_widget_grab_focus(tree_view);

	return frame;
}

 /**
 * @brief	Callback for adding a language in the configuration dialog
 * @param	button	the button, not used here
 * @param	data	gtktreeview where to act
 * 
 * @return	void
 */
static void on_configure_add_language(GtkWidget *button, gpointer data)
{
	log_func();
	
	/* Add a line */
	GtkTreeIter tree_iter;
	gtk_list_store_append(list_store, &tree_iter);
	
	GtkWidget *tree_view = (GtkWidget *)data;
	
	/* and give the focus to it */
	gint nb_lines = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(list_store), NULL);
	GtkTreePath *path = gtk_tree_path_new_from_indices(nb_lines-1, -1);
	GtkTreeViewColumn *column = gtk_tree_view_get_column(GTK_TREE_VIEW(tree_view), 0);
	gtk_tree_view_set_cursor(GTK_TREE_VIEW(tree_view), path, column, TRUE);
	
	gtk_tree_path_free(path);
}

/**
 * @brief	Callback for removing a language in the configuration dialog
 * @param	button	the button, not used here
 * @param	data	gtktreeview where to act
 * 
 * @return	void
 */
static void on_configure_remove_language(GtkWidget *button, gpointer data)
{
	log_func();
	
	GtkTreeView *tree_view = (GtkTreeView *)data;
	GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);
	GtkTreeIter tree_iter;
	
	if (!gtk_tree_selection_get_selected(selection, NULL, &tree_iter)) {
		log_debug("Delete without selection!");
		return;
	}
	/* Remove the element */
	gtk_list_store_remove(list_store, &tree_iter);
}

/**
 * @brief	Callback for reset to default languages in the configuration dialog
 * @param	button	the button, not used here
 * @param	data	null
 * 
 * @return	void
 */
static void on_configure_reset_to_default(GtkWidget *button, gpointer data)
{
	GtkWidget *dialog_new;
	
	/* ask to user if he's sure */
	dialog_new = gtk_message_dialog_new(GTK_WINDOW(geany->main_widgets->window),
										GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
										GTK_BUTTONS_OK_CANCEL,
										_("Are you sure you want to delete all languages "
										  "and restore defaults?\nThis action cannot be undone."));
	gtk_window_set_title(GTK_WINDOW(dialog_new), "Geany");
	
	if (gtk_dialog_run(GTK_DIALOG(dialog_new)) == GTK_RESPONSE_OK)
	{
		/* OK, reset! */
		fill_default_languages_list();
		
		/* clear and refill the GtkListStore with default extensions */
		gtk_list_store_clear(list_store);
		
		for (GSList *iter_lang = switch_head_impl_get_languages(); 
				iter_lang != NULL; iter_lang = iter_lang->next)
			add_language(list_store, (Language*)(iter_lang->data));
	}
	gtk_widget_destroy(dialog_new);
}


/**
 * @brief	Callback called when a cell has been edited in the configuration dialog
 * @param	renderer	field object
 * @param	path
 * @param	text		the new text
 * @param	data		column where event is called
 * 
 * @return	void
 */
static void on_configure_cell_edited(GtkCellRendererText *renderer, gchar *path,
									 gchar *text, gpointer data)
{
	log_func();
	
	gchar character = text[0];
	gint i = 1;
	
	while (character != '\0') {
		if (!g_ascii_isalpha(character) && character != ',') {
			log_debug("Not-valid char");
			return; // invalid extension
		}
		character=text[i++];
	}
	
	GtkTreeIter iter;
	Column col = (Column)(GPOINTER_TO_INT(data));
	
	/* Replace old text with new */
	gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(list_store), &iter, path);
	gtk_list_store_set(list_store, &iter, col, text, -1);
}


/* Show help */
static void plugin_codenav_help(G_GNUC_UNUSED GeanyPlugin *plugin,
							    G_GNUC_UNUSED gpointer pdata)
{
	utils_open_browser("https://plugins.geany.org/codenav.html");
}


/* Load module */
G_MODULE_EXPORT
void geany_load_module(GeanyPlugin *plugin)
{
	/* Setup translation */
	main_locale_init(LOCALEDIR, GETTEXT_PACKAGE);
	
	/* Set metadata */
	plugin->info->name = _("Code navigation");
	plugin->info->description = _("This plugin adds features to facilitate "
								  "navigation between source files.");
	plugin->info->version = CODE_NAVIGATION_VERSION;
	plugin->info->author = "Lionel Fuentes, Federico Reghenzani";
	
	/* Set functions */
	plugin->funcs->init = plugin_codenav_init;
	plugin->funcs->cleanup = plugin_codenav_cleanup;
	plugin->funcs->help = plugin_codenav_help;
	plugin->funcs->configure = plugin_codenav_configure;
	
	/* Register! */
	GEANY_PLUGIN_REGISTER(plugin, 226);
}
