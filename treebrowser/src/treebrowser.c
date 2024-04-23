/*
 *      treebrowser.c - v0.20
 *
 *      Copyright 2010 Adrian Dimitrov <dimitrov.adrian@gmail.com>
 */

#ifdef HAVE_CONFIG_H
	#include "config.h"		// for the gettext domain
#endif

#ifdef G_OS_WIN32
	#include <windows.h>
#endif

#include <fcntl.h>
#include <glib/gstdio.h>
#include <gdk/gdkkeysyms.h>

#include <geanyplugin.h>	// includes geany.h

#include "../../utils/src/spawn.h"
#include "../../utils/src/ui_plugins.h"


/* These items are set by Geany before plugin_init() is called. */
GeanyPlugin					*geany_plugin;
GeanyData					*geany_data;

static gint					page_number					= 0;
static GtkTreeStore			*treestore;
static GtkWidget			*treeview;
static GtkWidget			*sidebar_vbox;
static GtkWidget			*sidebar_vbox_bars;
static GtkWidget			*filter;
static GtkWidget			*addressbar;
static gchar				*addressbar_last_address	= NULL;

static GtkTreeIter			bookmarks_iter;
static gboolean				bookmarks_expanded			= FALSE;

static GtkTreeViewColumn	*treeview_column_text;
static GtkCellRenderer		*render_icon, *render_text;

static GtkWidget			*btn_proj_path;

static GtkWidget			*last_focused_widget		= NULL;

static GdkColor				color_parent = {0, 0xFFFF, 0, 0};

/* ------------------
 * FLAGS
 * ------------------ */

static gboolean				flag_on_expand_refresh		= FALSE;

/* ------------------
 *  CONFIG VARS
 * ------------------ */

#ifndef G_OS_WIN32
# define CONFIG_OPEN_EXTERNAL_CMD_DEFAULT "xdg-open '%d'"
# define CONFIG_OPEN_TERMINAL_DEFAULT "xterm"
#else
# define CONFIG_OPEN_EXTERNAL_CMD_DEFAULT "explorer '%d'"
# define CONFIG_OPEN_TERMINAL_DEFAULT "cmd"
#endif

# define CONFIG_OBJECT_FILES_MASK_DEFAULT \
	".o .obj .so .dll .a .lib .la .lo .pyc .class .beam"

static gchar				*CONFIG_FILE				= NULL;
static gchar				*CONFIG_OPEN_EXTERNAL_CMD	= NULL;
static gchar				*CONFIG_OPEN_TERMINAL		= NULL;
static gboolean				CONFIG_REVERSE_FILTER		= FALSE;
static gboolean				CONFIG_ONE_CLICK_CHDOC		= FALSE;
static gboolean				CONFIG_SHOW_HIDDEN_FILES	= FALSE;
static gboolean				CONFIG_HIDE_OBJECT_FILES	= FALSE;
static gchar				*CONFIG_OBJECT_FILES_MASK	= NULL;
static gboolean				CONFIG_HIDE_IGNORED_DIRS	= FALSE;
static gchar				*CONFIG_IGNORED_DIRS_MASK	= NULL;
static gint					CONFIG_SHOW_BARS			= 1;
static gboolean				CONFIG_CHROOT_ON_DCLICK		= FALSE;
static gboolean				CONFIG_FOLLOW_CURRENT_DOC	= TRUE;
static gboolean				CONFIG_ON_DELETE_CLOSE_FILE	= TRUE;
static gboolean				CONFIG_ON_OPEN_FOCUS_EDITOR	= FALSE;
static gboolean				CONFIG_SHOW_TREE_LINES		= TRUE;
static gboolean				CONFIG_SHOW_BOOKMARKS		= FALSE;
static gint					CONFIG_SHOW_ICONS			= 2;
static gboolean				CONFIG_OPEN_NEW_FILES		= TRUE;

static gchar **OBJECT_FILE_EXTS = NULL;
static gchar **IGNORED_DIR_NAMES = NULL;

static void treebrowser_track_current_cb(void);

/* ------------------
 * TREEVIEW STRUCT
 * ------------------ */

enum
{
	TREEBROWSER_COLUMNC									= 5,
	
	TREEBROWSER_COLUMN_ICON								= 0,
	TREEBROWSER_COLUMN_NAME								= 1,
	TREEBROWSER_COLUMN_URI								= 2,
	TREEBROWSER_COLUMN_FLAG								= 3,
	TREEBROWSER_COLUMN_COLOR							= 4,
	
	TREEBROWSER_FLAGS_SEPARATOR							= -1
};


/* Keybinding(s) */
enum
{
	KB_FOCUS_FILE_LIST,
	KB_FOCUS_FILE_LIST_2,
	KB_FOCUS_PATH_ENTRY,
	KB_FOCUS_FILTER_ENTRY,
	KB_RENAME_OBJECT,
	KB_CREATE_FILE,
	KB_CREATE_DIR,
	KB_REFRESH,
	KB_TRACK_CURRENT,
	KB_COUNT
};


/* SearchFilter (for is_passed_filter) */
typedef struct {
	gchar **filters;
	gboolean reverse;
} SearchFilter;


/* ------------------
 * PLUGIN INFO
 * ------------------ */

PLUGIN_VERSION_CHECK(224)

PLUGIN_SET_TRANSLATABLE_INFO(
	LOCALEDIR,
	GETTEXT_PACKAGE,
	_("TreeBrowser"),
	_("This plugin adds a tree browser to Geany, allowing the user to "
	  "browse files using a tree view of the directory being browsed."),
	"1.32" ,
	"Adrian Dimitrov (dimitrov.adrian@gmail.com)")


/* ------------------
 * PREDEFINES
 * ------------------ */

#define foreach_slist_free(node, list)			\
	for (node = list, list = NULL;				\
		 g_slist_free_1(list), node != NULL;	\
		 list = node, node = node->next)


/* ------------------
 * PROTOTYPES
 * ------------------ */

static void 	project_open_cb(G_GNUC_UNUSED GObject *obj,
								G_GNUC_UNUSED GKeyFile *config,
								G_GNUC_UNUSED gpointer data);
static void 	project_save_cb(G_GNUC_UNUSED GObject *obj,
								G_GNUC_UNUSED GKeyFile *config,
								G_GNUC_UNUSED gpointer data);
static void 	project_close_cb(G_GNUC_UNUSED GObject *obj,
								 G_GNUC_UNUSED gpointer data);
static void 	treebrowser_browse(gchar *directory, gpointer parent);
static void 	treebrowser_bookmarks_set_state(void);
static void 	treebrowser_load_bookmarks(void);
static void 	treebrowser_tree_store_iter_clear_nodes(gpointer iter,
														gboolean delete_root);
static void 	treebrowser_rename_current(void);
static void 	on_menu_create_new_object(GtkMenuItem *menuitem,
										  const gchar *type);
static void 	on_button_project_path(void);
static void 	on_button_current_path(void);
static void 	on_button_go_up(void);
static void 	load_settings(void);
static gboolean save_settings(void);

static gboolean treebrowser_expand_to_path(gchar *root, gchar *find,
										   gboolean set_cursor);

/* ------------------
 * PLUGIN CALLBACKS
 * ------------------ */

PluginCallback plugin_callbacks[] =
{
	{"project-open", (GCallback) &project_open_cb, TRUE, NULL},
	{"project-save", (GCallback) &project_save_cb, TRUE, NULL},
	{"project-close", (GCallback) &project_close_cb, TRUE, NULL},
	{NULL, NULL, FALSE, NULL}
};


/* ------------------
 * TREEBROWSER CORE FUNCTIONS
 * ------------------ */

static gboolean tree_view_row_expanded_iter(GtkTreeView *tree_view,
											GtkTreeIter *iter)
{
	GtkTreePath *tree_path = gtk_tree_model_get_path(
									gtk_tree_view_get_model(tree_view),
									iter);
	gboolean expanded = gtk_tree_view_row_expanded(tree_view, tree_path);
	gtk_tree_path_free(tree_path);
	
	return expanded;
}

#if GTK_CHECK_VERSION(3, 10, 0)
static GdkPixbuf *utils_pixbuf_from_name(const gchar *icon_name)
{
	GError *error = NULL;
	
	GtkIconTheme *icon_theme = gtk_icon_theme_get_default();
	GdkPixbuf *pixbuf = gtk_icon_theme_load_icon(icon_theme, icon_name,
												 16, 0, &error);
	if (!pixbuf)
	{
		g_warning("Couldn't load icon: %s", error->message);
		g_error_free(error);
		return NULL;
	}
	return pixbuf;
}
#else
static GdkPixbuf *utils_pixbuf_from_stock(const gchar *stock_id)
{
	GtkIconSet *icon_set = gtk_icon_factory_lookup_default(stock_id);
	
	if (icon_set)
		return gtk_icon_set_render_icon(icon_set, gtk_widget_get_default_style(),
										gtk_widget_get_default_direction(),
										GTK_STATE_NORMAL, GTK_ICON_SIZE_MENU,
										NULL, NULL);
	return NULL;
}
#endif

static GdkPixbuf *utils_pixbuf_from_path(gchar *path)
{
	GdkPixbuf *ret = NULL;
	
	gchar *ctype = g_content_type_guess(path, NULL, 0, NULL);
	GIcon *icon = g_content_type_get_icon(ctype);
	g_free(ctype);
	
	if (icon != NULL)
	{
		GtkIconInfo *info;
		gint width;
		
		gtk_icon_size_lookup(GTK_ICON_SIZE_MENU, &width, NULL);
		info = gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_default(),
											  icon, width,
											  GTK_ICON_LOOKUP_USE_BUILTIN);
		g_object_unref(icon);
		if (!info)
		{
			icon = g_themed_icon_new("text-x-generic");
			if (icon != NULL)
			{
				info = gtk_icon_theme_lookup_by_gicon(gtk_icon_theme_get_default(),
													  icon, width,
													  GTK_ICON_LOOKUP_USE_BUILTIN);
				g_object_unref(icon);
			}
		}
		if (!info)
			return NULL;
		
		ret = gtk_icon_info_load_icon(info, NULL);
		gtk_icon_info_free(info);
	}
	return ret;
}

/* result must be freed */
static gchar *path_is_in_dir(gchar *src, gchar *find)
{
	gchar **src_segments = g_strsplit(src, G_DIR_SEPARATOR_S, 0);
	gchar **find_segments = g_strsplit(find, G_DIR_SEPARATOR_S, 0);
	
	guint src_segments_n = g_strv_length(src_segments);
	guint find_segments_n = g_strv_length(find_segments);
	
	if (find_segments_n < src_segments_n)
		src_segments_n = find_segments_n;
	
	GString *path = g_string_new("");
	
	for (guint i = 1; i < src_segments_n; i++)
	{
		if (!utils_str_equal(find_segments[i], src_segments[i]))
			break;
		else
		{
			g_string_append_c(path, G_DIR_SEPARATOR);
			g_string_append(path, find_segments[i]);
		}
	}
	g_strfreev(src_segments);
	g_strfreev(find_segments);
	
	gchar *diffed_path = g_strdup(path->str);
	g_string_free(path, TRUE);
	
	return diffed_path;
}

static SearchFilter *get_filter(void)
{
	SearchFilter *fobj = g_new0(SearchFilter, 1);
	fobj->reverse = CONFIG_REVERSE_FILTER;
	fobj->filters = NULL;
	
	const gchar *entry_text = gtk_entry_get_text(GTK_ENTRY(filter));
	if (EMPTY(entry_text))
		return fobj;
	
	while (TRUE)
	{
		if (*entry_text == '!')
			fobj->reverse = TRUE;
		else if (*entry_text != ' ' && *entry_text != ';')
			break;
		entry_text++;
	}
	fobj->filters = g_strsplit(entry_text, ";", 0);
	
	return fobj;
}

