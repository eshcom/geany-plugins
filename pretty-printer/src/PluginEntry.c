/**
 *   Copyright (C) 2009  Cedric Tabin
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
 *   You should have received a copy of the GNU General Public License along
 *   with this program; if not, write to the Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/*
 * Basic plugin structure, based of Geany Plugin howto :
 *       http://www.geany.org/manual/reference/howto.html
 */

#ifdef HAVE_CONFIG_H
	#include "config.h" // for the gettext domain
#endif

#include <ctype.h>
#include <errno.h>

#include "PluginEntry.h"

/*========================================== PLUGIN INFORMATION ==========================================================*/

PLUGIN_VERSION_CHECK(224)
PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("XML PrettyPrinter"),
	_("Formats an XML and makes it human-readable. \nThis plugin currently "
	  "has no maintainer. Would you like to help by contributing to this plugin?"),
	PRETTY_PRINTER_VERSION, "Cédric Tabin - http://www.astorm.ch")

/*========================================== DECLARATIONS ================================================================*/

/* the main menu of the plugin */
static GtkWidget *menu_item_minify = NULL;
static GtkWidget *menu_item_prettify = NULL;

/* declaration of the functions */
static void item_activate_cb(GtkMenuItem *menuitem, gpointer gdata);
static void kb_activate(G_GNUC_UNUSED guint key_id);
static void config_closed(GtkWidget *configWidget, gint response, gpointer data);

/*========================================== FUNCTIONS ===================================================================*/

static gchar *get_config_file(void)
{
	gchar *filepath = get_config_filepath(PLUGIN, NULL);
	
	if (!g_file_test(filepath, G_FILE_TEST_EXISTS))
	{
		PrettyPrintingOptions *ppo = createDefaultPrettyPrintingOptions();
		if (!ppo) return NULL;
		
		GKeyFile *config = prefsToConfig(ppo);
		gboolean result = write_config_to_file(config, filepath, SYSLOG);
		g_key_file_free(config);
		
		if (!result)
		{
			g_free(filepath);
			return NULL;
		}
	}
	return filepath;
}

void plugin_init(GeanyData *data)
{
	gchar *filepath = get_config_file();
	
	/* load preferences */
	if (!prefsLoad(filepath))
		g_critical("failed to load preferences file '%s'", filepath);
	g_free(filepath);
	
	/* initializes the libxml2 */
	LIBXML_TEST_VERSION
	
	/* put the menu into the Tools */
	menu_item_minify = gtk_menu_item_new_with_mnemonic(_("XML Minify"));
	gtk_widget_show(menu_item_minify);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu),
					  menu_item_minify);
	/* add activation callback */
	g_signal_connect(menu_item_minify, "activate",
					 G_CALLBACK(item_activate_cb), GINT_TO_POINTER(0));
	
	/* put the menu into the Tools */
	menu_item_prettify = gtk_menu_item_new_with_mnemonic(_("XML Prettify"));
	gtk_widget_show(menu_item_prettify);
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu),
					  menu_item_prettify);
	/* add activation callback */
	g_signal_connect(menu_item_prettify, "activate",
					 G_CALLBACK(item_activate_cb), GINT_TO_POINTER(1));
	
	/* do not activate if there are do documents opened */
	ui_add_document_sensitive(menu_item_minify);
	ui_add_document_sensitive(menu_item_prettify);
	
	/* init keybindings */
	GeanyKeyGroup *key_group = plugin_set_key_group(geany_plugin, PLUGIN, 2, NULL);
	keybindings_set_item(key_group, 0, kb_activate, 0, 0,
						 "run_xml_minifier", _("Run the XML Minifier"),
						 menu_item_minify);
	keybindings_set_item(key_group, 1, kb_activate, 0, 0,
						 "run_xml_prettifier", _("Run the XML Prettifier"),
						 menu_item_prettify);
}

void plugin_cleanup(void)
{
	/* destroys the plugin */
	gtk_widget_destroy(menu_item_minify);
	gtk_widget_destroy(menu_item_prettify);
}

