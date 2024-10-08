/*
 * Copyright © 2018  Ebrahim Byagowi
 * Copyright © 2020  Google, Inc.
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
 * Google Author(s): Calder Kitagawa
 */

#ifndef OT_COLOR_COLR_COLR_HH
#define OT_COLOR_COLR_COLR_HH

#include "../../../hb.hh"
#include "../../../hb-open-type.hh"
#include "../../../hb-ot-var-common.hh"
#include "../../../hb-paint.hh"
#include "../../../hb-paint-extents.hh"

/*
 * COLR -- Color
 * https://docs.microsoft.com/en-us/typography/opentype/spec/colr
 */
#define HB_OT_TAG_COLR HB_TAG('C','O','L','R')

namespace OT {
struct hb_paint_context_t;
}

namespace OT {

struct COLR;

struct Paint;

struct hb_paint_context_t :
       hb_dispatch_context_t<hb_paint_context_t>
{
  const char *get_name () { return "PAINT"; }
  template <typename T>
  return_t dispatch (const T &obj) { obj.paint_glyph (this); return hb_empty_t (); }
  static return_t default_return_value () { return hb_empty_t (); }

  const COLR* get_colr_table () const
  { return reinterpret_cast<const COLR *> (base); }

public:
  const void *base;
  hb_paint_funcs_t *funcs;
  void *data;
  hb_font_t *font;
  unsigned int palette_index;
  hb_color_t foreground;
  ItemVarStoreInstancer &instancer;
  hb_map_t current_glyphs;
  hb_map_t current_layers;
  int depth_left = HB_MAX_NESTING_LEVEL;
  int edge_count = HB_MAX_GRAPH_EDGE_COUNT;

  hb_paint_context_t (const void *base_,
		      hb_paint_funcs_t *funcs_,
		      void *data_,
                      hb_font_t *font_,
                      unsigned int palette_,
                      hb_color_t foreground_,
		      ItemVarStoreInstancer &instancer_) :
    base (base_),
    funcs (funcs_),
    data (data_),
    font (font_),
    palette_index (palette_),
    foreground (foreground_),
    instancer (instancer_)
  { }

  hb_color_t get_color (unsigned int color_index, float alpha, hb_bool_t *is_foreground)
  {
    hb_color_t color = foreground;

    *is_foreground = true;

    if (color_index != 0xffff)
    {
      if (!funcs->custom_palette_color (data, color_index, &color))
      {
	unsigned int clen = 1;
	hb_face_t *face = hb_font_get_face (font);

	hb_ot_color_palette_get_colors (face, palette_index, color_index, &clen, &color);
      }

      *is_foreground = false;
    }

    return HB_COLOR (hb_color_get_blue (color),
                     hb_color_get_green (color),
                     hb_color_get_red (color),
                     hb_color_get_alpha (color) * alpha);
  }

  inline void recurse (const Paint &paint);
};

struct hb_colrv1_closure_context_t :
       hb_dispatch_context_t<hb_colrv1_closure_context_t>
{
  template <typename T>
  return_t dispatch (const T &obj)
  {
    if (unlikely (nesting_level_left == 0))
      return hb_empty_t ();

    if (paint_visited (&obj))
      return hb_empty_t ();

    nesting_level_left--;
    obj.closurev1 (this);
    nesting_level_left++;
    return hb_empty_t ();
  }
  static return_t default_return_value () { return hb_empty_t (); }

  bool paint_visited (const void *paint)
  {
    hb_codepoint_t delta = (hb_codepoint_t) ((uintptr_t) paint - (uintptr_t) base);
    if (visited_paint.in_error() || visited_paint.has (delta))
      return true;

    visited_paint.add (delta);
    return false;
  }

  const COLR* get_colr_table () const
  { return reinterpret_cast<const COLR *> (base); }

  void add_glyph (unsigned glyph_id)
  { glyphs->add (glyph_id); }

  void add_layer_indices (unsigned first_layer_index, unsigned num_of_layers)
  { layer_indices->add_range (first_layer_index, first_layer_index + num_of_layers - 1); }

  void add_palette_index (unsigned palette_index)
  { palette_indices->add (palette_index); }

  void add_var_idxes (unsigned first_var_idx, unsigned num_idxes)
  {
    if (!num_idxes || first_var_idx == VarIdx::NO_VARIATION) return;
    variation_indices->add_range (first_var_idx, first_var_idx + num_idxes - 1);
  }

  public:
  const void *base;
  hb_set_t visited_paint;
  hb_set_t *glyphs;
  hb_set_t *layer_indices;
  hb_set_t *palette_indices;
  hb_set_t *variation_indices;
  unsigned num_var_idxes;
  unsigned nesting_level_left;

  hb_colrv1_closure_context_t (const void *base_,
                               hb_set_t *glyphs_,
                               hb_set_t *layer_indices_,
                               hb_set_t *palette_indices_,
                               hb_set_t *variation_indices_,
                               unsigned num_var_idxes_ = 1,
                               unsigned nesting_level_left_ = HB_MAX_NESTING_LEVEL) :
                          base (base_),
                          glyphs (glyphs_),
                          layer_indices (layer_indices_),
                          palette_indices (palette_indices_),
                          variation_indices (variation_indices_),
                          num_var_idxes (num_var_idxes_),
                          nesting_level_left (nesting_level_left_)
  {}
};

struct LayerRecord
{
  operator hb_ot_color_layer_t () const { return {glyphId, colorIdx}; }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  public:
  HBGlyphID16	glyphId;	/* Glyph ID of layer glyph */
  Index		colorIdx;	/* Index value to use with a
				 * selected color palette.
				 * An index value of 0xFFFF
				 * is a special case indicating
				 * that the text foreground
				 * color (defined by a
				 * higher-level client) should
				 * be used and shall not be
				 * treated as actual index
				 * into CPAL ColorRecord array. */
  public:
  DEFINE_SIZE_STATIC (4);
};

struct BaseGlyphRecord
{
  int cmp (hb_codepoint_t g) const
  { return g < glyphId ? -1 : g > glyphId ? 1 : 0; }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  public:
  HBGlyphID16	glyphId;	/* Glyph ID of reference glyph */
  HBUINT16	firstLayerIdx;	/* Index (from beginning of
				 * the Layer Records) to the
				 * layer record. There will be
				 * numLayers consecutive entries
				 * for this base glyph. */
  HBUINT16	numLayers;	/* Number of color layers
				 * associated with this glyph */
  public:
  DEFINE_SIZE_STATIC (6);
};

template <typename T>
struct Variable
{
  static constexpr bool is_variable = true;

  Variable<T>* copy (hb_serialize_context_t *c) const
  {
    TRACE_SERIALIZE (this);
    return_trace (c->embed (this));
  }

  void closurev1 (hb_colrv1_closure_context_t* c) const
  {
    c->num_var_idxes = 0;
    // update c->num_var_idxes during value closure
    value.closurev1 (c);
    c->add_var_idxes (varIdxBase, c->num_var_idxes);
  }

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer) const
  {
    TRACE_SUBSET (this);
    if (!value.subset (c, instancer, varIdxBase)) return_trace (false);
    if (c->plan->all_axes_pinned)
      return_trace (true);

    VarIdx new_varidx;
    new_varidx = varIdxBase;
    if (varIdxBase != VarIdx::NO_VARIATION)
    {
      hb_pair_t<unsigned, int> *new_varidx_delta;
      if (!c->plan->colrv1_variation_idx_delta_map.has (varIdxBase, &new_varidx_delta))
        return_trace (false);

      new_varidx = hb_first (*new_varidx_delta);
    }

    return_trace (c->serializer->embed (new_varidx));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && value.sanitize (c));
  }

  void paint_glyph (hb_paint_context_t *c) const
  {
    TRACE_PAINT (this);
    value.paint_glyph (c, varIdxBase);
  }

  void get_color_stop (hb_paint_context_t *c,
                       hb_color_stop_t *stop,
		       const ItemVarStoreInstancer &instancer) const
  {
    value.get_color_stop (c, stop, varIdxBase, instancer);
  }

  hb_paint_extend_t get_extend () const
  {
    return value.get_extend ();
  }

  protected:
  T      value;
  public:
  VarIdx varIdxBase;
  public:
  DEFINE_SIZE_MIN (VarIdx::static_size + T::min_size);
};

template <typename T>
struct NoVariable
{
  static constexpr bool is_variable = false;

  static constexpr uint32_t varIdxBase = VarIdx::NO_VARIATION;

  NoVariable<T>* copy (hb_serialize_context_t *c) const
  {
    TRACE_SERIALIZE (this);
    return_trace (c->embed (this));
  }

  void closurev1 (hb_colrv1_closure_context_t* c) const
  { value.closurev1 (c); }

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer) const
  {
    TRACE_SUBSET (this);
    return_trace (value.subset (c, instancer, varIdxBase));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && value.sanitize (c));
  }

  void paint_glyph (hb_paint_context_t *c) const
  {
    TRACE_PAINT (this);
    value.paint_glyph (c, varIdxBase);
  }

  void get_color_stop (hb_paint_context_t *c,
                       hb_color_stop_t *stop,
		       const ItemVarStoreInstancer &instancer) const
  {
    value.get_color_stop (c, stop, VarIdx::NO_VARIATION, instancer);
  }

  hb_paint_extend_t get_extend () const
  {
    return value.get_extend ();
  }

  T      value;
  public:
  DEFINE_SIZE_MIN (T::min_size);
};

// Color structures

struct ColorStop
{
  void closurev1 (hb_colrv1_closure_context_t* c) const
  {
    c->add_palette_index (paletteIndex);
    c->num_var_idxes = 2;
  }

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer,
               uint32_t varIdxBase) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (*this);
    if (unlikely (!out)) return_trace (false);

    if (instancer && !c->plan->pinned_at_default && varIdxBase != VarIdx::NO_VARIATION)
    {
      out->stopOffset.set_float (stopOffset.to_float(instancer (varIdxBase, 0)));
      out->alpha.set_float (alpha.to_float (instancer (varIdxBase, 1)));
    }

    return_trace (c->serializer->check_assign (out->paletteIndex, c->plan->colr_palettes.get (paletteIndex),
                                               HB_SERIALIZE_ERROR_INT_OVERFLOW));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  void get_color_stop (hb_paint_context_t *c,
                       hb_color_stop_t *out,
		       uint32_t varIdx,
		       const ItemVarStoreInstancer &instancer) const
  {
    out->offset = stopOffset.to_float(instancer (varIdx, 0));
    out->color = c->get_color (paletteIndex,
                               alpha.to_float (instancer (varIdx, 1)),
                               &out->is_foreground);
  }

  F2DOT14	stopOffset;
  HBUINT16	paletteIndex;
  F2DOT14	alpha;
  public:
  DEFINE_SIZE_STATIC (2 + 2 * F2DOT14::static_size);
};

struct Extend : HBUINT8
{
  enum {
    EXTEND_PAD     = 0,
    EXTEND_REPEAT  = 1,
    EXTEND_REFLECT = 2,
  };
  public:
  DEFINE_SIZE_STATIC (1);
};