static void free_filter(SearchFilter *fobj)
{
	g_strfreev(fobj->filters);
	g_free(fobj);
}

/* Return: FALSE - if the file has not passed the filter and should not be shown
 *         TRUE  - if the file passed the filter and should be shown */
static gboolean is_passed_filter(const gchar *base_name,
								 const SearchFilter *fobj)
{
	if (CONFIG_HIDE_OBJECT_FILES)
	{
		gchar **ext;
		foreach_strv(ext, OBJECT_FILE_EXTS)
		{
			if (**ext && g_str_has_suffix(base_name, *ext))
				return FALSE; // not passed
		}
	}
	
	if (!fobj->filters || !(*fobj->filters))
		return TRUE; // passed
	
	gchar **filt;
	foreach_strv(filt, fobj->filters)
	{
		if (**filt && g_pattern_match_simple(*filt, base_name))
			return !fobj->reverse;
	}
	return fobj->reverse;
}

static gboolean is_ignored_dir(const gchar *base_name)
{
	gchar **dir;
	foreach_strv(dir, IGNORED_DIR_NAMES)
	{
		if (**dir && g_pattern_match_simple(*dir, base_name))
			return TRUE;
	}
	return FALSE;
}

#ifndef G_OS_WIN32
static GString *generate_find_cmd(void)
{
	SearchFilter *fobj = get_filter();
	
	if (!fobj->filters || !(*fobj->filters))
	{
		free_filter(fobj);
		return NULL;
	}
	
	GString *gstr = g_string_new("find -L . -type f");
	
	if (!CONFIG_SHOW_HIDDEN_FILES)
		g_string_append(gstr, " -not -path '*/.*' -not -name '.*'");
	
	if (CONFIG_HIDE_OBJECT_FILES)
	{
		gchar **ext;
		foreach_strv(ext, OBJECT_FILE_EXTS)
		{
			if (**ext)
			{
				g_string_append(gstr, " -not -name '");
				g_string_append_c(gstr, '*');
				g_string_append(gstr, *ext);
				g_string_append_c(gstr, '\'');
			}
		}
	}
	
	if (CONFIG_HIDE_IGNORED_DIRS)
	{
		gchar **dir;
		foreach_strv(dir, IGNORED_DIR_NAMES)
		{
			if (**dir)
			{
				g_string_append(gstr, " -not -path '*/");
				g_string_append(gstr, *dir);
				g_string_append(gstr, "/*'");
			}
		}
	}
	
	gboolean set_filter = FALSE;
	gboolean first = TRUE;
	gchar **filt;
	foreach_strv(filt, fobj->filters)
	{
		if (**filt)
		{
			if (first)
			{
				first = FALSE;
				g_string_append(gstr, fobj->reverse ? " \\( -not -name '"
													: " \\( -name '");
			}
			else
				g_string_append(gstr, fobj->reverse ? " -not -name '"
													: " -o -name '");
			g_string_append(gstr, *filt);
			g_string_append_c(gstr, '\'');
			
			if (!set_filter) set_filter = TRUE;
		}
	}
	if (set_filter)
		g_string_append(gstr, " \\)");
	
	g_string_append(gstr, " -exec dirname {} \\; | sort -u");
	
	free_filter(fobj);
	return gstr;
}

static gboolean find_and_expand_to_paths(void)
{
	GString *findcmd = generate_find_cmd();
	if (findcmd)
	{
		SpawnResult *result = call_spawn_sync(findcmd->str,
											  addressbar_last_address);
		gboolean first = TRUE;
		gchar **dirs = g_strsplit(result->output2, "\n", 0);
		
		for (gchar **dir_next, **dir = dirs; *dir && **dir; dir++)
		{
			dir_next = dir + 1;
			if (*dir_next && **dir_next &&
				utils_match_dirs(*dir, *dir_next) == MATCH_DIRS_PREF_1)
				continue;
			
			gchar *path = g_build_filename(addressbar_last_address,
										   *dir + 1, NULL); // +1 - for skip dot
			treebrowser_expand_to_path(addressbar_last_address, path, first);
			g_free(path);
			
			if (first) first = FALSE;
		}
		g_strfreev(dirs);
		free_spawn_result(result);
		g_string_free(findcmd, TRUE);
		
		return TRUE;
	}
	return FALSE;
}
#endif

#ifdef G_OS_WIN32
static gboolean win32_is_hidden(const gchar *filename)
{
	static wchar_t w_filename[MAX_PATH];
	MultiByteToWideChar(CP_UTF8, 0, filename, -1,
						w_filename, sizeof(w_filename));
	DWORD attrs = GetFileAttributesW(w_filename);
	if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_HIDDEN))
		return TRUE;
	return FALSE;
}
#endif

/* Returns: whether name should be hidden. */
static gboolean is_hidden(const gchar *filename)
{
	gchar *base_name = g_path_get_basename(filename);
	if (EMPTY(base_name))
	{
		g_free(base_name);
		return FALSE;
	}
	
	gboolean result;
	
#ifdef G_OS_WIN32
	result = win32_is_hidden(filename);
#else
	result = base_name[0] == '.';
#endif
	
	if (!result)
	{
		gsize len = strlen(base_name);
		result = base_name[len - 1] == '~';
	}
	g_free(base_name);
	return result;
}

static gchar *get_default_dir(void)
{
	GeanyDocument *doc = document_get_current();
	
	if (doc != NULL && doc->file_name != NULL &&
		g_path_is_absolute(doc->file_name))
	{
		gchar *dir_name = g_path_get_dirname(doc->file_name);
		gchar *ret = utils_get_locale_from_utf8(dir_name);
		g_free(dir_name);
		return ret;
	}
	
	GeanyProject *project = geany->app->project;
	const gchar *dir = project ? project->base_path
							   : geany->prefs->default_open_path;
	if (!EMPTY(dir))
		return utils_get_locale_from_utf8(dir);
	
	return g_get_current_dir();
}

static gchar *get_terminal(void)
{
	gchar *terminal;
#ifdef G_OS_WIN32
	terminal = g_strdup("cmd");
#else
	terminal = g_strdup(CONFIG_OPEN_TERMINAL);
#endif
	return terminal;
}

static gboolean treebrowser_checkdir(gchar *directory)
{
#if !GTK_CHECK_VERSION(3, 0, 0)
	static const GdkColor red 	= {0, 0xffff, 0x6666, 0x6666};
	static const GdkColor white = {0, 0xffff, 0xffff, 0xffff};
#endif
	
	static gboolean old_value = TRUE;
	gboolean is_dir = g_file_test(directory, G_FILE_TEST_IS_DIR);
	
	if (old_value != is_dir)
	{
#if GTK_CHECK_VERSION(3, 0, 0)
		GtkStyleContext *context;
		context = gtk_widget_get_style_context(GTK_WIDGET(addressbar));
		if (is_dir)
			gtk_style_context_remove_class(context, "invalid");
		else
			gtk_style_context_add_class(context, "invalid");
#else
		gtk_widget_modify_base(GTK_WIDGET(addressbar), GTK_STATE_NORMAL,
							   is_dir ? NULL : &red);
		gtk_widget_modify_text(GTK_WIDGET(addressbar), GTK_STATE_NORMAL,
							   is_dir ? NULL : &white);
#endif
		old_value = is_dir;
	}
	if (!is_dir)
	{
		if (CONFIG_SHOW_BARS == 0)
			dialogs_show_msgbox(GTK_MESSAGE_ERROR, _("%s: no such directory."),
								directory);
		return FALSE;
	}
	return is_dir;
}

static void treebrowser_chroot(const gchar *dir)
{
	gchar *directory;
	
	if (g_str_has_suffix(dir, G_DIR_SEPARATOR_S))
		directory = g_strndup(dir, strlen(dir) - 1);
	else
		directory = g_strdup(dir);
	
	gtk_entry_set_text(GTK_ENTRY(addressbar), directory);
	
	if (EMPTY(directory))
		setptr(directory, g_strdup(G_DIR_SEPARATOR_S));
	
	if (!treebrowser_checkdir(directory))
	{
		g_free(directory);
		return;
	}
	treebrowser_bookmarks_set_state();
	setptr(addressbar_last_address, directory);
	treebrowser_browse(addressbar_last_address, NULL);
	treebrowser_load_bookmarks();
}

static void treebrowser_browse(gchar *directory, gpointer parent)
{
	GtkTreeIter iter_empty;
	gboolean expanded = FALSE, has_parent;
	
	directory = g_strconcat(directory, G_DIR_SEPARATOR_S, NULL);
	
	has_parent = parent ? gtk_tree_store_iter_is_valid(treestore, parent)
						: FALSE;
	if (has_parent)
	{
		if (parent == &bookmarks_iter)
			treebrowser_load_bookmarks();
	}
	else
		parent = NULL;
	
	if (has_parent &&
		tree_view_row_expanded_iter(GTK_TREE_VIEW(treeview), parent))
	{
		expanded = TRUE;
		treebrowser_bookmarks_set_state();
	}
	if (parent)
		treebrowser_tree_store_iter_clear_nodes(parent, FALSE);
	else
		gtk_tree_store_clear(treestore);
	
	GSList *list = utils_get_file_list(directory, NULL, NULL);
	if (list != NULL)
	{
		SearchFilter *fobj = get_filter();
		GtkTreeIter iter, *last_dir_iter = NULL;
		GSList *node;
		foreach_slist_free(node, list)
		{
			gchar *fname = node->data; // esh: locale encoding
			gchar *uri   = g_strconcat(directory, fname, NULL);
			
			if (CONFIG_SHOW_HIDDEN_FILES || !is_hidden(uri))
			{
				GdkPixbuf *icon = NULL;
				gchar *utf8_name = utils_get_utf8_from_locale(fname);
				
				if (g_file_test(uri, G_FILE_TEST_IS_DIR))
				{
					if (!CONFIG_HIDE_IGNORED_DIRS || !is_ignored_dir(utf8_name))
					{
						if (last_dir_iter == NULL)
							gtk_tree_store_prepend(treestore, &iter, parent);
						else
						{
							gtk_tree_store_insert_after(treestore, &iter,
														parent, last_dir_iter);
							gtk_tree_iter_free(last_dir_iter);
						}
						last_dir_iter = gtk_tree_iter_copy(&iter);
#if GTK_CHECK_VERSION(3, 10, 0)
						icon = CONFIG_SHOW_ICONS ? utils_pixbuf_from_name("folder")
												 : NULL;
#else
						icon = CONFIG_SHOW_ICONS ? utils_pixbuf_from_stock(GTK_STOCK_DIRECTORY)
												 : NULL;
#endif
						gtk_tree_store_set(treestore, &iter,
										   TREEBROWSER_COLUMN_COLOR, &color_parent,
										   TREEBROWSER_COLUMN_ICON, icon,
										   TREEBROWSER_COLUMN_NAME, fname,
										   TREEBROWSER_COLUMN_URI,  uri, -1);
						
						gtk_tree_store_prepend(treestore, &iter_empty, &iter);
						
						gtk_tree_store_set(treestore, &iter_empty,
										   TREEBROWSER_COLUMN_ICON, NULL,
										   TREEBROWSER_COLUMN_NAME, _("(Empty)"),
										   TREEBROWSER_COLUMN_URI,  NULL, -1);
					}
				}
				else
				{
					if (is_passed_filter(utf8_name, fobj))
					{
						icon = CONFIG_SHOW_ICONS == 2
										? utils_pixbuf_from_path(uri)
										: CONFIG_SHOW_ICONS
#if GTK_CHECK_VERSION(3, 10, 0)
										? utils_pixbuf_from_name("text-x-generic")
#else
										? utils_pixbuf_from_stock(GTK_STOCK_FILE)
#endif
										: NULL;
						
						gtk_tree_store_append(treestore, &iter, parent);
						
						gtk_tree_store_set(treestore, &iter,
										   TREEBROWSER_COLUMN_ICON, icon,
										   TREEBROWSER_COLUMN_NAME, fname,
										   TREEBROWSER_COLUMN_URI,  uri, -1);
					}
				}
				g_free(utf8_name);
				if (icon)
					g_object_unref(icon);
			}
			g_free(uri);
			g_free(fname);
		}
		free_filter(fobj);
	}
	else
	{
		gtk_tree_store_prepend(treestore, &iter_empty, parent);
		gtk_tree_store_set(treestore, &iter_empty,
						   TREEBROWSER_COLUMN_ICON, NULL,
						   TREEBROWSER_COLUMN_NAME, _("(Empty)"),
						   TREEBROWSER_COLUMN_URI,  NULL, -1);
	}
	if (has_parent)
	{
		if (expanded)
			gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview),
					gtk_tree_model_get_path(GTK_TREE_MODEL(treestore), parent),
					FALSE);
	}
	else
		treebrowser_load_bookmarks();
	
	g_free(directory);
}

