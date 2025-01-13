/*
 * Copyright © 2009  Red Hat, Inc.
 * Copyright © 2009  Keith Stribley
 * Copyright © 2015  Google, Inc.
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

#ifdef HAVE_FREETYPE

#include "hb-ft.h"

#include "hb-cache.hh"
#include "hb-draw.hh"
#include "hb-font.hh"
#include "hb-machinery.hh"
#include "hb-ot-os2-table.hh"
#include "hb-ot-shaper-arabic-pua.hh"
#include "hb-paint.hh"

#include FT_MODULE_H
#include FT_ADVANCES_H
#include FT_MULTIPLE_MASTERS_H
#include FT_OUTLINE_H
#include FT_TRUETYPE_TABLES_H
#include FT_SYNTHESIS_H
#if (FREETYPE_MAJOR*10000 + FREETYPE_MINOR*100 + FREETYPE_PATCH) >= 21300
#include FT_COLOR_H
#endif


/**
 * SECTION:hb-ft
 * @title: hb-ft
 * @short_description: FreeType integration
 * @include: hb-ft.h
 *
 * Functions for using HarfBuzz with the FreeType library.
 *
 * HarfBuzz supports using FreeType to provide face and
 * font data.
 *
 * <note>Note that FreeType is not thread-safe, therefore these
 * functions are not thread-safe either.</note>
 **/


/* TODO:
 *
 * In general, this file does a fine job of what it's supposed to do.
 * There are, however, things that need more work:
 *
 *   - FreeType works in 26.6 mode.  Clients can decide to use that mode, and everything
 *     would work fine.  However, we also abuse this API for performing in font-space,
 *     but don't pass the correct flags to FreeType.  We just abuse the no-hinting mode
 *     for that, such that no rounding etc happens.  As such, we don't set ppem, and
 *     pass NO_HINTING as load_flags.  Would be much better to use NO_SCALE, and scale
 *     ourselves.
 *
 *   - We don't handle / allow for emboldening / obliqueing.
 *
 *   - In the future, we should add constructors to create fonts in font space?
 */


using hb_ft_advance_cache_t = hb_cache_t<16, 24, 8, false>;

struct hb_ft_font_t
{
  int load_flags;
  bool symbol; /* Whether selected cmap is symbol cmap. */
  bool unref; /* Whether to destroy ft_face when done. */
  bool transform; /* Whether to apply FT_Face's transform. */

  mutable hb_mutex_t lock; /* Protects members below. */
  FT_Face ft_face;
  mutable unsigned cached_serial;
  mutable hb_ft_advance_cache_t advance_cache;
};

static hb_ft_font_t *
_hb_ft_font_create (FT_Face ft_face, bool symbol, bool unref)
{
  hb_ft_font_t *ft_font = (hb_ft_font_t *) hb_calloc (1, sizeof (hb_ft_font_t));
  if (unlikely (!ft_font)) return nullptr;

  ft_font->lock.init ();
  ft_font->ft_face = ft_face;
  ft_font->symbol = symbol;
  ft_font->unref = unref;

  ft_font->load_flags = FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING;

  ft_font->cached_serial = (unsigned) -1;
  new (&ft_font->advance_cache) hb_ft_advance_cache_t;

  return ft_font;
}

static void
_hb_ft_face_destroy (void *data)
{
  FT_Done_Face ((FT_Face) data);
}

static void
_hb_ft_font_destroy (void *data)
{
  hb_ft_font_t *ft_font = (hb_ft_font_t *) data;

  if (ft_font->unref)
    _hb_ft_face_destroy (ft_font->ft_face);

  ft_font->lock.fini ();

  hb_free (ft_font);
}


/* hb_font changed, update FT_Face. */
static void _hb_ft_hb_font_changed (hb_font_t *font, FT_Face ft_face)
{
  hb_ft_font_t *ft_font = (hb_ft_font_t *) font->user_data;

  float x_mult = 1.f, y_mult = 1.f;

  if (font->x_scale < 0) x_mult = -x_mult;
  if (font->y_scale < 0) y_mult = -y_mult;

  if (FT_Set_Char_Size (ft_face,
			abs (font->x_scale), abs (font->y_scale),
			0, 0
#if 0
		    font->x_ppem * 72 * 64 / font->x_scale,
		    font->y_ppem * 72 * 64 / font->y_scale
#endif
     ) && ft_face->num_fixed_sizes)
  {
#ifdef HAVE_FT_GET_TRANSFORM
    /* Bitmap font, eg. bitmap color emoji. */
    /* Pick largest size? */
    int x_scale  = ft_face->available_sizes[ft_face->num_fixed_sizes - 1].x_ppem;
    int y_scale = ft_face->available_sizes[ft_face->num_fixed_sizes - 1].y_ppem;
    FT_Set_Char_Size (ft_face,
		      x_scale, y_scale,
		      0, 0);

    /* This contains the sign that was previously in x_mult/y_mult. */
    x_mult = (float) font->x_scale / x_scale;
    y_mult = (float) font->y_scale / y_scale;
#endif
  }
  else
  { /* Shrug */ }


  if (x_mult != 1.f || y_mult != 1.f)
  {
    FT_Matrix matrix = { (int) roundf (x_mult * (1<<16)), 0,
			  0, (int) roundf (y_mult * (1<<16))};
    FT_Set_Transform (ft_face, &matrix, nullptr);
    ft_font->transform = true;
  }

#if defined(HAVE_FT_GET_VAR_BLEND_COORDINATES) && !defined(HB_NO_VAR)
  unsigned int num_coords;
  const float *coords = hb_font_get_var_coords_design (font, &num_coords);
  if (num_coords)
  {
    FT_Fixed *ft_coords = (FT_Fixed *) hb_calloc (num_coords, sizeof (FT_Fixed));
    if (ft_coords)
    {
      for (unsigned int i = 0; i < num_coords; i++)
	  ft_coords[i] = coords[i] * 65536.f;
      FT_Set_Var_Design_Coordinates (ft_face, num_coords, ft_coords);
      hb_free (ft_coords);
    }
  }
#endif
}

/* Check if hb_font changed, update FT_Face. */
static inline bool
_hb_ft_hb_font_check_changed (hb_font_t *font,
			      const hb_ft_font_t *ft_font)
{
  if (font->serial != ft_font->cached_serial)
  {
    _hb_ft_hb_font_changed (font, ft_font->ft_face);
    ft_font->advance_cache.clear ();
    ft_font->cached_serial = font->serial;
    return true;
  }
  return false;
}


/**
 * hb_ft_font_set_load_flags:
 * @font: #hb_font_t to work upon
 * @load_flags: The FreeType load flags to set
 *
 * Sets the FT_Load_Glyph load flags for the specified #hb_font_t.
 *
 * For more information, see
 * <https://freetype.org/freetype2/docs/reference/ft2-glyph_retrieval.html#ft_load_xxx>
 *
 * This function works with #hb_font_t objects created by
 * hb_ft_font_create() or hb_ft_font_create_referenced().
 *
 * Since: 1.0.5
 **/
