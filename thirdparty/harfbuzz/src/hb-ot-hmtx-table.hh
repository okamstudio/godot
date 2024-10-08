/*
 * Copyright © 2011,2012  Google, Inc.
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
 * Google Author(s): Behdad Esfahbod, Roderick Sheeter
 */

#ifndef HB_OT_HMTX_TABLE_HH
#define HB_OT_HMTX_TABLE_HH

#include "hb-open-type.hh"
#include "hb-ot-maxp-table.hh"
#include "hb-ot-hhea-table.hh"
#include "hb-ot-os2-table.hh"
#include "hb-ot-var-hvar-table.hh"
#include "hb-ot-var-mvar-table.hh"
#include "hb-ot-metrics.hh"

/*
 * hmtx -- Horizontal Metrics
 * https://docs.microsoft.com/en-us/typography/opentype/spec/hmtx
 * vmtx -- Vertical Metrics
 * https://docs.microsoft.com/en-us/typography/opentype/spec/vmtx
 */
#define HB_OT_TAG_hmtx HB_TAG('h','m','t','x')
#define HB_OT_TAG_vmtx HB_TAG('v','m','t','x')


HB_INTERNAL bool
_glyf_get_leading_bearing_with_var_unscaled (hb_font_t *font, hb_codepoint_t glyph, bool is_vertical, int *lsb);

HB_INTERNAL unsigned
_glyf_get_advance_with_var_unscaled (hb_font_t *font, hb_codepoint_t glyph, bool is_vertical);

HB_INTERNAL bool
_glyf_get_leading_bearing_without_var_unscaled (hb_face_t *face, hb_codepoint_t gid, bool is_vertical, int *lsb);


namespace OT {


struct LongMetric
{
  UFWORD	advance; /* Advance width/height. */
  FWORD		sb; /* Leading (left/top) side bearing. */
  public:
  DEFINE_SIZE_STATIC (4);
};


template <typename T/*Data table type*/, typename H/*Header table type*/, typename V/*Var table type*/>
struct hmtxvmtx
{
  bool sanitize (hb_sanitize_context_t *c HB_UNUSED) const
  {
    TRACE_SANITIZE (this);
    /* We don't check for anything specific here.  The users of the
     * struct do all the hard work... */
    return_trace (true);
  }

  const hb_hashmap_t<hb_codepoint_t, hb_pair_t<unsigned, int>>* get_mtx_map (const hb_subset_plan_t *plan) const
  { return T::is_horizontal ? &plan->hmtx_map : &plan->vmtx_map; }