static void treebrowser_bookmarks_set_state(void)
{
	if (gtk_tree_store_iter_is_valid(treestore, &bookmarks_iter))
		bookmarks_expanded = tree_view_row_expanded_iter(
												GTK_TREE_VIEW(treeview),
												&bookmarks_iter);
	else
		bookmarks_expanded = FALSE;
}

static void treebrowser_load_bookmarks(void)
{
	if (!CONFIG_SHOW_BOOKMARKS)
		return;
	
	gchar *contents;
	gchar *bookmarks = g_build_filename(g_get_home_dir(),
										".gtk-bookmarks", NULL);
	if (g_file_get_contents(bookmarks, &contents, NULL, NULL))
	{
		GtkTreeIter iter;
		GdkPixbuf *icon = NULL;
		
		if (gtk_tree_store_iter_is_valid(treestore, &bookmarks_iter))
		{
			bookmarks_expanded = tree_view_row_expanded_iter(
												GTK_TREE_VIEW(treeview),
												&bookmarks_iter);
			treebrowser_tree_store_iter_clear_nodes(&bookmarks_iter, FALSE);
		}
		else
		{
			gtk_tree_store_prepend(treestore, &bookmarks_iter, NULL);
#if GTK_CHECK_VERSION(3, 10, 0)
			icon = CONFIG_SHOW_ICONS ? utils_pixbuf_from_name("go-home")
									 : NULL;
#else
			icon = CONFIG_SHOW_ICONS ? utils_pixbuf_from_stock(GTK_STOCK_HOME)
									 : NULL;
#endif
			gtk_tree_store_set(treestore, &bookmarks_iter,
							   TREEBROWSER_COLUMN_ICON, icon,
							   TREEBROWSER_COLUMN_NAME, _("Bookmarks"),
							   TREEBROWSER_COLUMN_URI,  NULL, -1);
			if (icon)
				g_object_unref(icon);
			
			gtk_tree_store_insert_after(treestore, &iter, NULL, &bookmarks_iter);
			gtk_tree_store_set(treestore, &iter,
							   TREEBROWSER_COLUMN_ICON, NULL,
							   TREEBROWSER_COLUMN_NAME, NULL,
							   TREEBROWSER_COLUMN_URI,  NULL,
							   TREEBROWSER_COLUMN_FLAG, TREEBROWSER_FLAGS_SEPARATOR,
							   -1);
		}
		
		gchar **line, **lines = g_strsplit(contents, "\n", 0);
		foreach_strv(line, lines)
		{
			if (**line)
			{
				gchar *pos = g_utf8_strchr(*line, -1, ' ');
				if (pos != NULL)
					*pos = '\0';
			}
			
			gchar *path_full = g_filename_from_uri(*line, NULL, NULL);
			if (path_full != NULL)
			{
				if (g_file_test(path_full, G_FILE_TEST_EXISTS |
										   G_FILE_TEST_IS_DIR))
				{
					gchar *file_name = g_path_get_basename(path_full);
					
					gtk_tree_store_append(treestore, &iter, &bookmarks_iter);
#if GTK_CHECK_VERSION(3, 10, 0)
					icon = CONFIG_SHOW_ICONS ? utils_pixbuf_from_name("folder")
											 : NULL;
#else
					icon = CONFIG_SHOW_ICONS ? utils_pixbuf_from_stock(GTK_STOCK_DIRECTORY)
											 : NULL;
#endif
					gtk_tree_store_set(treestore, &iter,
									   TREEBROWSER_COLUMN_COLOR, &color_parent,
									   TREEBROWSER_COLUMN_ICON, icon,
									   TREEBROWSER_COLUMN_NAME, file_name,
									   TREEBROWSER_COLUMN_URI,  path_full, -1);
					g_free(file_name);
					
					if (icon)
						g_object_unref(icon);
					
					gtk_tree_store_append(treestore, &iter, &iter);
					gtk_tree_store_set(treestore, &iter,
									   TREEBROWSER_COLUMN_ICON, NULL,
									   TREEBROWSER_COLUMN_NAME, _("(Empty)"),
									   TREEBROWSER_COLUMN_URI,  NULL, -1);
				}
				g_free(path_full);
			}
		}
		g_strfreev(lines);
		g_free(contents);
		
		if (bookmarks_expanded)
		{
			GtkTreePath *tree_path;
			tree_path = gtk_tree_model_get_path(GTK_TREE_MODEL(treestore),
												&bookmarks_iter);
			gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview),
									 tree_path, FALSE);
			gtk_tree_path_free(tree_path);
		}
	}
	g_free(bookmarks);
}

static gboolean treebrowser_search(gchar *uri, gpointer parent,
								   gboolean set_cursor)
{
	GtkTreeIter iter;
	if (gtk_tree_model_iter_children(GTK_TREE_MODEL(treestore), &iter, parent))
	{
		do
		{
			if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(treestore), &iter))
				if (treebrowser_search(uri, &iter, set_cursor))
					return TRUE;
			
			gchar *uri_current;
			gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
							   TREEBROWSER_COLUMN_URI, &uri_current, -1);
			
			if (utils_str_equal(uri, uri_current))
			{
				GtkTreePath *path = gtk_tree_model_get_path(
											GTK_TREE_MODEL(treestore), &iter);
				gtk_tree_view_expand_to_path(GTK_TREE_VIEW(treeview), path);
				if (set_cursor)
				{
					gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), path,
												 NULL, FALSE, 0, 0);
					gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview), path,
											 treeview_column_text, FALSE);
				}
				gtk_tree_path_free(path);
				g_free(uri_current);
				return TRUE;
			}
			g_free(uri_current);
			
		} while (gtk_tree_model_iter_next(GTK_TREE_MODEL(treestore), &iter));
	}
	return FALSE;
}

static void fs_remove(gchar *root, gboolean delete_root)
{
	if (!g_file_test(root, G_FILE_TEST_EXISTS))
		return;
	
	if (g_file_test(root, G_FILE_TEST_IS_DIR))
	{
		GDir *dir = g_dir_open(root, 0, NULL);
		if (!dir)
		{
			if (delete_root)
				g_remove(root);
			else
				return;
		}
		
		const gchar *name;
		while ((name = g_dir_read_name(dir)))
		{
			gchar *path = g_build_filename(root, name, NULL);
			if (g_file_test(path, G_FILE_TEST_IS_DIR))
				fs_remove(path, delete_root);
			
			g_remove(path);
			g_free(path);
		}
		g_dir_close(dir);
	}
	else
		delete_root = TRUE;
	
	if (delete_root)
		g_remove(root);
	
	return;
}

static void showbars(gboolean state)
{
	if (state)
	{
		gtk_widget_show(sidebar_vbox_bars);
		if (!CONFIG_SHOW_BARS)
			CONFIG_SHOW_BARS = 1;
	}
	else
	{
		gtk_widget_hide(sidebar_vbox_bars);
		CONFIG_SHOW_BARS = 0;
	}
	save_settings();
}

static void treebrowser_tree_store_iter_clear_nodes(gpointer iter,
													gboolean delete_root)
{
	GtkTreeIter i;
	
	if (gtk_tree_model_iter_children(GTK_TREE_MODEL(treestore), &i, iter))
	{
		while (gtk_tree_store_remove(GTK_TREE_STORE(treestore), &i))
			/* do nothing */;
	}
	if (delete_root)
		gtk_tree_store_remove(GTK_TREE_STORE(treestore), iter);
}

static gboolean treebrowser_expand_to_path(gchar *root, gchar *find,
										   gboolean set_cursor)
{
	gchar **find_segments = g_strsplit(find, G_DIR_SEPARATOR_S, 0);
	guint find_segments_n = g_strv_length(find_segments);
	
	gboolean founded = FALSE, global_founded = FALSE;
	GString *path = g_string_new("");
	
	for (guint i = 1; i < find_segments_n; i++)
	{
		g_string_append_c(path, G_DIR_SEPARATOR);
		g_string_append(path, find_segments[i]);
		
		if (founded)
		{
			if (treebrowser_search(path->str, NULL,
								   set_cursor && i == find_segments_n - 1))
				global_founded = TRUE;
		}
		else
			if (utils_str_equal(root, path->str))
				founded = TRUE;
	}
	g_string_free(path, TRUE);
	g_strfreev(find_segments);
	
	return global_founded;
}

static gboolean treebrowser_track_current(void)
{
	GeanyDocument *doc = document_get_current();
	
	if (doc != NULL && doc->file_name != NULL &&
		g_path_is_absolute(doc->file_name))
	{
		gchar *froot = NULL;
		gchar *path_current = utils_get_locale_from_utf8(doc->file_name);
		
		// Checking if the document is in the expanded or collapsed files
		if (!treebrowser_search(path_current, NULL, TRUE))
		{	// Else we have to chroting to the document`s nearles path
			froot = path_is_in_dir(addressbar_last_address,
								   g_path_get_dirname(path_current));
			if (froot == NULL)
				froot = g_strdup(G_DIR_SEPARATOR_S);
			
			if (!utils_str_equal(froot, addressbar_last_address))
				treebrowser_chroot(froot);
			
			treebrowser_expand_to_path(froot, path_current, TRUE);
		}
		g_free(froot);
		g_free(path_current);
		return FALSE;
	}
	return FALSE;
}

static gboolean treebrowser_iter_rename(gpointer iter)
{
	if (gtk_tree_store_iter_is_valid(treestore, iter))
	{
		GtkTreePath *path = gtk_tree_model_get_path(GTK_TREE_MODEL(treestore),
													iter);
		if (G_LIKELY(path != NULL))
		{
			GtkTreeViewColumn *column = gtk_tree_view_get_column(
												GTK_TREE_VIEW(treeview), 0);
			GList *renderers = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(column));
			
			GtkCellRenderer *renderer = g_list_nth_data(renderers,
												TREEBROWSER_COLUMN_NAME);
			
			g_object_set(G_OBJECT(renderer), "editable", TRUE, NULL);
			gtk_tree_view_set_cursor_on_cell(GTK_TREE_VIEW(treeview), path,
											 column, renderer, TRUE);
			
			gtk_tree_path_free(path);
			g_list_free(renderers);
			return TRUE;
		}
	}
	return FALSE;
}

static void treebrowser_rename_current(void)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(
												GTK_TREE_VIEW(treeview));
	GtkTreeModel *model;
	GtkTreeIter iter;
	
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
		treebrowser_iter_rename(&iter);
}

static void treebrowser_create_new_current(const gchar *type)
{
	on_menu_create_new_object(NULL, type);
}


/* ------------------
 * RIGHTCLICK MENU EVENTS
 * ------------------*/

static void on_menu_go_up(GtkMenuItem *menuitem, gpointer *user_data)
{
	on_button_go_up();
}

