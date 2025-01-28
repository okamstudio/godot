/*
 * Copyright © 2009  Red Hat, Inc.
 * Copyright © 2012  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Red Hat Author(s): Behdad Esfahbod
 * Google Author(s): Behdad Esfahbod
 */

#include "hb.hh"

#include "hb-font.hh"
#include "hb-draw.hh"
#include "hb-paint.hh"
#include "hb-machinery.hh"

#include "hb-ot.h"

#include "hb-ot-var-avar-table.hh"
#include "hb-ot-var-fvar-table.hh"


/**
 * SECTION:hb-font
 * @title: hb-font
 * @short_description: Font objects
 * @include: hb.h
 *
 * Functions for working with font objects.
 *
 * A font object represents a font face at a specific size and with
 * certain other parameters (pixels-per-em, points-per-em, variation
 * settings) specified. Font objects are created from font face
 * objects, and are used as input to hb_shape(), among other things.
 *
 * Client programs can optionally pass in their own functions that
 * implement the basic, lower-level queries of font objects. This set
 * of font functions is defined by the virtual methods in
 * #hb_font_funcs_t.
 *
 * HarfBuzz provides a built-in set of lightweight default
 * functions for each method in #hb_font_funcs_t.
 *
 * The default font functions are implemented in terms of the
 * #hb_font_funcs_t methods of the parent font object.  This allows
 * client programs to override only the methods they need to, and
 * otherwise inherit the parent font's implementation, if any.
 **/


/*
 * hb_font_funcs_t
 */

static hb_bool_t
hb_font_get_font_h_extents_nil (hb_font_t         *font HB_UNUSED,
				void              *font_data HB_UNUSED,
				hb_font_extents_t *extents,
				void              *user_data HB_UNUSED)
{
  hb_memset (extents, 0, sizeof (*extents));
  return false;
}

static hb_bool_t
hb_font_get_font_h_extents_default (hb_font_t         *font,
				    void              *font_data HB_UNUSED,
				    hb_font_extents_t *extents,
				    void              *user_data HB_UNUSED)
{
  hb_bool_t ret = font->parent->get_font_h_extents (extents);
  if (ret) {
    extents->ascender = font->parent_scale_y_distance (extents->ascender);
    extents->descender = font->parent_scale_y_distance (extents->descender);
    extents->line_gap = font->parent_scale_y_distance (extents->line_gap);
  }
  return ret;
}

static hb_bool_t
hb_font_get_font_v_extents_nil (hb_font_t         *font HB_UNUSED,
				void              *font_data HB_UNUSED,
				hb_font_extents_t *extents,
				void              *user_data HB_UNUSED)
{
  hb_memset (extents, 0, sizeof (*extents));
  return false;
}

static hb_bool_t
hb_font_get_font_v_extents_default (hb_font_t         *font,
				    void              *font_data HB_UNUSED,
				    hb_font_extents_t *extents,
				    void              *user_data HB_UNUSED)
{
  hb_bool_t ret = font->parent->get_font_v_extents (extents);
  if (ret) {
    extents->ascender = font->parent_scale_x_distance (extents->ascender);
    extents->descender = font->parent_scale_x_distance (extents->descender);
    extents->line_gap = font->parent_scale_x_distance (extents->line_gap);
  }
  return ret;
}

static hb_bool_t
hb_font_get_nominal_glyph_nil (hb_font_t      *font HB_UNUSED,
			       void           *font_data HB_UNUSED,
			       hb_codepoint_t  unicode HB_UNUSED,
			       hb_codepoint_t *glyph,
			       void           *user_data HB_UNUSED)
{
  *glyph = 0;
  return false;
}

static hb_bool_t
hb_font_get_nominal_glyph_default (hb_font_t      *font,
				   void           *font_data HB_UNUSED,
				   hb_codepoint_t  unicode,
				   hb_codepoint_t *glyph,
				   void           *user_data HB_UNUSED)
{
  if (font->has_nominal_glyphs_func_set ())
  {
    return font->get_nominal_glyphs (1, &unicode, 0, glyph, 0);
  }
  return font->parent->get_nominal_glyph (unicode, glyph);
}

#define hb_font_get_nominal_glyphs_nil hb_font_get_nominal_glyphs_default

static unsigned int
hb_font_get_nominal_glyphs_default (hb_font_t            *font,
				    void                 *font_data HB_UNUSED,
				    unsigned int          count,
				    const hb_codepoint_t *first_unicode,
				    unsigned int          unicode_stride,
				    hb_codepoint_t       *first_glyph,
				    unsigned int          glyph_stride,
				    void                 *user_data HB_UNUSED)
{
  if (font->has_nominal_glyph_func_set ())
  {
    for (unsigned int i = 0; i < count; i++)
    {
      if (!font->get_nominal_glyph (*first_unicode, first_glyph))
	return i;

      first_unicode = &StructAtOffsetUnaligned<hb_codepoint_t> (first_unicode, unicode_stride);
      first_glyph = &StructAtOffsetUnaligned<hb_codepoint_t> (first_glyph, glyph_stride);
    }
    return count;
  }

  return font->parent->get_nominal_glyphs (count,
					   first_unicode, unicode_stride,
					   first_glyph, glyph_stride);
}

static hb_bool_t
hb_font_get_variation_glyph_nil (hb_font_t      *font HB_UNUSED,
				 void           *font_data HB_UNUSED,
				 hb_codepoint_t  unicode HB_UNUSED,
				 hb_codepoint_t  variation_selector HB_UNUSED,
				 hb_codepoint_t *glyph,
				 void           *user_data HB_UNUSED)
{
  *glyph = 0;
  return false;
}

static hb_bool_t
hb_font_get_variation_glyph_default (hb_font_t      *font,
				     void           *font_data HB_UNUSED,
				     hb_codepoint_t  unicode,
				     hb_codepoint_t  variation_selector,
				     hb_codepoint_t *glyph,
				     void           *user_data HB_UNUSED)
{
  return font->parent->get_variation_glyph (unicode, variation_selector, glyph);
}


static hb_position_t
hb_font_get_glyph_h_advance_nil (hb_font_t      *font,
				 void           *font_data HB_UNUSED,
				 hb_codepoint_t  glyph HB_UNUSED,
				 void           *user_data HB_UNUSED)
{
  return font->x_scale;
}

static hb_position_t
hb_font_get_glyph_h_advance_default (hb_font_t      *font,
				     void           *font_data HB_UNUSED,
				     hb_codepoint_t  glyph,
				     void           *user_data HB_UNUSED)
{
  if (font->has_glyph_h_advances_func_set ())
  {
    hb_position_t ret;
    font->get_glyph_h_advances (1, &glyph, 0, &ret, 0);
    return ret;
  }
  return font->parent_scale_x_distance (font->parent->get_glyph_h_advance (glyph));
}

static hb_position_t
hb_font_get_glyph_v_advance_nil (hb_font_t      *font,
				 void           *font_data HB_UNUSED,
				 hb_codepoint_t  glyph HB_UNUSED,
				 void           *user_data HB_UNUSED)
{
  /* TODO use font_extents.ascender+descender */
  return -font->y_scale;
}

static hb_position_t
hb_font_get_glyph_v_advance_default (hb_font_t      *font,
				     void           *font_data HB_UNUSED,
				     hb_codepoint_t  glyph,
				     void           *user_data HB_UNUSED)
{
  if (font->has_glyph_v_advances_func_set ())
  {
    hb_position_t ret;
    font->get_glyph_v_advances (1, &glyph, 0, &ret, 0);
    return ret;
  }
  return font->parent_scale_y_distance (font->parent->get_glyph_v_advance (glyph));
}

#define hb_font_get_glyph_h_advances_nil hb_font_get_glyph_h_advances_default

static void
hb_font_get_glyph_h_advances_default (hb_font_t*            font,
				      void*                 font_data HB_UNUSED,
				      unsigned int          count,
				      const hb_codepoint_t *first_glyph,
				      unsigned int          glyph_stride,
				      hb_position_t        *first_advance,
				      unsigned int          advance_stride,
				      void                 *user_data HB_UNUSED)
{
  if (font->has_glyph_h_advance_func_set ())
  {
    for (unsigned int i = 0; i < count; i++)
    {
      *first_advance = font->get_glyph_h_advance (*first_glyph);
      first_glyph = &StructAtOffsetUnaligned<hb_codepoint_t> (first_glyph, glyph_stride);
      first_advance = &StructAtOffsetUnaligned<hb_position_t> (first_advance, advance_stride);
    }
    return;
  }

  font->parent->get_glyph_h_advances (count,
				      first_glyph, glyph_stride,
				      first_advance, advance_stride);
  for (unsigned int i = 0; i < count; i++)
  {
    *first_advance = font->parent_scale_x_distance (*first_advance);
    first_advance = &StructAtOffsetUnaligned<hb_position_t> (first_advance, advance_stride);
  }
}

#define hb_font_get_glyph_v_advances_nil hb_font_get_glyph_v_advances_default
static void
hb_font_get_glyph_v_advances_default (hb_font_t*            font,
				      void*                 font_data HB_UNUSED,
				      unsigned int          count,
				      const hb_codepoint_t *first_glyph,
				      unsigned int          glyph_stride,
				      hb_position_t        *first_advance,
				      unsigned int          advance_stride,
				      void                 *user_data HB_UNUSED)
{
  if (font->has_glyph_v_advance_func_set ())
  {
    for (unsigned int i = 0; i < count; i++)
    {
      *first_advance = font->get_glyph_v_advance (*first_glyph);
      first_glyph = &StructAtOffsetUnaligned<hb_codepoint_t> (first_glyph, glyph_stride);
      first_advance = &StructAtOffsetUnaligned<hb_position_t> (first_advance, advance_stride);
    }
    return;
  }

  font->parent->get_glyph_v_advances (count,
				      first_glyph, glyph_stride,
				      first_advance, advance_stride);
  for (unsigned int i = 0; i < count; i++)
  {
    *first_advance = font->parent_scale_y_distance (*first_advance);
    first_advance = &StructAtOffsetUnaligned<hb_position_t> (first_advance, advance_stride);
  }
}

static hb_bool_t
hb_font_get_glyph_h_origin_nil (hb_font_t      *font HB_UNUSED,
				void           *font_data HB_UNUSED,
				hb_codepoint_t  glyph HB_UNUSED,
				hb_position_t  *x,
				hb_position_t  *y,
				void           *user_data HB_UNUSED)
{
  *x = *y = 0;
  return true;
}

static hb_bool_t
hb_font_get_glyph_h_origin_default (hb_font_t      *font,
				    void           *font_data HB_UNUSED,
				    hb_codepoint_t  glyph,
				    hb_position_t  *x,
				    hb_position_t  *y,
				    void           *user_data HB_UNUSED)
{
  hb_bool_t ret = font->parent->get_glyph_h_origin (glyph, x, y);
  if (ret)
    font->parent_scale_position (x, y);
  return ret;
}

static hb_bool_t
hb_font_get_glyph_v_origin_nil (hb_font_t      *font HB_UNUSED,
				void           *font_data HB_UNUSED,
				hb_codepoint_t  glyph HB_UNUSED,
				hb_position_t  *x,
				hb_position_t  *y,
				void           *user_data HB_UNUSED)
{
  *x = *y = 0;
  return false;
}

static hb_bool_t
hb_font_get_glyph_v_origin_default (hb_font_t      *font,
				    void           *font_data HB_UNUSED,
				    hb_codepoint_t  glyph,
				    hb_position_t  *x,
				    hb_position_t  *y,
				    void           *user_data HB_UNUSED)
{
  hb_bool_t ret = font->parent->get_glyph_v_origin (glyph, x, y);
  if (ret)
    font->parent_scale_position (x, y);
  return ret;
}

