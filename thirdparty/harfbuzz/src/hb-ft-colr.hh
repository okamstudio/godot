/*
 * Copyright © 2022  Behdad Esfahbod
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
 */

#ifndef HB_FT_COLR_HH
#define HB_FT_COLR_HH

#include "hb.hh"

#include "hb-paint-extents.hh"

#include FT_COLOR_H


static hb_paint_composite_mode_t
_hb_ft_paint_composite_mode (FT_Composite_Mode mode)
{
  switch (mode)
  {
    case FT_COLR_COMPOSITE_CLEAR:          return HB_PAINT_COMPOSITE_MODE_CLEAR;
    case FT_COLR_COMPOSITE_SRC:            return HB_PAINT_COMPOSITE_MODE_SRC;
    case FT_COLR_COMPOSITE_DEST:           return HB_PAINT_COMPOSITE_MODE_DEST;
    case FT_COLR_COMPOSITE_SRC_OVER:       return HB_PAINT_COMPOSITE_MODE_SRC_OVER;
    case FT_COLR_COMPOSITE_DEST_OVER:      return HB_PAINT_COMPOSITE_MODE_DEST_OVER;
    case FT_COLR_COMPOSITE_SRC_IN:         return HB_PAINT_COMPOSITE_MODE_SRC_IN;
    case FT_COLR_COMPOSITE_DEST_IN:        return HB_PAINT_COMPOSITE_MODE_DEST_IN;
    case FT_COLR_COMPOSITE_SRC_OUT:        return HB_PAINT_COMPOSITE_MODE_SRC_OUT;
    case FT_COLR_COMPOSITE_DEST_OUT:       return HB_PAINT_COMPOSITE_MODE_DEST_OUT;
    case FT_COLR_COMPOSITE_SRC_ATOP:       return HB_PAINT_COMPOSITE_MODE_SRC_ATOP;
    case FT_COLR_COMPOSITE_DEST_ATOP:      return HB_PAINT_COMPOSITE_MODE_DEST_ATOP;
    case FT_COLR_COMPOSITE_XOR:            return HB_PAINT_COMPOSITE_MODE_XOR;
    case FT_COLR_COMPOSITE_PLUS:           return HB_PAINT_COMPOSITE_MODE_PLUS;
    case FT_COLR_COMPOSITE_SCREEN:         return HB_PAINT_COMPOSITE_MODE_SCREEN;
    case FT_COLR_COMPOSITE_OVERLAY:        return HB_PAINT_COMPOSITE_MODE_OVERLAY;
    case FT_COLR_COMPOSITE_DARKEN:         return HB_PAINT_COMPOSITE_MODE_DARKEN;
    case FT_COLR_COMPOSITE_LIGHTEN:        return HB_PAINT_COMPOSITE_MODE_LIGHTEN;
    case FT_COLR_COMPOSITE_COLOR_DODGE:    return HB_PAINT_COMPOSITE_MODE_COLOR_DODGE;
    case FT_COLR_COMPOSITE_COLOR_BURN:     return HB_PAINT_COMPOSITE_MODE_COLOR_BURN;
    case FT_COLR_COMPOSITE_HARD_LIGHT:     return HB_PAINT_COMPOSITE_MODE_HARD_LIGHT;
    case FT_COLR_COMPOSITE_SOFT_LIGHT:     return HB_PAINT_COMPOSITE_MODE_SOFT_LIGHT;
    case FT_COLR_COMPOSITE_DIFFERENCE:     return HB_PAINT_COMPOSITE_MODE_DIFFERENCE;
    case FT_COLR_COMPOSITE_EXCLUSION:      return HB_PAINT_COMPOSITE_MODE_EXCLUSION;
    case FT_COLR_COMPOSITE_MULTIPLY:       return HB_PAINT_COMPOSITE_MODE_MULTIPLY;
    case FT_COLR_COMPOSITE_HSL_HUE:        return HB_PAINT_COMPOSITE_MODE_HSL_HUE;
    case FT_COLR_COMPOSITE_HSL_SATURATION: return HB_PAINT_COMPOSITE_MODE_HSL_SATURATION;
    case FT_COLR_COMPOSITE_HSL_COLOR:      return HB_PAINT_COMPOSITE_MODE_HSL_COLOR;
    case FT_COLR_COMPOSITE_HSL_LUMINOSITY: return HB_PAINT_COMPOSITE_MODE_HSL_LUMINOSITY;

    case FT_COLR_COMPOSITE_MAX:            HB_FALLTHROUGH;
    default:                               return HB_PAINT_COMPOSITE_MODE_CLEAR;
  }
}