void
hb_ft_font_set_load_flags (hb_font_t *font, int load_flags)
{
  if (hb_object_is_immutable (font))
    return;

  if (unlikely (font->destroy != (hb_destroy_func_t) _hb_ft_font_destroy))
    return;

  hb_ft_font_t *ft_font = (hb_ft_font_t *) font->user_data;

  ft_font->load_flags = load_flags;
}

/**
 * hb_ft_font_get_load_flags:
 * @font: #hb_font_t to work upon
 *
 * Fetches the FT_Load_Glyph load flags of the specified #hb_font_t.
 *
 * For more information, see
 * <https://freetype.org/freetype2/docs/reference/ft2-glyph_retrieval.html#ft_load_xxx>
 *
 * This function works with #hb_font_t objects created by
 * hb_ft_font_create() or hb_ft_font_create_referenced().
 *
 * Return value: FT_Load_Glyph flags found, or 0
 *
 * Since: 1.0.5
 **/
int
hb_ft_font_get_load_flags (hb_font_t *font)
{
  if (unlikely (font->destroy != (hb_destroy_func_t) _hb_ft_font_destroy))
    return 0;

  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font->user_data;

  return ft_font->load_flags;
}

/**
 * hb_ft_font_get_face: (skip)
 * @font: #hb_font_t to work upon
 *
 * Fetches the FT_Face associated with the specified #hb_font_t
 * font object.
 *
 * This function works with #hb_font_t objects created by
 * hb_ft_font_create() or hb_ft_font_create_referenced().
 *
 * Return value: (nullable): the FT_Face found or `NULL`
 *
 * Since: 0.9.2
 **/
FT_Face
hb_ft_font_get_face (hb_font_t *font)
{
  if (unlikely (font->destroy != (hb_destroy_func_t) _hb_ft_font_destroy))
    return nullptr;

  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font->user_data;

  return ft_font->ft_face;
}

/**
 * hb_ft_font_lock_face: (skip)
 * @font: #hb_font_t to work upon
 *
 * Gets the FT_Face associated with @font.
 *
 * This face will be kept around and access to the FT_Face object
 * from other HarfBuzz API wil be blocked until you call hb_ft_font_unlock_face().
 *
 * This function works with #hb_font_t objects created by
 * hb_ft_font_create() or hb_ft_font_create_referenced().
 *
 * Return value: (nullable) (transfer none): the FT_Face associated with @font or `NULL`
 * Since: 2.6.5
 **/
FT_Face
hb_ft_font_lock_face (hb_font_t *font)
{
  if (unlikely (font->destroy != (hb_destroy_func_t) _hb_ft_font_destroy))
    return nullptr;

  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font->user_data;

  ft_font->lock.lock ();

  return ft_font->ft_face;
}

/**
 * hb_ft_font_unlock_face: (skip)
 * @font: #hb_font_t to work upon
 *
 * Releases an FT_Face previously obtained with hb_ft_font_lock_face().
 *
 * Since: 2.6.5
 **/
void
hb_ft_font_unlock_face (hb_font_t *font)
{
  if (unlikely (font->destroy != (hb_destroy_func_t) _hb_ft_font_destroy))
    return;

  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font->user_data;

  ft_font->lock.unlock ();
}


static hb_bool_t
hb_ft_get_nominal_glyph (hb_font_t *font,
			 void *font_data,
			 hb_codepoint_t unicode,
			 hb_codepoint_t *glyph,
			 void *user_data HB_UNUSED)
{
  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font_data;
  hb_lock_t lock (ft_font->lock);
  unsigned int g = FT_Get_Char_Index (ft_font->ft_face, unicode);

  if (unlikely (!g))
  {
    if (unlikely (ft_font->symbol))
    {
      switch ((unsigned) font->face->table.OS2->get_font_page ()) {
      case OT::OS2::font_page_t::FONT_PAGE_NONE:
	if (unicode <= 0x00FFu)
	  /* For symbol-encoded OpenType fonts, we duplicate the
	   * U+F000..F0FF range at U+0000..U+00FF.  That's what
	   * Windows seems to do, and that's hinted about at:
	   * https://docs.microsoft.com/en-us/typography/opentype/spec/recom
	   * under "Non-Standard (Symbol) Fonts". */
	  g = FT_Get_Char_Index (ft_font->ft_face, 0xF000u + unicode);
	break;
#ifndef HB_NO_OT_SHAPER_ARABIC_FALLBACK
      case OT::OS2::font_page_t::FONT_PAGE_SIMP_ARABIC:
	g = FT_Get_Char_Index (ft_font->ft_face, _hb_arabic_pua_simp_map (unicode));
	break;
      case OT::OS2::font_page_t::FONT_PAGE_TRAD_ARABIC:
	g = FT_Get_Char_Index (ft_font->ft_face, _hb_arabic_pua_trad_map (unicode));
	break;
#endif
      default:
	break;
      }
      if (!g)
	return false;
    }
    else
      return false;
  }

  *glyph = g;
  return true;
}

static unsigned int
hb_ft_get_nominal_glyphs (hb_font_t *font HB_UNUSED,
			  void *font_data,
			  unsigned int count,
			  const hb_codepoint_t *first_unicode,
			  unsigned int unicode_stride,
			  hb_codepoint_t *first_glyph,
			  unsigned int glyph_stride,
			  void *user_data HB_UNUSED)
{
  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font_data;
  hb_lock_t lock (ft_font->lock);
  unsigned int done;
  for (done = 0;
       done < count && (*first_glyph = FT_Get_Char_Index (ft_font->ft_face, *first_unicode));
       done++)
  {
    first_unicode = &StructAtOffsetUnaligned<hb_codepoint_t> (first_unicode, unicode_stride);
    first_glyph = &StructAtOffsetUnaligned<hb_codepoint_t> (first_glyph, glyph_stride);
  }
  /* We don't need to do ft_font->symbol dance here, since HB calls the singular
   * nominal_glyph() for what we don't handle here. */
  return done;
}


static hb_bool_t
hb_ft_get_variation_glyph (hb_font_t *font HB_UNUSED,
			   void *font_data,
			   hb_codepoint_t unicode,
			   hb_codepoint_t variation_selector,
			   hb_codepoint_t *glyph,
			   void *user_data HB_UNUSED)
{
  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font_data;
  hb_lock_t lock (ft_font->lock);
  unsigned int g = FT_Face_GetCharVariantIndex (ft_font->ft_face, unicode, variation_selector);

  if (unlikely (!g))
    return false;

  *glyph = g;
  return true;
}