template <template<typename> class Var>
struct ColorLine
{
  void closurev1 (hb_colrv1_closure_context_t* c) const
  {
    for (const auto &stop : stops.iter ())
      stop.closurev1 (c);
  }

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    if (!c->serializer->check_assign (out->extend, extend, HB_SERIALIZE_ERROR_INT_OVERFLOW)) return_trace (false);
    if (!c->serializer->check_assign (out->stops.len, stops.len, HB_SERIALIZE_ERROR_ARRAY_OVERFLOW)) return_trace (false);

    for (const auto& stop : stops.iter ())
    {
      if (!stop.subset (c, instancer)) return_trace (false);
    }
    return_trace (true);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
                  stops.sanitize (c));
  }

  /* get up to count stops from start */
  unsigned int
  get_color_stops (hb_paint_context_t *c,
                   unsigned int start,
		   unsigned int *count,
		   hb_color_stop_t *color_stops,
		   const ItemVarStoreInstancer &instancer) const
  {
    unsigned int len = stops.len;

    if (count && color_stops)
    {
      unsigned int i;
      for (i = 0; i < *count && start + i < len; i++)
        stops[start + i].get_color_stop (c, &color_stops[i], instancer);
      *count = i;
    }

    return len;
  }

  HB_INTERNAL static unsigned int static_get_color_stops (hb_color_line_t *color_line,
							  void *color_line_data,
							  unsigned int start,
							  unsigned int *count,
							  hb_color_stop_t *color_stops,
							  void *user_data)
  {
    const ColorLine *thiz = (const ColorLine *) color_line_data;
    hb_paint_context_t *c = (hb_paint_context_t *) user_data;
    return thiz->get_color_stops (c, start, count, color_stops, c->instancer);
  }

  hb_paint_extend_t get_extend () const
  {
    return (hb_paint_extend_t) (unsigned int) extend;
  }

  HB_INTERNAL static hb_paint_extend_t static_get_extend (hb_color_line_t *color_line,
							  void *color_line_data,
							  void *user_data)
  {
    const ColorLine *thiz = (const ColorLine *) color_line_data;
    return thiz->get_extend ();
  }

  Extend	extend;
  Array16Of<Var<ColorStop>>	stops;
  public:
  DEFINE_SIZE_ARRAY_SIZED (3, stops);
};

// Composition modes

// Compositing modes are taken from https://www.w3.org/TR/compositing-1/
// NOTE: a brief audit of major implementations suggests most support most
// or all of the specified modes.
struct CompositeMode : HBUINT8
{
  enum {
    // Porter-Duff modes
    // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators
    COMPOSITE_CLEAR          =  0,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_clear
    COMPOSITE_SRC            =  1,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_src
    COMPOSITE_DEST           =  2,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_dst
    COMPOSITE_SRC_OVER       =  3,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_srcover
    COMPOSITE_DEST_OVER      =  4,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_dstover
    COMPOSITE_SRC_IN         =  5,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_srcin
    COMPOSITE_DEST_IN        =  6,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_dstin
    COMPOSITE_SRC_OUT        =  7,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_srcout
    COMPOSITE_DEST_OUT       =  8,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_dstout
    COMPOSITE_SRC_ATOP       =  9,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_srcatop
    COMPOSITE_DEST_ATOP      = 10,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_dstatop
    COMPOSITE_XOR            = 11,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_xor
    COMPOSITE_PLUS           = 12,  // https://www.w3.org/TR/compositing-1/#porterduffcompositingoperators_plus

    // Blend modes
    // https://www.w3.org/TR/compositing-1/#blending
    COMPOSITE_SCREEN         = 13,  // https://www.w3.org/TR/compositing-1/#blendingscreen
    COMPOSITE_OVERLAY        = 14,  // https://www.w3.org/TR/compositing-1/#blendingoverlay
    COMPOSITE_DARKEN         = 15,  // https://www.w3.org/TR/compositing-1/#blendingdarken
    COMPOSITE_LIGHTEN        = 16,  // https://www.w3.org/TR/compositing-1/#blendinglighten
    COMPOSITE_COLOR_DODGE    = 17,  // https://www.w3.org/TR/compositing-1/#blendingcolordodge
    COMPOSITE_COLOR_BURN     = 18,  // https://www.w3.org/TR/compositing-1/#blendingcolorburn
    COMPOSITE_HARD_LIGHT     = 19,  // https://www.w3.org/TR/compositing-1/#blendinghardlight
    COMPOSITE_SOFT_LIGHT     = 20,  // https://www.w3.org/TR/compositing-1/#blendingsoftlight
    COMPOSITE_DIFFERENCE     = 21,  // https://www.w3.org/TR/compositing-1/#blendingdifference
    COMPOSITE_EXCLUSION      = 22,  // https://www.w3.org/TR/compositing-1/#blendingexclusion
    COMPOSITE_MULTIPLY       = 23,  // https://www.w3.org/TR/compositing-1/#blendingmultiply

    // Modes that, uniquely, do not operate on components
    // https://www.w3.org/TR/compositing-1/#blendingnonseparable
    COMPOSITE_HSL_HUE        = 24,  // https://www.w3.org/TR/compositing-1/#blendinghue
    COMPOSITE_HSL_SATURATION = 25,  // https://www.w3.org/TR/compositing-1/#blendingsaturation
    COMPOSITE_HSL_COLOR      = 26,  // https://www.w3.org/TR/compositing-1/#blendingcolor
    COMPOSITE_HSL_LUMINOSITY = 27,  // https://www.w3.org/TR/compositing-1/#blendingluminosity
  };
  public:
  DEFINE_SIZE_STATIC (1);
};

struct Affine2x3
{
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  void closurev1 (hb_colrv1_closure_context_t* c) const
  { c->num_var_idxes = 6; }

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer,
               uint32_t varIdxBase) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (*this);
    if (unlikely (!out)) return_trace (false);
    if (instancer && !c->plan->pinned_at_default && varIdxBase != VarIdx::NO_VARIATION)
    {
      out->xx.set_float (xx.to_float(instancer (varIdxBase, 0)));
      out->yx.set_float (yx.to_float(instancer (varIdxBase, 1)));
      out->xy.set_float (xy.to_float(instancer (varIdxBase, 2)));
      out->yy.set_float (yy.to_float(instancer (varIdxBase, 3)));
      out->dx.set_float (dx.to_float(instancer (varIdxBase, 4)));
      out->dy.set_float (dy.to_float(instancer (varIdxBase, 5)));
    }
    return_trace (true);
  }

  void paint_glyph (hb_paint_context_t *c, uint32_t varIdxBase) const
  {
    TRACE_PAINT (this);
    c->funcs->push_transform (c->data,
			      xx.to_float (c->instancer (varIdxBase, 0)),
			      yx.to_float (c->instancer (varIdxBase, 1)),
                              xy.to_float (c->instancer (varIdxBase, 2)),
			      yy.to_float (c->instancer (varIdxBase, 3)),
                              dx.to_float (c->instancer (varIdxBase, 4)),
			      dy.to_float (c->instancer (varIdxBase, 5)));
  }

  F16DOT16 xx;
  F16DOT16 yx;
  F16DOT16 xy;
  F16DOT16 yy;
  F16DOT16 dx;
  F16DOT16 dy;
  public:
  DEFINE_SIZE_STATIC (6 * F16DOT16::static_size);
};

struct PaintColrLayers
{
  void closurev1 (hb_colrv1_closure_context_t* c) const;

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer HB_UNUSED) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);
    return_trace (c->serializer->check_assign (out->firstLayerIndex, c->plan->colrv1_layers.get (firstLayerIndex),
                                               HB_SERIALIZE_ERROR_INT_OVERFLOW));

    return_trace (true);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  inline void paint_glyph (hb_paint_context_t *c) const;

  HBUINT8	format; /* format = 1 */
  HBUINT8	numLayers;
  HBUINT32	firstLayerIndex;  /* index into COLRv1::layerList */
  public:
  DEFINE_SIZE_STATIC (6);
};

struct PaintSolid
{
  void closurev1 (hb_colrv1_closure_context_t* c) const
  {
    c->add_palette_index (paletteIndex);
    c->num_var_idxes = 1;
  }

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer,
               uint32_t varIdxBase) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (*this);
    if (unlikely (!out)) return_trace (false);

    if (instancer && !c->plan->pinned_at_default && varIdxBase != VarIdx::NO_VARIATION)
      out->alpha.set_float (alpha.to_float (instancer (varIdxBase, 0)));

    if (format == 3 && c->plan->all_axes_pinned)
        out->format = 2;

    return_trace (c->serializer->check_assign (out->paletteIndex, c->plan->colr_palettes.get (paletteIndex),
                                               HB_SERIALIZE_ERROR_INT_OVERFLOW));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  void paint_glyph (hb_paint_context_t *c, uint32_t varIdxBase) const
  {
    TRACE_PAINT (this);
    hb_bool_t is_foreground;
    hb_color_t color;

    color = c->get_color (paletteIndex,
                          alpha.to_float (c->instancer (varIdxBase, 0)),
                          &is_foreground);
    c->funcs->color (c->data, is_foreground, color);
  }

  HBUINT8	format; /* format = 2(noVar) or 3(Var)*/
  HBUINT16	paletteIndex;
  F2DOT14	alpha;
  public:
  DEFINE_SIZE_STATIC (3 + F2DOT14::static_size);
};

template <template<typename> class Var>
struct PaintLinearGradient
{
  void closurev1 (hb_colrv1_closure_context_t* c) const
  {
    (this+colorLine).closurev1 (c);
    c->num_var_idxes = 6;
  }

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer,
               uint32_t varIdxBase) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    if (instancer && !c->plan->pinned_at_default && varIdxBase != VarIdx::NO_VARIATION)
    {
      out->x0 = x0 + (int) roundf (instancer (varIdxBase, 0));
      out->y0 = y0 + (int) roundf (instancer (varIdxBase, 1));
      out->x1 = x1 + (int) roundf (instancer (varIdxBase, 2));
      out->y1 = y1 + (int) roundf (instancer (varIdxBase, 3));
      out->x2 = x2 + (int) roundf (instancer (varIdxBase, 4));
      out->y2 = y2 + (int) roundf (instancer (varIdxBase, 5));
    }

    if (format == 5 && c->plan->all_axes_pinned)
        out->format = 4;

    return_trace (out->colorLine.serialize_subset (c, colorLine, this, instancer));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && colorLine.sanitize (c, this));
  }

  void paint_glyph (hb_paint_context_t *c, uint32_t varIdxBase) const
  {
    TRACE_PAINT (this);
    hb_color_line_t cl = {
      (void *) &(this+colorLine),
      (this+colorLine).static_get_color_stops, c,
      (this+colorLine).static_get_extend, nullptr
    };

    c->funcs->linear_gradient (c->data, &cl,
			       x0 + c->instancer (varIdxBase, 0),
			       y0 + c->instancer (varIdxBase, 1),
			       x1 + c->instancer (varIdxBase, 2),
			       y1 + c->instancer (varIdxBase, 3),
			       x2 + c->instancer (varIdxBase, 4),
			       y2 + c->instancer (varIdxBase, 5));
  }

  HBUINT8			format; /* format = 4(noVar) or 5 (Var) */
  Offset24To<ColorLine<Var>>	colorLine; /* Offset (from beginning of PaintLinearGradient
                                            * table) to ColorLine subtable. */
  FWORD			x0;
  FWORD			y0;
  FWORD			x1;
  FWORD			y1;
  FWORD			x2;
  FWORD			y2;
  public:
  DEFINE_SIZE_STATIC (4 + 6 * FWORD::static_size);
};