static hb_position_t
hb_font_get_glyph_h_kerning_nil (hb_font_t      *font HB_UNUSED,
				 void           *font_data HB_UNUSED,
				 hb_codepoint_t  left_glyph HB_UNUSED,
				 hb_codepoint_t  right_glyph HB_UNUSED,
				 void           *user_data HB_UNUSED)
{
  return 0;
}

static hb_position_t
hb_font_get_glyph_h_kerning_default (hb_font_t      *font,
				     void           *font_data HB_UNUSED,
				     hb_codepoint_t  left_glyph,
				     hb_codepoint_t  right_glyph,
				     void           *user_data HB_UNUSED)
{
  return font->parent_scale_x_distance (font->parent->get_glyph_h_kerning (left_glyph, right_glyph));
}

#ifndef HB_DISABLE_DEPRECATED
static hb_position_t
hb_font_get_glyph_v_kerning_nil (hb_font_t      *font HB_UNUSED,
				 void           *font_data HB_UNUSED,
				 hb_codepoint_t  top_glyph HB_UNUSED,
				 hb_codepoint_t  bottom_glyph HB_UNUSED,
				 void           *user_data HB_UNUSED)
{
  return 0;
}

static hb_position_t
hb_font_get_glyph_v_kerning_default (hb_font_t      *font,
				     void           *font_data HB_UNUSED,
				     hb_codepoint_t  top_glyph,
				     hb_codepoint_t  bottom_glyph,
				     void           *user_data HB_UNUSED)
{
  return font->parent_scale_y_distance (font->parent->get_glyph_v_kerning (top_glyph, bottom_glyph));
}
#endif

static hb_bool_t
hb_font_get_glyph_extents_nil (hb_font_t          *font HB_UNUSED,
			       void               *font_data HB_UNUSED,
			       hb_codepoint_t      glyph HB_UNUSED,
			       hb_glyph_extents_t *extents,
			       void               *user_data HB_UNUSED)
{
  hb_memset (extents, 0, sizeof (*extents));
  return false;
}

static hb_bool_t
hb_font_get_glyph_extents_default (hb_font_t          *font,
				   void               *font_data HB_UNUSED,
				   hb_codepoint_t      glyph,
				   hb_glyph_extents_t *extents,
				   void               *user_data HB_UNUSED)
{
  hb_bool_t ret = font->parent->get_glyph_extents (glyph, extents);
  if (ret) {
    font->parent_scale_position (&extents->x_bearing, &extents->y_bearing);
    font->parent_scale_distance (&extents->width, &extents->height);
  }
  return ret;
}

static hb_bool_t
hb_font_get_glyph_contour_point_nil (hb_font_t      *font HB_UNUSED,
				     void           *font_data HB_UNUSED,
				     hb_codepoint_t  glyph HB_UNUSED,
				     unsigned int    point_index HB_UNUSED,
				     hb_position_t  *x,
				     hb_position_t  *y,
				     void           *user_data HB_UNUSED)
{
  *x = *y = 0;
  return false;
}

static hb_bool_t
hb_font_get_glyph_contour_point_default (hb_font_t      *font,
					 void           *font_data HB_UNUSED,
					 hb_codepoint_t  glyph,
					 unsigned int    point_index,
					 hb_position_t  *x,
					 hb_position_t  *y,
					 void           *user_data HB_UNUSED)
{
  hb_bool_t ret = font->parent->get_glyph_contour_point (glyph, point_index, x, y);
  if (ret)
    font->parent_scale_position (x, y);
  return ret;
}

static hb_bool_t
hb_font_get_glyph_name_nil (hb_font_t      *font HB_UNUSED,
			    void           *font_data HB_UNUSED,
			    hb_codepoint_t  glyph HB_UNUSED,
			    char           *name,
			    unsigned int    size,
			    void           *user_data HB_UNUSED)
{
  if (size) *name = '\0';
  return false;
}

static hb_bool_t
hb_font_get_glyph_name_default (hb_font_t      *font,
				void           *font_data HB_UNUSED,
				hb_codepoint_t  glyph,
				char           *name,
				unsigned int    size,
				void           *user_data HB_UNUSED)
{
  return font->parent->get_glyph_name (glyph, name, size);
}

static hb_bool_t
hb_font_get_glyph_from_name_nil (hb_font_t      *font HB_UNUSED,
				 void           *font_data HB_UNUSED,
				 const char     *name HB_UNUSED,
				 int             len HB_UNUSED, /* -1 means nul-terminated */
				 hb_codepoint_t *glyph,
				 void           *user_data HB_UNUSED)
{
  *glyph = 0;
  return false;
}

static hb_bool_t
hb_font_get_glyph_from_name_default (hb_font_t      *font,
				     void           *font_data HB_UNUSED,
				     const char     *name,
				     int             len, /* -1 means nul-terminated */
				     hb_codepoint_t *glyph,
				     void           *user_data HB_UNUSED)
{
  return font->parent->get_glyph_from_name (name, len, glyph);
}

static void
hb_font_draw_glyph_nil (hb_font_t       *font HB_UNUSED,
			void            *font_data HB_UNUSED,
			hb_codepoint_t   glyph,
			hb_draw_funcs_t *draw_funcs,
			void            *draw_data,
			void            *user_data HB_UNUSED)
{
}

static void
hb_font_paint_glyph_nil (hb_font_t *font HB_UNUSED,
                         void *font_data HB_UNUSED,
                         hb_codepoint_t glyph HB_UNUSED,
                         hb_paint_funcs_t *paint_funcs HB_UNUSED,
                         void *paint_data HB_UNUSED,
                         unsigned int palette HB_UNUSED,
                         hb_color_t foreground HB_UNUSED,
                         void *user_data HB_UNUSED)
{
}

typedef struct hb_font_draw_glyph_default_adaptor_t {
  hb_draw_funcs_t *draw_funcs;
  void		  *draw_data;
  float		   x_scale;
  float		   y_scale;
  float		   slant;
} hb_font_draw_glyph_default_adaptor_t;

static void
hb_draw_move_to_default (hb_draw_funcs_t *dfuncs HB_UNUSED,
			 void *draw_data,
			 hb_draw_state_t *st,
			 float to_x, float to_y,
			 void *user_data HB_UNUSED)
{
  hb_font_draw_glyph_default_adaptor_t *adaptor = (hb_font_draw_glyph_default_adaptor_t *) draw_data;
  float x_scale = adaptor->x_scale;
  float y_scale = adaptor->y_scale;
  float slant   = adaptor->slant;

  adaptor->draw_funcs->emit_move_to (adaptor->draw_data, *st,
				     x_scale * to_x + slant * to_y, y_scale * to_y);
}

static void
hb_draw_line_to_default (hb_draw_funcs_t *dfuncs HB_UNUSED, void *draw_data,
			 hb_draw_state_t *st,
			 float to_x, float to_y,
			 void *user_data HB_UNUSED)
{
  hb_font_draw_glyph_default_adaptor_t *adaptor = (hb_font_draw_glyph_default_adaptor_t *) draw_data;
  float x_scale = adaptor->x_scale;
  float y_scale = adaptor->y_scale;
  float slant   = adaptor->slant;

  st->current_x = st->current_x * x_scale + st->current_y * slant;
  st->current_y = st->current_y * y_scale;

  adaptor->draw_funcs->emit_line_to (adaptor->draw_data, *st,
				     x_scale * to_x + slant * to_y, y_scale * to_y);
}

static void
hb_draw_quadratic_to_default (hb_draw_funcs_t *dfuncs HB_UNUSED, void *draw_data,
			      hb_draw_state_t *st,
			      float control_x, float control_y,
			      float to_x, float to_y,
			      void *user_data HB_UNUSED)
{
  hb_font_draw_glyph_default_adaptor_t *adaptor = (hb_font_draw_glyph_default_adaptor_t *) draw_data;
  float x_scale = adaptor->x_scale;
  float y_scale = adaptor->y_scale;
  float slant   = adaptor->slant;

  st->current_x = st->current_x * x_scale + st->current_y * slant;
  st->current_y = st->current_y * y_scale;

  adaptor->draw_funcs->emit_quadratic_to (adaptor->draw_data, *st,
					  x_scale * control_x + slant * control_y, y_scale * control_y,
					  x_scale * to_x + slant * to_y, y_scale * to_y);
}

static void
hb_draw_cubic_to_default (hb_draw_funcs_t *dfuncs HB_UNUSED, void *draw_data,
			  hb_draw_state_t *st,
			  float control1_x, float control1_y,
			  float control2_x, float control2_y,
			  float to_x, float to_y,
			  void *user_data HB_UNUSED)
{
  hb_font_draw_glyph_default_adaptor_t *adaptor = (hb_font_draw_glyph_default_adaptor_t *) draw_data;
  float x_scale = adaptor->x_scale;
  float y_scale = adaptor->y_scale;
  float slant   = adaptor->slant;

  st->current_x = st->current_x * x_scale + st->current_y * slant;
  st->current_y = st->current_y * y_scale;

  adaptor->draw_funcs->emit_cubic_to (adaptor->draw_data, *st,
				      x_scale * control1_x + slant * control1_y, y_scale * control1_y,
				      x_scale * control2_x + slant * control2_y, y_scale * control2_y,
				      x_scale * to_x + slant * to_y, y_scale * to_y);
}

static void
hb_draw_close_path_default (hb_draw_funcs_t *dfuncs HB_UNUSED, void *draw_data,
			    hb_draw_state_t *st,
			    void *user_data HB_UNUSED)
{
  hb_font_draw_glyph_default_adaptor_t *adaptor = (hb_font_draw_glyph_default_adaptor_t *) draw_data;

  adaptor->draw_funcs->emit_close_path (adaptor->draw_data, *st);
}

static const hb_draw_funcs_t _hb_draw_funcs_default = {
  HB_OBJECT_HEADER_STATIC,

  {
#define HB_DRAW_FUNC_IMPLEMENT(name) hb_draw_##name##_default,
    HB_DRAW_FUNCS_IMPLEMENT_CALLBACKS
#undef HB_DRAW_FUNC_IMPLEMENT
  }
};

static void
hb_font_draw_glyph_default (hb_font_t       *font,
				 void            *font_data HB_UNUSED,
				 hb_codepoint_t   glyph,
				 hb_draw_funcs_t *draw_funcs,
				 void            *draw_data,
				 void            *user_data HB_UNUSED)
{
  hb_font_draw_glyph_default_adaptor_t adaptor = {
    draw_funcs,
    draw_data,
    font->parent->x_scale ? (float) font->x_scale / (float) font->parent->x_scale : 0.f,
    font->parent->y_scale ? (float) font->y_scale / (float) font->parent->y_scale : 0.f,
    font->parent->y_scale ? (font->slant - font->parent->slant) *
			    (float) font->x_scale / (float) font->parent->y_scale : 0.f
  };

  font->parent->draw_glyph (glyph,
				 const_cast<hb_draw_funcs_t *> (&_hb_draw_funcs_default),
				 &adaptor);
}

static void
hb_font_paint_glyph_default (hb_font_t *font,
                             void *font_data,
                             hb_codepoint_t glyph,
                             hb_paint_funcs_t *paint_funcs,
                             void *paint_data,
                             unsigned int palette,
                             hb_color_t foreground,
                             void *user_data)
{
  paint_funcs->push_transform (paint_data,
    font->parent->x_scale ? (float) font->x_scale / (float) font->parent->x_scale : 0.f,
    font->parent->y_scale ? (font->slant - font->parent->slant) *
			    (float) font->x_scale / (float) font->parent->y_scale : 0.f,
    0.f,
    font->parent->y_scale ? (float) font->y_scale / (float) font->parent->y_scale : 0.f,
    0.f, 0.f);

  font->parent->paint_glyph (glyph, paint_funcs, paint_data, palette, foreground);

  paint_funcs->pop_transform (paint_data);
}