static void
hb_ft_get_glyph_h_advances (hb_font_t* font, void* font_data,
			    unsigned count,
			    const hb_codepoint_t *first_glyph,
			    unsigned glyph_stride,
			    hb_position_t *first_advance,
			    unsigned advance_stride,
			    void *user_data HB_UNUSED)
{
  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font_data;
  hb_position_t *orig_first_advance = first_advance;
  hb_lock_t lock (ft_font->lock);
  FT_Face ft_face = ft_font->ft_face;
  int load_flags = ft_font->load_flags;
  float x_mult;
#ifdef HAVE_FT_GET_TRANSFORM
  if (ft_font->transform)
  {
    FT_Matrix matrix;
    FT_Get_Transform (ft_face, &matrix, nullptr);
    x_mult = sqrtf ((float)matrix.xx * matrix.xx + (float)matrix.xy * matrix.xy) / 65536.f;
    x_mult *= font->x_scale < 0 ? -1 : +1;
  }
  else
#endif
  {
    x_mult = font->x_scale < 0 ? -1 : +1;
  }

  for (unsigned int i = 0; i < count; i++)
  {
    FT_Fixed v = 0;
    hb_codepoint_t glyph = *first_glyph;

    unsigned int cv;
    if (ft_font->advance_cache.get (glyph, &cv))
      v = cv;
    else
    {
      FT_Get_Advance (ft_face, glyph, load_flags, &v);
      /* Work around bug that FreeType seems to return negative advance
       * for variable-set fonts if x_scale is negative! */
      v = abs (v);
      v = (int) (v * x_mult + (1<<9)) >> 10;
      ft_font->advance_cache.set (glyph, v);
    }

    *first_advance = v;
    first_glyph = &StructAtOffsetUnaligned<hb_codepoint_t> (first_glyph, glyph_stride);
    first_advance = &StructAtOffsetUnaligned<hb_position_t> (first_advance, advance_stride);
  }

  if (font->x_strength && !font->embolden_in_place)
  {
    /* Emboldening. */
    hb_position_t x_strength = font->x_scale >= 0 ? font->x_strength : -font->x_strength;
    first_advance = orig_first_advance;
    for (unsigned int i = 0; i < count; i++)
    {
      *first_advance += *first_advance ? x_strength : 0;
      first_advance = &StructAtOffsetUnaligned<hb_position_t> (first_advance, advance_stride);
    }
  }
}

#ifndef HB_NO_VERTICAL
static hb_position_t
hb_ft_get_glyph_v_advance (hb_font_t *font,
			   void *font_data,
			   hb_codepoint_t glyph,
			   void *user_data HB_UNUSED)
{
  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font_data;
  hb_lock_t lock (ft_font->lock);
  FT_Fixed v;
  float y_mult;
#ifdef HAVE_FT_GET_TRANSFORM
  if (ft_font->transform)
  {
    FT_Matrix matrix;
    FT_Get_Transform (ft_font->ft_face, &matrix, nullptr);
    y_mult = sqrtf ((float)matrix.yx * matrix.yx + (float)matrix.yy * matrix.yy) / 65536.f;
    y_mult *= font->y_scale < 0 ? -1 : +1;
  }
  else
#endif
  {
    y_mult = font->y_scale < 0 ? -1 : +1;
  }

  if (unlikely (FT_Get_Advance (ft_font->ft_face, glyph, ft_font->load_flags | FT_LOAD_VERTICAL_LAYOUT, &v)))
    return 0;

  v = (int) (y_mult * v);

  /* Note: FreeType's vertical metrics grows downward while other FreeType coordinates
   * have a Y growing upward.  Hence the extra negation. */

  hb_position_t y_strength = font->y_scale >= 0 ? font->y_strength : -font->y_strength;
  return ((-v + (1<<9)) >> 10) + (font->embolden_in_place ? 0 : y_strength);
}
#endif

#ifndef HB_NO_VERTICAL
static hb_bool_t
hb_ft_get_glyph_v_origin (hb_font_t *font,
			  void *font_data,
			  hb_codepoint_t glyph,
			  hb_position_t *x,
			  hb_position_t *y,
			  void *user_data HB_UNUSED)
{
  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font_data;
  hb_lock_t lock (ft_font->lock);
  FT_Face ft_face = ft_font->ft_face;
  float x_mult, y_mult;
#ifdef HAVE_FT_GET_TRANSFORM
  if (ft_font->transform)
  {
    FT_Matrix matrix;
    FT_Get_Transform (ft_face, &matrix, nullptr);
    x_mult = sqrtf ((float)matrix.xx * matrix.xx + (float)matrix.xy * matrix.xy) / 65536.f;
    x_mult *= font->x_scale < 0 ? -1 : +1;
    y_mult = sqrtf ((float)matrix.yx * matrix.yx + (float)matrix.yy * matrix.yy) / 65536.f;
    y_mult *= font->y_scale < 0 ? -1 : +1;
  }
  else
#endif
  {
    x_mult = font->x_scale < 0 ? -1 : +1;
    y_mult = font->y_scale < 0 ? -1 : +1;
  }

  if (unlikely (FT_Load_Glyph (ft_face, glyph, ft_font->load_flags)))
    return false;

  /* Note: FreeType's vertical metrics grows downward while other FreeType coordinates
   * have a Y growing upward.  Hence the extra negation. */
  *x = ft_face->glyph->metrics.horiBearingX -   ft_face->glyph->metrics.vertBearingX;
  *y = ft_face->glyph->metrics.horiBearingY - (-ft_face->glyph->metrics.vertBearingY);

  *x = (hb_position_t) (x_mult * *x);
  *y = (hb_position_t) (y_mult * *y);

  return true;
}
#endif

#ifndef HB_NO_OT_SHAPE_FALLBACK
static hb_position_t
hb_ft_get_glyph_h_kerning (hb_font_t *font,
			   void *font_data,
			   hb_codepoint_t left_glyph,
			   hb_codepoint_t right_glyph,
			   void *user_data HB_UNUSED)
{
  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font_data;
  hb_lock_t lock (ft_font->lock);
  FT_Vector kerningv;

  FT_Kerning_Mode mode = font->x_ppem ? FT_KERNING_DEFAULT : FT_KERNING_UNFITTED;
  if (FT_Get_Kerning (ft_font->ft_face, left_glyph, right_glyph, mode, &kerningv))
    return 0;

  return kerningv.x;
}
#endif