typedef struct hb_ft_paint_context_t hb_ft_paint_context_t;

static void
_hb_ft_paint (hb_ft_paint_context_t *c,
	      FT_OpaquePaint opaque_paint);

struct hb_ft_paint_context_t
{
  hb_ft_paint_context_t (const hb_ft_font_t *ft_font,
			 hb_font_t *font,
			 hb_paint_funcs_t *paint_funcs, void *paint_data,
			 FT_Color *palette,
			 unsigned palette_index,
			 hb_color_t foreground) :
    ft_font (ft_font), font(font),
    funcs (paint_funcs), data (paint_data),
    palette (palette), palette_index (palette_index), foreground (foreground) {}

  void recurse (FT_OpaquePaint paint)
  {
    if (unlikely (depth_left <= 0 || edge_count <= 0)) return;
    depth_left--;
    edge_count--;
    _hb_ft_paint (this, paint);
    depth_left++;
  }

  const hb_ft_font_t *ft_font;
  hb_font_t *font;
  hb_paint_funcs_t *funcs;
  void *data;
  FT_Color *palette;
  unsigned palette_index;
  hb_color_t foreground;
  hb_map_t current_glyphs;
  hb_map_t current_layers;
  int depth_left = HB_MAX_NESTING_LEVEL;
  int edge_count = HB_MAX_GRAPH_EDGE_COUNT;
};

static unsigned
_hb_ft_color_line_get_color_stops (hb_color_line_t *color_line,
				   void *color_line_data,
				   unsigned int start,
				   unsigned int *count,
				   hb_color_stop_t *color_stops,
				   void *user_data)
{
  FT_ColorLine *cl = (FT_ColorLine *) color_line_data;
  hb_ft_paint_context_t *c = (hb_ft_paint_context_t *) user_data;

  if (count)
  {
    FT_ColorStop stop;
    unsigned wrote = 0;
    FT_ColorStopIterator iter = cl->color_stop_iterator;

    if (start >= cl->color_stop_iterator.num_color_stops)
    {
      *count = 0;
      return cl->color_stop_iterator.num_color_stops;
    }

    while (cl->color_stop_iterator.current_color_stop < start)
      FT_Get_Colorline_Stops(c->ft_font->ft_face,
			     &stop,
			     &cl->color_stop_iterator);

    while (count && *count &&
	   FT_Get_Colorline_Stops(c->ft_font->ft_face,
				  &stop,
				  &cl->color_stop_iterator))
    {
      // https://github.com/harfbuzz/harfbuzz/issues/4013
      if (sizeof stop.stop_offset == 2)
	color_stops->offset = stop.stop_offset / 16384.f;
      else
	color_stops->offset = stop.stop_offset / 65536.f;

      color_stops->is_foreground = stop.color.palette_index == 0xFFFF;
      if (color_stops->is_foreground)
	color_stops->color = HB_COLOR (hb_color_get_blue (c->foreground),
				       hb_color_get_green (c->foreground),
				       hb_color_get_red (c->foreground),
				       (hb_color_get_alpha (c->foreground) * stop.color.alpha) >> 14);
      else
      {
	hb_color_t color;
        if (c->funcs->custom_palette_color (c->data, stop.color.palette_index, &color))
	{
	  color_stops->color = HB_COLOR (hb_color_get_blue (color),
					 hb_color_get_green (color),
					 hb_color_get_red (color),
					 (hb_color_get_alpha (color) * stop.color.alpha) >> 14);
	}
	else
	{
	  FT_Color ft_color = c->palette[stop.color.palette_index];
	  color_stops->color = HB_COLOR (ft_color.blue,
					 ft_color.green,
					 ft_color.red,
					 (ft_color.alpha * stop.color.alpha) >> 14);
	}
      }

      color_stops++;
      wrote++;
    }

    *count = wrote;

    // reset the iterator for next time
    cl->color_stop_iterator = iter;
  }

  return cl->color_stop_iterator.num_color_stops;
}