template <template<typename> class Var>
struct PaintRadialGradient
{
  void closurev1 (hb_colrv1_closure_context_t* c) const
  {
    (this+colorLine).closurev1 (c);
    c->num_var_idxes = 6;
  }

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer,
               uint32_t varIdxBase) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    if (instancer && !c->plan->pinned_at_default && varIdxBase != VarIdx::NO_VARIATION)
    {
      out->x0 = x0 + (int) roundf (instancer (varIdxBase, 0));
      out->y0 = y0 + (int) roundf (instancer (varIdxBase, 1));
      out->radius0 = radius0 + (unsigned) roundf (instancer (varIdxBase, 2));
      out->x1 = x1 + (int) roundf (instancer (varIdxBase, 3));
      out->y1 = y1 + (int) roundf (instancer (varIdxBase, 4));
      out->radius1 = radius1 + (unsigned) roundf (instancer (varIdxBase, 5));
    }

    if (format == 7 && c->plan->all_axes_pinned)
        out->format = 6;

    return_trace (out->colorLine.serialize_subset (c, colorLine, this, instancer));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && colorLine.sanitize (c, this));
  }

  void paint_glyph (hb_paint_context_t *c, uint32_t varIdxBase) const
  {
    TRACE_PAINT (this);
    hb_color_line_t cl = {
      (void *) &(this+colorLine),
      (this+colorLine).static_get_color_stops, c,
      (this+colorLine).static_get_extend, nullptr
    };

    c->funcs->radial_gradient (c->data, &cl,
			       x0 + c->instancer (varIdxBase, 0),
			       y0 + c->instancer (varIdxBase, 1),
			       radius0 + c->instancer (varIdxBase, 2),
			       x1 + c->instancer (varIdxBase, 3),
			       y1 + c->instancer (varIdxBase, 4),
			       radius1 + c->instancer (varIdxBase, 5));
  }

  HBUINT8			format; /* format = 6(noVar) or 7 (Var) */
  Offset24To<ColorLine<Var>>	colorLine; /* Offset (from beginning of PaintRadialGradient
                                            * table) to ColorLine subtable. */
  FWORD			x0;
  FWORD			y0;
  UFWORD		radius0;
  FWORD			x1;
  FWORD			y1;
  UFWORD		radius1;
  public:
  DEFINE_SIZE_STATIC (4 + 6 * FWORD::static_size);
};

template <template<typename> class Var>
struct PaintSweepGradient
{
  void closurev1 (hb_colrv1_closure_context_t* c) const
  {
    (this+colorLine).closurev1 (c);
    c->num_var_idxes = 4;
  }

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer,
               uint32_t varIdxBase) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    if (instancer && !c->plan->pinned_at_default && varIdxBase != VarIdx::NO_VARIATION)
    {
      out->centerX = centerX + (int) roundf (instancer (varIdxBase, 0));
      out->centerY = centerY + (int) roundf (instancer (varIdxBase, 1));
      out->startAngle.set_float (startAngle.to_float (instancer (varIdxBase, 2)));
      out->endAngle.set_float (endAngle.to_float (instancer (varIdxBase, 3)));
    }

    if (format == 9 && c->plan->all_axes_pinned)
        out->format = 8;

    return_trace (out->colorLine.serialize_subset (c, colorLine, this, instancer));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && colorLine.sanitize (c, this));
  }

  void paint_glyph (hb_paint_context_t *c, uint32_t varIdxBase) const
  {
    TRACE_PAINT (this);
    hb_color_line_t cl = {
      (void *) &(this+colorLine),
      (this+colorLine).static_get_color_stops, c,
      (this+colorLine).static_get_extend, nullptr
    };

    c->funcs->sweep_gradient (c->data, &cl,
			      centerX + c->instancer (varIdxBase, 0),
			      centerY + c->instancer (varIdxBase, 1),
                              (startAngle.to_float (c->instancer (varIdxBase, 2)) + 1) * HB_PI,
                              (endAngle.to_float   (c->instancer (varIdxBase, 3)) + 1) * HB_PI);
  }

  HBUINT8			format; /* format = 8(noVar) or 9 (Var) */
  Offset24To<ColorLine<Var>>	colorLine; /* Offset (from beginning of PaintSweepGradient
                                            * table) to ColorLine subtable. */
  FWORD			centerX;
  FWORD			centerY;
  F2DOT14		startAngle;
  F2DOT14		endAngle;
  public:
  DEFINE_SIZE_STATIC (4 + 2 * FWORD::static_size + 2 * F2DOT14::static_size);
};

// Paint a non-COLR glyph, filled as indicated by paint.
struct PaintGlyph
{
  void closurev1 (hb_colrv1_closure_context_t* c) const;

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    if (! c->serializer->check_assign (out->gid, c->plan->glyph_map->get (gid),
                                       HB_SERIALIZE_ERROR_INT_OVERFLOW))
      return_trace (false);

    return_trace (out->paint.serialize_subset (c, paint, this, instancer));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && paint.sanitize (c, this));
  }

  void paint_glyph (hb_paint_context_t *c) const
  {
    TRACE_PAINT (this);
    c->funcs->push_inverse_root_transform (c->data, c->font);
    c->funcs->push_clip_glyph (c->data, gid, c->font);
    c->funcs->push_root_transform (c->data, c->font);
    c->recurse (this+paint);
    c->funcs->pop_transform (c->data);
    c->funcs->pop_clip (c->data);
    c->funcs->pop_transform (c->data);
  }

  HBUINT8		format; /* format = 10 */
  Offset24To<Paint>	paint;  /* Offset (from beginning of PaintGlyph table) to Paint subtable. */
  HBUINT16		gid;
  public:
  DEFINE_SIZE_STATIC (6);
};

struct PaintColrGlyph
{
  void closurev1 (hb_colrv1_closure_context_t* c) const;

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer HB_UNUSED) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    return_trace (c->serializer->check_assign (out->gid, c->plan->glyph_map->get (gid),
                                               HB_SERIALIZE_ERROR_INT_OVERFLOW));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  inline void paint_glyph (hb_paint_context_t *c) const;

  HBUINT8	format; /* format = 11 */
  HBUINT16	gid;
  public:
  DEFINE_SIZE_STATIC (3);
};

template <template<typename> class Var>
struct PaintTransform
{
  HB_INTERNAL void closurev1 (hb_colrv1_closure_context_t* c) const;

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);
    if (!out->transform.serialize_subset (c, transform, this, instancer)) return_trace (false);
    if (format == 13 && c->plan->all_axes_pinned)
      out->format = 12;
    return_trace (out->src.serialize_subset (c, src, this, instancer));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
                  src.sanitize (c, this) &&
                  transform.sanitize (c, this));
  }

  void paint_glyph (hb_paint_context_t *c) const
  {
    TRACE_PAINT (this);
    (this+transform).paint_glyph (c);
    c->recurse (this+src);
    c->funcs->pop_transform (c->data);
  }

  HBUINT8			format; /* format = 12(noVar) or 13 (Var) */
  Offset24To<Paint>		src; /* Offset (from beginning of PaintTransform table) to Paint subtable. */
  Offset24To<Var<Affine2x3>>	transform;
  public:
  DEFINE_SIZE_STATIC (7);
};

struct PaintTranslate
{
  HB_INTERNAL void closurev1 (hb_colrv1_closure_context_t* c) const;

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer,
               uint32_t varIdxBase) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    if (instancer && !c->plan->pinned_at_default && varIdxBase != VarIdx::NO_VARIATION)
    {
      out->dx = dx + (int) roundf (instancer (varIdxBase, 0));
      out->dy = dy + (int) roundf (instancer (varIdxBase, 1));
    }

    if (format == 15 && c->plan->all_axes_pinned)
        out->format = 14;

    return_trace (out->src.serialize_subset (c, src, this, instancer));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && src.sanitize (c, this));
  }

  void paint_glyph (hb_paint_context_t *c, uint32_t varIdxBase) const
  {
    TRACE_PAINT (this);
    float ddx = dx + c->instancer (varIdxBase, 0);
    float ddy = dy + c->instancer (varIdxBase, 1);

    bool p1 = c->funcs->push_translate (c->data, ddx, ddy);
    c->recurse (this+src);
    if (p1) c->funcs->pop_transform (c->data);
  }

  HBUINT8		format; /* format = 14(noVar) or 15 (Var) */
  Offset24To<Paint>	src; /* Offset (from beginning of PaintTranslate table) to Paint subtable. */
  FWORD		dx;
  FWORD		dy;
  public:
  DEFINE_SIZE_STATIC (4 + 2 * FWORD::static_size);
};

struct PaintScale
{
  HB_INTERNAL void closurev1 (hb_colrv1_closure_context_t* c) const;

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer,
               uint32_t varIdxBase) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    if (instancer && !c->plan->pinned_at_default && varIdxBase != VarIdx::NO_VARIATION)
    {
      out->scaleX.set_float (scaleX.to_float (instancer (varIdxBase, 0)));
      out->scaleY.set_float (scaleY.to_float (instancer (varIdxBase, 1)));
    }

    if (format == 17 && c->plan->all_axes_pinned)
        out->format = 16;

    return_trace (out->src.serialize_subset (c, src, this, instancer));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && src.sanitize (c, this));
  }

  void paint_glyph (hb_paint_context_t *c, uint32_t varIdxBase) const
  {
    TRACE_PAINT (this);
    float sx = scaleX.to_float (c->instancer (varIdxBase, 0));
    float sy = scaleY.to_float (c->instancer (varIdxBase, 1));

    bool p1 = c->funcs->push_scale (c->data, sx, sy);
    c->recurse (this+src);
    if (p1) c->funcs->pop_transform (c->data);
  }

  HBUINT8		format; /* format = 16 (noVar) or 17(Var) */
  Offset24To<Paint>	src; /* Offset (from beginning of PaintScale table) to Paint subtable. */
  F2DOT14		scaleX;
  F2DOT14		scaleY;
  public:
  DEFINE_SIZE_STATIC (4 + 2 * F2DOT14::static_size);
};