static void on_menu_project_path(GtkMenuItem *menuitem, gpointer *user_data)
{
	on_button_project_path();
}

static void on_menu_current_path(GtkMenuItem *menuitem, gpointer *user_data)
{
	on_button_current_path();
}

static void on_menu_open_externally(GtkMenuItem *menuitem, gchar *uri)
{
	GString *cmd_str = g_string_new(CONFIG_OPEN_EXTERNAL_CMD);
	
	gchar *dir = g_file_test(uri, G_FILE_TEST_IS_DIR) ? g_strdup(uri)
													  : g_path_get_dirname(uri);
	utils_string_replace_all(cmd_str, "%f", uri);
	utils_string_replace_all(cmd_str, "%d", dir);
	
	gchar *cmd = g_string_free(cmd_str, FALSE);
	gchar *locale_cmd = utils_get_locale_from_utf8(cmd);
	
	GError *error = NULL;
	if (!g_spawn_command_line_async(locale_cmd, &error))
	{
		gchar *c = strchr(cmd, ' ');
		if (c != NULL)
			*c = '\0';
		
		ui_set_statusbar(TRUE, _("Could not execute configured "
								 "external command '%s' (%s)."),
						 cmd, error->message);
		g_error_free(error);
	}
	g_free(locale_cmd);
	g_free(cmd);
	g_free(dir);
}

static void on_menu_open_terminal(GtkMenuItem *menuitem, gchar *uri)
{
	gchar *argv[2] = {NULL, NULL};
	argv[0] = get_terminal();
	
	if (g_file_test(uri, G_FILE_TEST_EXISTS))
		uri = g_file_test(uri, G_FILE_TEST_IS_DIR) ? g_strdup(uri)
												   : g_path_get_dirname(uri);
	else
		uri = g_strdup(addressbar_last_address);
	
	g_spawn_async(uri, argv, NULL, G_SPAWN_SEARCH_PATH,
				  NULL, NULL, NULL, NULL);
	g_free(uri);
	g_free(argv[0]);
}

static void on_menu_set_as_root(GtkMenuItem *menuitem, gchar *uri)
{
	if (g_file_test(uri, G_FILE_TEST_IS_DIR))
		treebrowser_chroot(uri);
}

static void on_menu_find_in_files(GtkMenuItem *menuitem, gchar *uri)
{
	search_show_find_in_files_dialog(uri);
}

static void on_menu_create_new_object(GtkMenuItem *menuitem,
									  const gchar *type)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(
												GTK_TREE_VIEW(treeview));
	GtkTreeModel *model;
	GtkTreeIter iter;
	gchar *uri;
	gboolean refresh_root = FALSE;
	
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gtk_tree_model_get(model, &iter, TREEBROWSER_COLUMN_URI, &uri, -1);
		/* If not a directory, find parent directory */
		if (!g_file_test(uri, G_FILE_TEST_IS_DIR))
		{
			GtkTreeIter iter_parent;
			if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(treestore),
										   &iter_parent, &iter))
			{
				iter = iter_parent;
				/* Set URI from parent iter */
				g_free(uri);
				gtk_tree_model_get(model, &iter_parent, TREEBROWSER_COLUMN_URI,
								   &uri, -1);
			}
			else
				refresh_root = TRUE;
		}
	}
	else
	{
		refresh_root = TRUE;
		uri = g_strdup(addressbar_last_address);
	}
	
	gchar *uri_new = NULL;
	if (utils_str_equal(type, "directory"))
		uri_new = g_strconcat(uri, G_DIR_SEPARATOR_S, _("NewDirectory"), NULL);
	else if (utils_str_equal(type, "file"))
		uri_new = g_strconcat(uri, G_DIR_SEPARATOR_S, _("NewFile"), NULL);
	
	if (uri_new)
	{
		if (!(g_file_test(uri_new, G_FILE_TEST_EXISTS) &&
			  !dialogs_show_question(
						_("Target file '%s' exists.\nDo you really want "
						  "to replace it with an empty file?"), uri_new)))
		{
			gboolean creation_success = FALSE;
			
			while(g_file_test(uri_new, G_FILE_TEST_EXISTS))
				setptr(uri_new, g_strconcat(uri_new, "_", NULL));
			
			if (utils_str_equal(type, "directory"))
				creation_success = (g_mkdir(uri_new, 0755) == 0);
			else
				creation_success = (g_creat(uri_new, 0644) != -1);
			
			if (creation_success)
			{
				treebrowser_browse(uri, refresh_root ? NULL : &iter);
				if (treebrowser_search(uri_new, NULL, TRUE))
					treebrowser_rename_current();
				if (CONFIG_OPEN_NEW_FILES && utils_str_equal(type, "file"))
					document_open_file(uri_new,FALSE, NULL, NULL);
			}
		}
		g_free(uri_new);
	}
	g_free(uri);
}

static void on_menu_rename(GtkMenuItem *menuitem, gpointer *user_data)
{
	treebrowser_rename_current();
}

static void on_menu_delete(GtkMenuItem *menuitem, gpointer *user_data)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(
												GTK_TREE_VIEW(treeview));
	GtkTreeModel *model;
	GtkTreeIter iter;
	
	if (!gtk_tree_selection_get_selected(selection, &model, &iter))
		return;
	
	gchar *uri;
	gtk_tree_model_get(model, &iter, TREEBROWSER_COLUMN_URI, &uri, -1);
	
	if (dialogs_show_question(_("Do you really want to delete '%s' ?"), uri))
	{
		if (CONFIG_ON_DELETE_CLOSE_FILE &&
			!g_file_test(uri, G_FILE_TEST_IS_DIR))
			document_close(document_find_by_filename(uri));
		
		gchar *uri_parent = g_path_get_dirname(uri);
		fs_remove(uri, TRUE);
		
		GtkTreeIter iter_parent;
		if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(treestore),
									   &iter_parent, &iter))
			treebrowser_browse(uri_parent, &iter_parent);
		else
			treebrowser_browse(uri_parent, NULL);
		
		g_free(uri_parent);
	}
	g_free(uri);
}

static void on_menu_refresh(GtkMenuItem *menuitem, gpointer *user_data)
{
	GtkTreeSelection *selection = gtk_tree_view_get_selection(
												GTK_TREE_VIEW(treeview));
	GtkTreeModel *model;
	GtkTreeIter iter;
	
	if (gtk_tree_selection_get_selected(selection, &model, &iter))
	{
		gchar *uri;
		gtk_tree_model_get(model, &iter, TREEBROWSER_COLUMN_URI, &uri, -1);
		
		if (g_file_test(uri, G_FILE_TEST_IS_DIR))
			treebrowser_browse(uri, &iter);
		
		g_free(uri);
	}
	else
		treebrowser_browse(addressbar_last_address, NULL);
}

static void on_menu_expand_all(GtkMenuItem *menuitem, gpointer *user_data)
{
	gtk_tree_view_expand_all(GTK_TREE_VIEW(treeview));
}

static void on_menu_collapse_all(GtkMenuItem *menuitem, gpointer *user_data)
{
	gtk_tree_view_collapse_all(GTK_TREE_VIEW(treeview));
}

static void on_menu_close(GtkMenuItem *menuitem, gchar *uri)
{
	if (g_file_test(uri, G_FILE_TEST_EXISTS))
		document_close(document_find_by_filename(uri));
}

static void on_menu_close_children(GtkMenuItem *menuitem, gchar *uri)
{
	size_t uri_len = strlen(uri);
	for (guint i = 0; i < GEANY(documents_array)->len; i++)
	{
		if (documents[i]->is_valid)
		{	/* the document filename should always be longer
			 * than the uri when closing children
			 * Compare the beginning of the filename string
			 * to see if it matchs the uri*/
			if (strlen(documents[i]->file_name) > uri_len &&
				strncmp(uri, documents[i]->file_name, uri_len) == 0)
				document_close(documents[i]);
		}
	}
}

static void on_menu_copy_item(GtkMenuItem *menuitem, gchar *item)
{
	GtkClipboard *cb = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(cb, item, -1);
}

static void on_menu_show_bookmarks(GtkMenuItem *menuitem, gpointer *user_data)
{
	CONFIG_SHOW_BOOKMARKS = gtk_check_menu_item_get_active(
											GTK_CHECK_MENU_ITEM(menuitem));
	save_settings();
	treebrowser_chroot(addressbar_last_address);
}

static void on_menu_show_hidden_files(GtkMenuItem *menuitem,
									  gpointer *user_data)
{
	CONFIG_SHOW_HIDDEN_FILES = gtk_check_menu_item_get_active(
											GTK_CHECK_MENU_ITEM(menuitem));
	save_settings();
	treebrowser_browse(addressbar_last_address, NULL);
}

static void on_menu_show_bars(GtkMenuItem *menuitem, gpointer *user_data)
{
	showbars(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menuitem)));
}

