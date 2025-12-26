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

#ifndef GP_UTILS_COMMON_H
#define GP_UTILS_COMMON_H

#define CONFIG_SECTION "settings"

G_BEGIN_DECLS

typedef enum
{
	SYSLOG = 0,
	MSGBOX,
} ErrorLogType;

gchar *get_geany_configfile();

gchar *get_data_filepath(const gchar *plugin_name, const gchar *filename);
gchar *get_config_filepath(const gchar *plugin_name, const gchar *filename);

GKeyFile *load_config_from_file(const gchar *filepath, gboolean *p_result);
GKeyFile *load_config_from_data(const gchar *data, gboolean *p_result);
GKeyFile *load_plugin_config(const gchar *plugin_name, gboolean *p_result);

gboolean write_config_to_file(GKeyFile *config, const gchar *filepath,
							  ErrorLogType error_log_type);
gboolean write_data_to_file(const gchar *data, const gchar *filepath,
							ErrorLogType error_log_type);
gboolean write_plugin_config(const gchar *plugin_name, GKeyFile *config);

GKeyFile *create_copy_config(GKeyFile *src_config);

gboolean ok_apply(gint response);
gboolean ok_accept(gint response);
gboolean ok_apply_accept(gint response);
gboolean accept_cancel(gint response);

G_END_DECLS

#endif /* GP_UTILS_COMMON_H */