static hb_bool_t
hb_ft_get_glyph_extents (hb_font_t *font,
			 void *font_data,
			 hb_codepoint_t glyph,
			 hb_glyph_extents_t *extents,
			 void *user_data HB_UNUSED)
{
  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font_data;
  hb_lock_t lock (ft_font->lock);
  FT_Face ft_face = ft_font->ft_face;
  float x_mult, y_mult;
  float slant_xy = font->slant_xy;
#ifdef HAVE_FT_GET_TRANSFORM
  if (ft_font->transform)
  {
    FT_Matrix matrix;
    FT_Get_Transform (ft_face, &matrix, nullptr);
    x_mult = sqrtf ((float)matrix.xx * matrix.xx + (float)matrix.xy * matrix.xy) / 65536.f;
    x_mult *= font->x_scale < 0 ? -1 : +1;
    y_mult = sqrtf ((float)matrix.yx * matrix.yx + (float)matrix.yy * matrix.yy) / 65536.f;
    y_mult *= font->y_scale < 0 ? -1 : +1;
  }
  else
#endif
  {
    x_mult = font->x_scale < 0 ? -1 : +1;
    y_mult = font->y_scale < 0 ? -1 : +1;
  }

  if (unlikely (FT_Load_Glyph (ft_face, glyph, ft_font->load_flags)))
    return false;

  /* Copied from hb_font_t::scale_glyph_extents. */

  float x1 = x_mult * ft_face->glyph->metrics.horiBearingX;
  float y1 = y_mult * ft_face->glyph->metrics.horiBearingY;
  float x2 = x1 + x_mult *  ft_face->glyph->metrics.width;
  float y2 = y1 + y_mult * -ft_face->glyph->metrics.height;

  /* Apply slant. */
  if (slant_xy)
  {
    x1 += hb_min (y1 * slant_xy, y2 * slant_xy);
    x2 += hb_max (y1 * slant_xy, y2 * slant_xy);
  }

  extents->x_bearing = floorf (x1);
  extents->y_bearing = floorf (y1);
  extents->width = ceilf (x2) - extents->x_bearing;
  extents->height = ceilf (y2) - extents->y_bearing;

  if (font->x_strength || font->y_strength)
  {
    /* Y */
    int y_shift = font->y_strength;
    if (font->y_scale < 0) y_shift = -y_shift;
    extents->y_bearing += y_shift;
    extents->height -= y_shift;

    /* X */
    int x_shift = font->x_strength;
    if (font->x_scale < 0) x_shift = -x_shift;
    if (font->embolden_in_place)
      extents->x_bearing -= x_shift / 2;
    extents->width += x_shift;
  }

  return true;
}

static hb_bool_t
hb_ft_get_glyph_contour_point (hb_font_t *font HB_UNUSED,
			       void *font_data,
			       hb_codepoint_t glyph,
			       unsigned int point_index,
			       hb_position_t *x,
			       hb_position_t *y,
			       void *user_data HB_UNUSED)
{
  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font_data;
  hb_lock_t lock (ft_font->lock);
  FT_Face ft_face = ft_font->ft_face;

  if (unlikely (FT_Load_Glyph (ft_face, glyph, ft_font->load_flags)))
      return false;

  if (unlikely (ft_face->glyph->format != FT_GLYPH_FORMAT_OUTLINE))
      return false;

  if (unlikely (point_index >= (unsigned int) ft_face->glyph->outline.n_points))
      return false;

  *x = ft_face->glyph->outline.points[point_index].x;
  *y = ft_face->glyph->outline.points[point_index].y;

  return true;
}

static hb_bool_t
hb_ft_get_glyph_name (hb_font_t *font HB_UNUSED,
		      void *font_data,
		      hb_codepoint_t glyph,
		      char *name, unsigned int size,
		      void *user_data HB_UNUSED)
{
  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font_data;
  hb_lock_t lock (ft_font->lock);
  FT_Face ft_face = ft_font->ft_face;

  hb_bool_t ret = !FT_Get_Glyph_Name (ft_face, glyph, name, size);
  if (ret && (size && !*name))
    ret = false;

  return ret;
}

static hb_bool_t
hb_ft_get_glyph_from_name (hb_font_t *font HB_UNUSED,
			   void *font_data,
			   const char *name, int len, /* -1 means nul-terminated */
			   hb_codepoint_t *glyph,
			   void *user_data HB_UNUSED)
{
  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font_data;
  hb_lock_t lock (ft_font->lock);
  FT_Face ft_face = ft_font->ft_face;

  if (len < 0)
    *glyph = FT_Get_Name_Index (ft_face, (FT_String *) name);
  else {
    /* Make a nul-terminated version. */
    char buf[128];
    len = hb_min (len, (int) sizeof (buf) - 1);
    strncpy (buf, name, len);
    buf[len] = '\0';
    *glyph = FT_Get_Name_Index (ft_face, buf);
  }

  if (*glyph == 0)
  {
    /* Check whether the given name was actually the name of glyph 0. */
    char buf[128];
    if (!FT_Get_Glyph_Name(ft_face, 0, buf, sizeof (buf)) &&
	len < 0 ? !strcmp (buf, name) : !strncmp (buf, name, len))
      return true;
  }

  return *glyph != 0;
}

static hb_bool_t
hb_ft_get_font_h_extents (hb_font_t *font HB_UNUSED,
			  void *font_data,
			  hb_font_extents_t *metrics,
			  void *user_data HB_UNUSED)
{
  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font_data;
  hb_lock_t lock (ft_font->lock);
  FT_Face ft_face = ft_font->ft_face;
  float y_mult;
#ifdef HAVE_FT_GET_TRANSFORM
  if (ft_font->transform)
  {
    FT_Matrix matrix;
    FT_Get_Transform (ft_face, &matrix, nullptr);
    y_mult = sqrtf ((float)matrix.yx * matrix.yx + (float)matrix.yy * matrix.yy) / 65536.f;
    y_mult *= font->y_scale < 0 ? -1 : +1;
  }
  else
#endif
  {
    y_mult = font->y_scale < 0 ? -1 : +1;
  }

  if (ft_face->units_per_EM != 0)
  {
    metrics->ascender = FT_MulFix(ft_face->ascender, ft_face->size->metrics.y_scale);
    metrics->descender = FT_MulFix(ft_face->descender, ft_face->size->metrics.y_scale);
    metrics->line_gap = FT_MulFix( ft_face->height, ft_face->size->metrics.y_scale ) - (metrics->ascender - metrics->descender);
  }
  else
  {
    /* Bitmap-only font, eg. color bitmap font. */
    metrics->ascender = ft_face->size->metrics.ascender;
    metrics->descender = ft_face->size->metrics.descender;
    metrics->line_gap = ft_face->size->metrics.height - (metrics->ascender - metrics->descender);
  }

  metrics->ascender  = (hb_position_t) (y_mult * (metrics->ascender + font->y_strength));
  metrics->descender = (hb_position_t) (y_mult * metrics->descender);
  metrics->line_gap  = (hb_position_t) (y_mult * metrics->line_gap);

  return true;
}

#ifndef HB_NO_DRAW

static int
_hb_ft_move_to (const FT_Vector *to,
		void *arg)
{
  hb_draw_session_t *drawing = (hb_draw_session_t *) arg;
  drawing->move_to (to->x, to->y);
  return FT_Err_Ok;
}

static int
_hb_ft_line_to (const FT_Vector *to,
		void *arg)
{
  hb_draw_session_t *drawing = (hb_draw_session_t *) arg;
  drawing->line_to (to->x, to->y);
  return FT_Err_Ok;
}

