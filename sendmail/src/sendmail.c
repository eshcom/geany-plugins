/*
 *      sendmail.c
 *
 *      Copyright 2007-2016 Frank Lanitz <frank(at)frank(dot)uvena(dot)de>
 *      Copyright 2007 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2007, 2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *      Copyright 2008, 2009 Timothy Boronczyk <tboronczyk(at)gmail(dot)com>
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
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/* A little plugin to send a document as attachment using the preferred mail client */

#ifdef HAVE_CONFIG_H
	#include "config.h"		// for the gettext domain
#endif

#include <geanyplugin.h>	// includes geany.h, gtkcompat.h, etc.

#include "mail-icon.xpm"
#include "../../utils/src/common.h"

GeanyPlugin	*geany_plugin;
GeanyData	*geany_data;	// the code uses the macro "geany" (see geany->)


PLUGIN_VERSION_CHECK(224)

PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("SendMail"),
	_("Sends the current file as attachment with your favorite mailer"),
	VERSION,
	"Frank Lanitz <frank@frank.uvena.de>")

/* Keybinding(s) */
enum
{
	SENDMAIL_KB,
	COUNT_KB
};

static gchar *config_file = NULL;
static gchar *mailer = NULL;
static gchar *address = NULL;
gboolean icon_in_toolbar = FALSE;
gboolean use_address_dialog = FALSE;
/* Needed global to remove from toolbar again */
GtkWidget *mailbutton = NULL;
static GtkWidget *main_menu_item = NULL;


/* Callback for sending file as attachment */
static void send_as_attachment(G_GNUC_UNUSED GtkMenuItem *menuitem,
							   G_GNUC_UNUSED gpointer gdata)
{
	GeanyDocument *doc = document_get_current();
	doc->file_name ? document_save_file(doc, FALSE) : dialogs_show_save_as();
	
	if (!doc->file_name)
	{
		ui_set_statusbar(FALSE, _("File has to be saved before sending."));
		return;
	}
	if (!mailer)
	{
		ui_set_statusbar(FALSE, _("Please define a mail client first."));
		return;
	}
	
	gchar *locale_filename = utils_get_locale_from_utf8(doc->file_name);
	GString	*cmd_str = g_string_new(mailer);
	
	if (use_address_dialog && g_strrstr(mailer, "%r"))
	{
		gchar *input = dialogs_show_input(_("Recipient's Address"),
										  GTK_WINDOW(geany->main_widgets->window),
										  _("Enter the recipient's e-mail address:"),
										  address);
		if (!input)
		{
			g_string_free(cmd_str, TRUE);
			g_free(locale_filename);
			return;
		}
		
		GKeyFile *config = load_config_from_file(config_file, NULL);
		
		SETPTR(address, input);
		g_key_file_set_string(config, "tools", "address", address);
		
		write_config_to_file(config, config_file, MSGBOX);
		g_key_file_free(config);
	}
	
	if (!utils_string_replace_all(cmd_str, "%f", locale_filename))
		ui_set_statusbar(FALSE, _("Filename placeholder not found. "
								  "The executed command might have failed."));
	
	if (use_address_dialog && address)
	{
		if (!utils_string_replace_all(cmd_str, "%r", address))
			ui_set_statusbar(FALSE, _("Recipient address placeholder not found. "
									  "The executed command might have failed."));
	}
	else
	{	/* Removes %r if option was not activ but was included into command */
		utils_string_replace_all(cmd_str, "%r", "");
	}
	
	gchar *basename = g_path_get_basename(locale_filename);
	utils_string_replace_all(cmd_str, "%b", basename);
	g_free(basename);
	
	GError *error = NULL;
	gchar *command = g_string_free(cmd_str, FALSE);
	g_spawn_command_line_async(command, &error);
	
	if (error)
	{
		ui_set_statusbar(FALSE, _("Could not execute mailer. "
								  "Please check your configuration."));
		g_error_free(error);
	}
	g_free(locale_filename);
	g_free(command);
}

static void key_send_as_attachment(G_GNUC_UNUSED guint key_id)
{
	send_as_attachment(NULL, NULL);
}