static hb_paint_extend_t
_hb_ft_color_line_get_extend (hb_color_line_t *color_line,
			      void *color_line_data,
			      void *user_data)
{
  FT_ColorLine *c = (FT_ColorLine *) color_line_data;
  switch (c->extend)
  {
    default:
    case FT_COLR_PAINT_EXTEND_PAD:     return HB_PAINT_EXTEND_PAD;
    case FT_COLR_PAINT_EXTEND_REPEAT:  return HB_PAINT_EXTEND_REPEAT;
    case FT_COLR_PAINT_EXTEND_REFLECT: return HB_PAINT_EXTEND_REFLECT;
  }
}

void
_hb_ft_paint (hb_ft_paint_context_t *c,
	      FT_OpaquePaint opaque_paint)
{
  FT_Face ft_face = c->ft_font->ft_face;
  FT_COLR_Paint paint;
  if (!FT_Get_Paint (ft_face, opaque_paint, &paint))
    return;

  switch (paint.format)
  {
    case FT_COLR_PAINTFORMAT_COLR_LAYERS:
    {
      FT_OpaquePaint other_paint = {0};
      while (FT_Get_Paint_Layers (ft_face,
				  &paint.u.colr_layers.layer_iterator,
				  &other_paint))
      {
        unsigned i = paint.u.colr_layers.layer_iterator.layer;

	if (unlikely (c->current_layers.has (i)))
	  continue;

	c->current_layers.add (i);

	c->funcs->push_group (c->data);
	c->recurse (other_paint);
	c->funcs->pop_group (c->data, HB_PAINT_COMPOSITE_MODE_SRC_OVER);

	c->current_layers.del (i);
      }
    }
    break;
    case FT_COLR_PAINTFORMAT_SOLID:
    {
      bool is_foreground = paint.u.solid.color.palette_index ==  0xFFFF;
      hb_color_t color;
      if (is_foreground)
	color = HB_COLOR (hb_color_get_blue (c->foreground),
			  hb_color_get_green (c->foreground),
			  hb_color_get_red (c->foreground),
			  (hb_color_get_alpha (c->foreground) * paint.u.solid.color.alpha) >> 14);
      else
      {
	if (c->funcs->custom_palette_color (c->data, paint.u.solid.color.palette_index, &color))
	{
	  color = HB_COLOR (hb_color_get_blue (color),
			    hb_color_get_green (color),
			    hb_color_get_red (color),
			    (hb_color_get_alpha (color) * paint.u.solid.color.alpha) >> 14);
	}
	else
	{
	  FT_Color ft_color = c->palette[paint.u.solid.color.palette_index];
	  color = HB_COLOR (ft_color.blue,
			    ft_color.green,
			    ft_color.red,
			    (ft_color.alpha * paint.u.solid.color.alpha) >> 14);
	}
      }
      c->funcs->color (c->data, is_foreground, color);
    }
    break;
    case FT_COLR_PAINTFORMAT_LINEAR_GRADIENT:
    {
      hb_color_line_t cl = {
	&paint.u.linear_gradient.colorline,
	_hb_ft_color_line_get_color_stops, c,
	_hb_ft_color_line_get_extend, nullptr
      };

      c->funcs->linear_gradient (c->data, &cl,
				 paint.u.linear_gradient.p0.x / 65536.f,
				 paint.u.linear_gradient.p0.y / 65536.f,
				 paint.u.linear_gradient.p1.x / 65536.f,
				 paint.u.linear_gradient.p1.y / 65536.f,
				 paint.u.linear_gradient.p2.x / 65536.f,
				 paint.u.linear_gradient.p2.y / 65536.f);
    }
    break;
    case FT_COLR_PAINTFORMAT_RADIAL_GRADIENT:
    {
      hb_color_line_t cl = {
	&paint.u.linear_gradient.colorline,
	_hb_ft_color_line_get_color_stops, c,
	_hb_ft_color_line_get_extend, nullptr
      };

      c->funcs->radial_gradient (c->data, &cl,
				 paint.u.radial_gradient.c0.x / 65536.f,
				 paint.u.radial_gradient.c0.y / 65536.f,
				 paint.u.radial_gradient.r0 / 65536.f,
				 paint.u.radial_gradient.c1.x / 65536.f,
				 paint.u.radial_gradient.c1.y / 65536.f,
				 paint.u.radial_gradient.r1 / 65536.f);
    }
    break;
    case FT_COLR_PAINTFORMAT_SWEEP_GRADIENT:
    {
      hb_color_line_t cl = {
	&paint.u.linear_gradient.colorline,
	_hb_ft_color_line_get_color_stops, c,
	_hb_ft_color_line_get_extend, nullptr
      };

      c->funcs->sweep_gradient (c->data, &cl,
				paint.u.sweep_gradient.center.x / 65536.f,
				paint.u.sweep_gradient.center.y / 65536.f,
				(paint.u.sweep_gradient.start_angle / 65536.f + 1) * HB_PI,
				(paint.u.sweep_gradient.end_angle / 65536.f + 1) * HB_PI);
    }
    break;
    case FT_COLR_PAINTFORMAT_GLYPH:
    {
      c->funcs->push_inverse_root_transform (c->data, c->font);
      c->ft_font->lock.unlock ();
      c->funcs->push_clip_glyph (c->data, paint.u.glyph.glyphID, c->font);
      c->ft_font->lock.lock ();
      c->funcs->push_root_transform (c->data, c->font);
      c->recurse (paint.u.glyph.paint);
      c->funcs->pop_transform (c->data);
      c->funcs->pop_clip (c->data);
      c->funcs->pop_transform (c->data);
    }
    break;
    case FT_COLR_PAINTFORMAT_COLR_GLYPH:
    {
      hb_codepoint_t gid = paint.u.colr_glyph.glyphID;

      if (unlikely (c->current_glyphs.has (gid)))
	return;

      c->current_glyphs.add (gid);

      c->funcs->push_inverse_root_transform (c->data, c->font);
      c->ft_font->lock.unlock ();
      if (c->funcs->color_glyph (c->data, gid, c->font))
      {
	c->ft_font->lock.lock ();
	c->funcs->pop_transform (c->data);
	c->current_glyphs.del (gid);
	return;
      }
      c->ft_font->lock.lock ();
      c->funcs->pop_transform (c->data);

      FT_OpaquePaint other_paint = {0};
      if (FT_Get_Color_Glyph_Paint (ft_face, gid,
				    FT_COLOR_NO_ROOT_TRANSFORM,
				    &other_paint))
      {
        bool has_clip_box;
        FT_ClipBox clip_box;
        has_clip_box = FT_Get_Color_Glyph_ClipBox (ft_face, paint.u.colr_glyph.glyphID, &clip_box);

        if (has_clip_box)
	{
	  /* The FreeType ClipBox is in scaled coordinates, whereas we need
	   * unscaled clipbox here. Oh well...
	   */

	  float upem = c->font->face->get_upem ();
	  float xscale = upem / (c->font->x_scale ? c->font->x_scale : upem);
	  float yscale = upem / (c->font->y_scale ? c->font->y_scale : upem);

          c->funcs->push_clip_rectangle (c->data,
					 clip_box.bottom_left.x * xscale,
					 clip_box.bottom_left.y * yscale,
					 clip_box.top_right.x * xscale,
					 clip_box.top_right.y * yscale);
	}

	c->recurse (other_paint);

        if (has_clip_box)
          c->funcs->pop_clip (c->data);

	c->current_glyphs.del (gid);
      }
    }
    break;
    case FT_COLR_PAINTFORMAT_TRANSFORM:
    {
      c->funcs->push_transform (c->data,
				paint.u.transform.affine.xx / 65536.f,
				paint.u.transform.affine.yx / 65536.f,
				paint.u.transform.affine.xy / 65536.f,
				paint.u.transform.affine.yy / 65536.f,
				paint.u.transform.affine.dx / 65536.f,
				paint.u.transform.affine.dy / 65536.f);
      c->recurse (paint.u.transform.paint);
      c->funcs->pop_transform (c->data);
    }
    break;
    case FT_COLR_PAINTFORMAT_TRANSLATE:
    {
      float dx = paint.u.translate.dx / 65536.f;
      float dy = paint.u.translate.dy / 65536.f;

      bool p1 = c->funcs->push_translate (c->data, dx, dy);
      c->recurse (paint.u.translate.paint);
      if (p1) c->funcs->pop_transform (c->data);
    }
    break;
    case FT_COLR_PAINTFORMAT_SCALE:
    {
      float dx = paint.u.scale.center_x / 65536.f;
      float dy = paint.u.scale.center_y / 65536.f;
      float sx = paint.u.scale.scale_x / 65536.f;
      float sy = paint.u.scale.scale_y / 65536.f;

      bool p1 = c->funcs->push_translate (c->data, +dx, +dy);
      bool p2 = c->funcs->push_scale (c->data, sx, sy);
      bool p3 = c->funcs->push_translate (c->data, -dx, -dy);
      c->recurse (paint.u.scale.paint);
      if (p3) c->funcs->pop_transform (c->data);
      if (p2) c->funcs->pop_transform (c->data);
      if (p1) c->funcs->pop_transform (c->data);
    }
    break;
    case FT_COLR_PAINTFORMAT_ROTATE:
    {
      float dx = paint.u.rotate.center_x / 65536.f;
      float dy = paint.u.rotate.center_y / 65536.f;
      float a = paint.u.rotate.angle / 65536.f;

      bool p1 = c->funcs->push_translate (c->data, +dx, +dy);
      bool p2 = c->funcs->push_rotate (c->data, a);
      bool p3 = c->funcs->push_translate (c->data, -dx, -dy);
      c->recurse (paint.u.rotate.paint);
      if (p3) c->funcs->pop_transform (c->data);
      if (p2) c->funcs->pop_transform (c->data);
      if (p1) c->funcs->pop_transform (c->data);
    }
    break;
    case FT_COLR_PAINTFORMAT_SKEW:
    {
      float dx = paint.u.skew.center_x / 65536.f;
      float dy = paint.u.skew.center_y / 65536.f;
      float sx = paint.u.skew.x_skew_angle / 65536.f;
      float sy = paint.u.skew.y_skew_angle / 65536.f;

      bool p1 = c->funcs->push_translate (c->data, +dx, +dy);
      bool p2 = c->funcs->push_skew (c->data, sx, sy);
      bool p3 = c->funcs->push_translate (c->data, -dx, -dy);
      c->recurse (paint.u.skew.paint);
      if (p3) c->funcs->pop_transform (c->data);
      if (p2) c->funcs->pop_transform (c->data);
      if (p1) c->funcs->pop_transform (c->data);
    }
    break;
    case FT_COLR_PAINTFORMAT_COMPOSITE:
    {
      c->recurse (paint.u.composite.backdrop_paint);
      c->funcs->push_group (c->data);
      c->recurse (paint.u.composite.source_paint);
      c->funcs->pop_group (c->data, _hb_ft_paint_composite_mode (paint.u.composite.composite_mode));
    }
    break;

    case FT_COLR_PAINT_FORMAT_MAX: break;
    default: HB_FALLTHROUGH;
    case FT_COLR_PAINTFORMAT_UNSUPPORTED: break;
  }
}