GtkWidget *plugin_configure(GtkDialog *dialog)
{
	/* creates the configuration widget */
	GtkWidget *widget = createPrettyPrinterConfigUI(dialog);
	g_signal_connect(dialog, "response", G_CALLBACK(config_closed), NULL);
	return widget;
}

/*========================================== LISTENERS ===================================================================*/

void config_closed(GtkWidget *configWidget, gint response, gpointer gdata)
{
	/* if the user clicked OK or APPLY, then save the settings */
	if (!ok_apply(response)) return;
	
	gchar *filename = get_config_file();
	if (!filename) return;
	
	fetchSettingsFromConfigUI(prettyPrintingOptions);
	
	GKeyFile *config = prefsToConfig(prettyPrintingOptions);
	write_config_to_file(config, filename, SYSLOG);
	g_key_file_free(config);
	g_free(filename);
}

void my_xml_prettify(GeanyDocument *doc, gboolean beautify)
{
	g_return_if_fail(doc != NULL);
	
	GeanyEditor *editor = doc->editor;
	ScintillaObject *sci = editor->sci;
	
	/* default printing options */
	if (!prettyPrintingOptions)
		prettyPrintingOptions = createDefaultPrettyPrintingOptions();
	
	gboolean has_selection = sci_has_selection(sci);
	/* retrieves the text */
	gchar *input_buffer = has_selection ? sci_get_selection_contents(sci)
										: sci_get_contents(sci, -1);
	
	/* checks if the data is an XML format */
	xmlDoc *parsedDocument = xmlParseDoc((const unsigned char *)input_buffer);
	
	/* this is not a valid xml => exit with an error message */
	if (parsedDocument == NULL)
	{
		g_free(input_buffer);
		dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Unable to parse the content as XML."));
		return;
	}
	
	/* free all */
	xmlFreeDoc(parsedDocument);
	
	gchar *output_buffer;
	
	if (beautify)
	{	/* process XML Prettifier */
		int input_length = has_selection ? sci_get_selected_text_length(sci)
										 : sci_get_length(sci);
		int output_length;
		int result = processXMLPrettyPrinting(input_buffer, input_length, &output_buffer,
											  &output_length, prettyPrintingOptions);
		g_free(input_buffer);
		
		if (result != PRETTY_PRINTING_SUCCESS)
		{
			dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("Unable to process PrettyPrinting on the specified XML "
													 "because some features are not supported.\n\n"
													 "See Help > Debug messages for more details..."));
			return;
		}
	}
	else
	{	/* process XML Minifier */
		output_buffer = input_buffer;
		
		const gchar *r;
		gchar *w = output_buffer;
		
		gboolean is_payload = FALSE;
		foreach_str(r, output_buffer)
		{
			if (is_payload)
			{
				if (isspace(*r))
				{
					*w++ = ' ';
					is_payload = FALSE;
				}
				else
					*w++ = *r;
			}
			else if (!isspace(*r))
			{
				*w++ = *r;
				is_payload = TRUE;
			}
		}
		if (*(w-1) == ' ')
			w--;
		*w = 0x0; // null terminated
	}
	
	/* updates the document */
	if (has_selection)
		sci_replace_sel(sci, output_buffer);
	else
		sci_set_text(sci, output_buffer);
	
	g_free(output_buffer);
	
	/* set the line */
	int xOffset = scintilla_send_message(sci, SCI_GETXOFFSET, 0, 0);
	/* TODO update with the right function-call for geany-0.19 */
	scintilla_send_message(sci, SCI_LINESCROLL, -xOffset, 0);
	
	/* sets the type */
	if (!has_selection)
	{
		GeanyIndentType indentType = prettyPrintingOptions->indentChar == '\t' ?
														GEANY_INDENT_TYPE_TABS :
														GEANY_INDENT_TYPE_SPACES;
		document_set_filetype_and_indent(doc, filetypes_index(GEANY_FILETYPES_XML),
										 indentType, prettyPrintingOptions->indentWidth);
	}
}

void item_activate_cb(GtkMenuItem *menuitem, gpointer gdata)
{
	my_xml_prettify(document_get_current(), GPOINTER_TO_INT(gdata));
}

void kb_activate(G_GNUC_UNUSED guint key_id)
{
	my_xml_prettify(document_get_current(), key_id);
}