struct PaintScaleAroundCenter
{
  HB_INTERNAL void closurev1 (hb_colrv1_closure_context_t* c) const;

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer,
               uint32_t varIdxBase) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    if (instancer && !c->plan->pinned_at_default && varIdxBase != VarIdx::NO_VARIATION)
    {
      out->scaleX.set_float (scaleX.to_float (instancer (varIdxBase, 0)));
      out->scaleY.set_float (scaleY.to_float (instancer (varIdxBase, 1)));
      out->centerX = centerX + (int) roundf (instancer (varIdxBase, 2));
      out->centerY = centerY + (int) roundf (instancer (varIdxBase, 3));
    }

    if (format == 19 && c->plan->all_axes_pinned)
        out->format = 18;

    return_trace (out->src.serialize_subset (c, src, this, instancer));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && src.sanitize (c, this));
  }

  void paint_glyph (hb_paint_context_t *c, uint32_t varIdxBase) const
  {
    TRACE_PAINT (this);
    float sx = scaleX.to_float (c->instancer (varIdxBase, 0));
    float sy = scaleY.to_float (c->instancer (varIdxBase, 1));
    float tCenterX = centerX + c->instancer (varIdxBase, 2);
    float tCenterY = centerY + c->instancer (varIdxBase, 3);

    bool p1 = c->funcs->push_translate (c->data, +tCenterX, +tCenterY);
    bool p2 = c->funcs->push_scale (c->data, sx, sy);
    bool p3 = c->funcs->push_translate (c->data, -tCenterX, -tCenterY);
    c->recurse (this+src);
    if (p3) c->funcs->pop_transform (c->data);
    if (p2) c->funcs->pop_transform (c->data);
    if (p1) c->funcs->pop_transform (c->data);
  }

  HBUINT8		format; /* format = 18 (noVar) or 19(Var) */
  Offset24To<Paint>	src; /* Offset (from beginning of PaintScaleAroundCenter table) to Paint subtable. */
  F2DOT14	scaleX;
  F2DOT14	scaleY;
  FWORD		centerX;
  FWORD		centerY;
  public:
  DEFINE_SIZE_STATIC (4 + 2 * F2DOT14::static_size + 2 * FWORD::static_size);
};

struct PaintScaleUniform
{
  HB_INTERNAL void closurev1 (hb_colrv1_closure_context_t* c) const;

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer,
               uint32_t varIdxBase) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    if (instancer && !c->plan->pinned_at_default && varIdxBase != VarIdx::NO_VARIATION)
      out->scale.set_float (scale.to_float (instancer (varIdxBase, 0)));

    if (format == 21 && c->plan->all_axes_pinned)
        out->format = 20;

    return_trace (out->src.serialize_subset (c, src, this, instancer));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && src.sanitize (c, this));
  }

  void paint_glyph (hb_paint_context_t *c, uint32_t varIdxBase) const
  {
    TRACE_PAINT (this);
    float s = scale.to_float (c->instancer (varIdxBase, 0));

    bool p1 = c->funcs->push_scale (c->data, s, s);
    c->recurse (this+src);
    if (p1) c->funcs->pop_transform (c->data);
  }

  HBUINT8		format; /* format = 20 (noVar) or 21(Var) */
  Offset24To<Paint>	src; /* Offset (from beginning of PaintScaleUniform table) to Paint subtable. */
  F2DOT14		scale;
  public:
  DEFINE_SIZE_STATIC (4 + F2DOT14::static_size);
};

struct PaintScaleUniformAroundCenter
{
  HB_INTERNAL void closurev1 (hb_colrv1_closure_context_t* c) const;

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer,
               uint32_t varIdxBase) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    if (instancer && !c->plan->pinned_at_default && varIdxBase != VarIdx::NO_VARIATION)
    {
      out->scale.set_float (scale.to_float (instancer (varIdxBase, 0)));
      out->centerX = centerX + (int) roundf (instancer (varIdxBase, 1));
      out->centerY = centerY + (int) roundf (instancer (varIdxBase, 2));
    }

    if (format == 23 && c->plan->all_axes_pinned)
        out->format = 22;

    return_trace (out->src.serialize_subset (c, src, this, instancer));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && src.sanitize (c, this));
  }

  void paint_glyph (hb_paint_context_t *c, uint32_t varIdxBase) const
  {
    TRACE_PAINT (this);
    float s = scale.to_float (c->instancer (varIdxBase, 0));
    float tCenterX = centerX + c->instancer (varIdxBase, 1);
    float tCenterY = centerY + c->instancer (varIdxBase, 2);

    bool p1 = c->funcs->push_translate (c->data, +tCenterX, +tCenterY);
    bool p2 = c->funcs->push_scale (c->data, s, s);
    bool p3 = c->funcs->push_translate (c->data, -tCenterX, -tCenterY);
    c->recurse (this+src);
    if (p3) c->funcs->pop_transform (c->data);
    if (p2) c->funcs->pop_transform (c->data);
    if (p1) c->funcs->pop_transform (c->data);
  }

  HBUINT8		format; /* format = 22 (noVar) or 23(Var) */
  Offset24To<Paint>	src; /* Offset (from beginning of PaintScaleUniformAroundCenter table) to Paint subtable. */
  F2DOT14	scale;
  FWORD		centerX;
  FWORD		centerY;
  public:
  DEFINE_SIZE_STATIC (4 + F2DOT14::static_size + 2 * FWORD::static_size);
};

struct PaintRotate
{
  HB_INTERNAL void closurev1 (hb_colrv1_closure_context_t* c) const;

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer,
               uint32_t varIdxBase) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    if (instancer && !c->plan->pinned_at_default && varIdxBase != VarIdx::NO_VARIATION)
      out->angle.set_float (angle.to_float (instancer (varIdxBase, 0)));

    if (format == 25 && c->plan->all_axes_pinned)
      out->format = 24;

    return_trace (out->src.serialize_subset (c, src, this, instancer));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && src.sanitize (c, this));
  }

  void paint_glyph (hb_paint_context_t *c, uint32_t varIdxBase) const
  {
    TRACE_PAINT (this);
    float a = angle.to_float (c->instancer (varIdxBase, 0));

    bool p1 = c->funcs->push_rotate (c->data, a);
    c->recurse (this+src);
    if (p1) c->funcs->pop_transform (c->data);
  }

  HBUINT8		format; /* format = 24 (noVar) or 25(Var) */
  Offset24To<Paint>	src; /* Offset (from beginning of PaintRotate table) to Paint subtable. */
  F2DOT14		angle;
  public:
  DEFINE_SIZE_STATIC (4 + F2DOT14::static_size);
};

struct PaintRotateAroundCenter
{
  HB_INTERNAL void closurev1 (hb_colrv1_closure_context_t* c) const;

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer,
               uint32_t varIdxBase) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    if (instancer && !c->plan->pinned_at_default && varIdxBase != VarIdx::NO_VARIATION)
    {
      out->angle.set_float (angle.to_float (instancer (varIdxBase, 0)));
      out->centerX = centerX + (int) roundf (instancer (varIdxBase, 1));
      out->centerY = centerY + (int) roundf (instancer (varIdxBase, 2));
    }

    if (format ==27 && c->plan->all_axes_pinned)
        out->format = 26;

    return_trace (out->src.serialize_subset (c, src, this, instancer));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && src.sanitize (c, this));
  }

  void paint_glyph (hb_paint_context_t *c, uint32_t varIdxBase) const
  {
    TRACE_PAINT (this);
    float a = angle.to_float (c->instancer (varIdxBase, 0));
    float tCenterX = centerX + c->instancer (varIdxBase, 1);
    float tCenterY = centerY + c->instancer (varIdxBase, 2);

    bool p1 = c->funcs->push_translate (c->data, +tCenterX, +tCenterY);
    bool p2 = c->funcs->push_rotate (c->data, a);
    bool p3 = c->funcs->push_translate (c->data, -tCenterX, -tCenterY);
    c->recurse (this+src);
    if (p3) c->funcs->pop_transform (c->data);
    if (p2) c->funcs->pop_transform (c->data);
    if (p1) c->funcs->pop_transform (c->data);
  }

  HBUINT8		format; /* format = 26 (noVar) or 27(Var) */
  Offset24To<Paint>	src; /* Offset (from beginning of PaintRotateAroundCenter table) to Paint subtable. */
  F2DOT14	angle;
  FWORD		centerX;
  FWORD		centerY;
  public:
  DEFINE_SIZE_STATIC (4 + F2DOT14::static_size + 2 * FWORD::static_size);
};

struct PaintSkew
{
  HB_INTERNAL void closurev1 (hb_colrv1_closure_context_t* c) const;

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer,
               uint32_t varIdxBase) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    if (instancer && !c->plan->pinned_at_default && varIdxBase != VarIdx::NO_VARIATION)
    {
      out->xSkewAngle.set_float (xSkewAngle.to_float (instancer (varIdxBase, 0)));
      out->ySkewAngle.set_float (ySkewAngle.to_float (instancer (varIdxBase, 1)));
    }

    if (format == 29 && c->plan->all_axes_pinned)
        out->format = 28;

    return_trace (out->src.serialize_subset (c, src, this, instancer));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && src.sanitize (c, this));
  }

  void paint_glyph (hb_paint_context_t *c, uint32_t varIdxBase) const
  {
    TRACE_PAINT (this);
    float sx = xSkewAngle.to_float(c->instancer (varIdxBase, 0));
    float sy = ySkewAngle.to_float(c->instancer (varIdxBase, 1));

    bool p1 = c->funcs->push_skew (c->data, sx, sy);
    c->recurse (this+src);
    if (p1) c->funcs->pop_transform (c->data);
  }

  HBUINT8		format; /* format = 28(noVar) or 29 (Var) */
  Offset24To<Paint>	src; /* Offset (from beginning of PaintSkew table) to Paint subtable. */
  F2DOT14		xSkewAngle;
  F2DOT14		ySkewAngle;
  public:
  DEFINE_SIZE_STATIC (4 + 2 * F2DOT14::static_size);
};

struct PaintSkewAroundCenter
{
  HB_INTERNAL void closurev1 (hb_colrv1_closure_context_t* c) const;

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer,
               uint32_t varIdxBase) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    if (instancer && !c->plan->pinned_at_default && varIdxBase != VarIdx::NO_VARIATION)
    {
      out->xSkewAngle.set_float (xSkewAngle.to_float (instancer (varIdxBase, 0)));
      out->ySkewAngle.set_float (ySkewAngle.to_float (instancer (varIdxBase, 1)));
      out->centerX = centerX + (int) roundf (instancer (varIdxBase, 2));
      out->centerY = centerY + (int) roundf (instancer (varIdxBase, 3));
    }

    if (format == 31 && c->plan->all_axes_pinned)
        out->format = 30;

    return_trace (out->src.serialize_subset (c, src, this, instancer));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && src.sanitize (c, this));
  }

  void paint_glyph (hb_paint_context_t *c, uint32_t varIdxBase) const
  {
    TRACE_PAINT (this);
    float sx = xSkewAngle.to_float(c->instancer (varIdxBase, 0));
    float sy = ySkewAngle.to_float(c->instancer (varIdxBase, 1));
    float tCenterX = centerX + c->instancer (varIdxBase, 2);
    float tCenterY = centerY + c->instancer (varIdxBase, 3);

    bool p1 = c->funcs->push_translate (c->data, +tCenterX, +tCenterY);
    bool p2 = c->funcs->push_skew (c->data, sx, sy);
    bool p3 = c->funcs->push_translate (c->data, -tCenterX, -tCenterY);
    c->recurse (this+src);
    if (p3) c->funcs->pop_transform (c->data);
    if (p2) c->funcs->pop_transform (c->data);
    if (p1) c->funcs->pop_transform (c->data);
  }

  HBUINT8		format; /* format = 30(noVar) or 31 (Var) */
  Offset24To<Paint>	src; /* Offset (from beginning of PaintSkewAroundCenter table) to Paint subtable. */
  F2DOT14	xSkewAngle;
  F2DOT14	ySkewAngle;
  FWORD		centerX;
  FWORD		centerY;
  public:
  DEFINE_SIZE_STATIC (4 + 2 * F2DOT14::static_size + 2 * FWORD::static_size);
};

