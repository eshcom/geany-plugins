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

#include <geanyplugin.h> // includes geany.h, gtkcompat.h, etc.
GeanyData *geany_data;

#include "common.h"

#define FLAGS G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS


gchar *get_geany_configfile()
{
	// geany - is a macro for geany_data
	// configdir example: /home/esh/.config/geany
	return g_build_filename(geany->app->configdir, "geany.conf", NULL);
}

gchar *get_data_filepath(const gchar *plugin_name, const gchar *filename)
{
	gchar *prefix = NULL;
	
#ifdef G_OS_WIN32
	prefix = g_win32_get_package_installation_directory_of_module(NULL);
#elif defined(__APPLE__)
	if (g_getenv("GEANY_PLUGINS_SHARE_PATH"))
		return g_build_filename(g_getenv("GEANY_PLUGINS_SHARE_PATH"), 
								plugin_name, filename, NULL);
#endif
	// PLUGINDATADIR example: /usr/local/share/geany-plugins
	gchar *filepath = g_build_filename(prefix ? prefix : "", PLUGINDATADIR,
									   plugin_name, filename, NULL);
	g_free(prefix);
	return filepath;
}

gchar *get_config_filepath(const gchar *plugin_name, const gchar *filename)
{
	gchar *filepath = filename ? g_strdup(filename)
							   : g_strconcat(plugin_name, ".conf", NULL);
	
	SETPTR(filepath, g_build_filename(geany->app->configdir, "plugins",
									  plugin_name, filepath, NULL));
	return filepath;
}

GKeyFile *load_config_from_file(const gchar *filepath, gboolean *p_result)
{
	GKeyFile *config = g_key_file_new();
	gboolean result = g_key_file_load_from_file(config, filepath, FLAGS, NULL);
	
	if (p_result) *p_result = result;
	return config;
}

GKeyFile *load_config_from_data(const gchar *data, gboolean *p_result)
{
	GKeyFile *config = g_key_file_new();
	gboolean result = g_key_file_load_from_data(config, data, sizeof(data),
												FLAGS, NULL);
	if (p_result) *p_result = result;
	return config;
}

GKeyFile *load_plugin_config(const gchar *plugin_name, gboolean *p_result)
{
	gchar *filepath = get_config_filepath(plugin_name, NULL);
	GKeyFile *config = load_config_from_file(filepath, p_result);
	g_free(filepath);
	return config;
}

void log_error(const gchar *errmsg, const gchar *path, gint error,
			   ErrorLogType error_log_type)
{
	switch (error_log_type)
	{
		case MSGBOX:
			dialogs_show_msgbox(GTK_MESSAGE_ERROR, errmsg, path, g_strerror(error));
			break;
		case SYSLOG:
		{
			gchar *format_errmsg = g_strdup_printf(errmsg, path, g_strerror(error));
			msgwin_status_add(_("Plugin error: %s"), format_errmsg);
			g_free(format_errmsg);
			g_error(errmsg, path, g_strerror(error)); // g_warning/g_error/g_critical/g_printerr
			break;
		}
	}
}

gboolean write_config_to_file(GKeyFile *config, const gchar *filepath,
							  ErrorLogType error_log_type)
{
	gchar *data = g_key_file_to_data(config, NULL, NULL);
	gboolean result = write_data_to_file(data, filepath, error_log_type);
	g_free(data);
	return result;
}

gboolean write_data_to_file(const gchar *data, const gchar *filepath,
							ErrorLogType error_log_type)
{
	gchar *config_dirpath = g_path_get_dirname(filepath);
	gboolean result = FALSE;
	
	gint error;
	if (!g_file_test(config_dirpath, G_FILE_TEST_IS_DIR)
		&& (error = utils_mkdir(config_dirpath, TRUE)) != 0)
	{
		log_error(_("Plugin configuration directory '%s' could not be created: %s"),
				  config_dirpath, error, error_log_type);
	}
	else
	{	/* write data to file */
		error = utils_write_file(filepath, data);
		
		if (!(result = (error == 0)))
			log_error(_("Plugin configuration file '%s' could not be writed: %s"),
					  filepath, error, error_log_type);
	}
	
	g_free(config_dirpath);
	return result;
}

gboolean write_plugin_config(const gchar *plugin_name, GKeyFile *config)
{
	gchar *filepath = get_config_filepath(plugin_name, NULL);
	gboolean result = write_config_to_file(config, filepath, SYSLOG);
	g_free(filepath);
	return result;
}

GKeyFile *create_copy_config(GKeyFile *src_config)
{
	gsize length;
	gchar *data = g_key_file_to_data(src_config, &length, NULL);
	
	GKeyFile *dst_config = g_key_file_new();
	g_key_file_load_from_data(dst_config, data, length, FLAGS, NULL);
	g_free(data);
	return dst_config;
}

gboolean ok_apply(gint response)
{
	return (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY);
}

gboolean ok_accept(gint response)
{
	return (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_ACCEPT);
}

gboolean ok_apply_accept(gint response)
{
	return (response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY ||
			response == GTK_RESPONSE_ACCEPT);
}

gboolean accept_cancel(gint response)
{
	return (response == GTK_RESPONSE_ACCEPT || response == GTK_RESPONSE_CANCEL);
}
