/*
 * Copyright 2017 LarsGit223
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "ui_plugins.h"


#define PACK_PADDING 4
#define DOUBLE_PACK_PADDING 8


#define ADD_AND_PACK_HBOX															\
	GtkWidget *hbox;																\
	if (GTK_IS_HBOX(parent_box))													\
		hbox = parent_box;															\
	else																			\
	{																				\
		hbox = gtk_hbox_new(FALSE, 0);												\
		gtk_box_pack_start(GTK_BOX(parent_box), hbox, FALSE, FALSE, PACK_PADDING);	\
	}

#define ADD_AND_PACK_OFFSET															\
	if (add_offset)																	\
	{																				\
		GtkWidget *offset = gtk_label_new("  ");									\
		gtk_box_pack_start(GTK_BOX(hbox), offset, FALSE, FALSE, PACK_PADDING);		\
	}

#define PACK_WIDGET(parent_box, widget, add_offset)									\
	ADD_AND_PACK_HBOX;																\
	ADD_AND_PACK_OFFSET;															\
	gtk_box_pack_start(GTK_BOX(hbox), widget, FALSE, FALSE, PACK_PADDING);

#define PACK_WIDGETS(parent_box, widget1, widget2, add_offset)						\
	ADD_AND_PACK_HBOX;																\
	ADD_AND_PACK_OFFSET;															\
	gtk_box_pack_start(GTK_BOX(hbox), widget1, FALSE, FALSE, PACK_PADDING);			\
	gtk_box_pack_start(GTK_BOX(hbox), widget2, FALSE, FALSE, PACK_PADDING);			\

#define SET_AND_PACK_WIDGETS(parent_box, widget1, widget2, widget2_width,			\
							 add_offset)											\
	ADD_AND_PACK_HBOX;																\
	ADD_AND_PACK_OFFSET;															\
	gtk_box_pack_start(GTK_BOX(hbox), widget1, FALSE, FALSE, PACK_PADDING);			\
	if (widget2_width > 0)															\
	{																				\
		gtk_widget_set_size_request(widget2, widget2_width, -1);					\
		gtk_box_pack_start(GTK_BOX(hbox), widget2, FALSE, FALSE, PACK_PADDING);		\
	}																				\
	else if (widget2_width == 0)													\
		gtk_box_pack_start(GTK_BOX(hbox), widget2, FALSE, FALSE, PACK_PADDING);		\
	else																			\
		gtk_box_pack_start(GTK_BOX(hbox), widget2, TRUE, TRUE, PACK_PADDING);


GtkWidget *add_unnamed_hbox(GtkWidget *parent_vbox)
{
	GtkWidget *child_hbox = gtk_hbox_new(FALSE, 0);
	
	gtk_box_pack_start(GTK_BOX(parent_vbox), child_hbox,
					   FALSE, FALSE, PACK_PADDING);
	return child_hbox;
}

GtkWidget *add_unnamed_vbox(GtkWidget *parent_vbox)
{
	GtkWidget *child_vbox = gtk_vbox_new(FALSE, 0);
	
	gtk_box_pack_start(GTK_BOX(parent_vbox), child_vbox,
					   FALSE, FALSE, DOUBLE_PACK_PADDING);
	return child_vbox;
}

GtkWidget *add_named_vbox(GtkWidget *parent_vbox, const gchar *label_text)
{
	GtkWidget *frame = gtk_frame_new(NULL);
	GtkWidget *child_vbox = gtk_vbox_new(FALSE, 0);
	
	gtk_frame_set_label(GTK_FRAME(frame), label_text);
	gtk_container_add(GTK_CONTAINER(frame), child_vbox);
	
	gtk_box_pack_start(GTK_BOX(parent_vbox), frame,
					   FALSE, FALSE, DOUBLE_PACK_PADDING);
	return child_vbox;
}

DoubleWidget add_checked_vbox(GtkWidget *parent_vbox, const gchar *label_text,
							  const gboolean checked, const gchar *tooltip_text)
{
	GtkWidget *frame = gtk_frame_new(NULL);
	GtkWidget *child_vbox = gtk_vbox_new(FALSE, 0);
	
	GtkWidget *check = gtk_check_button_new_with_label(label_text);
	
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(check, FALSE);
#else
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), checked);
	
	if (tooltip_text)
		gtk_widget_set_tooltip_text(check, tooltip_text);
	
	gtk_frame_set_label_widget(GTK_FRAME(frame), check);
	gtk_container_add(GTK_CONTAINER(frame), child_vbox);
	
	gtk_box_pack_start(GTK_BOX(parent_vbox), frame,
					   FALSE, FALSE, DOUBLE_PACK_PADDING);
	return (DoubleWidget){check, child_vbox};
}

GtkWidget *add_inputbox(GtkWidget *parent_box, const gchar *label_text,
						const gchar *input_text, const gint input_width,
						const gchar *tooltip_text, const gboolean add_clear_icon,
						const gboolean add_offset)
{
	GtkWidget *label = gtk_label_new(label_text);
	GtkWidget *input = gtk_entry_new();
	
	gtk_entry_set_text(GTK_ENTRY(input), input_text);
	
	if (add_clear_icon)
		ui_entry_add_clear_icon(GTK_ENTRY(input));
	
	if (tooltip_text)
		gtk_widget_set_tooltip_text(input, tooltip_text);
	
	SET_AND_PACK_WIDGETS(parent_box, label, input, input_width, add_offset);
	
	return input;
}

GtkWidget *add_combobox(GtkWidget *parent_box, const gchar *label_text,
						const gchar *combo_texts[], const gint combo_texts_len,
						const gint active_index, const gchar *tooltip_text,
						const gboolean add_offset)
{
	GtkWidget *label = gtk_label_new(label_text);
	GtkWidget *combo = gtk_combo_box_text_new();
	
	for (gint i = 0; i < combo_texts_len; i++)
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo),
									   combo_texts[i]);
	
	gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active_index);
	
	if (tooltip_text)
		gtk_widget_set_tooltip_text(combo, tooltip_text);
	
	PACK_WIDGETS(parent_box, label, combo, add_offset);
	
	return combo;
}

GtkWidget *add_spinbox(GtkWidget *parent_box, const gchar *label_text,
					   const gint min_value, const gint max_value,
					   const gint step, const gint value,
					   const gchar *tooltip_text, const gboolean add_offset)
{
	GtkWidget *label = gtk_label_new(label_text);
	GtkWidget *spin = gtk_spin_button_new_with_range(min_value, max_value, step);
	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), value);
	
	if (tooltip_text)
		gtk_widget_set_tooltip_text(spin, tooltip_text);
	
	PACK_WIDGETS(parent_box, label, spin, add_offset);
	
	return spin;
}

GtkWidget *add_init_radiobox(GtkWidget *parent_box, const gchar *label_text,
							 const gchar *tooltip_text, const gboolean add_offset)
{
	GtkWidget *radio = gtk_radio_button_new_with_mnemonic(NULL, label_text);
	
	if (tooltip_text)
		gtk_widget_set_tooltip_text(radio, tooltip_text);
	
	PACK_WIDGET(parent_box, radio, add_offset);
	
	return radio;
}

GtkWidget *add_next_radiobox(GtkWidget *parent_box, GtkWidget *init_radio,
							 const gchar *label_text, const gchar *tooltip_text,
							 const gboolean add_offset)
{
	GtkWidget *radio = gtk_radio_button_new_with_mnemonic_from_widget(
								GTK_RADIO_BUTTON(init_radio), label_text);
	if (tooltip_text)
		gtk_widget_set_tooltip_text(radio, tooltip_text);
	
	PACK_WIDGET(parent_box, radio, add_offset);
	
	return radio;
}

GtkWidget *add_checkbox(GtkWidget *parent_box, const gchar *label_text,
						const gboolean checked, const gchar *tooltip_text,
						const gboolean add_offset)
{
	GtkWidget *check = gtk_check_button_new_with_label(label_text);
	
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(check, FALSE);
#else
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), checked);
	
	if (tooltip_text)
		gtk_widget_set_tooltip_text(check, tooltip_text);
	
	PACK_WIDGET(parent_box, check, add_offset);
	
	return check;
}

DoubleWidget add_checkinputbox(GtkWidget *parent_box, const gchar *label_text,
							   const gchar *input_text, const gint input_width,
							   const gboolean checked, const gchar *tooltip_text,
							   const gboolean add_offset)
{
	GtkWidget *check = gtk_check_button_new_with_label(label_text);
	
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(check, FALSE);
#else
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), checked);
	
	GtkWidget *input = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(input), input_text);
	
	if (tooltip_text)
	{
		gtk_widget_set_tooltip_text(check, tooltip_text);
		gtk_widget_set_tooltip_text(input, tooltip_text);
	}
	
	SET_AND_PACK_WIDGETS(parent_box, check, input, input_width, add_offset);
	
	return (DoubleWidget){check, input};
}

DoubleWidget add_checkbutton(GtkWidget *parent_box, const gchar *check_text,
							 const gchar *button_text, const gint button_width,
							 const gboolean checked, const gchar *tooltip_text,
							 const gboolean add_offset)
{
	GtkWidget *check = gtk_check_button_new_with_label(check_text);
	
#if GTK_CHECK_VERSION(3, 20, 0)
	gtk_widget_set_focus_on_click(check, FALSE);
#else
	gtk_button_set_focus_on_click(GTK_BUTTON(check), FALSE);
#endif
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), checked);
	
	GtkWidget *button = gtk_button_new_with_label(button_text);
	
	if (tooltip_text)
	{
		gtk_widget_set_tooltip_text(check, tooltip_text);
		gtk_widget_set_tooltip_text(button, tooltip_text);
	}
	
	SET_AND_PACK_WIDGETS(parent_box, check, button, button_width, add_offset);
	
	return (DoubleWidget){check, button};
}