DEFINE_NULL_INSTANCE (hb_font_funcs_t) =
{
  HB_OBJECT_HEADER_STATIC,

  nullptr,
  nullptr,
  {
    {
#define HB_FONT_FUNC_IMPLEMENT(get_,name) hb_font_##get_##name##_nil,
      HB_FONT_FUNCS_IMPLEMENT_CALLBACKS
#undef HB_FONT_FUNC_IMPLEMENT
    }
  }
};

static const hb_font_funcs_t _hb_font_funcs_default = {
  HB_OBJECT_HEADER_STATIC,

  nullptr,
  nullptr,
  {
    {
#define HB_FONT_FUNC_IMPLEMENT(get_,name) hb_font_##get_##name##_default,
      HB_FONT_FUNCS_IMPLEMENT_CALLBACKS
#undef HB_FONT_FUNC_IMPLEMENT
    }
  }
};


/**
 * hb_font_funcs_create:
 *
 * Creates a new #hb_font_funcs_t structure of font functions.
 *
 * Return value: (transfer full): The font-functions structure
 *
 * Since: 0.9.2
 **/
hb_font_funcs_t *
hb_font_funcs_create ()
{
  hb_font_funcs_t *ffuncs;

  if (!(ffuncs = hb_object_create<hb_font_funcs_t> ()))
    return hb_font_funcs_get_empty ();

  ffuncs->get = _hb_font_funcs_default.get;

  return ffuncs;
}

/**
 * hb_font_funcs_get_empty:
 *
 * Fetches an empty font-functions structure.
 *
 * Return value: (transfer full): The font-functions structure
 *
 * Since: 0.9.2
 **/
hb_font_funcs_t *
hb_font_funcs_get_empty ()
{
  return const_cast<hb_font_funcs_t *> (&_hb_font_funcs_default);
}

/**
 * hb_font_funcs_reference: (skip)
 * @ffuncs: The font-functions structure
 *
 * Increases the reference count on a font-functions structure.
 *
 * Return value: The font-functions structure
 *
 * Since: 0.9.2
 **/
hb_font_funcs_t *
hb_font_funcs_reference (hb_font_funcs_t *ffuncs)
{
  return hb_object_reference (ffuncs);
}

/**
 * hb_font_funcs_destroy: (skip)
 * @ffuncs: The font-functions structure
 *
 * Decreases the reference count on a font-functions structure. When
 * the reference count reaches zero, the font-functions structure is
 * destroyed, freeing all memory.
 *
 * Since: 0.9.2
 **/
void
hb_font_funcs_destroy (hb_font_funcs_t *ffuncs)
{
  if (!hb_object_destroy (ffuncs)) return;

  if (ffuncs->destroy)
  {
#define HB_FONT_FUNC_IMPLEMENT(get_,name) if (ffuncs->destroy->name) \
    ffuncs->destroy->name (!ffuncs->user_data ? nullptr : ffuncs->user_data->name);
    HB_FONT_FUNCS_IMPLEMENT_CALLBACKS
#undef HB_FONT_FUNC_IMPLEMENT
  }

  hb_free (ffuncs->destroy);
  hb_free (ffuncs->user_data);

  hb_free (ffuncs);
}

/**
 * hb_font_funcs_set_user_data: (skip)
 * @ffuncs: The font-functions structure
 * @key: The user-data key to set
 * @data: A pointer to the user data set
 * @destroy: (nullable): A callback to call when @data is not needed anymore
 * @replace: Whether to replace an existing data with the same key
 *
 * Attaches a user-data key/data pair to the specified font-functions structure.
 *
 * Return value: `true` if success, `false` otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_font_funcs_set_user_data (hb_font_funcs_t    *ffuncs,
			     hb_user_data_key_t *key,
			     void *              data,
			     hb_destroy_func_t   destroy /* May be NULL. */,
			     hb_bool_t           replace)
{
  return hb_object_set_user_data (ffuncs, key, data, destroy, replace);
}

/**
 * hb_font_funcs_get_user_data: (skip)
 * @ffuncs: The font-functions structure
 * @key: The user-data key to query
 *
 * Fetches the user data associated with the specified key,
 * attached to the specified font-functions structure.
 *
 * Return value: (transfer none): A pointer to the user data
 *
 * Since: 0.9.2
 **/
void *
hb_font_funcs_get_user_data (const hb_font_funcs_t *ffuncs,
			     hb_user_data_key_t    *key)
{
  return hb_object_get_user_data (ffuncs, key);
}


/**
 * hb_font_funcs_make_immutable:
 * @ffuncs: The font-functions structure
 *
 * Makes a font-functions structure immutable.
 *
 * Since: 0.9.2
 **/
void
hb_font_funcs_make_immutable (hb_font_funcs_t *ffuncs)
{
  if (hb_object_is_immutable (ffuncs))
    return;

  hb_object_make_immutable (ffuncs);
}

/**
 * hb_font_funcs_is_immutable:
 * @ffuncs: The font-functions structure
 *
 * Tests whether a font-functions structure is immutable.
 *
 * Return value: `true` if @ffuncs is immutable, `false` otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_font_funcs_is_immutable (hb_font_funcs_t *ffuncs)
{
  return hb_object_is_immutable (ffuncs);
}


static bool
_hb_font_funcs_set_preamble (hb_font_funcs_t    *ffuncs,
			     bool                func_is_null,
			     void              **user_data,
			     hb_destroy_func_t  *destroy)
{
  if (hb_object_is_immutable (ffuncs))
  {
    if (*destroy)
      (*destroy) (*user_data);
    return false;
  }

  if (func_is_null)
  {
    if (*destroy)
      (*destroy) (*user_data);
    *destroy = nullptr;
    *user_data = nullptr;
  }

  return true;
}

static bool
_hb_font_funcs_set_middle (hb_font_funcs_t   *ffuncs,
			   void              *user_data,
			   hb_destroy_func_t  destroy)
{
  if (user_data && !ffuncs->user_data)
  {
    ffuncs->user_data = (decltype (ffuncs->user_data)) hb_calloc (1, sizeof (*ffuncs->user_data));
    if (unlikely (!ffuncs->user_data))
      goto fail;
  }
  if (destroy && !ffuncs->destroy)
  {
    ffuncs->destroy = (decltype (ffuncs->destroy)) hb_calloc (1, sizeof (*ffuncs->destroy));
    if (unlikely (!ffuncs->destroy))
      goto fail;
  }

  return true;

fail:
  if (destroy)
    (destroy) (user_data);
  return false;
}

#define HB_FONT_FUNC_IMPLEMENT(get_,name) \
									 \
void                                                                     \
hb_font_funcs_set_##name##_func (hb_font_funcs_t             *ffuncs,    \
				 hb_font_##get_##name##_func_t func,     \
				 void                        *user_data, \
				 hb_destroy_func_t            destroy)   \
{                                                                        \
  if (!_hb_font_funcs_set_preamble (ffuncs, !func, &user_data, &destroy))\
      return;                                                            \
									 \
  if (ffuncs->destroy && ffuncs->destroy->name)                          \
    ffuncs->destroy->name (!ffuncs->user_data ? nullptr : ffuncs->user_data->name); \
                                                                         \
  if (!_hb_font_funcs_set_middle (ffuncs, user_data, destroy))           \
      return;                                                            \
									 \
  if (func)                                                              \
    ffuncs->get.f.name = func;                                           \
  else                                                                   \
    ffuncs->get.f.name = hb_font_##get_##name##_default;                   \
									 \
  if (ffuncs->user_data)                                                 \
    ffuncs->user_data->name = user_data;                                 \
  if (ffuncs->destroy)                                                   \
    ffuncs->destroy->name = destroy;                                     \
}

HB_FONT_FUNCS_IMPLEMENT_CALLBACKS
#undef HB_FONT_FUNC_IMPLEMENT

bool
hb_font_t::has_func_set (unsigned int i)
{
  return this->klass->get.array[i] != _hb_font_funcs_default.get.array[i];
}

bool
hb_font_t::has_func (unsigned int i)
{
  return has_func_set (i) ||
	 (parent && parent != &_hb_Null_hb_font_t && parent->has_func (i));
}

/* Public getters */

/**
 * hb_font_get_h_extents:
 * @font: #hb_font_t to work upon
 * @extents: (out): The font extents retrieved
 *
 * Fetches the extents for a specified font, for horizontal
 * text segments.
 *
 * Return value: `true` if data found, `false` otherwise
 *
 * Since: 1.1.3
 **/
hb_bool_t
hb_font_get_h_extents (hb_font_t         *font,
		       hb_font_extents_t *extents)
{
  return font->get_font_h_extents (extents);
}

/**
 * hb_font_get_v_extents:
 * @font: #hb_font_t to work upon
 * @extents: (out): The font extents retrieved
 *
 * Fetches the extents for a specified font, for vertical
 * text segments.
 *
 * Return value: `true` if data found, `false` otherwise
 *
 * Since: 1.1.3
 **/
hb_bool_t
hb_font_get_v_extents (hb_font_t         *font,
		       hb_font_extents_t *extents)
{
  return font->get_font_v_extents (extents);
}

/**
 * hb_font_get_glyph:
 * @font: #hb_font_t to work upon
 * @unicode: The Unicode code point to query
 * @variation_selector: A variation-selector code point
 * @glyph: (out): The glyph ID retrieved
 *
 * Fetches the glyph ID for a Unicode code point in the specified
 * font, with an optional variation selector.
 *
 * If @variation_selector is 0, calls hb_font_get_nominal_glyph();
 * otherwise calls hb_font_get_variation_glyph().
 *
 * Return value: `true` if data found, `false` otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_font_get_glyph (hb_font_t      *font,
		   hb_codepoint_t  unicode,
		   hb_codepoint_t  variation_selector,
		   hb_codepoint_t *glyph)
{
  if (unlikely (variation_selector))
    return font->get_variation_glyph (unicode, variation_selector, glyph);
  return font->get_nominal_glyph (unicode, glyph);
}

/**
 * hb_font_get_nominal_glyph:
 * @font: #hb_font_t to work upon
 * @unicode: The Unicode code point to query
 * @glyph: (out): The glyph ID retrieved
 *
 * Fetches the nominal glyph ID for a Unicode code point in the
 * specified font.
 *
 * This version of the function should not be used to fetch glyph IDs
 * for code points modified by variation selectors. For variation-selector
 * support, user hb_font_get_variation_glyph() or use hb_font_get_glyph().
 *
 * Return value: `true` if data found, `false` otherwise
 *
 * Since: 1.2.3
 **/
hb_bool_t
hb_font_get_nominal_glyph (hb_font_t      *font,
			   hb_codepoint_t  unicode,
			   hb_codepoint_t *glyph)
{
  return font->get_nominal_glyph (unicode, glyph);
}

/**
 * hb_font_get_nominal_glyphs:
 * @font: #hb_font_t to work upon
 * @count: number of code points to query
 * @first_unicode: The first Unicode code point to query
 * @unicode_stride: The stride between successive code points
 * @first_glyph: (out): The first glyph ID retrieved
 * @glyph_stride: The stride between successive glyph IDs
 *
 * Fetches the nominal glyph IDs for a sequence of Unicode code points. Glyph
 * IDs must be returned in a #hb_codepoint_t output parameter. Stops at the
 * first unsupported glyph ID.
 *
 * Return value: the number of code points processed
 *
 * Since: 2.6.3
 **/