static GtkWidget *create_popup_menu(const gchar *name, const gchar *uri)
{
	GtkWidget *item, *menu = gtk_menu_new();
	
	gboolean is_exists = g_file_test(uri, G_FILE_TEST_EXISTS);
	gboolean is_dir = is_exists ? g_file_test(uri, G_FILE_TEST_IS_DIR)
								: FALSE;
	gboolean is_document = document_find_by_filename(uri) ? TRUE : FALSE;
	
#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("go-up", _("Go _Up"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_GO_UP, _("Go _Up"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_go_up), NULL);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("go-up", _("Set _Path From Document"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_GO_UP, _("Set _Path From Document"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_current_path), NULL);
	
	if (geany->app->project)
	{
#if GTK_CHECK_VERSION(3, 10, 0)
		item = ui_image_menu_item_new("go-up", _("Set Project _Path"));
#else
		item = ui_image_menu_item_new(GTK_STOCK_GO_UP, _("Set Project _Path"));
#endif
		gtk_container_add(GTK_CONTAINER(menu), item);
		g_signal_connect(item, "activate",
						 G_CALLBACK(on_menu_project_path), NULL);
	}
	
	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("document-open", _("_Open Externally"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_OPEN, _("_Open Externally"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_open_externally),
						  g_strdup(uri), (GClosureNotify)g_free, 0);
	gtk_widget_set_sensitive(item, is_exists);
	
	item = ui_image_menu_item_new("utilities-terminal", _("Open _Terminal"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_open_terminal),
						  g_strdup(uri), (GClosureNotify)g_free, 0);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("go-top", _("Set as _Root"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_GOTO_TOP, _("Set as _Root"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_set_as_root),
						  g_strdup(uri), (GClosureNotify)g_free, 0);
	gtk_widget_set_sensitive(item, is_dir);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("view-refresh", _("Refres_h"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_REFRESH, _("Refres_h"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_refresh), NULL);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("edit-find", _("_Find in Files"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_FIND, _("_Find in Files"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_find_in_files),
						  g_strdup(uri), (GClosureNotify)g_free, 0);
	gtk_widget_set_sensitive(item, is_dir);
	
	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("list-add", _("N_ew Folder"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_ADD, _("N_ew Folder"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_create_new_object),
					 (gpointer)"directory");
	
#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("document-new", _("_New File"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_NEW, _("_New File"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_create_new_object),
					 (gpointer)"file");
	
#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("document-save-as", _("Rena_me"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_SAVE_AS, _("Rena_me"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_rename), NULL);
	gtk_widget_set_sensitive(item, is_exists);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("edit-delete", _("_Delete"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_DELETE, _("_Delete"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_delete), NULL);
	gtk_widget_set_sensitive(item, is_exists);
	
	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("window-close",
								  g_strdup_printf(_("Close: %s"), name));
#else
	item = ui_image_menu_item_new(GTK_STOCK_CLOSE,
								  g_strdup_printf(_("Close: %s"), name));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_close),
						  g_strdup(uri), (GClosureNotify)g_free, 0);
	gtk_widget_set_sensitive(item, is_document);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("window-close",
								  g_strdup_printf(_("Clo_se Child Documents ")));
#else
	item = ui_image_menu_item_new(GTK_STOCK_CLOSE,
								  g_strdup_printf(_("Clo_se Child Documents ")));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_close_children),
						  g_strdup(uri), (GClosureNotify)g_free, 0);
	gtk_widget_set_sensitive(item, is_dir);
	
	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("edit-copy", _("_Copy Name to Clipboard"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_COPY, _("_Copy Name to Clipboard"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_copy_item),
						  g_strdup(name), (GClosureNotify)g_free, 0);
	gtk_widget_set_sensitive(item, is_exists);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("edit-copy",
								  _("_Copy Full Path to Clipboard"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_COPY,
								  _("_Copy Full Path to Clipboard"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect_data(item, "activate", G_CALLBACK(on_menu_copy_item),
						  g_strdup(uri), (GClosureNotify)g_free, 0);
	gtk_widget_set_sensitive(item, is_exists);
	
	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);
	
	gtk_widget_show(item);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("go-next", _("E_xpand All"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_GO_FORWARD, _("E_xpand All"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_expand_all), NULL);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	item = ui_image_menu_item_new("go-previous", _("Coll_apse All"));
#else
	item = ui_image_menu_item_new(GTK_STOCK_GO_BACK, _("Coll_apse All"));
#endif
	gtk_container_add(GTK_CONTAINER(menu), item);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_collapse_all), NULL);
	
	item = gtk_separator_menu_item_new();
	gtk_container_add(GTK_CONTAINER(menu), item);
	
	item = gtk_check_menu_item_new_with_mnemonic(_("Show Boo_kmarks"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
								   CONFIG_SHOW_BOOKMARKS);
	g_signal_connect(item, "activate",
					 G_CALLBACK(on_menu_show_bookmarks), NULL);
	
	item = gtk_check_menu_item_new_with_mnemonic(_("Sho_w Hidden Files"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
								   CONFIG_SHOW_HIDDEN_FILES);
	g_signal_connect(item, "activate",
					 G_CALLBACK(on_menu_show_hidden_files), NULL);
	
	item = gtk_check_menu_item_new_with_mnemonic(_("Show Tool_bars"));
	gtk_container_add(GTK_CONTAINER(menu), item);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item),
								   CONFIG_SHOW_BARS ? TRUE : FALSE);
	g_signal_connect(item, "activate", G_CALLBACK(on_menu_show_bars), NULL);
	
	gtk_widget_show_all(menu);
	return menu;
}


/* ------------------
 * TOOLBAR`S EVENTS
 * ------------------ */

static void on_button_go_up(void)
{
	gchar *uri = g_path_get_dirname(addressbar_last_address);
	treebrowser_chroot(uri);
	treebrowser_track_current_cb();
	g_free(uri);
}

static void on_button_refresh(void)
{
	treebrowser_chroot(addressbar_last_address);
	treebrowser_track_current_cb();
}

static void on_button_go_home(void)
{
	gchar *uri = g_strdup(g_get_home_dir());
	treebrowser_chroot(uri);
	treebrowser_track_current_cb();
	g_free(uri);
}

static void on_button_project_path(void)
{
	gchar *uri = project_get_base_path();
	if (uri)
	{
		treebrowser_chroot(uri);
		treebrowser_track_current_cb();
		g_free(uri);
	}
}

static void on_button_current_path(void)
{
	gchar *uri = get_default_dir();
	treebrowser_chroot(uri);
	treebrowser_track_current_cb();
	g_free(uri);
}

static void on_button_hide_bars(void)
{
	showbars(FALSE);
}

static void on_addressbar_activate(GtkEntry *entry, gpointer user_data)
{
	gchar *directory = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
	treebrowser_chroot(directory);
}

static void on_addressbar_grabfocus(GtkEntry *entry, gpointer user_data)
{
	last_focused_widget = GTK_WIDGET(entry);
}

static void on_filter_activate(GtkEntry *entry, gpointer user_data)
{
	treebrowser_chroot(addressbar_last_address);
	
	gboolean result = FALSE;
#ifndef G_OS_WIN32
	result = find_and_expand_to_paths();
#endif
	if (!result)
		treebrowser_track_current_cb();
}

static void on_filter_clear(GtkEntry *entry, gint icon_pos,
							GdkEvent *event, gpointer data)
{
	gtk_entry_set_text(entry, "");
	treebrowser_chroot(addressbar_last_address);
	treebrowser_track_current_cb();
}

static gboolean on_filter_focus(GtkEntry *entry, GtkDirectionType direction,
								gpointer user_data)
{
	if (last_focused_widget && last_focused_widget != GTK_WIDGET(entry))
	{
		gtk_widget_grab_focus(last_focused_widget);
		return TRUE;
	}
	return FALSE;
}

static void on_filter_grabfocus(GtkEntry *entry, gpointer user_data)
{
	last_focused_widget = GTK_WIDGET(entry);
}


/* ------------------
 * TREEVIEW EVENTS
 * ------------------ */

static void on_treeview_grabfocus(GtkWidget *widget, gpointer user_data)
{
	last_focused_widget = widget;
}

static gboolean on_treeview_mouseclick(GtkWidget *widget,
									   GdkEventButton *event,
									   GtkTreeSelection *selection)
{
	if (event->button == 3)
	{
		GtkTreePath *path;
		/* Get tree path for row that was clicked */
		if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview),
			(gint)event->x, (gint)event->y, &path, NULL, NULL, NULL))
		{
			/* Unselect current selection; select clicked row from path */
			gtk_tree_selection_unselect_all(selection);
			gtk_tree_selection_select_path(selection, path);
			gtk_tree_path_free(path);
		}
		
		gchar *name = NULL, *uri = NULL;
		GtkTreeModel *model;
		GtkTreeIter iter;
		if (gtk_tree_selection_get_selected(selection, &model, &iter))
			gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
							   TREEBROWSER_COLUMN_NAME, &name,
							   TREEBROWSER_COLUMN_URI, &uri, -1);
		
		GtkWidget *menu = create_popup_menu(name != NULL ? name : "",
											uri != NULL ? uri : "");
#if GTK_CHECK_VERSION(3, 22, 0)
		gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
#else
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
					   event->button, event->time);
#endif
		g_free(name);
		g_free(uri);
		return TRUE;
	}
	return FALSE;
}

static gboolean on_treeview_keypress(GtkWidget *widget, GdkEventKey *event)
{
	GtkTreeModel *model;
	GtkTreePath *path;
	GtkTreeIter iter;
	
	if (event->keyval == GDK_space)
	{
		if (gtk_tree_selection_get_selected(
						gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)),
						&model, &iter))
		{
			path = gtk_tree_model_get_path(model, &iter);
			if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path))
				gtk_tree_view_collapse_row(GTK_TREE_VIEW(widget), path);
			else
				gtk_tree_view_expand_row(GTK_TREE_VIEW(widget), path, FALSE);
			return TRUE;
		}
	}
	if (event->keyval == GDK_BackSpace)
	{
		on_button_go_up();
		return TRUE;
	}
	
	GdkModifierType modifiers = gtk_accelerator_get_default_mod_mask();
	if ((event->keyval == GDK_Menu) ||
		(event->keyval == GDK_F10 &&
		 (event->state & modifiers) == GDK_SHIFT_MASK))
	{
		gchar *name = NULL, *uri = NULL;
		GtkWidget *menu;
		
		if (gtk_tree_selection_get_selected(
						gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)),
						&model, &iter))
			gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
							   TREEBROWSER_COLUMN_NAME, &name,
							   TREEBROWSER_COLUMN_URI, &uri, -1);
		
		menu = create_popup_menu(name != NULL ? name : "",
								 uri != NULL ? uri : "");
#if GTK_CHECK_VERSION(3, 22, 0)
		gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent *)event);
#else
		gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
					   0, event->time);
#endif
		g_free(name);
		g_free(uri);
		return TRUE;
	}
	if (event->keyval == GDK_Left)
	{
		if (gtk_tree_selection_get_selected(
						gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)),
						&model, &iter))
		{
			path = gtk_tree_model_get_path(model, &iter);
			if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path))
				gtk_tree_view_collapse_row(GTK_TREE_VIEW(widget), path);
			else if (gtk_tree_path_get_depth(path) > 1)
			{
				gtk_tree_path_up(path);
				gtk_tree_view_set_cursor(GTK_TREE_VIEW(widget), path,
										 NULL, FALSE);
				gtk_tree_selection_select_path(
							gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)),
							path);
			}
		}
		return TRUE;
	}
	if (event->keyval == GDK_Right)
	{
		if (gtk_tree_selection_get_selected(
						gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)),
						&model, &iter))
		{
			path = gtk_tree_model_get_path(model, &iter);
			if (!gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path))
				gtk_tree_view_expand_row(GTK_TREE_VIEW(widget), path, FALSE);
		}
		return TRUE;
	}
	return FALSE;
}

static void on_treeview_changed(GtkWidget *widget, gpointer user_data)
{
	GtkTreeModel *model;
	GtkTreeIter iter;
	
	if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(widget),
										&model, &iter))
	{
		gchar *uri;
		gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
						   TREEBROWSER_COLUMN_URI, &uri, -1);
		if (uri == NULL)
			return;
		
		if (g_file_test(uri, G_FILE_TEST_EXISTS))
		{
			if (CONFIG_ONE_CLICK_CHDOC &&
				!g_file_test(uri, G_FILE_TEST_IS_DIR))
				document_open_file(uri, FALSE, NULL, NULL);
		} else
			treebrowser_tree_store_iter_clear_nodes(&iter, TRUE);
		
		g_free(uri);
	}
}

static void on_treeview_row_activated(GtkWidget *widget, GtkTreePath *path,
									  GtkTreeViewColumn *column,
									  gpointer user_data)
{
	GtkTreeIter iter;
	gtk_tree_model_get_iter(GTK_TREE_MODEL(treestore), &iter, path);
	
	gchar *uri;
	gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
					   TREEBROWSER_COLUMN_URI, &uri, -1);
	if (uri == NULL)
		return;
	
	if (g_file_test(uri, G_FILE_TEST_IS_DIR))
	{
		if (CONFIG_CHROOT_ON_DCLICK)
			treebrowser_chroot(uri);
		else
		{
			if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(widget), path))
				gtk_tree_view_collapse_row(GTK_TREE_VIEW(widget), path);
			else
			{
				treebrowser_browse(uri, &iter);
				// esh: gtk_tree_view_expand_row was not called
				//      in treebrowser_browse - call here
				gtk_tree_view_expand_row(GTK_TREE_VIEW(widget), path, FALSE);
			}
		}
	}
	else
	{
		document_open_file(uri, FALSE, NULL, NULL);
		if (CONFIG_ON_OPEN_FOCUS_EDITOR)
			keybindings_send_command(GEANY_KEY_GROUP_FOCUS,
									 GEANY_KEYS_FOCUS_EDITOR);
	}
	g_free(uri);
}

static void on_treeview_row_expanded(GtkWidget *widget, GtkTreeIter *iter,
									 GtkTreePath *path, gpointer user_data)
{
	if (flag_on_expand_refresh)
		return;
	
	gchar *uri;
	gtk_tree_model_get(GTK_TREE_MODEL(treestore), iter,
					   TREEBROWSER_COLUMN_URI, &uri, -1);
	if (uri == NULL)
		return;
	
	flag_on_expand_refresh = TRUE;
	treebrowser_browse(uri, iter);
	// esh: gtk_tree_view_expand_row was called in
	//      treebrowser_browse, no need to call again
	//~ gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview), path, FALSE);
	flag_on_expand_refresh = FALSE;
	
	if (CONFIG_SHOW_ICONS)
	{
#if GTK_CHECK_VERSION(3, 10, 0)
		GdkPixbuf *icon = utils_pixbuf_from_name("document-open");
#else
		GdkPixbuf *icon = utils_pixbuf_from_stock(GTK_STOCK_OPEN);
#endif
		gtk_tree_store_set(treestore, iter,
						   TREEBROWSER_COLUMN_ICON, icon, -1);
		g_object_unref(icon);
	}
	g_free(uri);
}