struct PaintComposite
{
  void closurev1 (hb_colrv1_closure_context_t* c) const;

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    bool ret = false;
    ret |= out->src.serialize_subset (c, src, this, instancer);
    ret |= out->backdrop.serialize_subset (c, backdrop, this, instancer);
    return_trace (ret);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  c->check_ops (this->min_size) && // PainComposite can get exponential
                  src.sanitize (c, this) &&
                  backdrop.sanitize (c, this));
  }

  void paint_glyph (hb_paint_context_t *c) const
  {
    TRACE_PAINT (this);
    c->recurse (this+backdrop);
    c->funcs->push_group (c->data);
    c->recurse (this+src);
    c->funcs->pop_group (c->data, (hb_paint_composite_mode_t) (int) mode);
  }

  HBUINT8		format; /* format = 32 */
  Offset24To<Paint>	src; /* Offset (from beginning of PaintComposite table) to source Paint subtable. */
  CompositeMode		mode;   /* If mode is unrecognized use COMPOSITE_CLEAR */
  Offset24To<Paint>	backdrop; /* Offset (from beginning of PaintComposite table) to backdrop Paint subtable. */
  public:
  DEFINE_SIZE_STATIC (8);
};

struct ClipBoxData
{
  int xMin, yMin, xMax, yMax;
};

struct ClipBoxFormat1
{
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  void get_clip_box (ClipBoxData &clip_box, const ItemVarStoreInstancer &instancer HB_UNUSED) const
  {
    clip_box.xMin = xMin;
    clip_box.yMin = yMin;
    clip_box.xMax = xMax;
    clip_box.yMax = yMax;
  }

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer,
               uint32_t varIdxBase) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (*this);
    if (unlikely (!out)) return_trace (false);

    if (instancer && !c->plan->pinned_at_default && varIdxBase != VarIdx::NO_VARIATION)
    {
      out->xMin = xMin + (int) roundf (instancer (varIdxBase, 0));
      out->yMin = yMin + (int) roundf (instancer (varIdxBase, 1));
      out->xMax = xMax + (int) roundf (instancer (varIdxBase, 2));
      out->yMax = yMax + (int) roundf (instancer (varIdxBase, 3));
    }

    if (format == 2 && c->plan->all_axes_pinned)
        out->format = 1;

    return_trace (true);
  }

  public:
  HBUINT8	format; /* format = 1(noVar) or 2(Var)*/
  FWORD		xMin;
  FWORD		yMin;
  FWORD		xMax;
  FWORD		yMax;
  public:
  DEFINE_SIZE_STATIC (1 + 4 * FWORD::static_size);
};

struct ClipBoxFormat2 : Variable<ClipBoxFormat1>
{
  void get_clip_box (ClipBoxData &clip_box, const ItemVarStoreInstancer &instancer) const
  {
    value.get_clip_box(clip_box, instancer);
    if (instancer)
    {
      clip_box.xMin += roundf (instancer (varIdxBase, 0));
      clip_box.yMin += roundf (instancer (varIdxBase, 1));
      clip_box.xMax += roundf (instancer (varIdxBase, 2));
      clip_box.yMax += roundf (instancer (varIdxBase, 3));
    }
  }

  void closurev1 (hb_colrv1_closure_context_t* c) const
  { c->variation_indices->add_range (varIdxBase, varIdxBase + 3); }
};

struct ClipBox
{
  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer) const
  {
    TRACE_SUBSET (this);
    switch (u.format) {
    case 1: return_trace (u.format1.subset (c, instancer, VarIdx::NO_VARIATION));
    case 2: return_trace (u.format2.subset (c, instancer));
    default:return_trace (c->default_return_value ());
    }
  }

  void closurev1 (hb_colrv1_closure_context_t* c) const
  {
    switch (u.format) {
    case 2: u.format2.closurev1 (c);
    default:return;
    }
  }

  template <typename context_t, typename ...Ts>
  typename context_t::return_t dispatch (context_t *c, Ts&&... ds) const
  {
    if (unlikely (!c->may_dispatch (this, &u.format))) return c->no_dispatch_return_value ();
    TRACE_DISPATCH (this, u.format);
    switch (u.format) {
    case 1: return_trace (c->dispatch (u.format1, std::forward<Ts> (ds)...));
    case 2: return_trace (c->dispatch (u.format2, std::forward<Ts> (ds)...));
    default:return_trace (c->default_return_value ());
    }
  }

  bool get_extents (hb_glyph_extents_t *extents,
                    const ItemVarStoreInstancer &instancer) const
  {
    ClipBoxData clip_box;
    switch (u.format) {
    case 1:
      u.format1.get_clip_box (clip_box, instancer);
      break;
    case 2:
      u.format2.get_clip_box (clip_box, instancer);
      break;
    default:
      return false;
    }

    extents->x_bearing = clip_box.xMin;
    extents->y_bearing = clip_box.yMax;
    extents->width = clip_box.xMax - clip_box.xMin;
    extents->height = clip_box.yMin - clip_box.yMax;
    return true;
  }

  protected:
  union {
  HBUINT8		format;         /* Format identifier */
  ClipBoxFormat1	format1;
  ClipBoxFormat2	format2;
  } u;
};

struct ClipRecord
{
  int cmp (hb_codepoint_t g) const
  { return g < startGlyphID ? -1 : g <= endGlyphID ? 0 : +1; }

  void closurev1 (hb_colrv1_closure_context_t* c, const void *base) const
  {
    if (!c->glyphs->intersects (startGlyphID, endGlyphID)) return;
    (base+clipBox).closurev1 (c);
  }

  bool subset (hb_subset_context_t *c,
               const void *base,
               const ItemVarStoreInstancer &instancer) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (*this);
    if (unlikely (!out)) return_trace (false);

    return_trace (out->clipBox.serialize_subset (c, clipBox, base, instancer));
  }

  bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && clipBox.sanitize (c, base));
  }

  bool get_extents (hb_glyph_extents_t *extents,
		    const void *base,
		    const ItemVarStoreInstancer &instancer) const
  {
    return (base+clipBox).get_extents (extents, instancer);
  }

  public:
  HBUINT16		startGlyphID;  // first gid clip applies to
  HBUINT16		endGlyphID;    // last gid clip applies to, inclusive
  Offset24To<ClipBox>	clipBox;   // Box or VarBox
  public:
  DEFINE_SIZE_STATIC (7);
};
DECLARE_NULL_NAMESPACE_BYTES (OT, ClipRecord);

struct ClipList
{
  unsigned serialize_clip_records (hb_subset_context_t *c,
                                   const ItemVarStoreInstancer &instancer,
                                   const hb_set_t& gids,
                                   const hb_map_t& gid_offset_map) const
  {
    TRACE_SERIALIZE (this);
    if (gids.is_empty () ||
        gid_offset_map.get_population () != gids.get_population ())
      return_trace (0);

    unsigned count  = 0;

    hb_codepoint_t start_gid= gids.get_min ();
    hb_codepoint_t prev_gid = start_gid;

    unsigned offset = gid_offset_map.get (start_gid);
    unsigned prev_offset = offset;
    for (const hb_codepoint_t _ : gids.iter ())
    {
      if (_ == start_gid) continue;

      offset = gid_offset_map.get (_);
      if (_ == prev_gid + 1 &&  offset == prev_offset)
      {
        prev_gid = _;
        continue;
      }

      ClipRecord record;
      record.startGlyphID = start_gid;
      record.endGlyphID = prev_gid;
      record.clipBox = prev_offset;

      if (!record.subset (c, this, instancer)) return_trace (0);
      count++;

      start_gid = _;
      prev_gid = _;
      prev_offset = offset;
    }

    //last one
    {
      ClipRecord record;
      record.startGlyphID = start_gid;
      record.endGlyphID = prev_gid;
      record.clipBox = prev_offset;
      if (!record.subset (c, this, instancer)) return_trace (0);
      count++;
    }
    return_trace (count);
  }

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);
    if (!c->serializer->check_assign (out->format, format, HB_SERIALIZE_ERROR_INT_OVERFLOW)) return_trace (false);

    const hb_set_t& glyphset = c->plan->_glyphset_colred;
    const hb_map_t &glyph_map = *c->plan->glyph_map;

    hb_map_t new_gid_offset_map;
    hb_set_t new_gids;
    for (const ClipRecord& record : clips.iter ())
    {
      unsigned start_gid = record.startGlyphID;
      unsigned end_gid = record.endGlyphID;
      for (unsigned gid = start_gid; gid <= end_gid; gid++)
      {
        if (!glyphset.has (gid) || !glyph_map.has (gid)) continue;
        unsigned new_gid = glyph_map.get (gid);
        new_gid_offset_map.set (new_gid, record.clipBox);
        new_gids.add (new_gid);
      }
    }

    unsigned count = serialize_clip_records (c, instancer, new_gids, new_gid_offset_map);
    if (!count) return_trace (false);
    return_trace (c->serializer->check_assign (out->clips.len, count, HB_SERIALIZE_ERROR_INT_OVERFLOW));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    // TODO Make a formatted struct!
    return_trace (c->check_struct (this) && clips.sanitize (c, this));
  }

  bool
  get_extents (hb_codepoint_t gid,
	       hb_glyph_extents_t *extents,
	       const ItemVarStoreInstancer &instancer) const
  {
    auto *rec = clips.as_array ().bsearch (gid);
    if (rec)
    {
      rec->get_extents (extents, this, instancer);
      return true;
    }
    return false;
  }

  HBUINT8			format;  // Set to 1.
  SortedArray32Of<ClipRecord>	clips;  // Clip records, sorted by startGlyphID
  public:
  DEFINE_SIZE_ARRAY_SIZED (5, clips);
};

struct Paint
{

  template <typename ...Ts>
  bool sanitize (hb_sanitize_context_t *c, Ts&&... ds) const
  {
    TRACE_SANITIZE (this);

    if (unlikely (!c->check_start_recursion (HB_MAX_NESTING_LEVEL)))
      return_trace (c->no_dispatch_return_value ());

    return_trace (c->end_recursion (this->dispatch (c, std::forward<Ts> (ds)...)));
  }