static int
_hb_ft_conic_to (const FT_Vector *control,
		 const FT_Vector *to,
		 void *arg)
{
  hb_draw_session_t *drawing = (hb_draw_session_t *) arg;
  drawing->quadratic_to (control->x, control->y,
			 to->x, to->y);
  return FT_Err_Ok;
}

static int
_hb_ft_cubic_to (const FT_Vector *control1,
		 const FT_Vector *control2,
		 const FT_Vector *to,
		 void *arg)
{
  hb_draw_session_t *drawing = (hb_draw_session_t *) arg;
  drawing->cubic_to (control1->x, control1->y,
		     control2->x, control2->y,
		     to->x, to->y);
  return FT_Err_Ok;
}

static void
hb_ft_draw_glyph (hb_font_t *font,
		  void *font_data,
		  hb_codepoint_t glyph,
		  hb_draw_funcs_t *draw_funcs, void *draw_data,
		  void *user_data HB_UNUSED)
{
  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font_data;
  hb_lock_t lock (ft_font->lock);
  FT_Face ft_face = ft_font->ft_face;

  if (unlikely (FT_Load_Glyph (ft_face, glyph,
			       FT_LOAD_NO_BITMAP | ft_font->load_flags)))
    return;

  if (ft_face->glyph->format != FT_GLYPH_FORMAT_OUTLINE)
    return;

  const FT_Outline_Funcs outline_funcs = {
    _hb_ft_move_to,
    _hb_ft_line_to,
    _hb_ft_conic_to,
    _hb_ft_cubic_to,
    0, /* shift */
    0, /* delta */
  };

  hb_draw_session_t draw_session (draw_funcs, draw_data, font->slant_xy);

  /* Embolden */
  if (font->x_strength || font->y_strength)
  {
    FT_Outline_EmboldenXY (&ft_face->glyph->outline, font->x_strength, font->y_strength);

    int x_shift = 0;
    int y_shift = 0;
    if (font->embolden_in_place)
    {
      /* Undo the FreeType shift. */
      x_shift = -font->x_strength / 2;
      y_shift = 0;
      if (font->y_scale < 0) y_shift = -font->y_strength;
    }
    else
    {
      /* FreeType applied things in the wrong direction for negative scale; fix up. */
      if (font->x_scale < 0) x_shift = -font->x_strength;
      if (font->y_scale < 0) y_shift = -font->y_strength;
    }
    if (x_shift || y_shift)
    {
      auto &outline = ft_face->glyph->outline;
      for (auto &point : hb_iter (outline.points, outline.contours[outline.n_contours - 1] + 1))
      {
	point.x += x_shift;
	point.y += y_shift;
      }
    }
  }


  FT_Outline_Decompose (&ft_face->glyph->outline,
			&outline_funcs,
			&draw_session);
}
#endif

#ifndef HB_NO_PAINT
#if (FREETYPE_MAJOR*10000 + FREETYPE_MINOR*100 + FREETYPE_PATCH) >= 21300

#include "hb-ft-colr.hh"

static void
hb_ft_paint_glyph (hb_font_t *font,
                   void *font_data,
                   hb_codepoint_t gid,
                   hb_paint_funcs_t *paint_funcs, void *paint_data,
                   unsigned int palette_index,
                   hb_color_t foreground,
                   void *user_data)
{
  const hb_ft_font_t *ft_font = (const hb_ft_font_t *) font_data;
  hb_lock_t lock (ft_font->lock);
  FT_Face ft_face = ft_font->ft_face;

  /* We release the lock before calling into glyph callbacks, such that
   * eg. draw API can call back into the face.*/

  if (unlikely (FT_Load_Glyph (ft_face, gid,
			       ft_font->load_flags | FT_LOAD_COLOR)))
    return;

  if (ft_face->glyph->format == FT_GLYPH_FORMAT_OUTLINE)
  {
    if (hb_ft_paint_glyph_colr (font, font_data, gid,
				paint_funcs, paint_data,
				palette_index, foreground,
				user_data))
      return;

    /* Simple outline. */
    ft_font->lock.unlock ();
    paint_funcs->push_clip_glyph (paint_data, gid, font);
    ft_font->lock.lock ();
    paint_funcs->color (paint_data, true, foreground);
    paint_funcs->pop_clip (paint_data);

    return;
  }

  auto *glyph = ft_face->glyph;
  if (glyph->format == FT_GLYPH_FORMAT_BITMAP)
  {
    auto &bitmap = glyph->bitmap;
    if (bitmap.pixel_mode == FT_PIXEL_MODE_BGRA)
    {
      if (bitmap.pitch != (signed) bitmap.width * 4)
        return;

      ft_font->lock.unlock ();

      hb_blob_t *blob = hb_blob_create ((const char *) bitmap.buffer,
					bitmap.pitch * bitmap.rows,
					HB_MEMORY_MODE_DUPLICATE,
					nullptr, nullptr);

      hb_glyph_extents_t extents;
      if (!hb_font_get_glyph_extents (font, gid, &extents))
	goto out;

      if (!paint_funcs->image (paint_data,
			       blob,
			       bitmap.width,
			       bitmap.rows,
			       HB_PAINT_IMAGE_FORMAT_BGRA,
			       font->slant_xy,
			       &extents))
      {
        /* TODO Try a forced outline load and paint? */
      }

    out:
      hb_blob_destroy (blob);
      ft_font->lock.lock ();
    }

    return;
  }
}
#endif
#endif


static inline void free_static_ft_funcs ();