  bool subset_update_header (hb_subset_context_t *c,
			     unsigned int num_hmetrics,
			     const hb_hashmap_t<hb_codepoint_t, hb_pair_t<unsigned, int>> *mtx_map,
			     const hb_vector_t<unsigned> &bounds_vec) const
  {
    hb_blob_t *src_blob = hb_sanitize_context_t ().reference_table<H> (c->plan->source, H::tableTag);
    hb_blob_t *dest_blob = hb_blob_copy_writable_or_fail (src_blob);
    hb_blob_destroy (src_blob);

    if (unlikely (!dest_blob)) {
      return false;
    }

    unsigned int length;
    H *table = (H *) hb_blob_get_data (dest_blob, &length);
    c->serializer->check_assign (table->numberOfLongMetrics, num_hmetrics, HB_SERIALIZE_ERROR_INT_OVERFLOW);

#ifndef HB_NO_VAR
    if (c->plan->normalized_coords)
    {
      auto &MVAR = *c->plan->source->table.MVAR;
      if (T::is_horizontal)
      {
	HB_ADD_MVAR_VAR (HB_OT_METRICS_TAG_HORIZONTAL_CARET_RISE,   caretSlopeRise);
	HB_ADD_MVAR_VAR (HB_OT_METRICS_TAG_HORIZONTAL_CARET_RUN,    caretSlopeRun);
	HB_ADD_MVAR_VAR (HB_OT_METRICS_TAG_HORIZONTAL_CARET_OFFSET, caretOffset);
      }
      else
      {
	HB_ADD_MVAR_VAR (HB_OT_METRICS_TAG_VERTICAL_CARET_RISE,     caretSlopeRise);
	HB_ADD_MVAR_VAR (HB_OT_METRICS_TAG_VERTICAL_CARET_RUN,      caretSlopeRun);
	HB_ADD_MVAR_VAR (HB_OT_METRICS_TAG_VERTICAL_CARET_OFFSET,   caretOffset);
      }

      bool empty = true;
      int min_lsb = 0x7FFF;
      int min_rsb = 0x7FFF;
      int max_extent = -0x7FFF;
      unsigned max_adv = 0;
      for (const auto _ : *mtx_map)
      {
        hb_codepoint_t gid = _.first;
        unsigned adv = _.second.first;
        int lsb = _.second.second;
        max_adv = hb_max (max_adv, adv);

        if (bounds_vec[gid] != 0xFFFFFFFF)
        {
	  empty = false;
          unsigned bound_width = bounds_vec[gid];
          int rsb = adv - lsb - bound_width;
          int extent = lsb + bound_width;
          min_lsb = hb_min (min_lsb, lsb);
          min_rsb = hb_min (min_rsb, rsb);
          max_extent = hb_max (max_extent, extent);
        }
      }

      table->advanceMax = max_adv;
      if (!empty)
      {
        table->minLeadingBearing = min_lsb;
        table->minTrailingBearing = min_rsb;
        table->maxExtent = max_extent;
      }

      if (T::is_horizontal)
      {
        const auto &OS2 = *c->plan->source->table.OS2;
        if (OS2.has_data () &&
            table->ascender == OS2.sTypoAscender &&
            table->descender == OS2.sTypoDescender &&
            table->lineGap == OS2.sTypoLineGap)
        {
          table->ascender = static_cast<int> (roundf (OS2.sTypoAscender +
                                                      MVAR.get_var (HB_OT_METRICS_TAG_HORIZONTAL_ASCENDER,
                                                                    c->plan->normalized_coords.arrayZ,
                                                                    c->plan->normalized_coords.length)));
          table->descender = static_cast<int> (roundf (OS2.sTypoDescender +
                                                       MVAR.get_var (HB_OT_METRICS_TAG_HORIZONTAL_DESCENDER,
                                                                     c->plan->normalized_coords.arrayZ,
                                                                     c->plan->normalized_coords.length)));
          table->lineGap = static_cast<int> (roundf (OS2.sTypoLineGap +
                                                     MVAR.get_var (HB_OT_METRICS_TAG_HORIZONTAL_LINE_GAP,
                                                                   c->plan->normalized_coords.arrayZ,
                                                                   c->plan->normalized_coords.length)));
        }
      }
    }
#endif

    bool result = c->plan->add_table (H::tableTag, dest_blob);
    hb_blob_destroy (dest_blob);

    return result;
  }

  template<typename Iterator,
	   hb_requires (hb_is_iterator (Iterator))>
  void serialize (hb_serialize_context_t *c,
		  Iterator it,
		  const hb_vector_t<hb_codepoint_pair_t> new_to_old_gid_list,
		  unsigned num_long_metrics,
                  unsigned total_num_metrics)
  {
    LongMetric* long_metrics = c->allocate_size<LongMetric> (num_long_metrics * LongMetric::static_size);
    FWORD* short_metrics = c->allocate_size<FWORD> ((total_num_metrics - num_long_metrics) * FWORD::static_size);
    if (!long_metrics || !short_metrics) return;

    short_metrics -= num_long_metrics;

    for (auto _ : new_to_old_gid_list)
    {
      hb_codepoint_t gid = _.first;
      auto mtx = *it++;

      if (gid < num_long_metrics)
      {
	LongMetric& lm = long_metrics[gid];
	lm.advance = mtx.first;
	lm.sb = mtx.second;
      }
      // TODO(beyond-64k): This assumes that maxp.numGlyphs is 0xFFFF.
      else if (gid < 0x10000u)
        short_metrics[gid] = mtx.second;
      else
        ((UFWORD*) short_metrics)[gid] = mtx.first;
    }
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);

    auto *table_prime = c->serializer->start_embed <T> ();

    accelerator_t _mtx (c->plan->source);
    unsigned num_long_metrics;
    const hb_hashmap_t<hb_codepoint_t, hb_pair_t<unsigned, int>> *mtx_map = get_mtx_map (c->plan);
    {
      /* Determine num_long_metrics to encode. */
      auto& plan = c->plan;

      // TODO Don't consider retaingid holes here.

      num_long_metrics = hb_min (plan->num_output_glyphs (), 0xFFFFu);
      unsigned int last_advance = get_new_gid_advance_unscaled (plan, mtx_map, num_long_metrics - 1, _mtx);
      while (num_long_metrics > 1 &&
	     last_advance == get_new_gid_advance_unscaled (plan, mtx_map, num_long_metrics - 2, _mtx))
      {
	num_long_metrics--;
      }
    }