unsigned int
hb_font_get_nominal_glyphs (hb_font_t *font,
			    unsigned int count,
			    const hb_codepoint_t *first_unicode,
			    unsigned int unicode_stride,
			    hb_codepoint_t *first_glyph,
			    unsigned int glyph_stride)
{
  return font->get_nominal_glyphs (count,
				   first_unicode, unicode_stride,
				   first_glyph, glyph_stride);
}

/**
 * hb_font_get_variation_glyph:
 * @font: #hb_font_t to work upon
 * @unicode: The Unicode code point to query
 * @variation_selector: The  variation-selector code point to query
 * @glyph: (out): The glyph ID retrieved
 *
 * Fetches the glyph ID for a Unicode code point when followed by
 * by the specified variation-selector code point, in the specified
 * font.
 *
 * Return value: `true` if data found, `false` otherwise
 *
 * Since: 1.2.3
 **/
hb_bool_t
hb_font_get_variation_glyph (hb_font_t      *font,
			     hb_codepoint_t  unicode,
			     hb_codepoint_t  variation_selector,
			     hb_codepoint_t *glyph)
{
  return font->get_variation_glyph (unicode, variation_selector, glyph);
}

/**
 * hb_font_get_glyph_h_advance:
 * @font: #hb_font_t to work upon
 * @glyph: The glyph ID to query
 *
 * Fetches the advance for a glyph ID in the specified font,
 * for horizontal text segments.
 *
 * Return value: The advance of @glyph within @font
 *
 * Since: 0.9.2
 **/
hb_position_t
hb_font_get_glyph_h_advance (hb_font_t      *font,
			     hb_codepoint_t  glyph)
{
  return font->get_glyph_h_advance (glyph);
}

/**
 * hb_font_get_glyph_v_advance:
 * @font: #hb_font_t to work upon
 * @glyph: The glyph ID to query
 *
 * Fetches the advance for a glyph ID in the specified font,
 * for vertical text segments.
 *
 * Return value: The advance of @glyph within @font
 *
 * Since: 0.9.2
 **/
hb_position_t
hb_font_get_glyph_v_advance (hb_font_t      *font,
			     hb_codepoint_t  glyph)
{
  return font->get_glyph_v_advance (glyph);
}

/**
 * hb_font_get_glyph_h_advances:
 * @font: #hb_font_t to work upon
 * @count: The number of glyph IDs in the sequence queried
 * @first_glyph: The first glyph ID to query
 * @glyph_stride: The stride between successive glyph IDs
 * @first_advance: (out): The first advance retrieved
 * @advance_stride: The stride between successive advances
 *
 * Fetches the advances for a sequence of glyph IDs in the specified
 * font, for horizontal text segments.
 *
 * Since: 1.8.6
 **/
void
hb_font_get_glyph_h_advances (hb_font_t*            font,
			      unsigned int          count,
			      const hb_codepoint_t *first_glyph,
			      unsigned              glyph_stride,
			      hb_position_t        *first_advance,
			      unsigned              advance_stride)
{
  font->get_glyph_h_advances (count, first_glyph, glyph_stride, first_advance, advance_stride);
}
/**
 * hb_font_get_glyph_v_advances:
 * @font: #hb_font_t to work upon
 * @count: The number of glyph IDs in the sequence queried
 * @first_glyph: The first glyph ID to query
 * @glyph_stride: The stride between successive glyph IDs
 * @first_advance: (out): The first advance retrieved
 * @advance_stride: (out): The stride between successive advances
 *
 * Fetches the advances for a sequence of glyph IDs in the specified
 * font, for vertical text segments.
 *
 * Since: 1.8.6
 **/
void
hb_font_get_glyph_v_advances (hb_font_t*            font,
			      unsigned int          count,
			      const hb_codepoint_t *first_glyph,
			      unsigned              glyph_stride,
			      hb_position_t        *first_advance,
			      unsigned              advance_stride)
{
  font->get_glyph_v_advances (count, first_glyph, glyph_stride, first_advance, advance_stride);
}

/**
 * hb_font_get_glyph_h_origin:
 * @font: #hb_font_t to work upon
 * @glyph: The glyph ID to query
 * @x: (out): The X coordinate of the origin
 * @y: (out): The Y coordinate of the origin
 *
 * Fetches the (X,Y) coordinates of the origin for a glyph ID
 * in the specified font, for horizontal text segments.
 *
 * Return value: `true` if data found, `false` otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_font_get_glyph_h_origin (hb_font_t      *font,
			    hb_codepoint_t  glyph,
			    hb_position_t  *x,
			    hb_position_t  *y)
{
  return font->get_glyph_h_origin (glyph, x, y);
}

/**
 * hb_font_get_glyph_v_origin:
 * @font: #hb_font_t to work upon
 * @glyph: The glyph ID to query
 * @x: (out): The X coordinate of the origin
 * @y: (out): The Y coordinate of the origin
 *
 * Fetches the (X,Y) coordinates of the origin for a glyph ID
 * in the specified font, for vertical text segments.
 *
 * Return value: `true` if data found, `false` otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_font_get_glyph_v_origin (hb_font_t      *font,
			    hb_codepoint_t  glyph,
			    hb_position_t  *x,
			    hb_position_t  *y)
{
  return font->get_glyph_v_origin (glyph, x, y);
}

/**
 * hb_font_get_glyph_h_kerning:
 * @font: #hb_font_t to work upon
 * @left_glyph: The glyph ID of the left glyph in the glyph pair
 * @right_glyph: The glyph ID of the right glyph in the glyph pair
 *
 * Fetches the kerning-adjustment value for a glyph-pair in
 * the specified font, for horizontal text segments.
 *
 * <note>It handles legacy kerning only (as returned by the corresponding
 * #hb_font_funcs_t function).</note>
 *
 * Return value: The kerning adjustment value
 *
 * Since: 0.9.2
 **/
hb_position_t
hb_font_get_glyph_h_kerning (hb_font_t      *font,
			     hb_codepoint_t  left_glyph,
			     hb_codepoint_t  right_glyph)
{
  return font->get_glyph_h_kerning (left_glyph, right_glyph);
}

#ifndef HB_DISABLE_DEPRECATED
/**
 * hb_font_get_glyph_v_kerning:
 * @font: #hb_font_t to work upon
 * @top_glyph: The glyph ID of the top glyph in the glyph pair
 * @bottom_glyph: The glyph ID of the bottom glyph in the glyph pair
 *
 * Fetches the kerning-adjustment value for a glyph-pair in
 * the specified font, for vertical text segments.
 *
 * <note>It handles legacy kerning only (as returned by the corresponding
 * #hb_font_funcs_t function).</note>
 *
 * Return value: The kerning adjustment value
 *
 * Since: 0.9.2
 * Deprecated: 2.0.0
 **/
hb_position_t
hb_font_get_glyph_v_kerning (hb_font_t      *font,
			     hb_codepoint_t  top_glyph,
			     hb_codepoint_t  bottom_glyph)
{
  return font->get_glyph_v_kerning (top_glyph, bottom_glyph);
}
#endif

/**
 * hb_font_get_glyph_extents:
 * @font: #hb_font_t to work upon
 * @glyph: The glyph ID to query
 * @extents: (out): The #hb_glyph_extents_t retrieved
 *
 * Fetches the #hb_glyph_extents_t data for a glyph ID
 * in the specified font.
 *
 * Return value: `true` if data found, `false` otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_font_get_glyph_extents (hb_font_t          *font,
			   hb_codepoint_t      glyph,
			   hb_glyph_extents_t *extents)
{
  return font->get_glyph_extents (glyph, extents);
}

/**
 * hb_font_get_glyph_contour_point:
 * @font: #hb_font_t to work upon
 * @glyph: The glyph ID to query
 * @point_index: The contour-point index to query
 * @x: (out): The X value retrieved for the contour point
 * @y: (out): The Y value retrieved for the contour point
 *
 * Fetches the (x,y) coordinates of a specified contour-point index
 * in the specified glyph, within the specified font.
 *
 * Return value: `true` if data found, `false` otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_font_get_glyph_contour_point (hb_font_t      *font,
				 hb_codepoint_t  glyph,
				 unsigned int    point_index,
				 hb_position_t  *x,
				 hb_position_t  *y)
{
  return font->get_glyph_contour_point (glyph, point_index, x, y);
}

/**
 * hb_font_get_glyph_name:
 * @font: #hb_font_t to work upon
 * @glyph: The glyph ID to query
 * @name: (out) (array length=size): Name string retrieved for the glyph ID
 * @size: Length of the glyph-name string retrieved
 *
 * Fetches the glyph-name string for a glyph ID in the specified @font.
 *
 * According to the OpenType specification, glyph names are limited to 63
 * characters and can only contain (a subset of) ASCII.
 *
 * Return value: `true` if data found, `false` otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_font_get_glyph_name (hb_font_t      *font,
			hb_codepoint_t  glyph,
			char           *name,
			unsigned int    size)
{
  return font->get_glyph_name (glyph, name, size);
}

/**
 * hb_font_get_glyph_from_name:
 * @font: #hb_font_t to work upon
 * @name: (array length=len): The name string to query
 * @len: The length of the name queried
 * @glyph: (out): The glyph ID retrieved
 *
 * Fetches the glyph ID that corresponds to a name string in the specified @font.
 *
 * <note>Note: @len == -1 means the name string is null-terminated.</note>
 *
 * Return value: `true` if data found, `false` otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_font_get_glyph_from_name (hb_font_t      *font,
			     const char     *name,
			     int             len, /* -1 means nul-terminated */
			     hb_codepoint_t *glyph)
{
  return font->get_glyph_from_name (name, len, glyph);
}

#ifndef HB_DISABLE_DEPRECATED
/**
 * hb_font_get_glyph_shape:
 * @font: #hb_font_t to work upon
 * @glyph: The glyph ID
 * @dfuncs: #hb_draw_funcs_t to draw to
 * @draw_data: User data to pass to draw callbacks
 *
 * Fetches the glyph shape that corresponds to a glyph in the specified @font.
 * The shape is returned by way of calls to the callbacks of the @dfuncs
 * objects, with @draw_data passed to them.
 *
 * Since: 4.0.0
 * Deprecated: 7.0.0: Use hb_font_draw_glyph() instead
 */
void
hb_font_get_glyph_shape (hb_font_t *font,
		         hb_codepoint_t glyph,
		         hb_draw_funcs_t *dfuncs, void *draw_data)
{
  hb_font_draw_glyph (font, glyph, dfuncs, draw_data);
}
#endif

/**
 * hb_font_draw_glyph:
 * @font: #hb_font_t to work upon
 * @glyph: The glyph ID
 * @dfuncs: #hb_draw_funcs_t to draw to
 * @draw_data: User data to pass to draw callbacks
 *
 * Draws the outline that corresponds to a glyph in the specified @font.
 *
 * The outline is returned by way of calls to the callbacks of the @dfuncs
 * objects, with @draw_data passed to them.
 *
 * Since: 7.0.0
 **/
void
hb_font_draw_glyph (hb_font_t *font,
			 hb_codepoint_t glyph,
			 hb_draw_funcs_t *dfuncs, void *draw_data)
{
  font->draw_glyph (glyph, dfuncs, draw_data);
}