#define GEANYSENDMAIL_STOCK_MAIL "geanysendmail-mail"

static void add_stock_item(void)
{
	GtkIconSet *icon_set;
	GtkIconFactory *factory = gtk_icon_factory_new();
	GtkIconTheme *theme = gtk_icon_theme_get_default();
	GtkStockItem item = { (gchar*)(GEANYSENDMAIL_STOCK_MAIL), (gchar*)(N_("Mail")), 0, 0, (gchar*)(GETTEXT_PACKAGE) };

	if (gtk_icon_theme_has_icon(theme, "mail-message-new"))
	{
		GtkIconSource *icon_source = gtk_icon_source_new();
		icon_set = gtk_icon_set_new();
		gtk_icon_source_set_icon_name(icon_source, "mail-message-new");
		gtk_icon_set_add_source(icon_set, icon_source);
		gtk_icon_source_free(icon_source);
	}
	else
	{
		GdkPixbuf *pb = gdk_pixbuf_new_from_xpm_data(mail_icon);
		icon_set = gtk_icon_set_new_from_pixbuf(pb);
		g_object_unref(pb);
	}
	gtk_icon_factory_add(factory, item.stock_id, icon_set);
	gtk_stock_add(&item, 1);
	gtk_icon_factory_add_default(factory);

	g_object_unref(factory);
	gtk_icon_set_unref(icon_set);
}


static void show_icon(void)
{
	mailbutton = GTK_WIDGET(gtk_tool_button_new_from_stock(GEANYSENDMAIL_STOCK_MAIL));
	plugin_add_toolbar_item(geany_plugin, GTK_TOOL_ITEM(mailbutton));
	ui_add_document_sensitive(mailbutton);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_tool_item_set_tooltip_text(GTK_TOOL_ITEM(mailbutton), _("Send by mail"));
#endif
	g_signal_connect(G_OBJECT(mailbutton), "clicked", G_CALLBACK(send_as_attachment), NULL);
	gtk_widget_show_all(mailbutton);
	icon_in_toolbar = TRUE;
}

static void cleanup_icon(void)
{
	if (mailbutton)
		gtk_container_remove(GTK_CONTAINER(geany->main_widgets->toolbar), mailbutton);
	icon_in_toolbar = FALSE;
}


static struct
{
	GtkWidget *entry;
	GtkWidget *checkbox_icon_to_toolbar;
	GtkWidget *checkbox_use_addressdialog;
}
pref_widgets;


static void on_configure_response(G_GNUC_UNUSED GtkDialog *dialog, gint response,
								  G_GNUC_UNUSED  gpointer user_data)
{
	if (!ok_apply(response)) return;
	
	SETPTR(mailer, g_strdup(gtk_entry_get_text(GTK_ENTRY(pref_widgets.entry))));
	
	gboolean configure_toogle_status =
							gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
									pref_widgets.checkbox_icon_to_toolbar));
	
	if (icon_in_toolbar ^ configure_toogle_status)
	{	/* Only do anything if a status change is needed */
		icon_in_toolbar ? cleanup_icon() // We need to remove the toolbar icon
						: show_icon();   // We need to show the toolbar icon
	}
	
	use_address_dialog = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(
								pref_widgets.checkbox_use_addressdialog));
	
	GKeyFile *config = load_config_from_file(config_file, NULL);
	
	g_key_file_set_string(config, "tools", "mailer", mailer);
	g_key_file_set_boolean(config, "tools", "address_usage", use_address_dialog);
	g_key_file_set_boolean(config, "icon", "show_icon", icon_in_toolbar);
	
	write_config_to_file(config, config_file, MSGBOX);
	g_key_file_free(config);
}