    auto it =
    + hb_iter (c->plan->new_to_old_gid_list)
    | hb_map ([c, &_mtx, mtx_map] (hb_codepoint_pair_t _)
	      {
		hb_codepoint_t new_gid = _.first;
		hb_codepoint_t old_gid = _.second;

		hb_pair_t<unsigned, int> *v = nullptr;
		if (!mtx_map->has (new_gid, &v))
		{
		  int lsb = 0;
		  if (!_mtx.get_leading_bearing_without_var_unscaled (old_gid, &lsb))
		    (void) _glyf_get_leading_bearing_without_var_unscaled (c->plan->source, old_gid, !T::is_horizontal, &lsb);
		  return hb_pair (_mtx.get_advance_without_var_unscaled (old_gid), +lsb);
		}
		return *v;
	      })
    ;

    table_prime->serialize (c->serializer,
			    it,
			    c->plan->new_to_old_gid_list,
			    num_long_metrics,
			    c->plan->num_output_glyphs ());

    if (unlikely (c->serializer->in_error ()))
      return_trace (false);

    // Amend header num hmetrics
    if (unlikely (!subset_update_header (c, num_long_metrics, mtx_map,
                                         T::is_horizontal ? c->plan->bounds_width_vec : c->plan->bounds_height_vec)))
      return_trace (false);

    return_trace (true);
  }

  struct accelerator_t
  {
    friend struct hmtxvmtx;

    accelerator_t (hb_face_t *face)
    {
      table = hb_sanitize_context_t ().reference_table<hmtxvmtx> (face, T::tableTag);
      var_table = hb_sanitize_context_t ().reference_table<V> (face, T::variationsTag);

      default_advance = T::is_horizontal ? hb_face_get_upem (face) / 2 : hb_face_get_upem (face);

      /* Populate count variables and sort them out as we go */

      unsigned int len = table.get_length ();
      if (len & 1)
        len--;

      num_long_metrics = T::is_horizontal ?
			 face->table.hhea->numberOfLongMetrics :
#ifndef HB_NO_VERTICAL
			 face->table.vhea->numberOfLongMetrics
#else
			 0
#endif
			 ;
      if (unlikely (num_long_metrics * 4 > len))
	num_long_metrics = len / 4;
      len -= num_long_metrics * 4;

      num_bearings = face->table.maxp->get_num_glyphs ();

      if (unlikely (num_bearings < num_long_metrics))
        num_bearings = num_long_metrics;
      if (unlikely ((num_bearings - num_long_metrics) * 2 > len))
        num_bearings = num_long_metrics + len / 2;
      len -= (num_bearings - num_long_metrics) * 2;

      /* We MUST set num_bearings to zero if num_long_metrics is zero.
       * Our get_advance() depends on that. */
      if (unlikely (!num_long_metrics))
	num_bearings = num_long_metrics = 0;

      num_advances = num_bearings + len / 2;
      num_glyphs = face->get_num_glyphs ();
      if (num_glyphs < num_advances)
        num_glyphs = num_advances;
    }
    ~accelerator_t ()
    {
      table.destroy ();
      var_table.destroy ();
    }

    bool has_data () const { return (bool) num_bearings; }

    bool get_leading_bearing_without_var_unscaled (hb_codepoint_t glyph,
						   int *lsb) const
    {
      if (glyph < num_long_metrics)
      {
	*lsb = table->longMetricZ[glyph].sb;
	return true;
      }

      if (unlikely (glyph >= num_bearings))
	return false;

      const FWORD *bearings = (const FWORD *) &table->longMetricZ[num_long_metrics];
      *lsb = bearings[glyph - num_long_metrics];
      return true;
    }

    bool get_leading_bearing_with_var_unscaled (hb_font_t *font,
						hb_codepoint_t glyph,
						int *lsb) const
    {
      if (!font->num_coords)
	return get_leading_bearing_without_var_unscaled (glyph, lsb);

#ifndef HB_NO_VAR
      float delta;
      if (var_table->get_lsb_delta_unscaled (glyph, font->coords, font->num_coords, &delta) &&
	  get_leading_bearing_without_var_unscaled (glyph, lsb))
      {
	*lsb += roundf (delta);
	return true;
      }

      return _glyf_get_leading_bearing_with_var_unscaled (font, glyph, T::tableTag == HB_OT_TAG_vmtx, lsb);
#else
      return false;
#endif
    }

