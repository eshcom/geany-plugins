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

#include "ConfigUI.h"

#include "../../utils/src/ui_plugins.h"

/*================================ PRIVATE PROPERTIES ================================*/

static GtkWidget *commentOneLine;
static GtkWidget *commentInline;
static GtkWidget *commentAlign;
static GtkWidget *textOneLine;
static GtkWidget *textInline;
static GtkWidget *textAlign;
static GtkWidget *cdataOneLine;
static GtkWidget *cdataInline;
static GtkWidget *cdataAlign;
static GtkWidget *emptyNodeStripping;
static GtkWidget *emptyNodeStrippingSpace;
static GtkWidget *emptyNodeSplit;
static GtkWidget *indentChar;
static GtkWidget *indentCount;
static GtkWidget *lineBreak;

/*================================= PUBLIC FUNCTIONS =================================*/

/* redeclaration of extern variable */
PrettyPrintingOptions *prettyPrintingOptions;

GtkWidget *createPrettyPrinterConfigUI(GtkDialog *dialog)
{
	PrettyPrintingOptions *ppo;
	GtkWidget *vbox, *child_vbox, *container;
	
	/* default printing options */
	if (prettyPrintingOptions == NULL)
		prettyPrintingOptions = createDefaultPrettyPrintingOptions();
	
	ppo = prettyPrintingOptions;
	vbox = gtk_vbox_new(FALSE, 0);
	
	//----------------------------------------------------------------
	container = add_named_vbox(vbox, _("Comments"));
	
	commentOneLine = add_checkbox(container, _("Put on one line"),
								  ppo->oneLineComment, NULL, TRUE);
	commentInline = add_checkbox(container, _("Inline if possible"),
								 ppo->inlineComment, NULL, TRUE);
	commentAlign = add_checkbox(container, _("Alignment"),
								ppo->alignComment, NULL, TRUE);
	
	//----------------------------------------------------------------
	container = add_named_vbox(vbox, _("Text nodes"));
	
	textOneLine = add_checkbox(container, _("Put on one line"),
							   ppo->oneLineText, NULL, TRUE);
	textInline = add_checkbox(container, _("Inline if possible"),
							  ppo->inlineText, NULL, TRUE);
	textAlign = add_checkbox(container, _("Alignment"),
							 ppo->alignText, NULL, TRUE);
	
	//----------------------------------------------------------------
	container = add_named_vbox(vbox, _("CDATA"));
	
	cdataOneLine = add_checkbox(container, _("Put on one line"),
								ppo->oneLineCdata, NULL, TRUE);
	cdataInline = add_checkbox(container, _("Inline if possible"),
							   ppo->inlineCdata, NULL, TRUE);
	cdataAlign = add_checkbox(container, _("Alignment"),
							  ppo->alignCdata, NULL, TRUE);
	
	//----------------------------------------------------------------
	container = add_named_vbox(vbox, _("Empty nodes"));
	
	emptyNodeStripping = add_checkbox(container, _("Concatenation (<x></x> to <x/>)"),
									  ppo->emptyNodeStripping, NULL, TRUE);
	emptyNodeStrippingSpace = add_checkbox(container, _("Spacing (<x/> to <x />)"),
										   ppo->emptyNodeStrippingSpace, NULL, TRUE);
	emptyNodeSplit = add_checkbox(container, _("Expansion (<x/> to <x></x>)"),
								  ppo->forceEmptyNodeSplit, NULL, TRUE);
	
	//----------------------------------------------------------------
	child_vbox = add_unnamed_vbox(vbox);
	
	//--------------------------------------------
	container = add_unnamed_hbox(child_vbox);
	
	const gchar *INDENT_TEXTS[] = {_("Tab"), _("Space")};
	indentChar = add_combobox(container, _("Indentation:"), INDENT_TEXTS, 2,
							  ppo->indentChar == ' ' ? 1 : 0, NULL, FALSE);
	
	indentCount = add_spinbox(container, _("Symbols count:"), 0, 10, 1,
							  ppo->indentLength, NULL, TRUE);
	
	//--------------------------------------------
	container = add_unnamed_hbox(child_vbox);
	
	const gchar *LINEBREAK_TEXTS[] = {"\\r", "\\n", "\\r\\n"};
	
	gint lineBreakActive = 0;
	if (strlen(ppo->newLineChars) == 2)
		lineBreakActive = 2;
	else if (ppo->newLineChars[0] == '\n')
		lineBreakActive = 1;
	
	lineBreak = add_combobox(container, _("Line break:"), LINEBREAK_TEXTS, 3,
							 lineBreakActive, NULL, FALSE);
	
	//----------------------------------------------------------------
	gtk_widget_show_all(vbox);
	return vbox;
}