  template <typename context_t, typename ...Ts>
  typename context_t::return_t dispatch (context_t *c, Ts&&... ds) const
  {
    if (unlikely (!c->may_dispatch (this, &u.format))) return c->no_dispatch_return_value ();
    TRACE_DISPATCH (this, u.format);
    switch (u.format) {
    case 1: return_trace (c->dispatch (u.paintformat1, std::forward<Ts> (ds)...));
    case 2: return_trace (c->dispatch (u.paintformat2, std::forward<Ts> (ds)...));
    case 3: return_trace (c->dispatch (u.paintformat3, std::forward<Ts> (ds)...));
    case 4: return_trace (c->dispatch (u.paintformat4, std::forward<Ts> (ds)...));
    case 5: return_trace (c->dispatch (u.paintformat5, std::forward<Ts> (ds)...));
    case 6: return_trace (c->dispatch (u.paintformat6, std::forward<Ts> (ds)...));
    case 7: return_trace (c->dispatch (u.paintformat7, std::forward<Ts> (ds)...));
    case 8: return_trace (c->dispatch (u.paintformat8, std::forward<Ts> (ds)...));
    case 9: return_trace (c->dispatch (u.paintformat9, std::forward<Ts> (ds)...));
    case 10: return_trace (c->dispatch (u.paintformat10, std::forward<Ts> (ds)...));
    case 11: return_trace (c->dispatch (u.paintformat11, std::forward<Ts> (ds)...));
    case 12: return_trace (c->dispatch (u.paintformat12, std::forward<Ts> (ds)...));
    case 13: return_trace (c->dispatch (u.paintformat13, std::forward<Ts> (ds)...));
    case 14: return_trace (c->dispatch (u.paintformat14, std::forward<Ts> (ds)...));
    case 15: return_trace (c->dispatch (u.paintformat15, std::forward<Ts> (ds)...));
    case 16: return_trace (c->dispatch (u.paintformat16, std::forward<Ts> (ds)...));
    case 17: return_trace (c->dispatch (u.paintformat17, std::forward<Ts> (ds)...));
    case 18: return_trace (c->dispatch (u.paintformat18, std::forward<Ts> (ds)...));
    case 19: return_trace (c->dispatch (u.paintformat19, std::forward<Ts> (ds)...));
    case 20: return_trace (c->dispatch (u.paintformat20, std::forward<Ts> (ds)...));
    case 21: return_trace (c->dispatch (u.paintformat21, std::forward<Ts> (ds)...));
    case 22: return_trace (c->dispatch (u.paintformat22, std::forward<Ts> (ds)...));
    case 23: return_trace (c->dispatch (u.paintformat23, std::forward<Ts> (ds)...));
    case 24: return_trace (c->dispatch (u.paintformat24, std::forward<Ts> (ds)...));
    case 25: return_trace (c->dispatch (u.paintformat25, std::forward<Ts> (ds)...));
    case 26: return_trace (c->dispatch (u.paintformat26, std::forward<Ts> (ds)...));
    case 27: return_trace (c->dispatch (u.paintformat27, std::forward<Ts> (ds)...));
    case 28: return_trace (c->dispatch (u.paintformat28, std::forward<Ts> (ds)...));
    case 29: return_trace (c->dispatch (u.paintformat29, std::forward<Ts> (ds)...));
    case 30: return_trace (c->dispatch (u.paintformat30, std::forward<Ts> (ds)...));
    case 31: return_trace (c->dispatch (u.paintformat31, std::forward<Ts> (ds)...));
    case 32: return_trace (c->dispatch (u.paintformat32, std::forward<Ts> (ds)...));
    default:return_trace (c->default_return_value ());
    }
  }

  protected:
  union {
  HBUINT8					format;
  PaintColrLayers				paintformat1;
  NoVariable<PaintSolid>			paintformat2;
  Variable<PaintSolid>				paintformat3;
  NoVariable<PaintLinearGradient<NoVariable>>	paintformat4;
  Variable<PaintLinearGradient<Variable>>	paintformat5;
  NoVariable<PaintRadialGradient<NoVariable>>	paintformat6;
  Variable<PaintRadialGradient<Variable>>	paintformat7;
  NoVariable<PaintSweepGradient<NoVariable>>	paintformat8;
  Variable<PaintSweepGradient<Variable>>	paintformat9;
  PaintGlyph					paintformat10;
  PaintColrGlyph				paintformat11;
  PaintTransform<NoVariable>			paintformat12;
  PaintTransform<Variable>			paintformat13;
  NoVariable<PaintTranslate>			paintformat14;
  Variable<PaintTranslate>			paintformat15;
  NoVariable<PaintScale>			paintformat16;
  Variable<PaintScale>				paintformat17;
  NoVariable<PaintScaleAroundCenter>		paintformat18;
  Variable<PaintScaleAroundCenter>		paintformat19;
  NoVariable<PaintScaleUniform>			paintformat20;
  Variable<PaintScaleUniform>			paintformat21;
  NoVariable<PaintScaleUniformAroundCenter>	paintformat22;
  Variable<PaintScaleUniformAroundCenter>	paintformat23;
  NoVariable<PaintRotate>			paintformat24;
  Variable<PaintRotate>				paintformat25;
  NoVariable<PaintRotateAroundCenter>		paintformat26;
  Variable<PaintRotateAroundCenter>		paintformat27;
  NoVariable<PaintSkew>				paintformat28;
  Variable<PaintSkew>				paintformat29;
  NoVariable<PaintSkewAroundCenter>		paintformat30;
  Variable<PaintSkewAroundCenter>		paintformat31;
  PaintComposite				paintformat32;
  } u;
  public:
  DEFINE_SIZE_MIN (2);
};

struct BaseGlyphPaintRecord
{
  int cmp (hb_codepoint_t g) const
  { return g < glyphId ? -1 : g > glyphId ? 1 : 0; }

  bool serialize (hb_serialize_context_t *s, const hb_map_t* glyph_map,
                  const void* src_base, hb_subset_context_t *c,
                  const ItemVarStoreInstancer &instancer) const
  {
    TRACE_SERIALIZE (this);
    auto *out = s->embed (this);
    if (unlikely (!out)) return_trace (false);
    if (!s->check_assign (out->glyphId, glyph_map->get (glyphId),
                          HB_SERIALIZE_ERROR_INT_OVERFLOW))
      return_trace (false);

    return_trace (out->paint.serialize_subset (c, paint, src_base, instancer));
  }

  bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this) && paint.sanitize (c, base)));
  }

  public:
  HBGlyphID16		glyphId;    /* Glyph ID of reference glyph */
  Offset32To<Paint>	paint;      /* Offset (from beginning of BaseGlyphPaintRecord array) to Paint,
                                     * Typically PaintColrLayers */
  public:
  DEFINE_SIZE_STATIC (6);
};

struct BaseGlyphList : SortedArray32Of<BaseGlyphPaintRecord>
{
  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (this);
    if (unlikely (!c->serializer->extend_min (out)))  return_trace (false);
    const hb_set_t* glyphset = &c->plan->_glyphset_colred;

    for (const auto& _ : as_array ())
    {
      unsigned gid = _.glyphId;
      if (!glyphset->has (gid)) continue;

      if (_.serialize (c->serializer, c->plan->glyph_map, this, c, instancer)) out->len++;
      else return_trace (false);
    }

    return_trace (out->len != 0);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (SortedArray32Of<BaseGlyphPaintRecord>::sanitize (c, this));
  }
};

struct LayerList : Array32OfOffset32To<Paint>
{
  const Paint& get_paint (unsigned i) const
  { return this+(*this)[i]; }

  bool subset (hb_subset_context_t *c,
               const ItemVarStoreInstancer &instancer) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (this);
    if (unlikely (!c->serializer->extend_min (out)))  return_trace (false);

    bool ret = false;
    for (const auto& _ : + hb_enumerate (*this)
                         | hb_filter (c->plan->colrv1_layers, hb_first))

    {
      auto *o = out->serialize_append (c->serializer);
      if (unlikely (!o)) return_trace (false);
      ret |= o->serialize_subset (c, _.second, this, instancer);
    }
    return_trace (ret);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (Array32OfOffset32To<Paint>::sanitize (c, this));
  }
};

struct delta_set_index_map_subset_plan_t
{
  unsigned get_inner_bit_count () const { return inner_bit_count; }
  unsigned get_width ()           const { return ((outer_bit_count + inner_bit_count + 7) / 8); }
  hb_array_t<const uint32_t> get_output_map () const { return output_map.as_array (); }

  delta_set_index_map_subset_plan_t (const hb_map_t &new_deltaset_idx_varidx_map)
  {
    map_count = 0;
    outer_bit_count = 0;
    inner_bit_count = 1;
    output_map.init ();

    /* search backwards */
    unsigned count = new_deltaset_idx_varidx_map.get_population ();
    if (!count) return;

    unsigned last_idx = (unsigned)-1;
    unsigned last_varidx = (unsigned)-1;

    for (unsigned i = count; i; i--)
    {
      unsigned delta_set_idx = i - 1;
      unsigned var_idx = new_deltaset_idx_varidx_map.get (delta_set_idx);
      if (i == count)
      {
        last_idx = delta_set_idx;
        last_varidx = var_idx;
        continue;
      }
      if (var_idx != last_varidx)
        break;
      last_idx = delta_set_idx;
    }

    map_count = last_idx + 1;
  }

  bool remap (const hb_map_t &new_deltaset_idx_varidx_map)
  {
    /* recalculate bit_count */
    outer_bit_count = 1;
    inner_bit_count = 1;

    if (unlikely (!output_map.resize (map_count, false))) return false;

    for (unsigned idx = 0; idx < map_count; idx++)
    {
      uint32_t *var_idx;
      if (!new_deltaset_idx_varidx_map.has (idx, &var_idx)) return false;
      output_map.arrayZ[idx] = *var_idx;

      unsigned outer = (*var_idx) >> 16;
      unsigned bit_count = (outer == 0) ? 1 : hb_bit_storage (outer);
      outer_bit_count = hb_max (bit_count, outer_bit_count);
      
      unsigned inner = (*var_idx) & 0xFFFF;
      bit_count = (inner == 0) ? 1 : hb_bit_storage (inner);
      inner_bit_count = hb_max (bit_count, inner_bit_count);
    }
    return true;
  }

  private:
  unsigned map_count;
  unsigned outer_bit_count;
  unsigned inner_bit_count;
  hb_vector_t<uint32_t> output_map;
};

struct COLR
{
  static constexpr hb_tag_t tableTag = HB_OT_TAG_COLR;

  bool has_v0_data () const { return numBaseGlyphs; }
  bool has_v1_data () const
  {
    if (version != 1)
      return false;
    hb_barrier ();

    return (this+baseGlyphList).len > 0;
  }

  unsigned int get_glyph_layers (hb_codepoint_t       glyph,
				 unsigned int         start_offset,
				 unsigned int        *count, /* IN/OUT.  May be NULL. */
				 hb_ot_color_layer_t *layers /* OUT.     May be NULL. */) const
  {
    const BaseGlyphRecord &record = (this+baseGlyphsZ).bsearch (numBaseGlyphs, glyph);

    hb_array_t<const LayerRecord> all_layers = (this+layersZ).as_array (numLayers);
    hb_array_t<const LayerRecord> glyph_layers = all_layers.sub_array (record.firstLayerIdx,
								       record.numLayers);
    if (count)
    {
      + glyph_layers.sub_array (start_offset, count)
      | hb_sink (hb_array (layers, *count))
      ;
    }
    return glyph_layers.length;
  }

  struct accelerator_t
  {
    accelerator_t (hb_face_t *face)
    { colr = hb_sanitize_context_t ().reference_table<COLR> (face); }
    ~accelerator_t () { this->colr.destroy (); }

    bool is_valid () { return colr.get_blob ()->length; }