    unsigned int get_advance_without_var_unscaled (hb_codepoint_t glyph) const
    {
      /* OpenType case. */
      if (glyph < num_bearings)
	return table->longMetricZ[hb_min (glyph, (uint32_t) num_long_metrics - 1)].advance;

      /* If num_advances is zero, it means we don't have the metrics table
       * for this direction: return default advance.  Otherwise, there's a
       * well-defined answer. */
      if (unlikely (!num_advances))
	return default_advance;

#ifdef HB_NO_BEYOND_64K
      return 0;
#endif

      if (unlikely (glyph >= num_glyphs))
        return 0;

      /* num_bearings <= glyph < num_glyphs;
       * num_bearings <= num_advances */

      if (num_bearings == num_advances)
        return get_advance_without_var_unscaled (num_bearings - 1);

      const FWORD *bearings = (const FWORD *) &table->longMetricZ[num_long_metrics];
      const UFWORD *advances = (const UFWORD *) &bearings[num_bearings - num_long_metrics];

      return advances[hb_min (glyph - num_bearings, num_advances - num_bearings - 1)];
    }

    unsigned get_advance_with_var_unscaled (hb_codepoint_t  glyph,
					    hb_font_t      *font,
					    ItemVariationStore::cache_t *store_cache = nullptr) const
    {
      unsigned int advance = get_advance_without_var_unscaled (glyph);

#ifndef HB_NO_VAR
      if (unlikely (glyph >= num_bearings) || !font->num_coords)
	return advance;

      if (var_table.get_length ())
	return advance + roundf (var_table->get_advance_delta_unscaled (glyph,
									font->coords, font->num_coords,
									store_cache));

      unsigned glyf_advance = _glyf_get_advance_with_var_unscaled (font, glyph, T::tableTag == HB_OT_TAG_vmtx);
      return glyf_advance ? glyf_advance : advance;
#else
      return advance;
#endif
    }

    protected:
    // 0 <= num_long_metrics <= num_bearings <= num_advances <= num_glyphs
    unsigned num_long_metrics;
    unsigned num_bearings;
    unsigned num_advances;
    unsigned num_glyphs;

    unsigned int default_advance;

    public:
    hb_blob_ptr_t<hmtxvmtx> table;
    hb_blob_ptr_t<V> var_table;
  };

  /* get advance: when no variations, call get_advance_without_var_unscaled.
   * when there're variations, get advance value from mtx_map in subset_plan*/
  unsigned get_new_gid_advance_unscaled (const hb_subset_plan_t *plan,
                                         const hb_hashmap_t<hb_codepoint_t, hb_pair_t<unsigned, int>> *mtx_map,
                                         unsigned new_gid,
                                         const accelerator_t &_mtx) const
  {
    if (mtx_map->is_empty ())
    {
      hb_codepoint_t old_gid = 0;
      return plan->old_gid_for_new_gid (new_gid, &old_gid) ?
             _mtx.get_advance_without_var_unscaled (old_gid) : 0;
    }
    return mtx_map->get (new_gid).first;
  }

  protected:
  UnsizedArrayOf<LongMetric>
		longMetricZ;	/* Paired advance width and leading
				 * bearing values for each glyph. The
				 * value numOfHMetrics comes from
				 * the 'hhea' table. If the font is
				 * monospaced, only one entry need
				 * be in the array, but that entry is
				 * required. The last entry applies to
				 * all subsequent glyphs. */
/*UnsizedArrayOf<FWORD>	leadingBearingX;*/
				/* Here the advance is assumed
				 * to be the same as the advance
				 * for the last entry above. The
				 * number of entries in this array is
				 * derived from numGlyphs (from 'maxp'
				 * table) minus numberOfLongMetrics.
				 * This generally is used with a run
				 * of monospaced glyphs (e.g., Kanji
				 * fonts or Courier fonts). Only one
				 * run is allowed and it must be at
				 * the end. This allows a monospaced
				 * font to vary the side bearing
				 * values for each glyph. */
/*UnsizedArrayOf<UFWORD>advancesX;*/
				/* TODO Document. */
  public:
  DEFINE_SIZE_ARRAY (0, longMetricZ);
};

struct hmtx : hmtxvmtx<hmtx, hhea, HVAR> {
  static constexpr hb_tag_t tableTag = HB_OT_TAG_hmtx;
  static constexpr hb_tag_t variationsTag = HB_OT_TAG_HVAR;
  static constexpr bool is_horizontal = true;
};
struct vmtx : hmtxvmtx<vmtx, vhea, VVAR> {
  static constexpr hb_tag_t tableTag = HB_OT_TAG_vmtx;
  static constexpr hb_tag_t variationsTag = HB_OT_TAG_VVAR;
  static constexpr bool is_horizontal = false;
};

struct hmtx_accelerator_t : hmtx::accelerator_t {
  hmtx_accelerator_t (hb_face_t *face) : hmtx::accelerator_t (face) {}
};
struct vmtx_accelerator_t : vmtx::accelerator_t {
  vmtx_accelerator_t (hb_face_t *face) : vmtx::accelerator_t (face) {}
};

} /* namespace OT */


#endif /* HB_OT_HMTX_TABLE_HH */