static struct hb_ft_font_funcs_lazy_loader_t : hb_font_funcs_lazy_loader_t<hb_ft_font_funcs_lazy_loader_t>
{
  static hb_font_funcs_t *create ()
  {
    hb_font_funcs_t *funcs = hb_font_funcs_create ();

    hb_font_funcs_set_nominal_glyph_func (funcs, hb_ft_get_nominal_glyph, nullptr, nullptr);
    hb_font_funcs_set_nominal_glyphs_func (funcs, hb_ft_get_nominal_glyphs, nullptr, nullptr);
    hb_font_funcs_set_variation_glyph_func (funcs, hb_ft_get_variation_glyph, nullptr, nullptr);

    hb_font_funcs_set_font_h_extents_func (funcs, hb_ft_get_font_h_extents, nullptr, nullptr);
    hb_font_funcs_set_glyph_h_advances_func (funcs, hb_ft_get_glyph_h_advances, nullptr, nullptr);
    //hb_font_funcs_set_glyph_h_origin_func (funcs, hb_ft_get_glyph_h_origin, nullptr, nullptr);

#ifndef HB_NO_VERTICAL
    //hb_font_funcs_set_font_v_extents_func (funcs, hb_ft_get_font_v_extents, nullptr, nullptr);
    hb_font_funcs_set_glyph_v_advance_func (funcs, hb_ft_get_glyph_v_advance, nullptr, nullptr);
    hb_font_funcs_set_glyph_v_origin_func (funcs, hb_ft_get_glyph_v_origin, nullptr, nullptr);
#endif

#ifndef HB_NO_OT_SHAPE_FALLBACK
    hb_font_funcs_set_glyph_h_kerning_func (funcs, hb_ft_get_glyph_h_kerning, nullptr, nullptr);
#endif
    //hb_font_funcs_set_glyph_v_kerning_func (funcs, hb_ft_get_glyph_v_kerning, nullptr, nullptr);
    hb_font_funcs_set_glyph_extents_func (funcs, hb_ft_get_glyph_extents, nullptr, nullptr);
    hb_font_funcs_set_glyph_contour_point_func (funcs, hb_ft_get_glyph_contour_point, nullptr, nullptr);
    hb_font_funcs_set_glyph_name_func (funcs, hb_ft_get_glyph_name, nullptr, nullptr);
    hb_font_funcs_set_glyph_from_name_func (funcs, hb_ft_get_glyph_from_name, nullptr, nullptr);

#ifndef HB_NO_DRAW
    hb_font_funcs_set_draw_glyph_func (funcs, hb_ft_draw_glyph, nullptr, nullptr);
#endif

#ifndef HB_NO_PAINT
#if (FREETYPE_MAJOR*10000 + FREETYPE_MINOR*100 + FREETYPE_PATCH) >= 21300
    hb_font_funcs_set_paint_glyph_func (funcs, hb_ft_paint_glyph, nullptr, nullptr);
#endif
#endif

    hb_font_funcs_make_immutable (funcs);

    hb_atexit (free_static_ft_funcs);

    return funcs;
  }
} static_ft_funcs;

static inline
void free_static_ft_funcs ()
{
  static_ft_funcs.free_instance ();
}

static hb_font_funcs_t *
_hb_ft_get_font_funcs ()
{
  return static_ft_funcs.get_unconst ();
}

static void
_hb_ft_font_set_funcs (hb_font_t *font, FT_Face ft_face, bool unref)
{
  bool symbol = ft_face->charmap && ft_face->charmap->encoding == FT_ENCODING_MS_SYMBOL;

  hb_ft_font_t *ft_font = _hb_ft_font_create (ft_face, symbol, unref);
  if (unlikely (!ft_font)) return;

  hb_font_set_funcs (font,
		     _hb_ft_get_font_funcs (),
		     ft_font,
		     _hb_ft_font_destroy);
}


static hb_blob_t *
_hb_ft_reference_table (hb_face_t *face HB_UNUSED, hb_tag_t tag, void *user_data)
{
  FT_Face ft_face = (FT_Face) user_data;
  FT_Byte *buffer;
  FT_ULong  length = 0;
  FT_Error error;

  /* Note: FreeType like HarfBuzz uses the NONE tag for fetching the entire blob */

  error = FT_Load_Sfnt_Table (ft_face, tag, 0, nullptr, &length);
  if (error)
    return nullptr;

  buffer = (FT_Byte *) hb_malloc (length);
  if (!buffer)
    return nullptr;

  error = FT_Load_Sfnt_Table (ft_face, tag, 0, buffer, &length);
  if (error)
  {
    hb_free (buffer);
    return nullptr;
  }

  return hb_blob_create ((const char *) buffer, length,
			 HB_MEMORY_MODE_WRITABLE,
			 buffer, hb_free);
}

static unsigned
_hb_ft_get_table_tags (const hb_face_t *face HB_UNUSED,
		       unsigned int start_offset,
		       unsigned int *table_count,
		       hb_tag_t *table_tags,
		       void *user_data)
{
  FT_Face ft_face = (FT_Face) user_data;

  FT_ULong population = 0;
  FT_Sfnt_Table_Info (ft_face,
		      0, // table_index; ignored
		      nullptr,
                      &population);

  if (!table_count)
    return population;
  else
    *table_count = 0;

  if (unlikely (start_offset >= population))
    return population;

  unsigned end_offset = hb_min (start_offset + *table_count, (unsigned) population);
  if (unlikely (end_offset < start_offset))
    return population;

  *table_count = end_offset - start_offset;
  for (unsigned i = start_offset; i < end_offset; i++)
  {
    FT_ULong tag = 0, length;
    FT_Sfnt_Table_Info (ft_face, i, &tag, &length);
    table_tags[i - start_offset] = tag;
  }

  return population;
}


/**
 * hb_ft_face_create:
 * @ft_face: (destroy destroy) (scope notified): FT_Face to work upon
 * @destroy: (nullable): A callback to call when the face object is not needed anymore
 *
 * Creates an #hb_face_t face object from the specified FT_Face.
 *
 * Note that this is using the FT_Face object just to get at the underlying
 * font data, and fonts created from the returned #hb_face_t will use the native
 * HarfBuzz font implementation, unless you call hb_ft_font_set_funcs() on them.
 *
 * This variant of the function does not provide any life-cycle management.
 *
 * Most client programs should use hb_ft_face_create_referenced()
 * (or, perhaps, hb_ft_face_create_cached()) instead.
 *
 * If you know you have valid reasons not to use hb_ft_face_create_referenced(),
 * then it is the client program's responsibility to destroy @ft_face
 * after the #hb_face_t face object has been destroyed.
 *
 * Return value: (transfer full): the new #hb_face_t face object
 *
 * Since: 0.9.2
 **/
hb_face_t *
hb_ft_face_create (FT_Face           ft_face,
		   hb_destroy_func_t destroy)
{
  hb_face_t *face;

  if (!ft_face->stream->read) {
    hb_blob_t *blob;

    blob = hb_blob_create ((const char *) ft_face->stream->base,
			   (unsigned int) ft_face->stream->size,
			   HB_MEMORY_MODE_READONLY,
			   ft_face, destroy);
    face = hb_face_create (blob, ft_face->face_index);
    hb_blob_destroy (blob);
  } else {
    face = hb_face_create_for_tables (_hb_ft_reference_table, ft_face, destroy);
    hb_face_set_get_table_tags_func (face, _hb_ft_get_table_tags, ft_face, nullptr);
  }

  hb_face_set_index (face, ft_face->face_index);
  hb_face_set_upem (face, ft_face->units_per_EM);

  return face;
}

/**
 * hb_ft_face_create_referenced:
 * @ft_face: FT_Face to work upon
 *
 * Creates an #hb_face_t face object from the specified FT_Face.
 *
 * Note that this is using the FT_Face object just to get at the underlying
 * font data, and fonts created from the returned #hb_face_t will use the native
 * HarfBuzz font implementation, unless you call hb_ft_font_set_funcs() on them.
 *
 * This is the preferred variant of the hb_ft_face_create*
 * function family, because it calls FT_Reference_Face() on @ft_face,
 * ensuring that @ft_face remains alive as long as the resulting
 * #hb_face_t face object remains alive. Also calls FT_Done_Face()
 * when the #hb_face_t face object is destroyed.
 *
 * Use this version unless you know you have good reasons not to.
 *
 * Return value: (transfer full): the new #hb_face_t face object
 *
 * Since: 0.9.38
 **/
