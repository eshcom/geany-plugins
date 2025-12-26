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

#include "../../utils/src/common.h"
#include "../../utils/src/ui.h"

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
static GtkWidget *indentWidth;
static GtkWidget *lineBreak;

/*================================= PUBLIC FUNCTIONS =================================*/

/* redeclaration of extern variable */
PrettyPrintingOptions *prettyPrintingOptions;

GtkWidget *createPrettyPrinterConfigUI(GtkDialog *dialog)
{
	GtkWidget *vbox, *child_vbox, *container;
	
	/* default printing options */
	if (!prettyPrintingOptions)
		prettyPrintingOptions = createDefaultPrettyPrintingOptions();
	
	PrettyPrintingOptions *ppo = prettyPrintingOptions;
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
							  ppo->indentChar == '\t' ? 0 : 1, NULL, FALSE);
	
	indentWidth = add_spinbox(container, _("Indent width:"), 0, 10, 1,
							  ppo->indentWidth, NULL, TRUE);
	
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

static void setIndentCharCount(PrettyPrintingOptions *ppo)
{
	ppo->indentCharCount = ppo->indentChar == '\t' && ppo->indentWidth > 0 ?
														1 : ppo->indentWidth;
}

void fetchSettingsFromConfigUI(PrettyPrintingOptions *ppo)
{
	if (!ppo) return;
	
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
	
	ppo->indentWidth = gtk_spin_button_get_value(GTK_SPIN_BUTTON(indentWidth));
	ppo->indentChar = gtk_combo_box_get_active(GTK_COMBO_BOX(indentChar)) == 0 ?
																	'\t' : ' ';
	setIndentCharCount(ppo);
	
	int breakStyle = gtk_combo_box_get_active(GTK_COMBO_BOX(lineBreak));
	
	g_free((gpointer)ppo->newLineChars);
	
	if (breakStyle == 0)
		ppo->newLineChars = g_strdup("\r");
	else if (breakStyle == 1)
		ppo->newLineChars = g_strdup("\n");
	else
		ppo->newLineChars = g_strdup("\r\n");
}

GKeyFile *prefsToConfig(PrettyPrintingOptions *ppo)
{
	GKeyFile *kf = g_key_file_new();
	
	g_key_file_set_string(kf, CONFIG_SECTION, "newLineChars", ppo->newLineChars);
	g_key_file_set_integer(kf, CONFIG_SECTION, "indentChar", (int)ppo->indentChar);
	g_key_file_set_integer(kf, CONFIG_SECTION, "indentWidth", ppo->indentWidth);
	g_key_file_set_boolean(kf, CONFIG_SECTION, "oneLineText", ppo->oneLineText);
	g_key_file_set_boolean(kf, CONFIG_SECTION, "inlineText", ppo->inlineText);
	g_key_file_set_boolean(kf, CONFIG_SECTION, "oneLineComment", ppo->oneLineComment);
	g_key_file_set_boolean(kf, CONFIG_SECTION, "inlineComment", ppo->inlineComment);
	g_key_file_set_boolean(kf, CONFIG_SECTION, "oneLineCdata", ppo->oneLineCdata);
	g_key_file_set_boolean(kf, CONFIG_SECTION, "inlineCdata", ppo->inlineCdata);
	g_key_file_set_boolean(kf, CONFIG_SECTION, "emptyNodeStripping", ppo->emptyNodeStripping);
	g_key_file_set_boolean(kf, CONFIG_SECTION, "emptyNodeStrippingSpace", ppo->emptyNodeStrippingSpace);
	g_key_file_set_boolean(kf, CONFIG_SECTION, "forceEmptyNodeSplit", ppo->forceEmptyNodeSplit);
	g_key_file_set_boolean(kf, CONFIG_SECTION, "trimLeadingWhites", ppo->trimLeadingWhites);
	g_key_file_set_boolean(kf, CONFIG_SECTION, "trimTrailingWhites", ppo->trimTrailingWhites);
	g_key_file_set_boolean(kf, CONFIG_SECTION, "alignComment", ppo->alignComment);
	g_key_file_set_boolean(kf, CONFIG_SECTION, "alignText", ppo->alignText);
	g_key_file_set_boolean(kf, CONFIG_SECTION, "alignCdata", ppo->alignCdata);
	
	return kf;
}

