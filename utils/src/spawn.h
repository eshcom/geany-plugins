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

#ifndef GP_UTILS_SPAWN_H
#define GP_UTILS_SPAWN_H

#include <geanyplugin.h>


G_BEGIN_DECLS

typedef struct {
	gboolean success;
	gchar *errmsg;
	gchar *output1;
	gchar *output2;
} SpawnResult;

SpawnResult *call_spawn_sync(const gchar *locale_cmd,
							 const gchar *locale_dir);

void free_spawn_result(SpawnResult *result);

G_END_DECLS

#endif /* GP_UTILS_SPAWN_H */
