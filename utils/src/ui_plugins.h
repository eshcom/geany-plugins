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

#ifndef GP_UTILS_UI_PLUGINS_H
#define GP_UTILS_UI_PLUGINS_H

#include <geanyplugin.h> // includes geany.h


/* Pre-GTK 2.24 compatibility */
#ifndef GTK_COMBO_BOX_TEXT
	#define GTK_COMBO_BOX_TEXT GTK_COMBO_BOX
	#define gtk_combo_box_text_new gtk_combo_box_new_text
	#define gtk_combo_box_text_append_text gtk_combo_box_append_text
#endif


G_BEGIN_DECLS

typedef struct {
	GtkWidget *widget1;
	GtkWidget *widget2;
} DoubleWidget;

GtkWidget *add_unnamed_hbox(GtkWidget *parent_vbox);
GtkWidget *add_unnamed_vbox(GtkWidget *parent_vbox);
GtkWidget *add_named_vbox(GtkWidget *parent_vbox, const gchar *label_text);

DoubleWidget add_checked_vbox(GtkWidget *parent_vbox, const gchar *label_text,
							  const gboolean checked, const gchar *tooltip_text);

GtkWidget *add_inputbox(GtkWidget *parent_box, const gchar *label_text,
						const gchar *input_text, const gint input_width,
						const gchar *tooltip_text, const gboolean add_clear_icon,
						const gboolean add_offset);

GtkWidget *add_combobox(GtkWidget *parent_box, const gchar *label_text,
						const gchar *combo_texts[], const gint combo_texts_len,
						const gint active_index, const gchar *tooltip_text,
						const gboolean add_offset);

GtkWidget *add_spinbox(GtkWidget *parent_box, const gchar *label_text,
					   const gint min_value, const gint max_value,
					   const gint step, const gint value,
					   const gchar *tooltip_text, const gboolean add_offset);

GtkWidget *add_init_radiobox(GtkWidget *parent_box, const gchar *label_text,
							 const gchar *tooltip_text, const gboolean add_offset);
GtkWidget *add_next_radiobox(GtkWidget *parent_box, GtkWidget *init_radio,
							 const gchar *label_text, const gchar *tooltip_text,
							 const gboolean add_offset);

GtkWidget *add_checkbox(GtkWidget *parent_box, const gchar *label_text,
						const gboolean checked, const gchar *tooltip_text,
						const gboolean add_offset);

DoubleWidget add_checkinputbox(GtkWidget *parent_box, const gchar *label_text,
							   const gchar *input_text, const gint input_width,
							   const gboolean checked, const gchar *tooltip_text,
							   const gboolean add_offset);

DoubleWidget add_checkbutton(GtkWidget *parent_box, const gchar *check_text,
							 const gchar *button_text, const gint button_width,
							 const gboolean checked, const gchar *tooltip_text,
							 const gboolean add_offset);

G_END_DECLS

#endif /* GP_UTILS_UI_PLUGINS_H */