static void on_treeview_row_collapsed(GtkWidget *widget, GtkTreeIter *iter,
									  GtkTreePath *path, gpointer user_data)
{
	gchar *uri;
	gtk_tree_model_get(GTK_TREE_MODEL(treestore), iter,
					   TREEBROWSER_COLUMN_URI, &uri, -1);
	if (uri == NULL)
		return;
	
	if (CONFIG_SHOW_ICONS)
	{
#if GTK_CHECK_VERSION(3, 10, 0)
		GdkPixbuf *icon = utils_pixbuf_from_name("folder");
#else
		GdkPixbuf *icon = utils_pixbuf_from_stock(GTK_STOCK_DIRECTORY);
#endif
		gtk_tree_store_set(treestore, iter,
						   TREEBROWSER_COLUMN_ICON, icon, -1);
		g_object_unref(icon);
	}
	g_free(uri);
}

static void on_treeview_renamed(GtkCellRenderer *renderer,
								const gchar *path_string,
								const gchar *name_new,
								gpointer user_data)
{
	GtkTreeViewColumn *column = gtk_tree_view_get_column(
											GTK_TREE_VIEW(treeview), 0);
	
	GList *renderers = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(column));
	renderer = g_list_nth_data(renderers, TREEBROWSER_COLUMN_NAME);
	g_list_free(renderers);
	
	g_object_set(G_OBJECT(renderer), "editable", FALSE, NULL);
	
	GtkTreeIter iter;
	if (gtk_tree_model_get_iter_from_string(GTK_TREE_MODEL(treestore),
											&iter, path_string))
	{
		gchar *uri;
		gtk_tree_model_get(GTK_TREE_MODEL(treestore), &iter,
						   TREEBROWSER_COLUMN_URI, &uri, -1);
		if (uri)
		{
			gchar *dirname = g_path_get_dirname(uri);
			gchar *uri_new = g_strconcat(dirname, G_DIR_SEPARATOR_S,
										 name_new, NULL);
			g_free(dirname);
			
			if (!(g_file_test(uri_new, G_FILE_TEST_EXISTS) &&
				  strcmp(uri, uri_new) != 0 &&
				  !dialogs_show_question(_("Target file '%s' exists, do you really "
										   "want to replace it?"), uri_new)))
			{
				if (g_rename(uri, uri_new) == 0)
				{
					dirname = g_path_get_dirname(uri_new);
					gtk_tree_store_set(treestore, &iter,
									   TREEBROWSER_COLUMN_NAME, name_new,
									   TREEBROWSER_COLUMN_URI,  uri_new, -1);
					
					GtkTreeIter iter_parent;
					if (gtk_tree_model_iter_parent(GTK_TREE_MODEL(treestore),
												   &iter_parent, &iter))
						treebrowser_browse(dirname, &iter_parent);
					else
						treebrowser_browse(dirname, NULL);
					
					g_free(dirname);
					
					if (!g_file_test(uri, G_FILE_TEST_IS_DIR))
					{
						GeanyDocument *doc = document_find_by_filename(uri);
						if (doc && document_close(doc))
							document_open_file(uri_new, FALSE, NULL, NULL);
					}
				}
			}
			g_free(uri_new);
			g_free(uri);
		}
	}
}

static void treebrowser_track_current_cb(void)
{
	if (CONFIG_FOLLOW_CURRENT_DOC)
		treebrowser_track_current();
}


/* ------------------
 * TREEBROWSER INITIAL FUNCTIONS
 * ------------------ */

static gboolean treeview_separator_func(GtkTreeModel *model,
										GtkTreeIter *iter,
										gpointer data)
{
	gint flag;
	gtk_tree_model_get(model, iter, TREEBROWSER_COLUMN_FLAG, &flag, -1);
	return (flag == TREEBROWSER_FLAGS_SEPARATOR);
}

static GtkWidget *create_view_and_model(void)
{
	GtkWidget *view;
	
	view 				 = gtk_tree_view_new();
	treeview_column_text = gtk_tree_view_column_new();
	render_icon 		 = gtk_cell_renderer_pixbuf_new();
	render_text 		 = gtk_cell_renderer_text_new();
	
	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(view), treeview_column_text);
	
	gtk_tree_view_column_pack_start(treeview_column_text, render_icon, FALSE);
	gtk_tree_view_column_set_attributes(treeview_column_text, render_icon,
										"pixbuf", TREEBROWSER_COLUMN_ICON, NULL);
	
	gtk_tree_view_column_pack_start(treeview_column_text, render_text, TRUE);
	gtk_tree_view_column_set_attributes(treeview_column_text, render_text,
										"text", TREEBROWSER_COLUMN_NAME,
										"foreground-gdk", TREEBROWSER_COLUMN_COLOR,
										NULL);
	
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view), TRUE);
	gtk_tree_view_set_search_column(GTK_TREE_VIEW(view),
									TREEBROWSER_COLUMN_NAME);
	
	gtk_tree_view_set_row_separator_func(GTK_TREE_VIEW(view),
										 treeview_separator_func,
										 NULL, NULL);
	
	ui_widget_modify_font_from_string(view, geany->interface_prefs->tagbar_font);
	
	g_object_set(view, "has-tooltip", TRUE, "tooltip-column",
				 TREEBROWSER_COLUMN_URI, NULL);
	
	gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(view)),
								GTK_SELECTION_SINGLE);
	
	gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(view),
										CONFIG_SHOW_TREE_LINES);
	
	treestore = gtk_tree_store_new(TREEBROWSER_COLUMNC, GDK_TYPE_PIXBUF,
								   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,
								   GDK_TYPE_COLOR);
	
	gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(treestore));
	g_signal_connect(G_OBJECT(render_text), "edited",
					 G_CALLBACK(on_treeview_renamed), view);
	
	return view;
}

static void create_sidebar(void)
{
	GtkWidget *scrollwin;
	GtkWidget *toolbar;
	GtkWidget *wid;
	GtkTreeSelection *selection;
	
#if GTK_CHECK_VERSION(3, 0, 0)
	GtkCssProvider *provider = gtk_css_provider_new();
	GdkDisplay *display = gdk_display_get_default();
	GdkScreen *screen = gdk_display_get_default_screen(display);
	
	gtk_style_context_add_provider_for_screen(screen,
						GTK_STYLE_PROVIDER(provider),
						GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	gtk_css_provider_load_from_data(GTK_CSS_PROVIDER(provider),
				"#addressbar.invalid {color: #ffffff; background: #ff6666;}",
				-1, NULL);
#endif
	
	treeview 			= create_view_and_model();
	sidebar_vbox 		= gtk_vbox_new(FALSE, 0);
	sidebar_vbox_bars 	= gtk_vbox_new(FALSE, 0);
	selection 			= gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
	addressbar 			= gtk_entry_new();
#if GTK_CHECK_VERSION(3, 10, 0)
	gtk_widget_set_name(addressbar, "addressbar");
#endif
	filter 				= gtk_entry_new();
	scrollwin 			= gtk_scrolled_window_new(NULL, NULL);
	
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollwin),
								   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	
	toolbar = gtk_toolbar_new();
	gtk_toolbar_set_icon_size(GTK_TOOLBAR(toolbar), GTK_ICON_SIZE_MENU);
	gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	wid = gtk_image_new_from_icon_name("go-up", GTK_ICON_SIZE_SMALL_TOOLBAR);
	wid = GTK_WIDGET(gtk_tool_button_new(wid, NULL));
#else
	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_GO_UP));
#endif
	gtk_widget_set_tooltip_text(wid, _("Go up"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_go_up), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	wid = gtk_image_new_from_icon_name("view-refresh",
									   GTK_ICON_SIZE_SMALL_TOOLBAR);
	wid = GTK_WIDGET(gtk_tool_button_new(wid, NULL));
#else
	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_REFRESH));
#endif
	gtk_widget_set_tooltip_text(wid, _("Refresh"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_refresh), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	wid = gtk_image_new_from_icon_name("go-home", GTK_ICON_SIZE_SMALL_TOOLBAR);
	wid = GTK_WIDGET(gtk_tool_button_new(wid, NULL));
#else
	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_HOME));
#endif
	gtk_widget_set_tooltip_text(wid, _("Home"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_go_home), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	btn_proj_path = gtk_image_new_from_icon_name("go-first",
												 GTK_ICON_SIZE_SMALL_TOOLBAR);
	btn_proj_path = GTK_WIDGET(gtk_tool_button_new(btn_proj_path, NULL));
#else
	btn_proj_path = GTK_WIDGET(gtk_tool_button_new_from_stock(
												GTK_STOCK_GOTO_FIRST));
#endif
	gtk_widget_set_tooltip_text(btn_proj_path, _("Set project path"));
	g_signal_connect(btn_proj_path, "clicked",
					 G_CALLBACK(on_button_project_path), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), btn_proj_path);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	wid = gtk_image_new_from_icon_name("go-jump", GTK_ICON_SIZE_SMALL_TOOLBAR);
	wid = GTK_WIDGET(gtk_tool_button_new(wid, NULL));
#else
	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_JUMP_TO));
#endif
	gtk_widget_set_tooltip_text(wid, _("Set path from document"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_current_path), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	wid = gtk_image_new_from_icon_name("folder", GTK_ICON_SIZE_SMALL_TOOLBAR);
	wid = GTK_WIDGET(gtk_tool_button_new(wid, NULL));
#else
	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_DIRECTORY));
#endif
	gtk_widget_set_tooltip_text(wid, _("Track Current"));
	g_signal_connect(wid, "clicked", G_CALLBACK(treebrowser_track_current), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);
	
#if GTK_CHECK_VERSION(3, 10, 0)
	wid = gtk_image_new_from_icon_name("window-close",
									   GTK_ICON_SIZE_SMALL_TOOLBAR);
	wid = GTK_WIDGET(gtk_tool_button_new(wid, NULL));
#else
	wid = GTK_WIDGET(gtk_tool_button_new_from_stock(GTK_STOCK_CLOSE));