GtkWidget *plugin_configure(GtkDialog *dialog)
{
	GtkWidget	*label1, *label2, *vbox;

	vbox = gtk_vbox_new(FALSE, 6);

	/* add a label and a text entry to the dialog */
	label1 = gtk_label_new(_("Path and options for the mail client:"));
	gtk_widget_show(label1);
	gtk_misc_set_alignment(GTK_MISC(label1), 0, 0.5);
	pref_widgets.entry = gtk_entry_new();
	gtk_widget_show(pref_widgets.entry);
	if (mailer != NULL)
		gtk_entry_set_text(GTK_ENTRY(pref_widgets.entry), mailer);

	label2 = gtk_label_new(_("Note: \n\t%f will be replaced by your file."\
		"\n\t%r will be replaced by recipient's email address."\
		"\n\t%b will be replaced by basename of a file"\
		"\n\tExamples:"\
		"\n\tsylpheed --attach \"%f\" --compose \"%r\""\
		"\n\tmutt -s \"Sending \'%b\'\" -a \"%f\" \"%r\""));
	gtk_label_set_selectable(GTK_LABEL(label2), TRUE);
	gtk_widget_show(label2);
	gtk_misc_set_alignment(GTK_MISC(label2), 0, 0.5);

	pref_widgets.checkbox_icon_to_toolbar = gtk_check_button_new_with_label(_("Show toolbar icon"));
	gtk_widget_set_tooltip_text(pref_widgets.checkbox_icon_to_toolbar,
		_("Shows a icon in the toolbar to send file more easy."));
	gtk_button_set_focus_on_click(GTK_BUTTON(pref_widgets.checkbox_icon_to_toolbar), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pref_widgets.checkbox_icon_to_toolbar), icon_in_toolbar);
	gtk_widget_show(pref_widgets.checkbox_icon_to_toolbar);

	pref_widgets.checkbox_use_addressdialog = gtk_check_button_new_with_label(_
		("Use dialog for entering email address of recipients"));

	gtk_button_set_focus_on_click(GTK_BUTTON(pref_widgets.checkbox_use_addressdialog), FALSE);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(pref_widgets.checkbox_use_addressdialog), use_address_dialog);
	gtk_widget_show(pref_widgets.checkbox_use_addressdialog);

	gtk_box_pack_start(GTK_BOX(vbox), label1, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), pref_widgets.entry, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), label2, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), pref_widgets.checkbox_icon_to_toolbar, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox), pref_widgets.checkbox_use_addressdialog, FALSE, FALSE, 0);

	gtk_widget_show(vbox);

	g_signal_connect(dialog, "response", G_CALLBACK(on_configure_response), NULL);
	return vbox;
}

/* Called by Geany to initialize the plugin */
void plugin_init(GeanyData G_GNUC_UNUSED *data)
{
	config_file = get_config_filepath(PLUGIN, NULL);
	
	/* Initialising options from config file */
	GKeyFile *config = load_config_from_file(config_file, NULL);
	
	mailer = g_key_file_get_string(config, "tools", "mailer", NULL);
	address = g_key_file_get_string(config, "tools", "address", NULL);
	use_address_dialog = g_key_file_get_boolean(config, "tools", "address_usage", NULL);
	icon_in_toolbar = g_key_file_get_boolean(config, "icon", "show_icon", NULL);
	
	g_key_file_free(config);
	
	add_stock_item();
	if (icon_in_toolbar) show_icon();
	
	/* Build up menu entry */
	GtkWidget *menu_mail = gtk_menu_item_new_with_mnemonic(_("_Mail document"));
	gtk_container_add(GTK_CONTAINER(geany->main_widgets->tools_menu), menu_mail);
	gtk_widget_set_tooltip_text(menu_mail,
		_("Sends the opened file as unzipped attachment by any mailer from your $PATH"));
	g_signal_connect(G_OBJECT(menu_mail), "activate", G_CALLBACK(send_as_attachment), NULL);
	
	/* setup keybindings */
	GeanyKeyGroup *key_group = plugin_set_key_group(geany_plugin, PLUGIN,
													COUNT_KB, NULL);
	keybindings_set_item(key_group, SENDMAIL_KB, key_send_as_attachment,
		0, 0, "send_file_as_attachment", _("Send file by mail"), menu_mail);
	
	gtk_widget_show_all(menu_mail);
	ui_add_document_sensitive(menu_mail);
	main_menu_item = menu_mail;
}


void plugin_cleanup(void)
{
	gtk_widget_destroy(main_menu_item);
	cleanup_icon();
	g_free(mailer);
	g_free(address);
	g_free(config_file);
}