static void prefsFromConfig(PrettyPrintingOptions *ppo, GKeyFile *kf)
{
	if (g_key_file_has_key(kf, CONFIG_SECTION, "newLineChars", NULL))
	{
		g_free((gpointer)ppo->newLineChars);
		ppo->newLineChars = g_key_file_get_string(kf, CONFIG_SECTION,
												  "newLineChars", NULL);
	}
	if (g_key_file_has_key(kf, CONFIG_SECTION, "indentChar", NULL))
		ppo->indentChar = (char)g_key_file_get_integer(kf, CONFIG_SECTION,
													   "indentChar", NULL);
	
	if (g_key_file_has_key(kf, CONFIG_SECTION, "indentWidth", NULL))
		ppo->indentWidth = g_key_file_get_integer(kf, CONFIG_SECTION,
												  "indentWidth", NULL);
	setIndentCharCount(ppo);
	
	if (g_key_file_has_key(kf, CONFIG_SECTION, "oneLineText", NULL))
		ppo->oneLineText = g_key_file_get_boolean(kf, CONFIG_SECTION,
												  "oneLineText", NULL);
	
	if (g_key_file_has_key(kf, CONFIG_SECTION, "inlineText", NULL))
		ppo->inlineText = g_key_file_get_boolean(kf, CONFIG_SECTION,
												 "inlineText", NULL);
	
	if (g_key_file_has_key(kf, CONFIG_SECTION, "oneLineComment", NULL))
		ppo->oneLineComment = g_key_file_get_boolean(kf, CONFIG_SECTION,
													 "oneLineComment", NULL);
	
	if (g_key_file_has_key(kf, CONFIG_SECTION, "inlineComment", NULL))
		ppo->inlineComment = g_key_file_get_boolean(kf, CONFIG_SECTION,
													"inlineComment", NULL);
	
	if (g_key_file_has_key(kf, CONFIG_SECTION, "oneLineCdata", NULL))
		ppo->oneLineCdata = g_key_file_get_boolean(kf, CONFIG_SECTION,
												   "oneLineCdata", NULL);
	
	if (g_key_file_has_key(kf, CONFIG_SECTION, "inlineCdata", NULL))
		ppo->inlineCdata = g_key_file_get_boolean(kf, CONFIG_SECTION,
												  "inlineCdata", NULL);
	
	if (g_key_file_has_key(kf, CONFIG_SECTION, "emptyNodeStripping", NULL))
		ppo->emptyNodeStripping = g_key_file_get_boolean(kf, CONFIG_SECTION,
														 "emptyNodeStripping", NULL);
	
	if (g_key_file_has_key(kf, CONFIG_SECTION, "emptyNodeStrippingSpace", NULL))
		ppo->emptyNodeStrippingSpace = g_key_file_get_boolean(kf, CONFIG_SECTION,
															  "emptyNodeStrippingSpace", NULL);
	
	if (g_key_file_has_key(kf, CONFIG_SECTION, "forceEmptyNodeSplit", NULL))
		ppo->forceEmptyNodeSplit = g_key_file_get_boolean(kf, CONFIG_SECTION,
														  "forceEmptyNodeSplit", NULL);
	
	if (g_key_file_has_key(kf, CONFIG_SECTION, "trimLeadingWhites", NULL))
		ppo->trimLeadingWhites = g_key_file_get_boolean(kf, CONFIG_SECTION,
														"trimLeadingWhites", NULL);
	
	if (g_key_file_has_key(kf, CONFIG_SECTION, "trimTrailingWhites", NULL))
		ppo->trimTrailingWhites = g_key_file_get_boolean(kf, CONFIG_SECTION,
														 "trimTrailingWhites", NULL);
	
	if (g_key_file_has_key(kf, CONFIG_SECTION, "alignComment", NULL))
		ppo->alignComment = g_key_file_get_boolean(kf, CONFIG_SECTION,
												   "alignComment", NULL);
	
	if (g_key_file_has_key(kf, CONFIG_SECTION, "alignText", NULL))
		ppo->alignText = g_key_file_get_boolean(kf, CONFIG_SECTION, "alignText", NULL);
	
	if (g_key_file_has_key(kf, CONFIG_SECTION, "alignCdata", NULL))
		ppo->alignCdata = g_key_file_get_boolean(kf, CONFIG_SECTION, "alignCdata", NULL);
}

gboolean prefsLoad(const gchar *filename)
{
	g_return_val_if_fail(filename != NULL, FALSE);
	
	/* default printing options */
	if (!prettyPrintingOptions)
		prettyPrintingOptions = createDefaultPrettyPrintingOptions();
	
	gboolean result = FALSE;
	GKeyFile *config = load_config_from_file(filename, &result);
	if (result) prefsFromConfig(prettyPrintingOptions, config);
	g_key_file_free(config);
	return TRUE;
}
