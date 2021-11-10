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

#include "spawn.h"


SpawnResult *call_spawn_sync(const gchar *locale_cmd,
							 const gchar *locale_dir)
{
	gint exitcode;
	GError *error = NULL;
	GString *output1 = g_string_new(NULL);
	GString *output2 = g_string_new(NULL);
	gboolean success;
	
#ifndef G_OS_WIN32
	/* run within shell so we can use pipes */
	gchar **argv = g_new0(gchar *, 4);
	argv[0] = g_strdup("/bin/sh");
	argv[1] = g_strdup("-c");
	argv[2] = g_strdup(locale_cmd);
	argv[3] = NULL;
	
	success = spawn_sync(locale_dir, NULL, argv, NULL, NULL,
						 output2, output1, &exitcode, &error);
	g_strfreev(argv);
#else
	success = spawn_sync(locale_dir, locale_cmd, NULL, NULL, NULL,
						 output1, output2, &exitcode, &error);
#endif
	
	SpawnResult *result = g_new0(SpawnResult, 1);
	result->success = success && exitcode == 0;
	result->output1 = g_string_free(output1, FALSE);
	result->output2 = g_string_free(output2, FALSE);
	result->errmsg = NULL;
	
	if (!result->success && error != NULL)
	{
		if (error->message != NULL)
			result->errmsg = g_strdup(error->message);
		g_error_free(error);
	}
	return result;
}

void free_spawn_result(SpawnResult *result)
{
	g_free(result->errmsg);
	g_free(result->output1);
	g_free(result->output2);
	g_free(result);
}