/**
 * hb_font_paint_glyph:
 * @font: #hb_font_t to work upon
 * @glyph: The glyph ID
 * @pfuncs: #hb_paint_funcs_t to paint with
 * @paint_data: User data to pass to paint callbacks
 * @palette_index: The index of the font's color palette to use
 * @foreground: The foreground color, unpremultipled
 *
 * Paints the glyph.
 *
 * The painting instructions are returned by way of calls to
 * the callbacks of the @funcs object, with @paint_data passed
 * to them.
 *
 * If the font has color palettes (see hb_ot_color_has_palettes()),
 * then @palette_index selects the palette to use. If the font only
 * has one palette, this will be 0.
 *
 * Since: 7.0.0
 */
void
hb_font_paint_glyph (hb_font_t *font,
                     hb_codepoint_t glyph,
                     hb_paint_funcs_t *pfuncs, void *paint_data,
                     unsigned int palette_index,
                     hb_color_t foreground)
{
  font->paint_glyph (glyph, pfuncs, paint_data, palette_index, foreground);
}

/* A bit higher-level, and with fallback */

/**
 * hb_font_get_extents_for_direction:
 * @font: #hb_font_t to work upon
 * @direction: The direction of the text segment
 * @extents: (out): The #hb_font_extents_t retrieved
 *
 * Fetches the extents for a font in a text segment of the
 * specified direction.
 *
 * Calls the appropriate direction-specific variant (horizontal
 * or vertical) depending on the value of @direction.
 *
 * Since: 1.1.3
 **/
void
hb_font_get_extents_for_direction (hb_font_t         *font,
				   hb_direction_t     direction,
				   hb_font_extents_t *extents)
{
  font->get_extents_for_direction (direction, extents);
}
/**
 * hb_font_get_glyph_advance_for_direction:
 * @font: #hb_font_t to work upon
 * @glyph: The glyph ID to query
 * @direction: The direction of the text segment
 * @x: (out): The horizontal advance retrieved
 * @y: (out):  The vertical advance retrieved
 *
 * Fetches the advance for a glyph ID from the specified font,
 * in a text segment of the specified direction.
 *
 * Calls the appropriate direction-specific variant (horizontal
 * or vertical) depending on the value of @direction.
 *
 * Since: 0.9.2
 **/
void
hb_font_get_glyph_advance_for_direction (hb_font_t      *font,
					 hb_codepoint_t  glyph,
					 hb_direction_t  direction,
					 hb_position_t  *x,
					 hb_position_t  *y)
{
  font->get_glyph_advance_for_direction (glyph, direction, x, y);
}
/**
 * hb_font_get_glyph_advances_for_direction:
 * @font: #hb_font_t to work upon
 * @direction: The direction of the text segment
 * @count: The number of glyph IDs in the sequence queried
 * @first_glyph: The first glyph ID to query
 * @glyph_stride: The stride between successive glyph IDs
 * @first_advance: (out): The first advance retrieved
 * @advance_stride: (out): The stride between successive advances
 *
 * Fetches the advances for a sequence of glyph IDs in the specified
 * font, in a text segment of the specified direction.
 *
 * Calls the appropriate direction-specific variant (horizontal
 * or vertical) depending on the value of @direction.
 *
 * Since: 1.8.6
 **/
HB_EXTERN void
hb_font_get_glyph_advances_for_direction (hb_font_t*            font,
					  hb_direction_t        direction,
					  unsigned int          count,
					  const hb_codepoint_t *first_glyph,
					  unsigned              glyph_stride,
					  hb_position_t        *first_advance,
					  unsigned              advance_stride)
{
  font->get_glyph_advances_for_direction (direction, count, first_glyph, glyph_stride, first_advance, advance_stride);
}

/**
 * hb_font_get_glyph_origin_for_direction:
 * @font: #hb_font_t to work upon
 * @glyph: The glyph ID to query
 * @direction: The direction of the text segment
 * @x: (out): The X coordinate retrieved for the origin
 * @y: (out): The Y coordinate retrieved for the origin
 *
 * Fetches the (X,Y) coordinates of the origin for a glyph in
 * the specified font.
 *
 * Calls the appropriate direction-specific variant (horizontal
 * or vertical) depending on the value of @direction.
 *
 * Since: 0.9.2
 **/
void
hb_font_get_glyph_origin_for_direction (hb_font_t      *font,
					hb_codepoint_t  glyph,
					hb_direction_t  direction,
					hb_position_t  *x,
					hb_position_t  *y)
{
  return font->get_glyph_origin_for_direction (glyph, direction, x, y);
}

/**
 * hb_font_add_glyph_origin_for_direction:
 * @font: #hb_font_t to work upon
 * @glyph: The glyph ID to query
 * @direction: The direction of the text segment
 * @x: (inout): Input = The original X coordinate
 *     Output = The X coordinate plus the X-coordinate of the origin
 * @y: (inout): Input = The original Y coordinate
 *     Output = The Y coordinate plus the Y-coordinate of the origin
 *
 * Adds the origin coordinates to an (X,Y) point coordinate, in
 * the specified glyph ID in the specified font.
 *
 * Calls the appropriate direction-specific variant (horizontal
 * or vertical) depending on the value of @direction.
 *
 * Since: 0.9.2
 **/
void
hb_font_add_glyph_origin_for_direction (hb_font_t      *font,
					hb_codepoint_t  glyph,
					hb_direction_t  direction,
					hb_position_t  *x,
					hb_position_t  *y)
{
  return font->add_glyph_origin_for_direction (glyph, direction, x, y);
}

/**
 * hb_font_subtract_glyph_origin_for_direction:
 * @font: #hb_font_t to work upon
 * @glyph: The glyph ID to query
 * @direction: The direction of the text segment
 * @x: (inout): Input = The original X coordinate
 *     Output = The X coordinate minus the X-coordinate of the origin
 * @y: (inout): Input = The original Y coordinate
 *     Output = The Y coordinate minus the Y-coordinate of the origin
 *
 * Subtracts the origin coordinates from an (X,Y) point coordinate,
 * in the specified glyph ID in the specified font.
 *
 * Calls the appropriate direction-specific variant (horizontal
 * or vertical) depending on the value of @direction.
 *
 * Since: 0.9.2
 **/
void
hb_font_subtract_glyph_origin_for_direction (hb_font_t      *font,
					     hb_codepoint_t  glyph,
					     hb_direction_t  direction,
					     hb_position_t  *x,
					     hb_position_t  *y)
{
  return font->subtract_glyph_origin_for_direction (glyph, direction, x, y);
}

/**
 * hb_font_get_glyph_kerning_for_direction:
 * @font: #hb_font_t to work upon
 * @first_glyph: The glyph ID of the first glyph in the glyph pair to query
 * @second_glyph: The glyph ID of the second glyph in the glyph pair to query
 * @direction: The direction of the text segment
 * @x: (out): The horizontal kerning-adjustment value retrieved
 * @y: (out): The vertical kerning-adjustment value retrieved
 *
 * Fetches the kerning-adjustment value for a glyph-pair in the specified font.
 *
 * Calls the appropriate direction-specific variant (horizontal
 * or vertical) depending on the value of @direction.
 *
 * Since: 0.9.2
 **/
void
hb_font_get_glyph_kerning_for_direction (hb_font_t      *font,
					 hb_codepoint_t  first_glyph,
					 hb_codepoint_t  second_glyph,
					 hb_direction_t  direction,
					 hb_position_t  *x,
					 hb_position_t  *y)
{
  return font->get_glyph_kerning_for_direction (first_glyph, second_glyph, direction, x, y);
}

/**
 * hb_font_get_glyph_extents_for_origin:
 * @font: #hb_font_t to work upon
 * @glyph: The glyph ID to query
 * @direction: The direction of the text segment
 * @extents: (out): The #hb_glyph_extents_t retrieved
 *
 * Fetches the #hb_glyph_extents_t data for a glyph ID
 * in the specified font, with respect to the origin in
 * a text segment in the specified direction.
 *
 * Calls the appropriate direction-specific variant (horizontal
 * or vertical) depending on the value of @direction.
 *
 * Return value: `true` if data found, `false` otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_font_get_glyph_extents_for_origin (hb_font_t          *font,
				      hb_codepoint_t      glyph,
				      hb_direction_t      direction,
				      hb_glyph_extents_t *extents)
{
  return font->get_glyph_extents_for_origin (glyph, direction, extents);
}

/**
 * hb_font_get_glyph_contour_point_for_origin:
 * @font: #hb_font_t to work upon
 * @glyph: The glyph ID to query
 * @point_index: The contour-point index to query
 * @direction: The direction of the text segment
 * @x: (out): The X value retrieved for the contour point
 * @y: (out): The Y value retrieved for the contour point
 *
 * Fetches the (X,Y) coordinates of a specified contour-point index
 * in the specified glyph ID in the specified font, with respect
 * to the origin in a text segment in the specified direction.
 *
 * Calls the appropriate direction-specific variant (horizontal
 * or vertical) depending on the value of @direction.
 *
 * Return value: `true` if data found, `false` otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_font_get_glyph_contour_point_for_origin (hb_font_t      *font,
					    hb_codepoint_t  glyph,
					    unsigned int    point_index,
					    hb_direction_t  direction,
					    hb_position_t  *x,
					    hb_position_t  *y)
{
  return font->get_glyph_contour_point_for_origin (glyph, point_index, direction, x, y);
}

/**
 * hb_font_glyph_to_string:
 * @font: #hb_font_t to work upon
 * @glyph: The glyph ID to query
 * @s: (out) (array length=size): The string containing the glyph name
 * @size: Length of string @s
 *
 * Fetches the name of the specified glyph ID in @font and returns
 * it in string @s.
 *
 * If the glyph ID has no name in @font, a string of the form `gidDDD` is
 * generated, with `DDD` being the glyph ID.
 *
 * According to the OpenType specification, glyph names are limited to 63
 * characters and can only contain (a subset of) ASCII.
 *
 * Since: 0.9.2
 **/
void
hb_font_glyph_to_string (hb_font_t      *font,
			 hb_codepoint_t  glyph,
			 char           *s,
			 unsigned int    size)
{
  font->glyph_to_string (glyph, s, size);
}

/**
 * hb_font_glyph_from_string:
 * @font: #hb_font_t to work upon
 * @s: (array length=len) (element-type uint8_t): string to query
 * @len: The length of the string @s
 * @glyph: (out): The glyph ID corresponding to the string requested
 *
 * Fetches the glyph ID from @font that matches the specified string.
 * Strings of the format `gidDDD` or `uniUUUU` are parsed automatically.
 *
 * <note>Note: @len == -1 means the string is null-terminated.</note>
 *
 * Return value: `true` if data found, `false` otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_font_glyph_from_string (hb_font_t      *font,
			   const char     *s,
			   int             len,
			   hb_codepoint_t *glyph)
{
  return font->glyph_from_string (s, len, glyph);
}


/*
 * hb_font_t
 */

DEFINE_NULL_INSTANCE (hb_font_t) =
{
  HB_OBJECT_HEADER_STATIC,

  0, /* serial */
  0, /* serial_coords */

  nullptr, /* parent */
  const_cast<hb_face_t *> (&_hb_Null_hb_face_t),

  1000, /* x_scale */
  1000, /* y_scale */
  0.f, /* x_embolden */
  0.f, /* y_embolden */
  true, /* embolden_in_place */
  0, /* x_strength */
  0, /* y_strength */
  0.f, /* slant */
  0.f, /* slant_xy; */
  1.f, /* x_multf */
  1.f, /* y_multf */
  1<<16, /* x_mult */
  1<<16, /* y_mult */

  0, /* x_ppem */
  0, /* y_ppem */
  0, /* ptem */

  HB_FONT_NO_VAR_NAMED_INSTANCE, /* instance_index */
  0, /* num_coords */
  nullptr, /* coords */
  nullptr, /* design_coords */

  const_cast<hb_font_funcs_t *> (&_hb_Null_hb_font_funcs_t),

  /* Zero for the rest is fine. */
};