    void closure_glyphs (hb_codepoint_t glyph,
			 hb_set_t *related_ids /* OUT */) const
    { colr->closure_glyphs (glyph, related_ids); }

    void closure_V0palette_indices (const hb_set_t *glyphs,
				    hb_set_t *palettes /* OUT */) const
    { colr->closure_V0palette_indices (glyphs, palettes); }

    void closure_forV1 (hb_set_t *glyphset,
                        hb_set_t *layer_indices,
                        hb_set_t *palette_indices,
                        hb_set_t *variation_indices,
                        hb_set_t *delta_set_indices) const
    { colr->closure_forV1 (glyphset, layer_indices, palette_indices, variation_indices, delta_set_indices); }

    bool has_var_store () const
    { return colr->has_var_store (); }

    const ItemVariationStore &get_var_store () const
    { return colr->get_var_store (); }

    bool has_delta_set_index_map () const
    { return colr->has_delta_set_index_map (); }

    const DeltaSetIndexMap &get_delta_set_index_map () const
    { return colr->get_delta_set_index_map (); }

    private:
    hb_blob_ptr_t<COLR> colr;
  };

  void closure_glyphs (hb_codepoint_t glyph,
		       hb_set_t *related_ids /* OUT */) const
  {
    const BaseGlyphRecord *record = get_base_glyph_record (glyph);
    if (!record) return;

    auto glyph_layers = (this+layersZ).as_array (numLayers).sub_array (record->firstLayerIdx,
								       record->numLayers);
    if (!glyph_layers.length) return;
    related_ids->add_array (&glyph_layers[0].glyphId, glyph_layers.length, LayerRecord::min_size);
  }

  void closure_V0palette_indices (const hb_set_t *glyphs,
				  hb_set_t *palettes /* OUT */) const
  {
    if (!numBaseGlyphs || !numLayers) return;
    hb_array_t<const BaseGlyphRecord> baseGlyphs = (this+baseGlyphsZ).as_array (numBaseGlyphs);
    hb_array_t<const LayerRecord> all_layers = (this+layersZ).as_array (numLayers);

    for (const BaseGlyphRecord record : baseGlyphs)
    {
      if (!glyphs->has (record.glyphId)) continue;
      hb_array_t<const LayerRecord> glyph_layers = all_layers.sub_array (record.firstLayerIdx,
                                                                   record.numLayers);
      for (const LayerRecord layer : glyph_layers)
        palettes->add (layer.colorIdx);
    }
  }

  void closure_forV1 (hb_set_t *glyphset,
                      hb_set_t *layer_indices,
                      hb_set_t *palette_indices,
                      hb_set_t *variation_indices,
                      hb_set_t *delta_set_indices) const
  {
    if (version != 1) return;
    hb_barrier ();

    hb_set_t visited_glyphs;

    hb_colrv1_closure_context_t c (this, &visited_glyphs, layer_indices, palette_indices, variation_indices);
    const BaseGlyphList &baseglyph_paintrecords = this+baseGlyphList;

    for (const BaseGlyphPaintRecord &baseglyph_paintrecord: baseglyph_paintrecords.iter ())
    {
      unsigned gid = baseglyph_paintrecord.glyphId;
      if (!glyphset->has (gid)) continue;

      const Paint &paint = &baseglyph_paintrecords+baseglyph_paintrecord.paint;
      paint.dispatch (&c);
    }
    hb_set_union (glyphset, &visited_glyphs);

    const ClipList &cliplist = this+clipList;
    c.glyphs = glyphset;
    for (const ClipRecord &clip_record : cliplist.clips.iter())
      clip_record.closurev1 (&c, &cliplist);

    // if a DeltaSetIndexMap is included, collected variation indices are
    // actually delta set indices, we need to map them into variation indices
    if (has_delta_set_index_map ())
    {
      const DeltaSetIndexMap &var_idx_map = this+varIdxMap;
      delta_set_indices->set (*variation_indices);
      variation_indices->clear ();
      for (unsigned delta_set_idx : *delta_set_indices)
        variation_indices->add (var_idx_map.map (delta_set_idx));
    }
  }

  const LayerList& get_layerList () const
  { return (this+layerList); }

  const BaseGlyphList& get_baseglyphList () const
  { return (this+baseGlyphList); }

  bool has_var_store () const
  { return version >= 1 && varStore != 0; }

  bool has_delta_set_index_map () const
  { return version >= 1 && varIdxMap != 0; }

  const DeltaSetIndexMap &get_delta_set_index_map () const
  { return (version == 0 || varIdxMap == 0) ? Null (DeltaSetIndexMap) : this+varIdxMap; }

  const ItemVariationStore &get_var_store () const
  { return (version == 0 || varStore == 0) ? Null (ItemVariationStore) : this+varStore; }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
                  (this+baseGlyphsZ).sanitize (c, numBaseGlyphs) &&
                  (this+layersZ).sanitize (c, numLayers) &&
                  (version == 0 ||
		   (hb_barrier () &&
		    version == 1 &&
		    baseGlyphList.sanitize (c, this) &&
		    layerList.sanitize (c, this) &&
		    clipList.sanitize (c, this) &&
		    varIdxMap.sanitize (c, this) &&
		    varStore.sanitize (c, this))));
  }

  template<typename BaseIterator, typename LayerIterator,
	   hb_requires (hb_is_iterator (BaseIterator)),
	   hb_requires (hb_is_iterator (LayerIterator))>
  bool serialize_V0 (hb_serialize_context_t *c,
		     unsigned version,
		     BaseIterator base_it,
		     LayerIterator layer_it)
  {
    TRACE_SERIALIZE (this);
    if (unlikely (base_it.len () != layer_it.len ()))
      return_trace (false);

    this->version = version;
    numLayers = 0;
    numBaseGlyphs = base_it.len ();
    if (numBaseGlyphs == 0)
    {
      baseGlyphsZ = 0;
      layersZ = 0;
      return_trace (true);
    }

    c->push ();
    for (const hb_item_type<BaseIterator> _ : + base_it.iter ())
    {
      auto* record = c->embed (_);
      if (unlikely (!record)) return_trace (false);
      record->firstLayerIdx = numLayers;
      numLayers += record->numLayers;
    }
    c->add_link (baseGlyphsZ, c->pop_pack ());

    c->push ();
    for (const hb_item_type<LayerIterator>& _ : + layer_it.iter ())
      _.as_array ().copy (c);

    c->add_link (layersZ, c->pop_pack ());

    return_trace (true);
  }

  const BaseGlyphRecord* get_base_glyph_record (hb_codepoint_t gid) const
  {
    const BaseGlyphRecord* record = &(this+baseGlyphsZ).bsearch (numBaseGlyphs, (unsigned int) gid);
    if (record == &Null (BaseGlyphRecord) ||
        (record && (hb_codepoint_t) record->glyphId != gid))
      record = nullptr;
    return record;
  }

  const BaseGlyphPaintRecord* get_base_glyph_paintrecord (hb_codepoint_t gid) const
  {
    const BaseGlyphPaintRecord* record = &(this+baseGlyphList).bsearch ((unsigned) gid);
    if ((record && (hb_codepoint_t) record->glyphId != gid))
      record = nullptr;
    return record;
  }

  bool downgrade_to_V0 (const hb_set_t &glyphset) const
  {
    //no more COLRv1 glyphs, downgrade to version 0
    for (const BaseGlyphPaintRecord& _ : get_baseglyphList ())
      if (glyphset.has (_.glyphId))
        return false;

    return true;
  }

  bool subset_varstore (hb_subset_context_t *c,
                        COLR* out /* OUT */) const
  {
    TRACE_SUBSET (this);
    if (!varStore || c->plan->all_axes_pinned ||
        !c->plan->colrv1_variation_idx_delta_map)
      return_trace (true);

    const ItemVariationStore& var_store = this+varStore;
    if (c->plan->normalized_coords)
    {
      item_variations_t item_vars;
      /* turn off varstore optimization when varIdxMap is null, so we maintain
       * original var_idx sequence */
      bool optimize = (varIdxMap != 0) ? true : false;
      if (!item_vars.instantiate (var_store, c->plan,
                                  optimize, /* optimization */
                                  optimize, /* use_no_variation_idx = false */
                                  c->plan->colrv1_varstore_inner_maps.as_array ()))
        return_trace (false);

      /* do not serialize varStore if there's no variation data after
       * instancing: region_list or var_data is empty */
      if (item_vars.get_region_list () &&
          item_vars.get_vardata_encodings () &&
          !out->varStore.serialize_serialize (c->serializer,
                                              item_vars.has_long_word (),
                                              c->plan->axis_tags,
                                              item_vars.get_region_list (),
                                              item_vars.get_vardata_encodings ()))
        return_trace (false);

      /* if varstore is optimized, update colrv1_new_deltaset_idx_varidx_map in
       * subset plan.
       * If varstore is empty after instancing, varidx_map would be empty and
       * all var_idxes will be updated to VarIdx::NO_VARIATION */
      if (optimize)
      {
        const hb_map_t &varidx_map = item_vars.get_varidx_map ();
        for (auto _ : c->plan->colrv1_new_deltaset_idx_varidx_map.iter_ref ())
        {
          uint32_t varidx = _.second;
          uint32_t *new_varidx;
          if (varidx_map.has (varidx, &new_varidx))
            _.second = *new_varidx;
          else
            _.second = VarIdx::NO_VARIATION;
        }
      }
    }
    else
    {
      if (unlikely (!out->varStore.serialize_serialize (c->serializer,
                                                        &var_store,
                                                        c->plan->colrv1_varstore_inner_maps.as_array ())))
        return_trace (false);
    }

    return_trace (true);
  }

  bool subset_delta_set_index_map (hb_subset_context_t *c,
                                   COLR* out /* OUT */) const
  {
    TRACE_SUBSET (this);
    if (!varIdxMap || c->plan->all_axes_pinned ||
        !c->plan->colrv1_new_deltaset_idx_varidx_map)
      return_trace (true);

    const hb_map_t &deltaset_idx_varidx_map = c->plan->colrv1_new_deltaset_idx_varidx_map;
    delta_set_index_map_subset_plan_t index_map_plan (deltaset_idx_varidx_map);

    if (unlikely (!index_map_plan.remap (deltaset_idx_varidx_map)))
      return_trace (false);

    return_trace (out->varIdxMap.serialize_serialize (c->serializer, index_map_plan));
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    const hb_map_t &reverse_glyph_map = *c->plan->reverse_glyph_map;
    const hb_set_t& glyphset = c->plan->_glyphset_colred;

    auto base_it =
    + hb_range (c->plan->num_output_glyphs ())
    | hb_filter ([&](hb_codepoint_t new_gid)
		 {
		    hb_codepoint_t old_gid = reverse_glyph_map.get (new_gid);
		    if (glyphset.has (old_gid)) return true;
		    return false;
		 })
    | hb_map_retains_sorting ([&](hb_codepoint_t new_gid)
			      {
				hb_codepoint_t old_gid = reverse_glyph_map.get (new_gid);

				const BaseGlyphRecord* old_record = get_base_glyph_record (old_gid);
				if (unlikely (!old_record))
				  return hb_pair_t<bool, BaseGlyphRecord> (false, Null (BaseGlyphRecord));
				BaseGlyphRecord new_record = {};
				new_record.glyphId = new_gid;
				new_record.numLayers = old_record->numLayers;
				return hb_pair_t<bool, BaseGlyphRecord> (true, new_record);
			      })
    | hb_filter (hb_first)
    | hb_map_retains_sorting (hb_second)
    ;

    auto layer_it =
    + hb_range (c->plan->num_output_glyphs ())
    | hb_map (reverse_glyph_map)
    | hb_filter (glyphset)
    | hb_map_retains_sorting ([&](hb_codepoint_t old_gid)
			      {
				const BaseGlyphRecord* old_record = get_base_glyph_record (old_gid);
				hb_vector_t<LayerRecord> out_layers;

				if (unlikely (!old_record ||
					      old_record->firstLayerIdx >= numLayers ||
					      old_record->firstLayerIdx + old_record->numLayers > numLayers))
				  return hb_pair_t<bool, hb_vector_t<LayerRecord>> (false, out_layers);

				auto layers = (this+layersZ).as_array (numLayers).sub_array (old_record->firstLayerIdx,
											     old_record->numLayers);
				out_layers.resize (layers.length);
				for (unsigned int i = 0; i < layers.length; i++) {
				  out_layers[i] = layers[i];
				  hb_codepoint_t new_gid = 0;
				  if (unlikely (!c->plan->new_gid_for_old_gid (out_layers[i].glyphId, &new_gid)))
				    return hb_pair_t<bool, hb_vector_t<LayerRecord>> (false, out_layers);
				  out_layers[i].glyphId = new_gid;
				  out_layers[i].colorIdx = c->plan->colr_palettes.get (layers[i].colorIdx);
				}

				return hb_pair_t<bool, hb_vector_t<LayerRecord>> (true, out_layers);
			      })
    | hb_filter (hb_first)
    | hb_map_retains_sorting (hb_second)
    ;

    if (version == 0 && (!base_it || !layer_it))
      return_trace (false);

    auto *colr_prime = c->serializer->start_embed<COLR> ();
    if (unlikely (!c->serializer->extend_min (colr_prime)))  return_trace (false);

    if (version == 0 || downgrade_to_V0 (glyphset))
    return_trace (colr_prime->serialize_V0 (c->serializer, 0, base_it, layer_it));

    //start version 1
    if (!c->serializer->allocate_size<void> (5 * HBUINT32::static_size)) return_trace (false);
    if (!colr_prime->serialize_V0 (c->serializer, version, base_it, layer_it)) return_trace (false);

    /* subset ItemVariationStore first, cause varidx_map needs to be updated
     * after instancing */
    if (!subset_varstore (c, colr_prime)) return_trace (false);

    ItemVarStoreInstancer instancer (varStore ? &(this+varStore) : nullptr,
	                         varIdxMap ? &(this+varIdxMap) : nullptr,
	                         c->plan->normalized_coords.as_array ());

    if (!colr_prime->baseGlyphList.serialize_subset (c, baseGlyphList, this, instancer))
      return_trace (false);

    colr_prime->layerList.serialize_subset (c, layerList, this, instancer);
    colr_prime->clipList.serialize_subset (c, clipList, this, instancer);

    return_trace (subset_delta_set_index_map (c, colr_prime));
  }

  const Paint *get_base_glyph_paint (hb_codepoint_t glyph) const
  {
    const BaseGlyphList &baseglyph_paintrecords = this+baseGlyphList;
    const BaseGlyphPaintRecord* record = get_base_glyph_paintrecord (glyph);
    if (record)
    {
      const Paint &paint = &baseglyph_paintrecords+record->paint;
      return &paint;
    }
    else
      return nullptr;
  }