static void fetchSettingsFromConfigUI(PrettyPrintingOptions *ppo)
{
	if (ppo == NULL) return;
	
	ppo->oneLineComment = gtk_toggle_button_get_active(
										GTK_TOGGLE_BUTTON(commentOneLine));
	ppo->inlineComment = gtk_toggle_button_get_active(
										GTK_TOGGLE_BUTTON(commentInline));
	ppo->alignComment = gtk_toggle_button_get_active(
										GTK_TOGGLE_BUTTON(commentAlign));
	
	ppo->oneLineText = gtk_toggle_button_get_active(
										GTK_TOGGLE_BUTTON(textOneLine));
	ppo->inlineText = gtk_toggle_button_get_active(
										GTK_TOGGLE_BUTTON(textInline));
	ppo->alignText = gtk_toggle_button_get_active(
										GTK_TOGGLE_BUTTON(textAlign));
	
	ppo->oneLineCdata = gtk_toggle_button_get_active(
										GTK_TOGGLE_BUTTON(cdataOneLine));
	ppo->inlineCdata = gtk_toggle_button_get_active(
										GTK_TOGGLE_BUTTON(cdataInline));
	ppo->alignCdata = gtk_toggle_button_get_active(
										GTK_TOGGLE_BUTTON(cdataAlign));
	
	ppo->emptyNodeStripping = gtk_toggle_button_get_active(
										GTK_TOGGLE_BUTTON(emptyNodeStripping));
	ppo->emptyNodeStrippingSpace = gtk_toggle_button_get_active(
										GTK_TOGGLE_BUTTON(emptyNodeStrippingSpace));
	ppo->forceEmptyNodeSplit = gtk_toggle_button_get_active(
										GTK_TOGGLE_BUTTON(emptyNodeSplit));
	
	ppo->indentLength = gtk_spin_button_get_value(GTK_SPIN_BUTTON(indentCount));
	ppo->indentChar = gtk_combo_box_get_active(GTK_COMBO_BOX(indentChar)) == 0 ?
																	'\t' : ' ';
	
	int breakStyle = gtk_combo_box_get_active(GTK_COMBO_BOX(lineBreak));
	
	g_free((gpointer)ppo->newLineChars);
	
	if (breakStyle == 0)
		ppo->newLineChars = g_strdup("\r");
	else if (breakStyle == 1)
		ppo->newLineChars = g_strdup("\n");
	else
		ppo->newLineChars = g_strdup("\r\n");
}