static hb_font_t *
_hb_font_create (hb_face_t *face)
{
  hb_font_t *font;

  if (unlikely (!face))
    face = hb_face_get_empty ();

  if (!(font = hb_object_create<hb_font_t> ()))
    return hb_font_get_empty ();

  hb_face_make_immutable (face);
  font->parent = hb_font_get_empty ();
  font->face = hb_face_reference (face);
  font->klass = hb_font_funcs_get_empty ();
  font->data.init0 (font);
  font->x_scale = font->y_scale = face->get_upem ();
  font->embolden_in_place = true;
  font->x_multf = font->y_multf = 1.f;
  font->x_mult = font->y_mult = 1 << 16;
  font->instance_index = HB_FONT_NO_VAR_NAMED_INSTANCE;

  return font;
}

/**
 * hb_font_create:
 * @face: a face.
 *
 * Constructs a new font object from the specified face.
 *
 * <note>Note: If @face's index value (as passed to hb_face_create()
 * has non-zero top 16-bits, those bits minus one are passed to
 * hb_font_set_var_named_instance(), effectively loading a named-instance
 * of a variable font, instead of the default-instance.  This allows
 * specifying which named-instance to load by default when creating the
 * face.</note>
 *
 * Return value: (transfer full): The new font object
 *
 * Since: 0.9.2
 **/
hb_font_t *
hb_font_create (hb_face_t *face)
{
  hb_font_t *font = _hb_font_create (face);

#ifndef HB_NO_OT_FONT
  /* Install our in-house, very lightweight, funcs. */
  hb_ot_font_set_funcs (font);
#endif

#ifndef HB_NO_VAR
  if (face && face->index >> 16)
    hb_font_set_var_named_instance (font, (face->index >> 16) - 1);
#endif

  return font;
}

static void
_hb_font_adopt_var_coords (hb_font_t *font,
			   int *coords, /* 2.14 normalized */
			   float *design_coords,
			   unsigned int coords_length)
{
  hb_free (font->coords);
  hb_free (font->design_coords);

  font->coords = coords;
  font->design_coords = design_coords;
  font->num_coords = coords_length;

  font->mults_changed (); // Easiest to call this to drop cached data
}

/**
 * hb_font_create_sub_font:
 * @parent: The parent font object
 *
 * Constructs a sub-font font object from the specified @parent font,
 * replicating the parent's properties.
 *
 * Return value: (transfer full): The new sub-font font object
 *
 * Since: 0.9.2
 **/
hb_font_t *
hb_font_create_sub_font (hb_font_t *parent)
{
  if (unlikely (!parent))
    parent = hb_font_get_empty ();

  hb_font_t *font = _hb_font_create (parent->face);

  if (unlikely (hb_object_is_immutable (font)))
    return font;

  font->parent = hb_font_reference (parent);

  font->x_scale = parent->x_scale;
  font->y_scale = parent->y_scale;
  font->x_embolden = parent->x_embolden;
  font->y_embolden = parent->y_embolden;
  font->embolden_in_place = parent->embolden_in_place;
  font->slant = parent->slant;
  font->x_ppem = parent->x_ppem;
  font->y_ppem = parent->y_ppem;
  font->ptem = parent->ptem;

  unsigned int num_coords = parent->num_coords;
  if (num_coords)
  {
    int *coords = (int *) hb_calloc (num_coords, sizeof (parent->coords[0]));
    float *design_coords = (float *) hb_calloc (num_coords, sizeof (parent->design_coords[0]));
    if (likely (coords && design_coords))
    {
      hb_memcpy (coords, parent->coords, num_coords * sizeof (parent->coords[0]));
      hb_memcpy (design_coords, parent->design_coords, num_coords * sizeof (parent->design_coords[0]));
      _hb_font_adopt_var_coords (font, coords, design_coords, num_coords);
    }
    else
    {
      hb_free (coords);
      hb_free (design_coords);
    }
  }

  font->mults_changed ();

  return font;
}

/**
 * hb_font_get_empty:
 *
 * Fetches the empty font object.
 *
 * Return value: (transfer full): The empty font object
 *
 * Since: 0.9.2
 **/
hb_font_t *
hb_font_get_empty ()
{
  return const_cast<hb_font_t *> (&Null (hb_font_t));
}

/**
 * hb_font_reference: (skip)
 * @font: #hb_font_t to work upon
 *
 * Increases the reference count on the given font object.
 *
 * Return value: (transfer full): The @font object
 *
 * Since: 0.9.2
 **/
hb_font_t *
hb_font_reference (hb_font_t *font)
{
  return hb_object_reference (font);
}

/**
 * hb_font_destroy: (skip)
 * @font: #hb_font_t to work upon
 *
 * Decreases the reference count on the given font object. When the
 * reference count reaches zero, the font is destroyed,
 * freeing all memory.
 *
 * Since: 0.9.2
 **/
void
hb_font_destroy (hb_font_t *font)
{
  if (!hb_object_destroy (font)) return;

  font->data.fini ();

  if (font->destroy)
    font->destroy (font->user_data);

  hb_font_destroy (font->parent);
  hb_face_destroy (font->face);
  hb_font_funcs_destroy (font->klass);

  hb_free (font->coords);
  hb_free (font->design_coords);

  hb_free (font);
}

/**
 * hb_font_set_user_data: (skip)
 * @font: #hb_font_t to work upon
 * @key: The user-data key
 * @data: A pointer to the user data
 * @destroy: (nullable): A callback to call when @data is not needed anymore
 * @replace: Whether to replace an existing data with the same key
 *
 * Attaches a user-data key/data pair to the specified font object.
 *
 * Return value: `true` if success, `false` otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_font_set_user_data (hb_font_t          *font,
		       hb_user_data_key_t *key,
		       void *              data,
		       hb_destroy_func_t   destroy /* May be NULL. */,
		       hb_bool_t           replace)
{
  if (!hb_object_is_immutable (font))
    font->serial++;

  return hb_object_set_user_data (font, key, data, destroy, replace);
}

/**
 * hb_font_get_user_data: (skip)
 * @font: #hb_font_t to work upon
 * @key: The user-data key to query
 *
 * Fetches the user-data object associated with the specified key,
 * attached to the specified font object.
 *
 * Return value: (transfer none): Pointer to the user data
 *
 * Since: 0.9.2
 **/
void *
hb_font_get_user_data (const hb_font_t    *font,
		       hb_user_data_key_t *key)
{
  return hb_object_get_user_data (font, key);
}

/**
 * hb_font_make_immutable:
 * @font: #hb_font_t to work upon
 *
 * Makes @font immutable.
 *
 * Since: 0.9.2
 **/
void
hb_font_make_immutable (hb_font_t *font)
{
  if (hb_object_is_immutable (font))
    return;

  if (font->parent)
    hb_font_make_immutable (font->parent);

  hb_object_make_immutable (font);
}

/**
 * hb_font_is_immutable:
 * @font: #hb_font_t to work upon
 *
 * Tests whether a font object is immutable.
 *
 * Return value: `true` if @font is immutable, `false` otherwise
 *
 * Since: 0.9.2
 **/
hb_bool_t
hb_font_is_immutable (hb_font_t *font)
{
  return hb_object_is_immutable (font);
}

/**
 * hb_font_get_serial:
 * @font: #hb_font_t to work upon
 *
 * Returns the internal serial number of the font. The serial
 * number is increased every time a setting on the font is
 * changed, using a setter function.
 *
 * Return value: serial number
 *
 * Since: 4.4.0
 **/
unsigned int
hb_font_get_serial (hb_font_t *font)
{
  return font->serial;
}

/**
 * hb_font_changed:
 * @font: #hb_font_t to work upon
 *
 * Notifies the @font that underlying font data has changed.
 * This has the effect of increasing the serial as returned
 * by hb_font_get_serial(), which invalidates internal caches.
 *
 * Since: 4.4.0
 **/
void
hb_font_changed (hb_font_t *font)
{
  if (hb_object_is_immutable (font))
    return;

  font->serial++;

  font->mults_changed ();
}

/**
 * hb_font_set_parent:
 * @font: #hb_font_t to work upon
 * @parent: The parent font object to assign
 *
 * Sets the parent font of @font.
 *
 * Since: 1.0.5
 **/
void
hb_font_set_parent (hb_font_t *font,
		    hb_font_t *parent)
{
  if (hb_object_is_immutable (font))
    return;

  if (parent == font->parent)
    return;

  font->serial++;

  if (!parent)
    parent = hb_font_get_empty ();

  hb_font_t *old = font->parent;

  font->parent = hb_font_reference (parent);

  hb_font_destroy (old);
}

/**
 * hb_font_get_parent:
 * @font: #hb_font_t to work upon
 *
 * Fetches the parent font of @font.
 *
 * Return value: (transfer none): The parent font object
 *
 * Since: 0.9.2
 **/
hb_font_t *
hb_font_get_parent (hb_font_t *font)
{
  return font->parent;
}

/**
 * hb_font_set_face:
 * @font: #hb_font_t to work upon
 * @face: The #hb_face_t to assign
 *
 * Sets @face as the font-face value of @font.
 *
 * Since: 1.4.3
 **/
void
hb_font_set_face (hb_font_t *font,
		  hb_face_t *face)
{
  if (hb_object_is_immutable (font))
    return;

  if (face == font->face)
    return;

  font->serial++;

  if (unlikely (!face))
    face = hb_face_get_empty ();

  hb_face_t *old = font->face;

  hb_face_make_immutable (face);
  font->face = hb_face_reference (face);
  font->mults_changed ();

  hb_face_destroy (old);
}

/**
 * hb_font_get_face:
 * @font: #hb_font_t to work upon
 *
 * Fetches the face associated with the specified font object.
 *
 * Return value: (transfer none): The #hb_face_t value
 *
 * Since: 0.9.2
 **/
hb_face_t *
hb_font_get_face (hb_font_t *font)
{
  return font->face;
}


/**
 * hb_font_set_funcs:
 * @font: #hb_font_t to work upon
 * @klass: (closure font_data) (destroy destroy) (scope notified): The font-functions structure.
 * @font_data: Data to attach to @font
 * @destroy: (nullable): The function to call when @font_data is not needed anymore
 *
 * Replaces the font-functions structure attached to a font, updating
 * the font's user-data with @font-data and the @destroy callback.
 *
 * Since: 0.9.2
 **/
void
hb_font_set_funcs (hb_font_t         *font,
		   hb_font_funcs_t   *klass,
		   void              *font_data,
		   hb_destroy_func_t  destroy /* May be NULL. */)
{
  if (hb_object_is_immutable (font))
  {
    if (destroy)
      destroy (font_data);
    return;
  }

  font->serial++;

  if (font->destroy)
    font->destroy (font->user_data);

  if (!klass)
    klass = hb_font_funcs_get_empty ();

  hb_font_funcs_reference (klass);
  hb_font_funcs_destroy (font->klass);
  font->klass = klass;
  font->user_data = font_data;
  font->destroy = destroy;
}

/**
 * hb_font_set_funcs_data:
 * @font: #hb_font_t to work upon
 * @font_data: (destroy destroy) (scope notified): Data to attach to @font
 * @destroy: (nullable): The function to call when @font_data is not needed anymore
 *
 * Replaces the user data attached to a font, updating the font's
 * @destroy callback.
 *
 * Since: 0.9.2
 **/