#endif
	gtk_widget_set_tooltip_text(wid, _("Hide bars"));
	g_signal_connect(wid, "clicked", G_CALLBACK(on_button_hide_bars), NULL);
	gtk_container_add(GTK_CONTAINER(toolbar), wid);
	
	gtk_container_add(GTK_CONTAINER(scrollwin), treeview);
	gtk_box_pack_start(GTK_BOX(sidebar_vbox_bars), filter, FALSE, TRUE,  1);
	gtk_box_pack_start(GTK_BOX(sidebar_vbox_bars), addressbar, FALSE, TRUE,  1);
	gtk_box_pack_start(GTK_BOX(sidebar_vbox_bars), toolbar, FALSE, TRUE,  1);
	
	gtk_widget_set_tooltip_text(filter,
		_("Filter (*.c;*.h;*.cpp), and if you want temporary filter using "
		  "the '!' reverse try for example this '!;*.c;*.h;*.cpp'"));
	ui_entry_add_clear_icon(GTK_ENTRY(filter));
	g_signal_connect(filter, "icon-release", G_CALLBACK(on_filter_clear), NULL);
	
	gtk_widget_set_tooltip_text(addressbar,
		_("Addressbar for example '/projects/my-project'"));
	
	if (CONFIG_SHOW_BARS == 2)
	{
		gtk_box_pack_start(GTK_BOX(sidebar_vbox), scrollwin, 		 TRUE,  TRUE, 1);
		gtk_box_pack_start(GTK_BOX(sidebar_vbox), sidebar_vbox_bars, FALSE, TRUE, 1);
	}
	else
	{
		gtk_box_pack_start(GTK_BOX(sidebar_vbox), sidebar_vbox_bars, FALSE, TRUE, 1);
		gtk_box_pack_start(GTK_BOX(sidebar_vbox), scrollwin, 		 TRUE,  TRUE, 1);
	}
	
	//----------------------------------------------------------------
	g_signal_connect(selection,  "changed", 			G_CALLBACK(on_treeview_changed), 		NULL);
	g_signal_connect(treeview,   "button-press-event",	G_CALLBACK(on_treeview_mouseclick), 	selection);
	g_signal_connect(treeview,   "row-activated", 		G_CALLBACK(on_treeview_row_activated), 	NULL);
	g_signal_connect(treeview,   "row-collapsed", 		G_CALLBACK(on_treeview_row_collapsed), 	NULL);
	g_signal_connect(treeview,   "row-expanded", 		G_CALLBACK(on_treeview_row_expanded), 	NULL);
	g_signal_connect(treeview,   "key-press-event", 	G_CALLBACK(on_treeview_keypress), 		NULL);
	g_signal_connect(treeview,   "grab-focus", 			G_CALLBACK(on_treeview_grabfocus), 		NULL);
	g_signal_connect(addressbar, "activate", 			G_CALLBACK(on_addressbar_activate), 	NULL);
	g_signal_connect(addressbar, "grab-focus", 			G_CALLBACK(on_addressbar_grabfocus), 	NULL);
	g_signal_connect(filter,     "activate", 			G_CALLBACK(on_filter_activate), 		NULL);
	g_signal_connect(filter,     "focus", 				G_CALLBACK(on_filter_focus), 			NULL);
	g_signal_connect(filter,     "grab-focus", 			G_CALLBACK(on_filter_grabfocus), 		NULL);
	
	gtk_widget_show_all(sidebar_vbox);
	
	page_number = gtk_notebook_append_page(
							GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
							sidebar_vbox, gtk_label_new(_("Tree Browser")));
	
	gtk_widget_set_sensitive(btn_proj_path, geany->app->project != NULL);
	
	showbars(CONFIG_SHOW_BARS);
}


/* ------------------
 * CONFIG DIALOG
 * ------------------ */

static struct
{
	GtkWidget *OPEN_EXTERNAL_CMD;
	GtkWidget *OPEN_TERMINAL;
	GtkWidget *REVERSE_FILTER;
	GtkWidget *ONE_CLICK_CHDOC;
	GtkWidget *SHOW_HIDDEN_FILES;
	GtkWidget *HIDE_OBJECT_FILES;
	GtkWidget *OBJECT_FILES_MASK;
	GtkWidget *HIDE_IGNORED_DIRS;
	GtkWidget *IGNORED_DIRS_MASK;
	GtkWidget *SHOW_BARS;
	GtkWidget *CHROOT_ON_DCLICK;
	GtkWidget *FOLLOW_CURRENT_DOC;
	GtkWidget *ON_DELETE_CLOSE_FILE;
	GtkWidget *ON_OPEN_FOCUS_EDITOR;
	GtkWidget *SHOW_TREE_LINES;
	GtkWidget *SHOW_BOOKMARKS;
	GtkWidget *SHOW_ICONS;
	GtkWidget *OPEN_NEW_FILES;
} configure_widgets;

#define LOAD_IGNORE_LISTS												\
	g_strfreev(OBJECT_FILE_EXTS);										\
	OBJECT_FILE_EXTS = g_strsplit(CONFIG_OBJECT_FILES_MASK, " ", 0);	\
	g_strfreev(IGNORED_DIR_NAMES);										\
	IGNORED_DIR_NAMES = g_strsplit(CONFIG_IGNORED_DIRS_MASK, " ", 0);

static void load_settings(void)
{
	GKeyFile *config = g_key_file_new();
	
	g_key_file_load_from_file(config, CONFIG_FILE, G_KEY_FILE_NONE, NULL);
	
	CONFIG_OPEN_EXTERNAL_CMD	= utils_get_setting_string(config, "treebrowser", "open_external_cmd",		CONFIG_OPEN_EXTERNAL_CMD_DEFAULT);
	CONFIG_OPEN_TERMINAL		= utils_get_setting_string(config, "treebrowser", "open_terminal",			CONFIG_OPEN_TERMINAL_DEFAULT);
	CONFIG_REVERSE_FILTER		= utils_get_setting_boolean(config, "treebrowser", "reverse_filter",		CONFIG_REVERSE_FILTER);
	CONFIG_ONE_CLICK_CHDOC		= utils_get_setting_boolean(config, "treebrowser", "one_click_chdoc",		CONFIG_ONE_CLICK_CHDOC);
	CONFIG_SHOW_HIDDEN_FILES	= utils_get_setting_boolean(config, "treebrowser", "show_hidden_files",		CONFIG_SHOW_HIDDEN_FILES);
	CONFIG_HIDE_OBJECT_FILES	= utils_get_setting_boolean(config, "treebrowser", "hide_object_files",		CONFIG_HIDE_OBJECT_FILES);
	CONFIG_OBJECT_FILES_MASK	= utils_get_setting_string(config, "treebrowser", "object_files_mask",		CONFIG_OBJECT_FILES_MASK_DEFAULT);
	CONFIG_HIDE_IGNORED_DIRS	= utils_get_setting_boolean(config, "treebrowser", "hide_ignored_dirs",		CONFIG_HIDE_IGNORED_DIRS);
	CONFIG_IGNORED_DIRS_MASK	= utils_get_setting_string(config, "treebrowser", "ignored_dirs_mask",		"");
	CONFIG_SHOW_BARS			= utils_get_setting_integer(config, "treebrowser", "show_bars",				CONFIG_SHOW_BARS);
	CONFIG_CHROOT_ON_DCLICK		= utils_get_setting_boolean(config, "treebrowser", "chroot_on_dclick",		CONFIG_CHROOT_ON_DCLICK);
	CONFIG_FOLLOW_CURRENT_DOC	= utils_get_setting_boolean(config, "treebrowser", "follow_current_doc",	CONFIG_FOLLOW_CURRENT_DOC);
	CONFIG_ON_DELETE_CLOSE_FILE	= utils_get_setting_boolean(config, "treebrowser", "on_delete_close_file",	CONFIG_ON_DELETE_CLOSE_FILE);
	CONFIG_ON_OPEN_FOCUS_EDITOR	= utils_get_setting_boolean(config, "treebrowser", "on_open_focus_editor",	CONFIG_ON_OPEN_FOCUS_EDITOR);
	CONFIG_SHOW_TREE_LINES		= utils_get_setting_boolean(config, "treebrowser", "show_tree_lines",		CONFIG_SHOW_TREE_LINES);
	CONFIG_SHOW_BOOKMARKS		= utils_get_setting_boolean(config, "treebrowser", "show_bookmarks",		CONFIG_SHOW_BOOKMARKS);
	CONFIG_SHOW_ICONS			= utils_get_setting_integer(config, "treebrowser", "show_icons",			CONFIG_SHOW_ICONS);
	CONFIG_OPEN_NEW_FILES		= utils_get_setting_boolean(config, "treebrowser", "open_new_files",		CONFIG_OPEN_NEW_FILES);
	
	LOAD_IGNORE_LISTS;
	
	g_key_file_free(config);
}

static gboolean save_settings(void)
{
	GKeyFile *config = g_key_file_new();
	gchar *config_dir = g_path_get_dirname(CONFIG_FILE);
	
	g_key_file_load_from_file(config, CONFIG_FILE, G_KEY_FILE_NONE, NULL);
	if (!g_file_test(config_dir, G_FILE_TEST_IS_DIR) &&
		utils_mkdir(config_dir, TRUE) != 0)
	{
		g_free(config_dir);
		g_key_file_free(config);
		return FALSE;
	}
	
	g_key_file_set_string(config,	"treebrowser", "open_external_cmd",		CONFIG_OPEN_EXTERNAL_CMD);
	g_key_file_set_string(config,	"treebrowser", "open_terminal",			CONFIG_OPEN_TERMINAL);
	g_key_file_set_boolean(config,	"treebrowser", "reverse_filter",		CONFIG_REVERSE_FILTER);
	g_key_file_set_boolean(config,	"treebrowser", "one_click_chdoc",		CONFIG_ONE_CLICK_CHDOC);
	g_key_file_set_boolean(config,	"treebrowser", "show_hidden_files",		CONFIG_SHOW_HIDDEN_FILES);
	g_key_file_set_boolean(config,	"treebrowser", "hide_object_files",		CONFIG_HIDE_OBJECT_FILES);
	g_key_file_set_string(config,	"treebrowser", "object_files_mask",		CONFIG_OBJECT_FILES_MASK);
	g_key_file_set_boolean(config,	"treebrowser", "hide_ignored_dirs",		CONFIG_HIDE_IGNORED_DIRS);
	g_key_file_set_string(config,	"treebrowser", "ignored_dirs_mask",		CONFIG_IGNORED_DIRS_MASK);
	g_key_file_set_integer(config,	"treebrowser", "show_bars",				CONFIG_SHOW_BARS);
	g_key_file_set_boolean(config,	"treebrowser", "chroot_on_dclick",		CONFIG_CHROOT_ON_DCLICK);
	g_key_file_set_boolean(config,	"treebrowser", "follow_current_doc",	CONFIG_FOLLOW_CURRENT_DOC);
	g_key_file_set_boolean(config,	"treebrowser", "on_delete_close_file",	CONFIG_ON_DELETE_CLOSE_FILE);
	g_key_file_set_boolean(config,	"treebrowser", "on_open_focus_editor",	CONFIG_ON_OPEN_FOCUS_EDITOR);
	g_key_file_set_boolean(config,	"treebrowser", "show_tree_lines",		CONFIG_SHOW_TREE_LINES);
	g_key_file_set_boolean(config,	"treebrowser", "show_bookmarks",		CONFIG_SHOW_BOOKMARKS);
	g_key_file_set_integer(config,	"treebrowser", "show_icons",			CONFIG_SHOW_ICONS);
	g_key_file_set_boolean(config,	"treebrowser", "open_new_files",		CONFIG_OPEN_NEW_FILES);
	
	gchar *data = g_key_file_to_data(config, NULL, NULL);
	utils_write_file(CONFIG_FILE, data);
	g_free(data);
	
	g_free(config_dir);
	g_key_file_free(config);
	
	return TRUE;
}