static gchar *prefsToData(PrettyPrintingOptions *ppo, gsize* size, GError **error)
{
	GKeyFile *kf = g_key_file_new();
	
	g_key_file_set_string(kf, "pretty-printer", "newLineChars", ppo->newLineChars);
	g_key_file_set_integer(kf, "pretty-printer", "indentChar", (int)ppo->indentChar);
	g_key_file_set_integer(kf, "pretty-printer", "indentLength", ppo->indentLength);
	g_key_file_set_boolean(kf, "pretty-printer", "oneLineText", ppo->oneLineText);
	g_key_file_set_boolean(kf, "pretty-printer", "inlineText", ppo->inlineText);
	g_key_file_set_boolean(kf, "pretty-printer", "oneLineComment", ppo->oneLineComment);
	g_key_file_set_boolean(kf, "pretty-printer", "inlineComment", ppo->inlineComment);
	g_key_file_set_boolean(kf, "pretty-printer", "oneLineCdata", ppo->oneLineCdata);
	g_key_file_set_boolean(kf, "pretty-printer", "inlineCdata", ppo->inlineCdata);
	g_key_file_set_boolean(kf, "pretty-printer", "emptyNodeStripping", ppo->emptyNodeStripping);
	g_key_file_set_boolean(kf, "pretty-printer", "emptyNodeStrippingSpace", ppo->emptyNodeStrippingSpace);
	g_key_file_set_boolean(kf, "pretty-printer", "forceEmptyNodeSplit", ppo->forceEmptyNodeSplit);
	g_key_file_set_boolean(kf, "pretty-printer", "trimLeadingWhites", ppo->trimLeadingWhites);
	g_key_file_set_boolean(kf, "pretty-printer", "trimTrailingWhites", ppo->trimTrailingWhites);
	g_key_file_set_boolean(kf, "pretty-printer", "alignComment", ppo->alignComment);
	g_key_file_set_boolean(kf, "pretty-printer", "alignText", ppo->alignText);
	g_key_file_set_boolean(kf, "pretty-printer", "alignCdata", ppo->alignCdata);
	
	gchar *contents = g_key_file_to_data(kf, size, error);
	g_key_file_free(kf);
	return contents;
}