void
hb_font_set_funcs_data (hb_font_t         *font,
		        void              *font_data,
		        hb_destroy_func_t  destroy /* May be NULL. */)
{
  /* Destroy user_data? */
  if (hb_object_is_immutable (font))
  {
    if (destroy)
      destroy (font_data);
    return;
  }

  font->serial++;

  if (font->destroy)
    font->destroy (font->user_data);

  font->user_data = font_data;
  font->destroy = destroy;
}


/**
 * hb_font_set_scale:
 * @font: #hb_font_t to work upon
 * @x_scale: Horizontal scale value to assign
 * @y_scale: Vertical scale value to assign
 *
 * Sets the horizontal and vertical scale of a font.
 *
 * The font scale is a number related to, but not the same as,
 * font size. Typically the client establishes a scale factor
 * to be used between the two. For example, 64, or 256, which
 * would be the fractional-precision part of the font scale.
 * This is necessary because #hb_position_t values are integer
 * types and you need to leave room for fractional values
 * in there.
 *
 * For example, to set the font size to 20, with 64
 * levels of fractional precision you would call
 * `hb_font_set_scale(font, 20 * 64, 20 * 64)`.
 *
 * In the example above, even what font size 20 means is up to
 * you. It might be 20 pixels, or 20 points, or 20 millimeters.
 * HarfBuzz does not care about that.  You can set the point
 * size of the font using hb_font_set_ptem(), and the pixel
 * size using hb_font_set_ppem().
 *
 * The choice of scale is yours but needs to be consistent between
 * what you set here, and what you expect out of #hb_position_t
 * as well has draw / paint API output values.
 *
 * Fonts default to a scale equal to the UPEM value of their face.
 * A font with this setting is sometimes called an "unscaled" font.
 *
 * Since: 0.9.2
 **/
void
hb_font_set_scale (hb_font_t *font,
		   int        x_scale,
		   int        y_scale)
{
  if (hb_object_is_immutable (font))
    return;

  if (font->x_scale == x_scale && font->y_scale == y_scale)
    return;

  font->serial++;

  font->x_scale = x_scale;
  font->y_scale = y_scale;
  font->mults_changed ();
}

/**
 * hb_font_get_scale:
 * @font: #hb_font_t to work upon
 * @x_scale: (out): Horizontal scale value
 * @y_scale: (out): Vertical scale value
 *
 * Fetches the horizontal and vertical scale of a font.
 *
 * Since: 0.9.2
 **/
void
hb_font_get_scale (hb_font_t *font,
		   int       *x_scale,
		   int       *y_scale)
{
  if (x_scale) *x_scale = font->x_scale;
  if (y_scale) *y_scale = font->y_scale;
}

/**
 * hb_font_set_ppem:
 * @font: #hb_font_t to work upon
 * @x_ppem: Horizontal ppem value to assign
 * @y_ppem: Vertical ppem value to assign
 *
 * Sets the horizontal and vertical pixels-per-em (PPEM) of a font.
 *
 * These values are used for pixel-size-specific adjustment to
 * shaping and draw results, though for the most part they are
 * unused and can be left unset.
 *
 * Since: 0.9.2
 **/
void
hb_font_set_ppem (hb_font_t    *font,
		  unsigned int  x_ppem,
		  unsigned int  y_ppem)
{
  if (hb_object_is_immutable (font))
    return;

  if (font->x_ppem == x_ppem && font->y_ppem == y_ppem)
    return;

  font->serial++;

  font->x_ppem = x_ppem;
  font->y_ppem = y_ppem;
}

/**
 * hb_font_get_ppem:
 * @font: #hb_font_t to work upon
 * @x_ppem: (out): Horizontal ppem value
 * @y_ppem: (out): Vertical ppem value
 *
 * Fetches the horizontal and vertical points-per-em (ppem) of a font.
 *
 * Since: 0.9.2
 **/
void
hb_font_get_ppem (hb_font_t    *font,
		  unsigned int *x_ppem,
		  unsigned int *y_ppem)
{
  if (x_ppem) *x_ppem = font->x_ppem;
  if (y_ppem) *y_ppem = font->y_ppem;
}

/**
 * hb_font_set_ptem:
 * @font: #hb_font_t to work upon
 * @ptem: font size in points.
 *
 * Sets the "point size" of a font. Set to zero to unset.
 * Used in CoreText to implement optical sizing.
 *
 * <note>Note: There are 72 points in an inch.</note>
 *
 * Since: 1.6.0
 **/
void
hb_font_set_ptem (hb_font_t *font,
		  float      ptem)
{
  if (hb_object_is_immutable (font))
    return;

  if (font->ptem == ptem)
    return;

  font->serial++;

  font->ptem = ptem;
}

/**
 * hb_font_get_ptem:
 * @font: #hb_font_t to work upon
 *
 * Fetches the "point size" of a font. Used in CoreText to
 * implement optical sizing.
 *
 * Return value: Point size.  A value of zero means "not set."
 *
 * Since: 1.6.0
 **/
float
hb_font_get_ptem (hb_font_t *font)
{
  return font->ptem;
}

/**
 * hb_font_set_synthetic_bold:
 * @font: #hb_font_t to work upon
 * @x_embolden: the amount to embolden horizontally
 * @y_embolden: the amount to embolden vertically
 * @in_place: whether to embolden glyphs in-place
 *
 * Sets the "synthetic boldness" of a font.
 *
 * Positive values for @x_embolden / @y_embolden make a font
 * bolder, negative values thinner. Typical values are in the
 * 0.01 to 0.05 range. The default value is zero.
 *
 * Synthetic boldness is applied by offsetting the contour
 * points of the glyph shape.
 *
 * Synthetic boldness is applied when rendering a glyph via
 * hb_font_draw_glyph().
 *
 * If @in_place is `false`, then glyph advance-widths are also
 * adjusted, otherwise they are not.  The in-place mode is
 * useful for simulating [font grading](https://fonts.google.com/knowledge/glossary/grade).
 *
 *
 * Since: 7.0.0
 **/
void
hb_font_set_synthetic_bold (hb_font_t *font,
			    float x_embolden,
			    float y_embolden,
			    hb_bool_t in_place)
{
  if (hb_object_is_immutable (font))
    return;

  if (font->x_embolden == x_embolden &&
      font->y_embolden == y_embolden &&
      font->embolden_in_place == (bool) in_place)
    return;

  font->serial++;

  font->x_embolden = x_embolden;
  font->y_embolden = y_embolden;
  font->embolden_in_place = in_place;
  font->mults_changed ();
}

/**
 * hb_font_get_synthetic_bold:
 * @font: #hb_font_t to work upon
 * @x_embolden: (out): return location for horizontal value
 * @y_embolden: (out): return location for vertical value
 * @in_place: (out): return location for in-place value
 *
 * Fetches the "synthetic boldness" parameters of a font.
 *
 * Since: 7.0.0
 **/
void
hb_font_get_synthetic_bold (hb_font_t *font,
			    float *x_embolden,
			    float *y_embolden,
			    hb_bool_t *in_place)
{
  if (x_embolden) *x_embolden = font->x_embolden;
  if (y_embolden) *y_embolden = font->y_embolden;
  if (in_place) *in_place = font->embolden_in_place;
}

/**
 * hb_font_set_synthetic_slant:
 * @font: #hb_font_t to work upon
 * @slant: synthetic slant value.
 *
 * Sets the "synthetic slant" of a font.  By default is zero.
 * Synthetic slant is the graphical skew applied to the font
 * at rendering time.
 *
 * HarfBuzz needs to know this value to adjust shaping results,
 * metrics, and style values to match the slanted rendering.
 *
 * <note>Note: The glyph shape fetched via the hb_font_draw_glyph()
 * function is slanted to reflect this value as well.</note>
 *
 * <note>Note: The slant value is a ratio.  For example, a
 * 20% slant would be represented as a 0.2 value.</note>
 *
 * Since: 3.3.0
 **/
HB_EXTERN void
hb_font_set_synthetic_slant (hb_font_t *font, float slant)
{
  if (hb_object_is_immutable (font))
    return;

  if (font->slant == slant)
    return;

  font->serial++;

  font->slant = slant;
  font->mults_changed ();
}

/**
 * hb_font_get_synthetic_slant:
 * @font: #hb_font_t to work upon
 *
 * Fetches the "synthetic slant" of a font.
 *
 * Return value: Synthetic slant.  By default is zero.
 *
 * Since: 3.3.0
 **/
HB_EXTERN float
hb_font_get_synthetic_slant (hb_font_t *font)
{
  return font->slant;
}

#ifndef HB_NO_VAR
/*
 * Variations
 */

/**
 * hb_font_set_variations:
 * @font: #hb_font_t to work upon
 * @variations: (array length=variations_length): Array of variation settings to apply
 * @variations_length: Number of variations to apply
 *
 * Applies a list of font-variation settings to a font.
 *
 * Note that this overrides all existing variations set on @font.
 * Axes not included in @variations will be effectively set to their
 * default values.
 *
 * Since: 1.4.2
 */
void
hb_font_set_variations (hb_font_t            *font,
			const hb_variation_t *variations,
			unsigned int          variations_length)
{
  if (hb_object_is_immutable (font))
    return;

  font->serial_coords = ++font->serial;

  if (!variations_length && font->instance_index == HB_FONT_NO_VAR_NAMED_INSTANCE)
  {
    hb_font_set_var_coords_normalized (font, nullptr, 0);
    return;
  }

  const OT::fvar &fvar = *font->face->table.fvar;
  auto axes = fvar.get_axes ();
  const unsigned coords_length = axes.length;

  int *normalized = coords_length ? (int *) hb_calloc (coords_length, sizeof (int)) : nullptr;
  float *design_coords = coords_length ? (float *) hb_calloc (coords_length, sizeof (float)) : nullptr;

  if (unlikely (coords_length && !(normalized && design_coords)))
  {
    hb_free (normalized);
    hb_free (design_coords);
    return;
  }

  /* Initialize design coords. */
  for (unsigned int i = 0; i < coords_length; i++)
    design_coords[i] = axes[i].get_default ();
  if (font->instance_index != HB_FONT_NO_VAR_NAMED_INSTANCE)
  {
    unsigned count = coords_length;
    /* This may fail if index is out-of-range;
     * That's why we initialize design_coords from fvar above
     * unconditionally. */
    hb_ot_var_named_instance_get_design_coords (font->face, font->instance_index,
						&count, design_coords);
  }

  for (unsigned int i = 0; i < variations_length; i++)
  {
    const auto tag = variations[i].tag;
    const auto v = variations[i].value;
    for (unsigned axis_index = 0; axis_index < coords_length; axis_index++)
      if (axes[axis_index].axisTag == tag)
	design_coords[axis_index] = v;
  }

  hb_ot_var_normalize_coords (font->face, coords_length, design_coords, normalized);
  _hb_font_adopt_var_coords (font, normalized, design_coords, coords_length);
}

/**
 * hb_font_set_variation:
 * @font: #hb_font_t to work upon
 * @tag: The #hb_tag_t tag of the variation-axis name
 * @value: The value of the variation axis
 *
 * Change the value of one variation axis on the font.
 *
 * Note: This function is expensive to be called repeatedly.
 *   If you want to set multiple variation axes at the same time,
 *   use hb_font_set_variations() instead.
 *
 * Since: 7.1.0
 */