static void on_configure_response(GtkDialog *dialog, gint response,
								  gpointer user_data)
{
	if (!(response == GTK_RESPONSE_OK || response == GTK_RESPONSE_APPLY))
		return;
	
	CONFIG_OPEN_EXTERNAL_CMD	= gtk_editable_get_chars(GTK_EDITABLE(configure_widgets.OPEN_EXTERNAL_CMD), 0, -1);
	CONFIG_OPEN_TERMINAL		= gtk_editable_get_chars(GTK_EDITABLE(configure_widgets.OPEN_TERMINAL), 0, -1);
	CONFIG_REVERSE_FILTER		= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.REVERSE_FILTER));
	CONFIG_ONE_CLICK_CHDOC		= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.ONE_CLICK_CHDOC));
	CONFIG_SHOW_HIDDEN_FILES	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_HIDDEN_FILES));
	CONFIG_HIDE_OBJECT_FILES	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.HIDE_OBJECT_FILES));
	CONFIG_OBJECT_FILES_MASK	= gtk_editable_get_chars(GTK_EDITABLE(configure_widgets.OBJECT_FILES_MASK), 0, -1);
	CONFIG_HIDE_IGNORED_DIRS	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.HIDE_IGNORED_DIRS));
	CONFIG_IGNORED_DIRS_MASK	= gtk_editable_get_chars(GTK_EDITABLE(configure_widgets.IGNORED_DIRS_MASK), 0, -1);
	CONFIG_SHOW_BARS			= gtk_combo_box_get_active(GTK_COMBO_BOX(configure_widgets.SHOW_BARS));
	CONFIG_CHROOT_ON_DCLICK		= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.CHROOT_ON_DCLICK));
	CONFIG_FOLLOW_CURRENT_DOC	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.FOLLOW_CURRENT_DOC));
	CONFIG_ON_DELETE_CLOSE_FILE	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.ON_DELETE_CLOSE_FILE));
	CONFIG_ON_OPEN_FOCUS_EDITOR	= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.ON_OPEN_FOCUS_EDITOR));
	CONFIG_SHOW_TREE_LINES		= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_TREE_LINES));
	CONFIG_SHOW_BOOKMARKS		= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.SHOW_BOOKMARKS));
	CONFIG_SHOW_ICONS			= gtk_combo_box_get_active(GTK_COMBO_BOX(configure_widgets.SHOW_ICONS));
	CONFIG_OPEN_NEW_FILES		= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(configure_widgets.OPEN_NEW_FILES));
	
	LOAD_IGNORE_LISTS;
	
	if (save_settings())
	{
		gtk_tree_view_set_enable_tree_lines(GTK_TREE_VIEW(treeview),
											CONFIG_SHOW_TREE_LINES);
		treebrowser_chroot(addressbar_last_address);
		if (CONFIG_SHOW_BOOKMARKS)
			treebrowser_load_bookmarks();
		showbars(CONFIG_SHOW_BARS);
	}
	else
		dialogs_show_msgbox(GTK_MESSAGE_ERROR,
			_("Plugin configuration directory could not be created."));
}

GtkWidget *plugin_configure(GtkDialog *dialog)
{
	DoubleWidget double_widget;
	GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
	
	//----------------------------------------------------------------
	configure_widgets.OPEN_EXTERNAL_CMD = add_inputbox(vbox,
		_("External open command:"), CONFIG_OPEN_EXTERNAL_CMD, -1,
		_("The command to execute when using \"Open with\". You can use "
		  "%f and %d wildcards.\n%f will be replaced with the filename "
		  "including full path\n%d will be replaced with the path name "
		  "of the selected file without the filename"), FALSE, FALSE);
	
	//----------------------------------------------------------------
	configure_widgets.OPEN_TERMINAL = add_inputbox(vbox,
		_("Terminal:"), CONFIG_OPEN_TERMINAL, -1,
		_("The terminal to use with the command \"Open Terminal\""),
		FALSE, FALSE);
	
	//----------------------------------------------------------------
	const gchar *SHOW_BARS_TEXTS[] = {_("Hidden"), _("Top"), _("Bottom")};
	configure_widgets.SHOW_BARS = add_combobox(vbox,
		_("Toolbar:"), SHOW_BARS_TEXTS, 3, CONFIG_SHOW_BARS,
		_("If position is changed, the option require plugin restart."), FALSE);
	
	//----------------------------------------------------------------
	const gchar *SHOW_ICONS_TEXTS[] = {_("None"), _("Base"), _("Content-type")};
	configure_widgets.SHOW_ICONS = add_combobox(vbox,
		_("Show icons:"), SHOW_ICONS_TEXTS, 3, CONFIG_SHOW_ICONS, NULL, FALSE);
	
	//----------------------------------------------------------------
	configure_widgets.SHOW_HIDDEN_FILES = add_checkbox(vbox,
		_("Show hidden files"), CONFIG_SHOW_HIDDEN_FILES,
		_("On Windows, this just hide files that are prefixed with '.' (dot)"),
		FALSE);
	
	//----------------------------------------------------------------
	double_widget = add_checkinputbox(vbox, _("Hide object files:"),
	   CONFIG_OBJECT_FILES_MASK, -1, CONFIG_HIDE_OBJECT_FILES,
	   _("Don't show generated object files in the file browser, "
		 "this includes *.o, *.obj, *.so, *.dll, *.a, *.lib"), FALSE);
	
	configure_widgets.HIDE_OBJECT_FILES = double_widget.widget1;
	configure_widgets.OBJECT_FILES_MASK = double_widget.widget2;
	
	//----------------------------------------------------------------
	double_widget = add_checkinputbox(vbox, _("Hide specified dirs:"),
	   CONFIG_IGNORED_DIRS_MASK, -1, CONFIG_HIDE_IGNORED_DIRS,
	   _("Don't show specified dirs in the file browser"), FALSE);
	
	configure_widgets.HIDE_IGNORED_DIRS = double_widget.widget1;
	configure_widgets.IGNORED_DIRS_MASK = double_widget.widget2;
	
	//----------------------------------------------------------------
	configure_widgets.REVERSE_FILTER = add_checkbox(vbox,
		_("Reverse filter"), CONFIG_REVERSE_FILTER, NULL, FALSE);
	
	//----------------------------------------------------------------
	configure_widgets.FOLLOW_CURRENT_DOC = add_checkbox(vbox,
		_("Follow current document"), CONFIG_FOLLOW_CURRENT_DOC,
		NULL, FALSE);
	
	//----------------------------------------------------------------
	configure_widgets.ONE_CLICK_CHDOC = add_checkbox(vbox,
		_("Single click, open document and focus it"), CONFIG_ONE_CLICK_CHDOC,
		NULL, FALSE);
	
	//----------------------------------------------------------------
	configure_widgets.CHROOT_ON_DCLICK = add_checkbox(vbox,
		_("Double click open directory"), CONFIG_CHROOT_ON_DCLICK,
		NULL, FALSE);
	
	//----------------------------------------------------------------
	configure_widgets.ON_DELETE_CLOSE_FILE = add_checkbox(vbox,
		_("On delete file, close it if is opened"), CONFIG_ON_DELETE_CLOSE_FILE,
		NULL, FALSE);
	
	//----------------------------------------------------------------
	configure_widgets.ON_OPEN_FOCUS_EDITOR = add_checkbox(vbox,
		_("Focus editor on file open"), CONFIG_ON_OPEN_FOCUS_EDITOR,
		NULL, FALSE);
	
	//----------------------------------------------------------------
	configure_widgets.SHOW_TREE_LINES = add_checkbox(vbox,
		_("Show tree lines"), CONFIG_SHOW_TREE_LINES, NULL, FALSE);
	
	//----------------------------------------------------------------
	configure_widgets.SHOW_BOOKMARKS = add_checkbox(vbox,
		_("Show bookmarks"), CONFIG_SHOW_BOOKMARKS, NULL, FALSE);
	
	//----------------------------------------------------------------
	configure_widgets.OPEN_NEW_FILES = add_checkbox(vbox,
		_("Open new files"), CONFIG_OPEN_NEW_FILES, NULL, FALSE);
	
	//----------------------------------------------------------------
	g_signal_connect(dialog, "response",
					 G_CALLBACK(on_configure_response), NULL);
	
	gtk_widget_show_all(vbox);
	return vbox;
}


/* ------------------
 * GEANY HOOKS
 * ------------------ */

static void project_open_cb(G_GNUC_UNUSED GObject *obj,
							G_GNUC_UNUSED GKeyFile *config,
							G_GNUC_UNUSED gpointer data)
{
	on_button_current_path();
	gtk_widget_set_sensitive(btn_proj_path, TRUE);
}

static void project_save_cb(G_GNUC_UNUSED GObject *obj,
							G_GNUC_UNUSED GKeyFile *config,
							G_GNUC_UNUSED gpointer data)
{
	gtk_widget_set_sensitive(btn_proj_path, TRUE);
}

static void project_close_cb(G_GNUC_UNUSED GObject *obj,
							 G_GNUC_UNUSED gpointer data)
{
	gtk_widget_set_sensitive(btn_proj_path, FALSE);
}


static void kb_activate(guint key_id)
{
	gtk_notebook_set_current_page(
						GTK_NOTEBOOK(geany->main_widgets->sidebar_notebook),
						page_number);
	switch (key_id)
	{
		case KB_FOCUS_FILE_LIST:
		case KB_FOCUS_FILE_LIST_2:
			gtk_widget_grab_focus(treeview);
			break;
		case KB_FOCUS_PATH_ENTRY:
			gtk_widget_grab_focus(addressbar);
			break;
		case KB_FOCUS_FILTER_ENTRY:
			gtk_widget_grab_focus(filter);
			break;
		case KB_RENAME_OBJECT:
			treebrowser_rename_current();
			break;
		case KB_CREATE_FILE:
			treebrowser_create_new_current("file");
			break;
		case KB_CREATE_DIR:
			treebrowser_create_new_current("directory");
			break;
		case KB_REFRESH:
			on_menu_refresh(NULL, NULL);
			break;
		case KB_TRACK_CURRENT:
			treebrowser_track_current();
			break;
	}
}

void plugin_init(GeanyData *data)
{
	GeanyKeyGroup *key_group;
	
	CONFIG_FILE = g_strconcat(geany->app->configdir,
							  G_DIR_SEPARATOR_S, "plugins",
							  G_DIR_SEPARATOR_S, "treebrowser",
							  G_DIR_SEPARATOR_S, "treebrowser.conf", NULL);
	
	flag_on_expand_refresh = FALSE;
	
	load_settings();
	create_sidebar();
	
	ui_load_color("geany-sidebar-parent", &color_parent);
	
	on_button_current_path();
	
	/* setup keybindings */
	key_group = plugin_set_key_group(geany_plugin, "file_browser",
									 KB_COUNT, NULL);
	
	keybindings_set_item(key_group, KB_FOCUS_FILE_LIST, kb_activate,
		0, 0, "focus_file_list", _("Focus File List"), NULL);
	keybindings_set_item(key_group, KB_FOCUS_FILE_LIST_2, kb_activate,
		0, 0, "focus_file_list_2", _("Focus File List (second hotkey)"), NULL);
	keybindings_set_item(key_group, KB_FOCUS_PATH_ENTRY, kb_activate,
		0, 0, "focus_path_entry", _("Focus Path Entry"), NULL);
	keybindings_set_item(key_group, KB_FOCUS_FILTER_ENTRY, kb_activate,
		0, 0, "focus_filter_entry", _("Focus Filter Entry"), NULL);
	keybindings_set_item(key_group, KB_RENAME_OBJECT, kb_activate,
		0, 0, "rename_object", _("Rename Object"), NULL);
	keybindings_set_item(key_group, KB_CREATE_FILE, kb_activate,
		0, 0, "create_file", _("New File"), NULL);
	keybindings_set_item(key_group, KB_CREATE_DIR, kb_activate,
		0, 0, "create_dir", _("New Folder"), NULL);
	keybindings_set_item(key_group, KB_REFRESH, kb_activate,
		0, 0, "rename_refresh", _("Refresh"), NULL);
	keybindings_set_item(key_group, KB_TRACK_CURRENT, kb_activate,
		0, 0, "track_current", _("Track Current"), NULL);
	
	plugin_signal_connect(geany_plugin, NULL, "document-activate", TRUE,
						  (GCallback) &treebrowser_track_current_cb, NULL);
}

void plugin_cleanup(void)
{
	g_free(addressbar_last_address);
	g_free(CONFIG_FILE);
	g_free(CONFIG_OBJECT_FILES_MASK);
	g_free(CONFIG_IGNORED_DIRS_MASK);
	g_free(CONFIG_OPEN_EXTERNAL_CMD);
	g_free(CONFIG_OPEN_TERMINAL);
	g_strfreev(OBJECT_FILE_EXTS);
	g_strfreev(IGNORED_DIR_NAMES);
	gtk_widget_destroy(sidebar_vbox);
}