#ifndef HB_NO_PAINT
  bool
  get_extents (hb_font_t *font, hb_codepoint_t glyph, hb_glyph_extents_t *extents) const
  {
    if (version != 1)
      return false;

    ItemVarStoreInstancer instancer (&(this+varStore),
				 &(this+varIdxMap),
				 hb_array (font->coords, font->num_coords));

    if (get_clip (glyph, extents, instancer))
    {
      font->scale_glyph_extents (extents);
      return true;
    }

    auto *extents_funcs = hb_paint_extents_get_funcs ();
    hb_paint_extents_context_t extents_data;
    bool ret = paint_glyph (font, glyph, extents_funcs, &extents_data, 0, HB_COLOR(0,0,0,0));

    hb_extents_t e = extents_data.get_extents ();
    if (e.is_void ())
    {
      extents->x_bearing = 0;
      extents->y_bearing = 0;
      extents->width = 0;
      extents->height = 0;
    }
    else
    {
      extents->x_bearing = e.xmin;
      extents->y_bearing = e.ymax;
      extents->width = e.xmax - e.xmin;
      extents->height = e.ymin - e.ymax;
    }

    return ret;
  }
#endif

  bool
  has_paint_for_glyph (hb_codepoint_t glyph) const
  {
    if (version == 1)
    {
      hb_barrier ();

      const Paint *paint = get_base_glyph_paint (glyph);

      return paint != nullptr;
    }

    return false;
  }

  bool get_clip (hb_codepoint_t glyph,
		 hb_glyph_extents_t *extents,
		 const ItemVarStoreInstancer instancer) const
  {
    return (this+clipList).get_extents (glyph,
					extents,
					instancer);
  }

#ifndef HB_NO_PAINT
  bool
  paint_glyph (hb_font_t *font, hb_codepoint_t glyph, hb_paint_funcs_t *funcs, void *data, unsigned int palette_index, hb_color_t foreground, bool clip = true) const
  {
    ItemVarStoreInstancer instancer (&(this+varStore),
	                         &(this+varIdxMap),
	                         hb_array (font->coords, font->num_coords));
    hb_paint_context_t c (this, funcs, data, font, palette_index, foreground, instancer);
    c.current_glyphs.add (glyph);

    if (version == 1)
    {
      hb_barrier ();

      const Paint *paint = get_base_glyph_paint (glyph);
      if (paint)
      {
        // COLRv1 glyph

	bool is_bounded = true;
	if (clip)
	{
	  hb_glyph_extents_t extents;
	  if (get_clip (glyph, &extents, instancer))
	  {
	    font->scale_glyph_extents (&extents);
	    c.funcs->push_clip_rectangle (c.data,
					  extents.x_bearing,
					  extents.y_bearing + extents.height,
					  extents.x_bearing + extents.width,
					  extents.y_bearing);
	  }
	  else
	  {
	    auto *extents_funcs = hb_paint_extents_get_funcs ();
	    hb_paint_extents_context_t extents_data;

	    paint_glyph (font, glyph,
			 extents_funcs, &extents_data,
			 palette_index, foreground,
			 false);

	    hb_extents_t extents = extents_data.get_extents ();
	    is_bounded = extents_data.is_bounded ();

	    c.funcs->push_clip_rectangle (c.data,
					  extents.xmin,
					  extents.ymin,
					  extents.xmax,
					  extents.ymax);
	  }
	}

	c.funcs->push_root_transform (c.data, font);

	if (is_bounded)
	  c.recurse (*paint);

	c.funcs->pop_transform (c.data);

	if (clip)
	  c.funcs->pop_clip (c.data);

        return true;
      }
    }

    const BaseGlyphRecord *record = get_base_glyph_record (glyph);
    if (record && ((hb_codepoint_t) record->glyphId == glyph))
    {
      // COLRv0 glyph
      for (const auto &r : (this+layersZ).as_array (numLayers)
			   .sub_array (record->firstLayerIdx, record->numLayers))
      {
        hb_bool_t is_foreground;
        hb_color_t color = c.get_color (r.colorIdx, 1., &is_foreground);
        c.funcs->push_clip_glyph (c.data, r.glyphId, c.font);
        c.funcs->color (c.data, is_foreground, color);
        c.funcs->pop_clip (c.data);
      }

      return true;
    }

    return false;
  }
#endif

  protected:
  HBUINT16	version;	/* Table version number (starts at 0). */
  HBUINT16	numBaseGlyphs;	/* Number of Base Glyph Records. */
  NNOffset32To<SortedUnsizedArrayOf<BaseGlyphRecord>>
		baseGlyphsZ;	/* Offset to Base Glyph records. */
  NNOffset32To<UnsizedArrayOf<LayerRecord>>
		layersZ;	/* Offset to Layer Records. */
  HBUINT16	numLayers;	/* Number of Layer Records. */
  // Version-1 additions
  Offset32To<BaseGlyphList>		baseGlyphList;
  Offset32To<LayerList>			layerList;
  Offset32To<ClipList>			clipList;   // Offset to ClipList table (may be NULL)
  Offset32To<DeltaSetIndexMap>		varIdxMap;  // Offset to DeltaSetIndexMap table (may be NULL)
  Offset32To<ItemVariationStore>	varStore;
  public:
  DEFINE_SIZE_MIN (14);
};

struct COLR_accelerator_t : COLR::accelerator_t {
  COLR_accelerator_t (hb_face_t *face) : COLR::accelerator_t (face) {}
};

void
hb_paint_context_t::recurse (const Paint &paint)
{
  if (unlikely (depth_left <= 0 || edge_count <= 0)) return;
  depth_left--;
  edge_count--;
  paint.dispatch (this);
  depth_left++;
}

void PaintColrLayers::paint_glyph (hb_paint_context_t *c) const
{
  TRACE_PAINT (this);
  const LayerList &paint_offset_lists = c->get_colr_table ()->get_layerList ();
  for (unsigned i = firstLayerIndex; i < firstLayerIndex + numLayers; i++)
  {
    if (unlikely (c->current_layers.has (i)))
      continue;

    c->current_layers.add (i);

    const Paint &paint = paint_offset_lists.get_paint (i);
    c->funcs->push_group (c->data);
    c->recurse (paint);
    c->funcs->pop_group (c->data, HB_PAINT_COMPOSITE_MODE_SRC_OVER);

    c->current_layers.del (i);
  }
}

void PaintColrGlyph::paint_glyph (hb_paint_context_t *c) const
{
  TRACE_PAINT (this);

  if (unlikely (c->current_glyphs.has (gid)))
    return;

  c->current_glyphs.add (gid);

  c->funcs->push_inverse_root_transform (c->data, c->font);
  if (c->funcs->color_glyph (c->data, gid, c->font))
  {
    c->funcs->pop_transform (c->data);
    c->current_glyphs.del (gid);
    return;
  }
  c->funcs->pop_transform (c->data);

  const COLR *colr_table = c->get_colr_table ();
  const Paint *paint = colr_table->get_base_glyph_paint (gid);

  hb_glyph_extents_t extents = {0};
  bool has_clip_box = colr_table->get_clip (gid, &extents, c->instancer);

  if (has_clip_box)
    c->funcs->push_clip_rectangle (c->data,
				   extents.x_bearing,
				   extents.y_bearing + extents.height,
				   extents.x_bearing + extents.width,
				   extents.y_bearing);

  if (paint)
    c->recurse (*paint);

  if (has_clip_box)
    c->funcs->pop_clip (c->data);

  c->current_glyphs.del (gid);
}

} /* namespace OT */

#endif /* OT_COLOR_COLR_COLR_HH */