static bool
hb_ft_paint_glyph_colr (hb_font_t *font,
			void *font_data,
			hb_codepoint_t gid,
			hb_paint_funcs_t *paint_funcs, void *paint_data,
			unsigned int palette_index,
			hb_color_t foreground,
			void *user_data)
{
  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font_data;
  FT_Face ft_face = ft_font->ft_face;

  /* Face is locked. */

  FT_Error error;
  FT_Color*         palette;
  FT_LayerIterator  iterator;

  FT_Bool  have_layers;
  FT_UInt  layer_glyph_index;
  FT_UInt  layer_color_index;

  error = FT_Palette_Select(ft_face, palette_index, &palette);
  if (error)
    palette = NULL;

  /* COLRv1 */
  FT_OpaquePaint paint = {0};
  if (FT_Get_Color_Glyph_Paint (ft_face, gid,
			        FT_COLOR_NO_ROOT_TRANSFORM,
			        &paint))
  {
    hb_ft_paint_context_t c (ft_font, font,
			     paint_funcs, paint_data,
			     palette, palette_index, foreground);
    c.current_glyphs.add (gid);

    bool is_bounded = true;
    FT_ClipBox clip_box;
    if (FT_Get_Color_Glyph_ClipBox (ft_face, gid, &clip_box))
    {
      c.funcs->push_clip_rectangle (c.data,
				    clip_box.bottom_left.x +
				      roundf (hb_min (font->slant_xy * clip_box.bottom_left.y,
						      font->slant_xy * clip_box.top_left.y)),
				    clip_box.bottom_left.y,
				    clip_box.top_right.x +
				      roundf (hb_max (font->slant_xy * clip_box.bottom_right.y,
						      font->slant_xy * clip_box.top_right.y)),
				    clip_box.top_right.y);
    }
    else
    {

      auto *extents_funcs = hb_paint_extents_get_funcs ();
      hb_paint_extents_context_t extents_data;
      hb_ft_paint_context_t ce (ft_font, font,
			        extents_funcs, &extents_data,
			        palette, palette_index, foreground);
      ce.current_glyphs.add (gid);
      ce.funcs->push_root_transform (ce.data, font);
      ce.recurse (paint);
      ce.funcs->pop_transform (ce.data);
      hb_extents_t extents = extents_data.get_extents ();
      is_bounded = extents_data.is_bounded ();

      c.funcs->push_clip_rectangle (c.data,
				    extents.xmin,
				    extents.ymin,
				    extents.xmax,
				    extents.ymax);
    }

    c.funcs->push_root_transform (c.data, font);

    if (is_bounded)
      c.recurse (paint);

    c.funcs->pop_transform (c.data);
    c.funcs->pop_clip (c.data);

    return true;
  }

  /* COLRv0 */
  iterator.p  = NULL;
  have_layers = FT_Get_Color_Glyph_Layer(ft_face,
					 gid,
					 &layer_glyph_index,
					 &layer_color_index,
					 &iterator);

  if (palette && have_layers)
  {
    do
    {
      hb_bool_t is_foreground = true;
      hb_color_t color = foreground;

      if ( layer_color_index != 0xFFFF )
      {
	FT_Color layer_color = palette[layer_color_index];
	color = HB_COLOR (layer_color.blue,
			  layer_color.green,
			  layer_color.red,
			  layer_color.alpha);
	is_foreground = false;
      }

      ft_font->lock.unlock ();
      paint_funcs->push_clip_glyph (paint_data, layer_glyph_index, font);
      ft_font->lock.lock ();
      paint_funcs->color (paint_data, is_foreground, color);
      paint_funcs->pop_clip (paint_data);

    } while (FT_Get_Color_Glyph_Layer(ft_face,
				      gid,
				      &layer_glyph_index,
				      &layer_color_index,
				      &iterator));
    return true;
  }

  return false;
}


#endif /* HB_FT_COLR_HH */
