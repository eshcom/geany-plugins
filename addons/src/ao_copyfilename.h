/*
 *      ao_copyfilename.h - this file is part of Addons, a Geany plugin
 *
 *      Copyright 2015 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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
 *
 */


#ifndef __AO_COPYFILENAME_H__
#define __AO_COPYFILENAME_H__

G_BEGIN_DECLS

#define AO_COPY_FILE_NAME_TYPE				(ao_copy_file_name_get_type())
#define AO_COPY_FILE_NAME(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			AO_COPY_FILE_NAME_TYPE, AoCopyFileName))
#define AO_COPY_FILE_NAME_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			AO_COPY_FILE_NAME_TYPE, AoCopyFileNameClass))
#define IS_AO_COPY_FILE_NAME(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			AO_COPY_FILE_NAME_TYPE))
#define IS_AO_COPY_FILE_NAME_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			AO_COPY_FILE_NAME_TYPE))

typedef struct _AoCopyFileName			AoCopyFileName;
typedef struct _AoCopyFileNameClass		AoCopyFileNameClass;


GType			ao_copy_file_name_get_type		(void);
AoCopyFileName*	ao_copy_file_name_new			(void);
void 			ao_copy_file_name_copy			(AoCopyFileName *self);
GtkWidget* 		ao_copy_file_name_get_menu_item	(AoCopyFileName *self);

G_END_DECLS

#endif /* __AO_COPYFILENAME_H__ */