hb_face_t *
hb_ft_face_create_referenced (FT_Face ft_face)
{
  FT_Reference_Face (ft_face);
  return hb_ft_face_create (ft_face, _hb_ft_face_destroy);
}

static void
hb_ft_face_finalize (void *arg)
{
  FT_Face ft_face = (FT_Face) arg;
  hb_face_destroy ((hb_face_t *) ft_face->generic.data);
}

/**
 * hb_ft_face_create_cached:
 * @ft_face: FT_Face to work upon
 *
 * Creates an #hb_face_t face object from the specified FT_Face.
 *
 * Note that this is using the FT_Face object just to get at the underlying
 * font data, and fonts created from the returned #hb_face_t will use the native
 * HarfBuzz font implementation, unless you call hb_ft_font_set_funcs() on them.
 *
 * This variant of the function caches the newly created #hb_face_t
 * face object, using the @generic pointer of @ft_face. Subsequent function
 * calls that are passed the same @ft_face parameter will have the same
 * #hb_face_t returned to them, and that #hb_face_t will be correctly
 * reference counted.
 *
 * However, client programs are still responsible for destroying
 * @ft_face after the last #hb_face_t face object has been destroyed.
 *
 * Return value: (transfer full): the new #hb_face_t face object
 *
 * Since: 0.9.2
 **/
hb_face_t *
hb_ft_face_create_cached (FT_Face ft_face)
{
  if (unlikely (!ft_face->generic.data || ft_face->generic.finalizer != hb_ft_face_finalize))
  {
    if (ft_face->generic.finalizer)
      ft_face->generic.finalizer (ft_face);

    ft_face->generic.data = hb_ft_face_create (ft_face, nullptr);
    ft_face->generic.finalizer = hb_ft_face_finalize;
  }

  return hb_face_reference ((hb_face_t *) ft_face->generic.data);
}

/**
 * hb_ft_font_create:
 * @ft_face: (destroy destroy) (scope notified): FT_Face to work upon
 * @destroy: (nullable): A callback to call when the font object is not needed anymore
 *
 * Creates an #hb_font_t font object from the specified FT_Face.
 *
 * <note>Note: You must set the face size on @ft_face before calling
 * hb_ft_font_create() on it. HarfBuzz assumes size is always set and will
 * access `size` member of FT_Face unconditionally.</note>
 *
 * This variant of the function does not provide any life-cycle management.
 *
 * Most client programs should use hb_ft_font_create_referenced()
 * instead.
 *
 * If you know you have valid reasons not to use hb_ft_font_create_referenced(),
 * then it is the client program's responsibility to destroy @ft_face
 * after the #hb_font_t font object has been destroyed.
 *
 * HarfBuzz will use the @destroy callback on the #hb_font_t font object
 * if it is supplied when you use this function. However, even if @destroy
 * is provided, it is the client program's responsibility to destroy @ft_face,
 * and it is the client program's responsibility to ensure that @ft_face is
 * destroyed only after the #hb_font_t font object has been destroyed.
 *
 * Return value: (transfer full): the new #hb_font_t font object
 *
 * Since: 0.9.2
 **/
hb_font_t *
hb_ft_font_create (FT_Face           ft_face,
		   hb_destroy_func_t destroy)
{
  hb_font_t *font;
  hb_face_t *face;

  face = hb_ft_face_create (ft_face, destroy);
  font = hb_font_create (face);
  hb_face_destroy (face);
  _hb_ft_font_set_funcs (font, ft_face, false);
  hb_ft_font_changed (font);
  return font;
}

/**
 * hb_ft_font_changed:
 * @font: #hb_font_t to work upon
 *
 * Refreshes the state of @font when the underlying FT_Face has changed.
 * This function should be called after changing the size or
 * variation-axis settings on the FT_Face.
 *
 * Since: 1.0.5
 **/
void
hb_ft_font_changed (hb_font_t *font)
{
  if (font->destroy != (hb_destroy_func_t) _hb_ft_font_destroy)
    return;

  hb_ft_font_t *ft_font = (hb_ft_font_t *) font->user_data;

  FT_Face ft_face = ft_font->ft_face;

  hb_font_set_scale (font,
		     (int) (((uint64_t) ft_face->size->metrics.x_scale * (uint64_t) ft_face->units_per_EM + (1u<<15)) >> 16),
		     (int) (((uint64_t) ft_face->size->metrics.y_scale * (uint64_t) ft_face->units_per_EM + (1u<<15)) >> 16));
#if 0 /* hb-ft works in no-hinting model */
  hb_font_set_ppem (font,
		    ft_face->size->metrics.x_ppem,
		    ft_face->size->metrics.y_ppem);
#endif

#if defined(HAVE_FT_GET_VAR_BLEND_COORDINATES) && !defined(HB_NO_VAR)
  FT_MM_Var *mm_var = nullptr;
  if (!FT_Get_MM_Var (ft_face, &mm_var))
  {
    FT_Fixed *ft_coords = (FT_Fixed *) hb_calloc (mm_var->num_axis, sizeof (FT_Fixed));
    int *coords = (int *) hb_calloc (mm_var->num_axis, sizeof (int));
    if (coords && ft_coords)
    {
      if (!FT_Get_Var_Blend_Coordinates (ft_face, mm_var->num_axis, ft_coords))
      {
	bool nonzero = false;

	for (unsigned int i = 0; i < mm_var->num_axis; ++i)
	 {
	  coords[i] = ft_coords[i] >>= 2;
	  nonzero = nonzero || coords[i];
	 }

	if (nonzero)
	  hb_font_set_var_coords_normalized (font, coords, mm_var->num_axis);
	else
	  hb_font_set_var_coords_normalized (font, nullptr, 0);
      }
    }
    hb_free (coords);
    hb_free (ft_coords);
#ifdef HAVE_FT_DONE_MM_VAR
    FT_Done_MM_Var (ft_face->glyph->library, mm_var);
#else
    hb_free (mm_var);
#endif
  }
#endif

  ft_font->advance_cache.clear ();
  ft_font->cached_serial = font->serial;
}

/**
 * hb_ft_hb_font_changed:
 * @font: #hb_font_t to work upon
 *
 * Refreshes the state of the underlying FT_Face of @font when the hb_font_t
 * @font has changed.
 * This function should be called after changing the size or
 * variation-axis settings on the @font.
 * This call is fast if nothing has changed on @font.
 *
 * Return value: true if changed, false otherwise
 *
 * Since: 4.4.0
 **/
hb_bool_t
hb_ft_hb_font_changed (hb_font_t *font)
{
  if (font->destroy != (hb_destroy_func_t) _hb_ft_font_destroy)
    return false;

  hb_ft_font_t *ft_font = (hb_ft_font_t *) font->user_data;

  return _hb_ft_hb_font_check_changed (font, ft_font);
}

