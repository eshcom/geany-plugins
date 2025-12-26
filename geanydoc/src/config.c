/*
 *  config.c
 *
 *  Copyright 2008 Yura Siamashka <yurand2@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <geanyplugin.h> // includes geany.h, gtkcompat.h, etc.

#include "geanydoc.h"
#include "../../utils/src/common.h"

extern GeanyData *geany_data;


const gchar defaults[] =
	"[C]\n"
	"internal = false\n"
	"command0 = man -P \"col -b\" -S 2:3:5 '%w'\n"
	"command1 = devhelp -s '%w'\n"
	"[C++]\n"
	"internal = false\n"
	"command0 = man -P \"col -b\" -S 2:3:5 '%w'\n"
	"command1 = devhelp -s '%w'\n"
	"[PHP]\n"
	"internal = false\n"
	"command0 = firefox \"http://www.php.net/%w\"\n"
	"[Sh]\n"
	"internal = true\n"
	"command0 = man -P \"col -b\" -S 1:4:5:6:7:8:9 '%w'\n"
	"[Python]\n"
	"internal = true\n"
	"command0 = pydoc '%w'\n"
	"[None]\n"
	"internal = false\n" "command0 = firefox \"http://www.google.com/search?q=%w\"\n";

static GKeyFile *config = NULL;
static gchar *config_file = NULL;

void config_init(void)
{
	config_file = get_config_filepath(PLUGIN, NULL);
	
	gboolean result = FALSE;
	config = load_config_from_file(config_file, &result);
	if (!result)
	{
		g_key_file_free(config);
		config = load_config_from_data(defaults, NULL);
	}
}

void config_uninit(void)
{
	g_free(config_file);
	config_file = NULL;
	g_key_file_free(config);
	config = NULL;
}

GKeyFile *config_clone(void)
{
	return create_copy_config(config);
}

void config_set(GKeyFile *cfg)
{
	g_key_file_free(config);
	config = cfg;
	write_config_to_file(config, config_file, SYSLOG);
}

gchar *config_get_command(const gchar *lang, gint cmd_num, gboolean *intern)
{
	gchar *key = g_strdup_printf("command%d", cmd_num);
	gchar *ret = utils_get_setting_string(config, lang, key, "");
	g_free(key);
	
	if (EMPTY(ret)) return ret;
	
	key = g_strdup_printf("command%d", cmd_num + 1);
	gchar *tmp = utils_get_setting_string(config, lang, key, "");
	g_free(key);
	
	*intern = EMPTY(tmp) ? utils_get_setting_boolean(config, lang, "internal", FALSE)
						 : TRUE;
	g_free(tmp);
	return ret;
}