void
hb_font_set_variation (hb_font_t *font,
		       hb_tag_t tag,
		       float    value)
{
  if (hb_object_is_immutable (font))
    return;

  font->serial_coords = ++font->serial;

  // TODO Share some of this code with set_variations()

  const OT::fvar &fvar = *font->face->table.fvar;
  auto axes = fvar.get_axes ();
  const unsigned coords_length = axes.length;

  int *normalized = coords_length ? (int *) hb_calloc (coords_length, sizeof (int)) : nullptr;
  float *design_coords = coords_length ? (float *) hb_calloc (coords_length, sizeof (float)) : nullptr;

  if (unlikely (coords_length && !(normalized && design_coords)))
  {
    hb_free (normalized);
    hb_free (design_coords);
    return;
  }

  /* Initialize design coords. */
  if (font->design_coords)
  {
    assert (coords_length == font->num_coords);
    for (unsigned int i = 0; i < coords_length; i++)
      design_coords[i] = font->design_coords[i];
  }
  else
  {
    for (unsigned int i = 0; i < coords_length; i++)
      design_coords[i] = axes[i].get_default ();
    if (font->instance_index != HB_FONT_NO_VAR_NAMED_INSTANCE)
    {
      unsigned count = coords_length;
      /* This may fail if index is out-of-range;
       * That's why we initialize design_coords from fvar above
       * unconditionally. */
      hb_ot_var_named_instance_get_design_coords (font->face, font->instance_index,
						  &count, design_coords);
    }
  }

  for (unsigned axis_index = 0; axis_index < coords_length; axis_index++)
    if (axes[axis_index].axisTag == tag)
      design_coords[axis_index] = value;

  hb_ot_var_normalize_coords (font->face, coords_length, design_coords, normalized);
  _hb_font_adopt_var_coords (font, normalized, design_coords, coords_length);

}

/**
 * hb_font_set_var_coords_design:
 * @font: #hb_font_t to work upon
 * @coords: (array length=coords_length): Array of variation coordinates to apply
 * @coords_length: Number of coordinates to apply
 *
 * Applies a list of variation coordinates (in design-space units)
 * to a font.
 *
 * Note that this overrides all existing variations set on @font.
 * Axes not included in @coords will be effectively set to their
 * default values.
 *
 * Since: 1.4.2
 */
void
hb_font_set_var_coords_design (hb_font_t    *font,
			       const float  *coords,
			       unsigned int  coords_length)
{
  if (hb_object_is_immutable (font))
    return;

  font->serial_coords = ++font->serial;

  int *normalized = coords_length ? (int *) hb_calloc (coords_length, sizeof (int)) : nullptr;
  float *design_coords = coords_length ? (float *) hb_calloc (coords_length, sizeof (float)) : nullptr;

  if (unlikely (coords_length && !(normalized && design_coords)))
  {
    hb_free (normalized);
    hb_free (design_coords);
    return;
  }

  if (coords_length)
    hb_memcpy (design_coords, coords, coords_length * sizeof (font->design_coords[0]));

  hb_ot_var_normalize_coords (font->face, coords_length, coords, normalized);
  _hb_font_adopt_var_coords (font, normalized, design_coords, coords_length);
}

/**
 * hb_font_set_var_named_instance:
 * @font: a font.
 * @instance_index: named instance index.
 *
 * Sets design coords of a font from a named-instance index.
 *
 * Since: 2.6.0
 */
void
hb_font_set_var_named_instance (hb_font_t *font,
				unsigned int instance_index)
{
  if (hb_object_is_immutable (font))
    return;

  if (font->instance_index == instance_index)
    return;

  font->serial_coords = ++font->serial;

  font->instance_index = instance_index;
  hb_font_set_variations (font, nullptr, 0);
}

/**
 * hb_font_get_var_named_instance:
 * @font: a font.
 *
 * Returns the currently-set named-instance index of the font.
 *
 * Return value: Named-instance index or %HB_FONT_NO_VAR_NAMED_INSTANCE.
 *
 * Since: 7.0.0
 **/
unsigned int
hb_font_get_var_named_instance (hb_font_t *font)
{
  return font->instance_index;
}

/**
 * hb_font_set_var_coords_normalized:
 * @font: #hb_font_t to work upon
 * @coords: (array length=coords_length): Array of variation coordinates to apply
 * @coords_length: Number of coordinates to apply
 *
 * Applies a list of variation coordinates (in normalized units)
 * to a font.
 *
 * Note that this overrides all existing variations set on @font.
 * Axes not included in @coords will be effectively set to their
 * default values.
 *
 * <note>Note: Coordinates should be normalized to 2.14.</note>
 *
 * Since: 1.4.2
 */
void
hb_font_set_var_coords_normalized (hb_font_t    *font,
				   const int    *coords, /* 2.14 normalized */
				   unsigned int  coords_length)
{
  if (hb_object_is_immutable (font))
    return;

  font->serial_coords = ++font->serial;

  int *copy = coords_length ? (int *) hb_calloc (coords_length, sizeof (coords[0])) : nullptr;
  int *unmapped = coords_length ? (int *) hb_calloc (coords_length, sizeof (coords[0])) : nullptr;
  float *design_coords = coords_length ? (float *) hb_calloc (coords_length, sizeof (design_coords[0])) : nullptr;

  if (unlikely (coords_length && !(copy && unmapped && design_coords)))
  {
    hb_free (copy);
    hb_free (unmapped);
    hb_free (design_coords);
    return;
  }

  if (coords_length)
  {
    hb_memcpy (copy, coords, coords_length * sizeof (coords[0]));
    hb_memcpy (unmapped, coords, coords_length * sizeof (coords[0]));
  }

  /* Best effort design coords simulation */
  font->face->table.avar->unmap_coords (unmapped, coords_length);
  for (unsigned int i = 0; i < coords_length; ++i)
    design_coords[i] = font->face->table.fvar->unnormalize_axis_value (i, unmapped[i]);
  hb_free (unmapped);

  _hb_font_adopt_var_coords (font, copy, design_coords, coords_length);
}

/**
 * hb_font_get_var_coords_normalized:
 * @font: #hb_font_t to work upon
 * @length: (out): Number of coordinates retrieved
 *
 * Fetches the list of normalized variation coordinates currently
 * set on a font.
 *
 * Note that this returned array may only contain values for some
 * (or none) of the axes; omitted axes effectively have zero values.
 *
 * Return value is valid as long as variation coordinates of the font
 * are not modified.
 *
 * Return value: coordinates array
 *
 * Since: 1.4.2
 */
const int *
hb_font_get_var_coords_normalized (hb_font_t    *font,
				   unsigned int *length)
{
  if (length)
    *length = font->num_coords;

  return font->coords;
}

/**
 * hb_font_get_var_coords_design:
 * @font: #hb_font_t to work upon
 * @length: (out): Number of coordinates retrieved
 *
 * Fetches the list of variation coordinates (in design-space units) currently
 * set on a font.
 *
 * Note that this returned array may only contain values for some
 * (or none) of the axes; omitted axes effectively have their default
 * values.
 *
 * Return value is valid as long as variation coordinates of the font
 * are not modified.
 *
 * Return value: coordinates array
 *
 * Since: 3.3.0
 */
const float *
hb_font_get_var_coords_design (hb_font_t *font,
			       unsigned int *length)
{
  if (length)
    *length = font->num_coords;

  return font->design_coords;
}
#endif

#ifndef HB_DISABLE_DEPRECATED
/*
 * Deprecated get_glyph_func():
 */

struct hb_trampoline_closure_t
{
  void *user_data;
  hb_destroy_func_t destroy;
  unsigned int ref_count;
};

template <typename FuncType>
struct hb_trampoline_t
{
  hb_trampoline_closure_t closure; /* Must be first. */
  FuncType func;
};

template <typename FuncType>
static hb_trampoline_t<FuncType> *
trampoline_create (FuncType           func,
		   void              *user_data,
		   hb_destroy_func_t  destroy)
{
  typedef hb_trampoline_t<FuncType> trampoline_t;

  trampoline_t *trampoline = (trampoline_t *) hb_calloc (1, sizeof (trampoline_t));

  if (unlikely (!trampoline))
    return nullptr;

  trampoline->closure.user_data = user_data;
  trampoline->closure.destroy = destroy;
  trampoline->closure.ref_count = 1;
  trampoline->func = func;

  return trampoline;
}

static void
trampoline_reference (hb_trampoline_closure_t *closure)
{
  closure->ref_count++;
}

static void
trampoline_destroy (void *user_data)
{
  hb_trampoline_closure_t *closure = (hb_trampoline_closure_t *) user_data;

  if (--closure->ref_count)
    return;

  if (closure->destroy)
    closure->destroy (closure->user_data);
  hb_free (closure);
}

typedef hb_trampoline_t<hb_font_get_glyph_func_t> hb_font_get_glyph_trampoline_t;

static hb_bool_t
hb_font_get_nominal_glyph_trampoline (hb_font_t      *font,
				      void           *font_data,
				      hb_codepoint_t  unicode,
				      hb_codepoint_t *glyph,
				      void           *user_data)
{
  hb_font_get_glyph_trampoline_t *trampoline = (hb_font_get_glyph_trampoline_t *) user_data;
  return trampoline->func (font, font_data, unicode, 0, glyph, trampoline->closure.user_data);
}

static hb_bool_t
hb_font_get_variation_glyph_trampoline (hb_font_t      *font,
					void           *font_data,
					hb_codepoint_t  unicode,
					hb_codepoint_t  variation_selector,
					hb_codepoint_t *glyph,
					void           *user_data)
{
  hb_font_get_glyph_trampoline_t *trampoline = (hb_font_get_glyph_trampoline_t *) user_data;
  return trampoline->func (font, font_data, unicode, variation_selector, glyph, trampoline->closure.user_data);
}

/**
 * hb_font_funcs_set_glyph_func:
 * @ffuncs: The font-functions structure
 * @func: (closure user_data) (destroy destroy) (scope notified): callback function
 * @user_data: data to pass to @func
 * @destroy: (nullable): function to call when @user_data is not needed anymore
 *
 * Deprecated.  Use hb_font_funcs_set_nominal_glyph_func() and
 * hb_font_funcs_set_variation_glyph_func() instead.
 *
 * Since: 0.9.2
 * Deprecated: 1.2.3
 **/
void
hb_font_funcs_set_glyph_func (hb_font_funcs_t          *ffuncs,
			      hb_font_get_glyph_func_t  func,
			      void                     *user_data,
			      hb_destroy_func_t         destroy /* May be NULL. */)
{
  if (hb_object_is_immutable (ffuncs))
  {
    if (destroy)
      destroy (user_data);
    return;
  }

  hb_font_get_glyph_trampoline_t *trampoline;

  trampoline = trampoline_create (func, user_data, destroy);
  if (unlikely (!trampoline))
  {
    if (destroy)
      destroy (user_data);
    return;
  }

  /* Since we pass it to two destroying functions. */
  trampoline_reference (&trampoline->closure);

  hb_font_funcs_set_nominal_glyph_func (ffuncs,
					hb_font_get_nominal_glyph_trampoline,
					trampoline,
					trampoline_destroy);

  hb_font_funcs_set_variation_glyph_func (ffuncs,
					  hb_font_get_variation_glyph_trampoline,
					  trampoline,
					  trampoline_destroy);
}
#endif


#ifndef HB_DISABLE_DEPRECATED
void
hb_font_funcs_set_glyph_shape_func (hb_font_funcs_t               *ffuncs,
                                   hb_font_get_glyph_shape_func_t  func,
                                   void                           *user_data,
                                   hb_destroy_func_t               destroy /* May be NULL. */)
{
  hb_font_funcs_set_draw_glyph_func (ffuncs, func, user_data, destroy);
}
#endif