/**
 * hb_ft_font_create_referenced:
 * @ft_face: FT_Face to work upon
 *
 * Creates an #hb_font_t font object from the specified FT_Face.
 *
 * <note>Note: You must set the face size on @ft_face before calling
 * hb_ft_font_create_referenced() on it. HarfBuzz assumes size is always set
 * and will access `size` member of FT_Face unconditionally.</note>
 *
 * This is the preferred variant of the hb_ft_font_create*
 * function family, because it calls FT_Reference_Face() on @ft_face,
 * ensuring that @ft_face remains alive as long as the resulting
 * #hb_font_t font object remains alive.
 *
 * Use this version unless you know you have good reasons not to.
 *
 * Return value: (transfer full): the new #hb_font_t font object
 *
 * Since: 0.9.38
 **/
hb_font_t *
hb_ft_font_create_referenced (FT_Face ft_face)
{
  FT_Reference_Face (ft_face);
  return hb_ft_font_create (ft_face, _hb_ft_face_destroy);
}


static void * _hb_ft_alloc (FT_Memory memory, long size)
{ return hb_malloc (size); }

static void _hb_ft_free (FT_Memory memory, void *block)
{ hb_free (block); }

static void * _hb_ft_realloc (FT_Memory memory, long cur_size, long new_size, void *block)
{ return hb_realloc (block, new_size); }

static FT_MemoryRec_ m =
{
  nullptr,
  _hb_ft_alloc,
  _hb_ft_free,
  _hb_ft_realloc
};

static inline void free_static_ft_library ();

static struct hb_ft_library_lazy_loader_t : hb_lazy_loader_t<hb_remove_pointer<FT_Library>,
							     hb_ft_library_lazy_loader_t>
{
  static FT_Library create ()
  {
    FT_Library l;
    if (FT_New_Library (&m, &l))
      return nullptr;

    FT_Add_Default_Modules (l);
    FT_Set_Default_Properties (l);

    hb_atexit (free_static_ft_library);

    return l;
  }
  static void destroy (FT_Library l)
  {
    FT_Done_Library (l);
  }
  static FT_Library get_null ()
  {
    return nullptr;
  }
} static_ft_library;

static inline
void free_static_ft_library ()
{
  static_ft_library.free_instance ();
}

static FT_Library
reference_ft_library ()
{
  FT_Library l = static_ft_library.get_unconst ();
  if (unlikely (FT_Reference_Library (l)))
  {
    DEBUG_MSG (FT, l, "FT_Reference_Library() failed");
    return nullptr;
  }
  return l;
}

static hb_user_data_key_t ft_library_key = {0};

static void
finalize_ft_library (void *arg)
{
  FT_Face ft_face = (FT_Face) arg;
  FT_Done_Library ((FT_Library) ft_face->generic.data);
}

static void
destroy_ft_library (void *arg)
{
  FT_Done_Library ((FT_Library) arg);
}

/**
 * hb_ft_face_create_from_file_or_fail:
 * @file_name: A font filename
 * @index: The index of the face within the file
 *
 * Creates an #hb_face_t face object from the specified
 * font file and face index.
 *
 * This is similar in functionality to hb_face_create_from_file_or_fail(),
 * but uses the FreeType library for loading the font file.
 *
 * Return value: (transfer full): The new face object, or `NULL` if
 * no face is found at the specified index or the file cannot be read.
 *
 * Since: 10.1.0
 */
hb_face_t *
hb_ft_face_create_from_file_or_fail (const char   *file_name,
				     unsigned int  index)
{
  FT_Library ft_library = reference_ft_library ();
  if (unlikely (!ft_library))
  {
    DEBUG_MSG (FT, ft_library, "reference_ft_library failed");
    return nullptr;
  }

  FT_Face ft_face;
  if (unlikely (FT_New_Face (ft_library,
			     file_name,
			     index,
			     &ft_face)))
    return nullptr;

  hb_face_t *face = hb_ft_face_create_referenced (ft_face);
  FT_Done_Face (ft_face);

  ft_face->generic.data = ft_library;
  ft_face->generic.finalizer = finalize_ft_library;

  if (hb_face_is_immutable (face))
    return nullptr;

  return face;
}

static void
_release_blob (void *arg)
{
  FT_Face ft_face = (FT_Face) arg;
  hb_blob_destroy ((hb_blob_t *) ft_face->generic.data);
}

/**
 * hb_ft_font_set_funcs:
 * @font: #hb_font_t to work upon
 *
 * Configures the font-functions structure of the specified
 * #hb_font_t font object to use FreeType font functions.
 *
 * In particular, you can use this function to configure an
 * existing #hb_face_t face object for use with FreeType font
 * functions even if that #hb_face_t face object was initially
 * created with hb_face_create(), and therefore was not
 * initially configured to use FreeType font functions.
 *
 * An #hb_font_t object created with hb_ft_font_create()
 * is preconfigured for FreeType font functions and does not
 * require this function to be used.
 *
 * Note that if you modify the underlying #hb_font_t after
 * calling this function, you need to call hb_ft_hb_font_changed()
 * to update the underlying FT_Face.
 *
 * <note>Note: Internally, this function creates an FT_Face.
* </note>
 *
 * Since: 1.0.5
 **/
void
hb_ft_font_set_funcs (hb_font_t *font)
{
  hb_blob_t *blob = hb_face_reference_blob (font->face);
  unsigned int blob_length;
  const char *blob_data = hb_blob_get_data (blob, &blob_length);
  if (unlikely (!blob_length))
    DEBUG_MSG (FT, font, "Font face has empty blob");

  FT_Library ft_library = reference_ft_library ();
  if (unlikely (!ft_library))
  {
    hb_blob_destroy (blob);
    DEBUG_MSG (FT, font, "reference_ft_library failed");
    return;
  }

  FT_Face ft_face = nullptr;
  if (unlikely (FT_New_Memory_Face (ft_library,
				    (const FT_Byte *) blob_data,
				    blob_length,
				    hb_face_get_index (font->face),
				    &ft_face)))
  {
    hb_blob_destroy (blob);
    DEBUG_MSG (FT, font, "FT_New_Memory_Face() failed");
    return;
  }

  if (FT_Select_Charmap (ft_face, FT_ENCODING_MS_SYMBOL))
    FT_Select_Charmap (ft_face, FT_ENCODING_UNICODE);

  // Hook the blob to the FT_Face
  ft_face->generic.data = blob;
  ft_face->generic.finalizer = _release_blob;

  // And the FT_Library to the blob
  hb_blob_set_user_data (blob, &ft_library_key, ft_library, destroy_ft_library, true);

  _hb_ft_font_set_funcs (font, ft_face, true);
  hb_ft_font_set_load_flags (font, FT_LOAD_DEFAULT | FT_LOAD_NO_HINTING);

  _hb_ft_hb_font_changed (font, ft_face);
}

#endif