static gboolean prefsFromData(PrettyPrintingOptions *ppo, const gchar *contents,
							  gssize size, GError **error)
{
	g_return_val_if_fail(contents != NULL, FALSE);
	
	GKeyFile *kf = g_key_file_new();
	
	if (!g_key_file_load_from_data(kf, contents, size,
								   G_KEY_FILE_KEEP_COMMENTS |
								   G_KEY_FILE_KEEP_TRANSLATIONS, error))
	{
		g_key_file_free(kf);
		return FALSE;
	}
	
	if (g_key_file_has_key(kf, "pretty-printer", "newLineChars", NULL))
	{
		g_free((gpointer)ppo->newLineChars);
		ppo->newLineChars = g_key_file_get_string(kf, "pretty-printer",
												  "newLineChars", error);
	}
	if (g_key_file_has_key(kf, "pretty-printer", "indentChar", NULL))
		ppo->indentChar = (char)g_key_file_get_integer(kf, "pretty-printer",
													   "indentChar", error);
	
	if (g_key_file_has_key(kf, "pretty-printer", "indentLength", NULL))
		ppo->indentLength = g_key_file_get_integer(kf, "pretty-printer",
												   "indentLength", error);
	
	if (g_key_file_has_key(kf, "pretty-printer", "oneLineText", NULL))
		ppo->oneLineText = g_key_file_get_boolean(kf, "pretty-printer",
												  "oneLineText", error);
	
	if (g_key_file_has_key(kf, "pretty-printer", "inlineText", NULL))
		ppo->inlineText = g_key_file_get_boolean(kf, "pretty-printer",
												 "inlineText", error);
	
	if (g_key_file_has_key(kf, "pretty-printer", "oneLineComment", NULL))
		ppo->oneLineComment = g_key_file_get_boolean(kf, "pretty-printer",
													 "oneLineComment", error);
	
	if (g_key_file_has_key(kf, "pretty-printer", "inlineComment", NULL))
		ppo->inlineComment = g_key_file_get_boolean(kf, "pretty-printer",
													"inlineComment", error);
	
	if (g_key_file_has_key(kf, "pretty-printer", "oneLineCdata", NULL))
		ppo->oneLineCdata = g_key_file_get_boolean(kf, "pretty-printer",
												   "oneLineCdata", error);
	
	if (g_key_file_has_key(kf, "pretty-printer", "inlineCdata", NULL))
		ppo->inlineCdata = g_key_file_get_boolean(kf, "pretty-printer",
												  "inlineCdata", error);
	
	if (g_key_file_has_key(kf, "pretty-printer", "emptyNodeStripping", NULL))
		ppo->emptyNodeStripping = g_key_file_get_boolean(kf, "pretty-printer",
														 "emptyNodeStripping", error);
	
	if (g_key_file_has_key(kf, "pretty-printer", "emptyNodeStrippingSpace", NULL))
		ppo->emptyNodeStrippingSpace = g_key_file_get_boolean(kf, "pretty-printer",
															  "emptyNodeStrippingSpace",
															  error);
	
	if (g_key_file_has_key(kf, "pretty-printer", "forceEmptyNodeSplit", NULL))
		ppo->forceEmptyNodeSplit = g_key_file_get_boolean(kf, "pretty-printer",
														  "forceEmptyNodeSplit", error);
	
	if (g_key_file_has_key(kf, "pretty-printer", "trimLeadingWhites", NULL))
		ppo->trimLeadingWhites = g_key_file_get_boolean(kf, "pretty-printer",
														"trimLeadingWhites", error);
	
	if (g_key_file_has_key(kf, "pretty-printer", "trimTrailingWhites", NULL))
		ppo->trimTrailingWhites = g_key_file_get_boolean(kf, "pretty-printer",
														 "trimTrailingWhites", error);
	
	if (g_key_file_has_key(kf, "pretty-printer", "alignComment", NULL))
		ppo->alignComment = g_key_file_get_boolean(kf, "pretty-printer",
												   "alignComment", error);
	
	if (g_key_file_has_key(kf, "pretty-printer", "alignText", NULL))
		ppo->alignText = g_key_file_get_boolean(kf, "pretty-printer",
												"alignText", error);
	
	if (g_key_file_has_key(kf, "pretty-printer", "alignCdata", NULL))
		ppo->alignCdata = g_key_file_get_boolean(kf, "pretty-printer",
												 "alignCdata", error);
	
	g_key_file_free(kf);
	return TRUE;
}

gboolean prefsLoad(const gchar *filename, GError **error)
{
	g_return_val_if_fail(filename != NULL, FALSE);
	
	PrettyPrintingOptions *ppo;
	gchar *contents = NULL;
	gsize size = 0;
	
	/* default printing options */
	if (prettyPrintingOptions == NULL)
		prettyPrintingOptions = createDefaultPrettyPrintingOptions();
	
	ppo = prettyPrintingOptions;
	
	if (!g_file_get_contents(filename, &contents, &size, error))
		return FALSE;
	if (!prefsFromData(ppo, contents, size, error))
	{
		g_free(contents);
		return FALSE;
	}
	g_free(contents);
	return TRUE;
}

gboolean prefsSave(const gchar *filename, GError **error)
{
	g_return_val_if_fail(filename != NULL, FALSE);
	
	PrettyPrintingOptions *ppo;
	gchar *contents = NULL;
	gsize size = 0;
	
	ppo = prettyPrintingOptions;
	fetchSettingsFromConfigUI(ppo);
	contents = prefsToData(ppo, &size, error);
	
	if (contents == NULL)
		return FALSE;
	
	if (!g_file_set_contents(filename, contents, size, error))
	{
		g_free(contents);
		return FALSE;
	}
	g_free(contents);
	return TRUE;
}

gchar *getDefaultPrefs(GError **error)
{
	PrettyPrintingOptions *ppo = createDefaultPrettyPrintingOptions();
	g_return_val_if_fail(ppo != NULL, NULL);
	
	gsize size = 0;
	gchar *contents = prefsToData(ppo, &size, error);
	
	return contents;
}
