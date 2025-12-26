/*
 * overviewprefs.c - This file is part of the Geany Overview plugin
 *
 * Copyright (c) 2015 Matthew Brush <mbrush@codebrainz.ca>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
  #include "config.h" // for the gettext domain
#endif

#include <gtkcompat.h>

#include "overviewprefs.h"
#include "overviewcolor.h"
#include "overviewscintilla.h"

#include "../../utils/src/common.h"


enum
{
  PROP_0,
  PROP_WIDTH,
  PROP_ZOOM,
  PROP_SHOW_TOOLTIP,
  PROP_SHOW_SCROLLBAR,
  PROP_DOUBLE_BUFFERED,
  PROP_SCROLL_LINES,
  PROP_OVERLAY_ENABLED,
  PROP_OVERLAY_COLOR,
  PROP_OVERLAY_OUTLINE_COLOR,
  PROP_OVERLAY_INVERTED,
  PROP_POSITION,
  PROP_VISIBLE,
  N_PROPERTIES
};

struct OverviewPrefs_
{
  GObject         parent;
  guint           width;
  gint            zoom;
  gboolean        show_tt;
  gboolean        show_sb;
  gboolean        dbl_buf;
  gint            scr_lines;
  gboolean        ovl_en;
  OverviewColor   ovl_clr;
  OverviewColor   out_clr;
  gboolean        ovl_inv;
  GtkPositionType position;
  gboolean        visible;
};

struct OverviewPrefsClass_
{
  GObjectClass parent_class;
};

static void overview_prefs_finalize     (GObject      *object);
static void overview_prefs_set_property (GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec);
static void overview_prefs_get_property (GObject      *object,
                                         guint         prop_id,
                                         GValue       *value,
                                         GParamSpec   *pspec);


static GParamSpec *pspecs[N_PROPERTIES] = { NULL };

G_DEFINE_TYPE (OverviewPrefs, overview_prefs, G_TYPE_OBJECT)

static void
overview_prefs_class_init (OverviewPrefsClass *klass)
{
  GObjectClass *g_object_class;

  g_object_class = G_OBJECT_CLASS (klass);

  g_object_class->finalize     = overview_prefs_finalize;
  g_object_class->set_property = overview_prefs_set_property;
  g_object_class->get_property = overview_prefs_get_property;

  pspecs[PROP_WIDTH] = g_param_spec_uint ("width", "Width", "Width of the overview", 16, 512, 120, G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  pspecs[PROP_ZOOM] = g_param_spec_int ("zoom", "Zoom", "Zoom level of the view", -10, 20, -10, G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  pspecs[PROP_SHOW_TOOLTIP] = g_param_spec_boolean ("show-tooltip", "ShowTooltip", "Whether to show informational tooltip over the overview", TRUE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  pspecs[PROP_SHOW_SCROLLBAR] = g_param_spec_boolean ("show-scrollbar", "ShowScrollbar", "Whether to show the normal editor scrollbar", TRUE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  pspecs[PROP_DOUBLE_BUFFERED] = g_param_spec_boolean ("double-buffered", "DoubleBuffered", "Whether the overview drawing is double-buffered", TRUE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  pspecs[PROP_SCROLL_LINES] = g_param_spec_uint ("scroll-lines", "ScrollLines", "The number of lines to scroll the overview by", 1, 512, 1, G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  pspecs[PROP_OVERLAY_ENABLED] = g_param_spec_boolean ("overlay-enabled", "OverlayEnabled", "Whether an overlay is drawn overtop the overview", TRUE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  pspecs[PROP_OVERLAY_COLOR] = g_param_spec_boxed ("overlay-color", "OverlayColor", "The color of the overlay", OVERVIEW_TYPE_COLOR, G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  pspecs[PROP_OVERLAY_OUTLINE_COLOR] = g_param_spec_boxed ("overlay-outline-color", "OverlayOutlineColor", "The color of the outlines drawn around the overlay", OVERVIEW_TYPE_COLOR, G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  pspecs[PROP_OVERLAY_INVERTED] = g_param_spec_boolean ("overlay-inverted", "OverlayInverted", "Whether to invert the drawing of the overlay", TRUE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  pspecs[PROP_POSITION] = g_param_spec_enum ("position", "Position", "Where to draw the overview", GTK_TYPE_POSITION_TYPE, GTK_POS_RIGHT, G_PARAM_CONSTRUCT | G_PARAM_READWRITE);
  pspecs[PROP_VISIBLE] = g_param_spec_boolean ("visible", "Visible", "Whether the overview is shown", TRUE, G_PARAM_CONSTRUCT | G_PARAM_READWRITE);

  g_object_class_install_properties (g_object_class, N_PROPERTIES, pspecs);
}

static void
overview_prefs_finalize (GObject *object)
{
  g_return_if_fail (OVERVIEW_IS_PREFS (object));
  G_OBJECT_CLASS (overview_prefs_parent_class)->finalize (object);
}

static void
overview_prefs_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  OverviewPrefs *self = OVERVIEW_PREFS (object);

  switch (prop_id)
    {
    case PROP_WIDTH:
      self->width = g_value_get_uint (value);
      g_object_notify (object, "width");
      break;
    case PROP_ZOOM:
      self->zoom = g_value_get_int (value);
      g_object_notify (object, "zoom");
      break;
    case PROP_SHOW_TOOLTIP:
      self->show_tt = g_value_get_boolean (value);
      g_object_notify (object, "show-tooltip");
      break;
    case PROP_SHOW_SCROLLBAR:
      self->show_sb = g_value_get_boolean (value);
      g_object_notify (object, "show-scrollbar");
      break;
    case PROP_DOUBLE_BUFFERED:
      self->dbl_buf = g_value_get_boolean (value);
      g_object_notify (object, "double-buffered");
      break;
    case PROP_SCROLL_LINES:
      self->scr_lines = g_value_get_uint (value);
      g_object_notify (object, "scroll-lines");
      break;
    case PROP_OVERLAY_ENABLED:
      self->ovl_en = g_value_get_boolean (value);
      g_object_notify (object, "overlay-enabled");
      break;
    case PROP_OVERLAY_COLOR:
      {
        OverviewColor *src = g_value_get_boxed (value);
        if (src != NULL)
          memcpy (&self->ovl_clr, src, sizeof (OverviewColor));
        g_object_notify (object, "overlay-color");
        break;
      }
    case PROP_OVERLAY_OUTLINE_COLOR:
      {
        OverviewColor *src = g_value_get_boxed (value);
        if (src != NULL)
          memcpy (&self->out_clr, src, sizeof (OverviewColor));
        g_object_notify (object, "overlay-outline-color");
        break;
      }
    case PROP_OVERLAY_INVERTED:
      self->ovl_inv = g_value_get_boolean (value);
      g_object_notify (G_OBJECT (self), "overlay-inverted");
      break;
    case PROP_POSITION:
      self->position = g_value_get_enum (value);
      g_object_notify (G_OBJECT (self), "position");
      break;
    case PROP_VISIBLE:
      self->visible = g_value_get_boolean (value);
      g_object_notify (G_OBJECT (self), "visible");
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
overview_prefs_get_property (GObject      *object,
                             guint         prop_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
  OverviewPrefs *self = OVERVIEW_PREFS (object);

  switch (prop_id)
    {
    case PROP_WIDTH:
      g_value_set_uint (value, self->width);
      break;
    case PROP_ZOOM:
      g_value_set_int (value, self->zoom);
      break;
    case PROP_SHOW_TOOLTIP:
      g_value_set_boolean (value, self->show_tt);
      break;
    case PROP_SHOW_SCROLLBAR:
      g_value_set_boolean (value, self->show_sb);
      break;
    case PROP_DOUBLE_BUFFERED:
      g_value_set_boolean (value, self->dbl_buf);
      break;
    case PROP_SCROLL_LINES:
      g_value_set_uint (value, self->scr_lines);
      break;
    case PROP_OVERLAY_ENABLED:
      g_value_set_boolean (value, self->ovl_en);
      break;
    case PROP_OVERLAY_COLOR:
      g_value_set_boxed (value, &self->ovl_clr);
      break;
    case PROP_OVERLAY_OUTLINE_COLOR:
      g_value_set_boxed (value, &self->out_clr);
      break;
    case PROP_OVERLAY_INVERTED:
      g_value_set_boolean (value, self->ovl_inv);
      break;
    case PROP_POSITION:
      g_value_set_enum (value, self->position);
      break;
    case PROP_VISIBLE:
      g_value_set_boolean (value, self->visible);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
overview_prefs_init (OverviewPrefs *self)
{
  self->position = GTK_POS_RIGHT;
  self->visible  = TRUE;
}

OverviewPrefs *overview_prefs_new(void)
{
  return g_object_new(OVERVIEW_TYPE_PREFS, NULL);
}

gboolean overview_prefs_load(OverviewPrefs *prefs, const gchar *filename, GError **error)
{
  g_return_val_if_fail(OVERVIEW_IS_PREFS(prefs), FALSE);
  g_return_val_if_fail(filename != NULL, FALSE);
  
  GKeyFile *config = load_config_from_file(filename, NULL);
  gboolean result = overview_prefs_from_config(prefs, config, error);
  g_key_file_free(config);
  
  return result;
}

#define GET(T, k, v)                                        \
  do {                                                      \
    if (g_key_file_has_key(kf, CONFIG_SECTION, k, NULL)) {  \
      v = g_key_file_get_##T(kf, CONFIG_SECTION, k, error); \
      if (error != NULL && *error != NULL) {                \
        g_key_file_free (kf);                               \
        return FALSE;                                       \
      } else {                                              \
        g_object_notify (G_OBJECT(prefs), k);               \
      }                                                     \
    }                                                       \
  } while (0)

gboolean overview_prefs_from_config(OverviewPrefs *prefs, GKeyFile *kf, GError **error)
{
  g_return_val_if_fail(OVERVIEW_IS_PREFS(prefs), FALSE);
  g_return_val_if_fail(kf != NULL, FALSE);

  GET(uint64,  "width",            prefs->width);
  GET(integer, "zoom",             prefs->zoom);
  GET(boolean, "show-tooltip",     prefs->show_tt);
  GET(boolean, "show-scrollbar",   prefs->show_sb);
  GET(boolean, "double-buffered",  prefs->dbl_buf);
  GET(uint64,  "scroll-lines",     prefs->scr_lines);
  GET(boolean, "overlay-enabled",  prefs->ovl_en);
  GET(boolean, "overlay-inverted", prefs->ovl_inv);
  GET(boolean, "visible",          prefs->visible);

  if (g_key_file_has_key(kf, CONFIG_SECTION, "position", NULL))
  {
    gchar *pos = g_key_file_get_string(kf, CONFIG_SECTION, "position", error);
    if (error != NULL && *error != NULL) return FALSE;
    
    if (g_ascii_strcasecmp(pos, "right") == 0)
      prefs->position = GTK_POS_RIGHT;
    else if (g_ascii_strcasecmp(pos, "left") == 0)
      prefs->position = GTK_POS_LEFT;
    else
    {
      g_warning("unknown value '%s' for 'position' key", pos);
      prefs->position = GTK_POS_RIGHT;
    }
    g_free(pos);
  }

  if (g_key_file_has_key(kf, CONFIG_SECTION, "overlay-color", NULL) &&
      g_key_file_has_key(kf, CONFIG_SECTION, "overlay-alpha", NULL))
  {
    if (!overview_color_from_keyfile(&prefs->ovl_clr, kf, CONFIG_SECTION,
                                     "overlay", error))
    {
      g_key_file_free(kf);
      return FALSE;
    }
    g_object_notify(G_OBJECT(prefs), "overlay-color");
  }

  if (g_key_file_has_key(kf, CONFIG_SECTION, "overlay-outline-color", NULL) &&
      g_key_file_has_key(kf, CONFIG_SECTION, "overlay-outline-alpha", NULL))
  {
    if (!overview_color_from_keyfile(&prefs->out_clr, kf, CONFIG_SECTION,
                                     "overlay-outline", error))
    {
      g_key_file_free(kf);
      return FALSE;
    }
    g_object_notify(G_OBJECT(prefs), "overlay-outline-color");
  }

  g_key_file_free(kf);
  return TRUE;
}

#define SET(T, k, v) g_key_file_set_##T(config, CONFIG_SECTION, k, v)

GKeyFile *overview_prefs_to_config(OverviewPrefs *prefs)
{
  GKeyFile *config = g_key_file_new();
  
  SET(uint64,  "width",            prefs->width);
  SET(integer, "zoom",             prefs->zoom);
  SET(boolean, "show-tooltip",     prefs->show_tt);
  SET(boolean, "show-scrollbar",   prefs->show_sb);
  SET(boolean, "double-buffered",  prefs->dbl_buf);
  SET(uint64,  "scroll-lines",     prefs->scr_lines);
  SET(boolean, "overlay-enabled",  prefs->ovl_en);
  SET(boolean, "overlay-inverted", prefs->ovl_inv);
  SET(boolean, "visible",          prefs->visible);
  
  g_key_file_set_string(config, CONFIG_SECTION, "position",
                        prefs->position == GTK_POS_LEFT ? "left" : "right");
  
  overview_color_to_keyfile(&prefs->ovl_clr, config, CONFIG_SECTION, "overlay");
  overview_color_to_keyfile(&prefs->out_clr, config, CONFIG_SECTION, "overlay-outline");
  
  return config;
}

#define BIND(prop) \
  g_object_bind_property(self, prop, sci, prop, G_BINDING_SYNC_CREATE)

void overview_prefs_bind_scintilla(OverviewPrefs *self, GObject *sci)
{
  g_return_if_fail(OVERVIEW_IS_PREFS(self));
  g_return_if_fail(OVERVIEW_IS_SCINTILLA(sci));

  BIND("width");
  BIND("zoom");
  BIND("show-tooltip");
  BIND("show-scrollbar");
  BIND("double-buffered");
  BIND("scroll-lines");
  BIND("overlay-enabled");
  BIND("overlay-color");
  BIND("overlay-outline-color");
  BIND("overlay-inverted");
  BIND("visible");
}
