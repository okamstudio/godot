/*
 * Copyright © 2007,2008,2009  Red Hat, Inc.
 * Copyright © 2010,2011,2012  Google, Inc.
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

#ifndef OT_LAYOUT_GDEF_GDEF_HH
#define OT_LAYOUT_GDEF_GDEF_HH

#include "../../../hb-ot-var-common.hh"

#include "../../../hb-font.hh"
#include "../../../hb-cache.hh"


namespace OT {


/*
 * Attachment List Table
 */

/* Array of contour point indices--in increasing numerical order */
struct AttachPoint : Array16Of<HBUINT16>
{
  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    return_trace (out->serialize (c->serializer, + iter ()));
  }
};

struct AttachList
{
  unsigned int get_attach_points (hb_codepoint_t glyph_id,
				  unsigned int start_offset,
				  unsigned int *point_count /* IN/OUT */,
				  unsigned int *point_array /* OUT */) const
  {
    unsigned int index = (this+coverage).get_coverage (glyph_id);
    if (index == NOT_COVERED)
    {
      if (point_count)
	*point_count = 0;
      return 0;
    }

    const AttachPoint &points = this+attachPoint[index];

    if (point_count)
    {
      + points.as_array ().sub_array (start_offset, point_count)
      | hb_sink (hb_array (point_array, *point_count))
      ;
    }

    return points.len;
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    const hb_set_t &glyphset = *c->plan->glyphset_gsub ();
    const hb_map_t &glyph_map = *c->plan->glyph_map;

    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    hb_sorted_vector_t<hb_codepoint_t> new_coverage;
    + hb_zip (this+coverage, attachPoint)
    | hb_filter (glyphset, hb_first)
    | hb_filter (subset_offset_array (c, out->attachPoint, this), hb_second)
    | hb_map (hb_first)
    | hb_map (glyph_map)
    | hb_sink (new_coverage)
    ;
    out->coverage.serialize_serialize (c->serializer, new_coverage.iter ());
    return_trace (bool (new_coverage));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (coverage.sanitize (c, this) && attachPoint.sanitize (c, this));
  }

  protected:
  Offset16To<Coverage>
		coverage;		/* Offset to Coverage table -- from
					 * beginning of AttachList table */
  Array16OfOffset16To<AttachPoint>
		attachPoint;		/* Array of AttachPoint tables
					 * in Coverage Index order */
  public:
  DEFINE_SIZE_ARRAY (4, attachPoint);
};

/*
 * Ligature Caret Table
 */

struct CaretValueFormat1
{
  friend struct CaretValue;
  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);
    return_trace (true);
  }

  private:
  hb_position_t get_caret_value (hb_font_t *font, hb_direction_t direction) const
  {
    return HB_DIRECTION_IS_HORIZONTAL (direction) ? font->em_scale_x (coordinate) : font->em_scale_y (coordinate);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  protected:
  HBUINT16	caretValueFormat;	/* Format identifier--format = 1 */
  FWORD		coordinate;		/* X or Y value, in design units */
  public:
  DEFINE_SIZE_STATIC (4);
};

struct CaretValueFormat2
{
  friend struct CaretValue;
  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);
    return_trace (true);
  }

  private:
  hb_position_t get_caret_value (hb_font_t *font, hb_direction_t direction, hb_codepoint_t glyph_id) const
  {
    hb_position_t x, y;
    font->get_glyph_contour_point_for_origin (glyph_id, caretValuePoint, direction, &x, &y);
    return HB_DIRECTION_IS_HORIZONTAL (direction) ? x : y;
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  protected:
  HBUINT16	caretValueFormat;	/* Format identifier--format = 2 */
  HBUINT16	caretValuePoint;	/* Contour point index on glyph */
  public:
  DEFINE_SIZE_STATIC (4);
};

struct CaretValueFormat3
{
  friend struct CaretValue;

  hb_position_t get_caret_value (hb_font_t *font, hb_direction_t direction,
				 const ItemVariationStore &var_store) const
  {
    return HB_DIRECTION_IS_HORIZONTAL (direction) ?
	   font->em_scale_x (coordinate) + (this+deviceTable).get_x_delta (font, var_store) :
	   font->em_scale_y (coordinate) + (this+deviceTable).get_y_delta (font, var_store);
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (!c->serializer->embed (caretValueFormat)) return_trace (false);
    if (!c->serializer->embed (coordinate)) return_trace (false);

    unsigned varidx = (this+deviceTable).get_variation_index ();
    hb_pair_t<unsigned, int> *new_varidx_delta;
    if (!c->plan->layout_variation_idx_delta_map.has (varidx, &new_varidx_delta))
      return_trace (false);

    uint32_t new_varidx = hb_first (*new_varidx_delta);
    int delta = hb_second (*new_varidx_delta);
    if (delta != 0)
    {
      if (!c->serializer->check_assign (out->coordinate, coordinate + delta, HB_SERIALIZE_ERROR_INT_OVERFLOW))
        return_trace (false);
    }

    if (new_varidx == HB_OT_LAYOUT_NO_VARIATIONS_INDEX)
      return_trace (c->serializer->check_assign (out->caretValueFormat, 1, HB_SERIALIZE_ERROR_INT_OVERFLOW));

    if (!c->serializer->embed (deviceTable))
      return_trace (false);

    return_trace (out->deviceTable.serialize_copy (c->serializer, deviceTable, this, c->serializer->to_bias (out),
						   hb_serialize_context_t::Head, &c->plan->layout_variation_idx_delta_map));
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const
  { (this+deviceTable).collect_variation_indices (c); }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && deviceTable.sanitize (c, this));
  }

  protected:
  HBUINT16	caretValueFormat;	/* Format identifier--format = 3 */
  FWORD		coordinate;		/* X or Y value, in design units */
  Offset16To<Device>
		deviceTable;		/* Offset to Device table for X or Y
					 * value--from beginning of CaretValue
					 * table */
  public:
  DEFINE_SIZE_STATIC (6);
};

struct CaretValue
{
  hb_position_t get_caret_value (hb_font_t *font,
				 hb_direction_t direction,
				 hb_codepoint_t glyph_id,
				 const ItemVariationStore &var_store) const
  {
    switch (u.format) {
    case 1: return u.format1.get_caret_value (font, direction);
    case 2: return u.format2.get_caret_value (font, direction, glyph_id);
    case 3: return u.format3.get_caret_value (font, direction, var_store);
    default:return 0;
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
    case 3: return_trace (c->dispatch (u.format3, std::forward<Ts> (ds)...));
    default:return_trace (c->default_return_value ());
    }
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const
  {
    switch (u.format) {
    case 1:
    case 2:
      return;
    case 3:
      u.format3.collect_variation_indices (c);
      return;
    default: return;
    }
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (!u.format.sanitize (c)) return_trace (false);
    hb_barrier ();
    switch (u.format) {
    case 1: return_trace (u.format1.sanitize (c));
    case 2: return_trace (u.format2.sanitize (c));
    case 3: return_trace (u.format3.sanitize (c));
    default:return_trace (true);
    }
  }

  protected:
  union {
  HBUINT16		format;		/* Format identifier */
  CaretValueFormat1	format1;
  CaretValueFormat2	format2;
  CaretValueFormat3	format3;
  } u;
  public:
  DEFINE_SIZE_UNION (2, format);
};

struct LigGlyph
{
  unsigned get_lig_carets (hb_font_t            *font,
			   hb_direction_t        direction,
			   hb_codepoint_t        glyph_id,
			   const ItemVariationStore &var_store,
			   unsigned              start_offset,
			   unsigned             *caret_count /* IN/OUT */,
			   hb_position_t        *caret_array /* OUT */) const
  {
    if (caret_count)
    {
      + carets.as_array ().sub_array (start_offset, caret_count)
      | hb_map (hb_add (this))
      | hb_map ([&] (const CaretValue &value) { return value.get_caret_value (font, direction, glyph_id, var_store); })
      | hb_sink (hb_array (caret_array, *caret_count))
      ;
    }

    return carets.len;
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    + hb_iter (carets)
    | hb_apply (subset_offset_array (c, out->carets, this))
    ;

    return_trace (bool (out->carets));
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const
  {
    for (const Offset16To<CaretValue>& offset : carets.iter ())
      (this+offset).collect_variation_indices (c);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (carets.sanitize (c, this));
  }

  protected:
  Array16OfOffset16To<CaretValue>
		carets;			/* Offset array of CaretValue tables
					 * --from beginning of LigGlyph table
					 * --in increasing coordinate order */
  public:
  DEFINE_SIZE_ARRAY (2, carets);
};

struct LigCaretList
{
  unsigned int get_lig_carets (hb_font_t *font,
			       hb_direction_t direction,
			       hb_codepoint_t glyph_id,
			       const ItemVariationStore &var_store,
			       unsigned int start_offset,
			       unsigned int *caret_count /* IN/OUT */,
			       hb_position_t *caret_array /* OUT */) const
  {
    unsigned int index = (this+coverage).get_coverage (glyph_id);
    if (index == NOT_COVERED)
    {
      if (caret_count)
	*caret_count = 0;
      return 0;
    }
    const LigGlyph &lig_glyph = this+ligGlyph[index];
    return lig_glyph.get_lig_carets (font, direction, glyph_id, var_store, start_offset, caret_count, caret_array);
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    const hb_set_t &glyphset = *c->plan->glyphset_gsub ();
    const hb_map_t &glyph_map = *c->plan->glyph_map;

    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    hb_sorted_vector_t<hb_codepoint_t> new_coverage;
    + hb_zip (this+coverage, ligGlyph)
    | hb_filter (glyphset, hb_first)
    | hb_filter (subset_offset_array (c, out->ligGlyph, this), hb_second)
    | hb_map (hb_first)
    | hb_map (glyph_map)
    | hb_sink (new_coverage)
    ;
    out->coverage.serialize_serialize (c->serializer, new_coverage.iter ());
    return_trace (bool (new_coverage));
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const
  {
    + hb_zip (this+coverage, ligGlyph)
    | hb_filter (c->glyph_set, hb_first)
    | hb_map (hb_second)
    | hb_map (hb_add (this))
    | hb_apply ([c] (const LigGlyph& _) { _.collect_variation_indices (c); })
    ;
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (coverage.sanitize (c, this) && ligGlyph.sanitize (c, this));
  }

  protected:
  Offset16To<Coverage>
		coverage;		/* Offset to Coverage table--from
					 * beginning of LigCaretList table */
  Array16OfOffset16To<LigGlyph>
		ligGlyph;		/* Array of LigGlyph tables
					 * in Coverage Index order */
  public:
  DEFINE_SIZE_ARRAY (4, ligGlyph);
};


struct MarkGlyphSetsFormat1
{
  bool covers (unsigned int set_index, hb_codepoint_t glyph_id) const
  { return (this+coverage[set_index]).get_coverage (glyph_id) != NOT_COVERED; }

  void collect_used_mark_sets (const hb_set_t& glyph_set,
                               hb_set_t& used_mark_sets /* OUT */) const
  {
    unsigned i = 0;
    for (const auto &offset : coverage)
     {
       const auto &cov = this+offset;
       if (cov.intersects (&glyph_set))
         used_mark_sets.add (i);

       i++;
     }
  }

  template <typename set_t>
  void collect_coverage (hb_vector_t<set_t> &sets) const
  {
     for (const auto &offset : coverage)
     {
       const auto &cov = this+offset;
       cov.collect_coverage (sets.push ());
     }
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);
    out->format = format;

    bool ret = true;
    for (const Offset32To<Coverage>& offset : coverage.iter ())
    {
      auto snap = c->serializer->snapshot ();
      auto *o = out->coverage.serialize_append (c->serializer);
      if (unlikely (!o))
      {
	ret = false;
	break;
      }

      //skip empty coverage
      c->serializer->push ();
      bool res = false;
      if (offset) res = c->dispatch (this+offset);
      if (!res)
      {
        c->serializer->pop_discard ();
        c->serializer->revert (snap);
        (out->coverage.len)--;
        continue;
      }
      c->serializer->add_link (*o, c->serializer->pop_pack ());
    }

    return_trace (ret && out->coverage.len);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (coverage.sanitize (c, this));
  }

  protected:
  HBUINT16	format;			/* Format identifier--format = 1 */
  Array16Of<Offset32To<Coverage>>
		coverage;		/* Array of long offsets to mark set
					 * coverage tables */
  public:
  DEFINE_SIZE_ARRAY (4, coverage);
};

struct MarkGlyphSets
{
  bool covers (unsigned int set_index, hb_codepoint_t glyph_id) const
  {
    switch (u.format) {
    case 1: return u.format1.covers (set_index, glyph_id);
    default:return false;
    }
  }

  template <typename set_t>
  void collect_coverage (hb_vector_t<set_t> &sets) const
  {
    switch (u.format) {
    case 1: u.format1.collect_coverage (sets); return;
    default:return;
    }
  }

  void collect_used_mark_sets (const hb_set_t& glyph_set,
                               hb_set_t& used_mark_sets /* OUT */) const
  {
    switch (u.format) {
    case 1: u.format1.collect_used_mark_sets (glyph_set, used_mark_sets); return;
    default:return;
    }
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    switch (u.format) {
    case 1: return_trace (u.format1.subset (c));
    default:return_trace (false);
    }
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (!u.format.sanitize (c)) return_trace (false);
    hb_barrier ();
    switch (u.format) {
    case 1: return_trace (u.format1.sanitize (c));
    default:return_trace (true);
    }
  }

  protected:
  union {
  HBUINT16		format;		/* Format identifier */
  MarkGlyphSetsFormat1	format1;
  } u;
  public:
  DEFINE_SIZE_UNION (2, format);
};


/*
 * GDEF -- Glyph Definition
 * https://docs.microsoft.com/en-us/typography/opentype/spec/gdef
 */


template <typename Types>
struct GDEFVersion1_2
{
  friend struct GDEF;

  protected:
  FixedVersion<>version;		/* Version of the GDEF table--currently
					 * 0x00010003u */
  typename Types::template OffsetTo<ClassDef>
		glyphClassDef;		/* Offset to class definition table
					 * for glyph type--from beginning of
					 * GDEF header (may be Null) */
  typename Types::template OffsetTo<AttachList>
		attachList;		/* Offset to list of glyphs with
					 * attachment points--from beginning
					 * of GDEF header (may be Null) */
  typename Types::template OffsetTo<LigCaretList>
		ligCaretList;		/* Offset to list of positioning points
					 * for ligature carets--from beginning
					 * of GDEF header (may be Null) */
  typename Types::template OffsetTo<ClassDef>
		markAttachClassDef;	/* Offset to class definition table for
					 * mark attachment type--from beginning
					 * of GDEF header (may be Null) */
  typename Types::template OffsetTo<MarkGlyphSets>
		markGlyphSetsDef;	/* Offset to the table of mark set
					 * definitions--from beginning of GDEF
					 * header (may be NULL).  Introduced
					 * in version 0x00010002. */
  Offset32To<ItemVariationStore>
		varStore;		/* Offset to the table of Item Variation
					 * Store--from beginning of GDEF
					 * header (may be NULL).  Introduced
					 * in version 0x00010003. */
  public:
  DEFINE_SIZE_MIN (4 + 4 * Types::size);

  unsigned int get_size () const
  {
    return min_size +
	   (version.to_int () >= 0x00010002u ? markGlyphSetsDef.static_size : 0) +
	   (version.to_int () >= 0x00010003u ? varStore.static_size : 0);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (version.sanitize (c) &&
		  glyphClassDef.sanitize (c, this) &&
		  attachList.sanitize (c, this) &&
		  ligCaretList.sanitize (c, this) &&
		  markAttachClassDef.sanitize (c, this) &&
		  hb_barrier () &&
		  ((version.to_int () < 0x00010002u && hb_barrier ()) || markGlyphSetsDef.sanitize (c, this)) &&
		  ((version.to_int () < 0x00010003u && hb_barrier ()) || varStore.sanitize (c, this)));
  }

  static void remap_varidx_after_instantiation (const hb_map_t& varidx_map,
                                                hb_hashmap_t<unsigned, hb_pair_t<unsigned, int>>& layout_variation_idx_delta_map /* IN/OUT */)
  {
    /* varidx_map is empty which means varstore is empty after instantiation,
     * no variations, map all varidx to HB_OT_LAYOUT_NO_VARIATIONS_INDEX.
     * varidx_map doesn't have original varidx, indicating delta row is all
     * zeros, map varidx to HB_OT_LAYOUT_NO_VARIATIONS_INDEX */
    for (auto _ : layout_variation_idx_delta_map.iter_ref ())
    {
      /* old_varidx->(varidx, delta) mapping generated for subsetting, then this
       * varidx is used as key of varidx_map during instantiation */
      uint32_t varidx = _.second.first;
      uint32_t *new_varidx;
      if (varidx_map.has (varidx, &new_varidx))
        _.second.first = *new_varidx;
      else
        _.second.first = HB_OT_LAYOUT_NO_VARIATIONS_INDEX;
    }
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    // Push var store first (if it's needed) so that it's last in the
    // serialization order. Some font consumers assume that varstore runs to
    // the end of the GDEF table.
    // See: https://github.com/harfbuzz/harfbuzz/issues/4636
    auto snapshot_version0 = c->serializer->snapshot ();
    if (unlikely (version.to_int () >= 0x00010002u && hb_barrier () && !c->serializer->embed (markGlyphSetsDef)))
      return_trace (false);

    bool subset_varstore = false;
    unsigned varstore_index = (unsigned) -1;
    auto snapshot_version2 = c->serializer->snapshot ();
    if (version.to_int () >= 0x00010003u && hb_barrier ())
    {
      if (unlikely (!c->serializer->embed (varStore))) return_trace (false);
      if (c->plan->all_axes_pinned)
        out->varStore = 0;
      else if (c->plan->normalized_coords)
      {
        if (varStore)
        {
          item_variations_t item_vars;
          if (item_vars.instantiate (this+varStore, c->plan, true, true,
                                     c->plan->gdef_varstore_inner_maps.as_array ())) {
            subset_varstore = out->varStore.serialize_serialize (c->serializer,
                                                                 item_vars.has_long_word (),
                                                                 c->plan->axis_tags,
                                                                 item_vars.get_region_list (),
                                                                 item_vars.get_vardata_encodings ());
            varstore_index = c->serializer->last_added_child_index();
          }
          remap_varidx_after_instantiation (item_vars.get_varidx_map (),
                                            c->plan->layout_variation_idx_delta_map);
        }
      }
      else
      {
        subset_varstore = out->varStore.serialize_subset (c, varStore, this, c->plan->gdef_varstore_inner_maps.as_array ());
        varstore_index = c->serializer->last_added_child_index();
      }
    }

    out->version.major = version.major;
    out->version.minor = version.minor;

    if (!subset_varstore && version.to_int () >= 0x00010002u) {
      c->serializer->revert (snapshot_version2);
    }

    bool subset_markglyphsetsdef = false;
    if (version.to_int () >= 0x00010002u && hb_barrier ())
    {
      subset_markglyphsetsdef = out->markGlyphSetsDef.serialize_subset (c, markGlyphSetsDef, this);
    }

    if (subset_varstore)
    {
      out->version.minor = 3;
      c->plan->has_gdef_varstore = true;
    } else if (subset_markglyphsetsdef) {
      out->version.minor = 2;      
    } else  {
      out->version.minor = 0;
      c->serializer->revert (snapshot_version0);
    }

    bool subset_glyphclassdef = out->glyphClassDef.serialize_subset (c, glyphClassDef, this, nullptr, false, true);
    bool subset_attachlist = out->attachList.serialize_subset (c, attachList, this);
    bool subset_markattachclassdef = out->markAttachClassDef.serialize_subset (c, markAttachClassDef, this, nullptr, false, true);
    bool subset_ligcaretlist = out->ligCaretList.serialize_subset (c, ligCaretList, this);

    if (subset_varstore && varstore_index != (unsigned) -1) {
      c->serializer->repack_last(varstore_index);
    }

    return_trace (subset_glyphclassdef || subset_attachlist ||
		  subset_ligcaretlist || subset_markattachclassdef ||
		  (out->version.to_int () >= 0x00010002u && subset_markglyphsetsdef) ||
		  (out->version.to_int () >= 0x00010003u && subset_varstore));
  }
};

struct GDEF
{
  static constexpr hb_tag_t tableTag = HB_OT_TAG_GDEF;

  enum GlyphClasses {
    UnclassifiedGlyph	= 0,
    BaseGlyph		= 1,
    LigatureGlyph	= 2,
    MarkGlyph		= 3,
    ComponentGlyph	= 4
  };

  unsigned int get_size () const
  {
    switch (u.version.major) {
    case 1: return u.version1.get_size ();
#ifndef HB_NO_BEYOND_64K
    case 2: return u.version2.get_size ();
#endif
    default: return u.version.static_size;
    }
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!u.version.sanitize (c))) return_trace (false);
    hb_barrier ();
    switch (u.version.major) {
    case 1: return_trace (u.version1.sanitize (c));
#ifndef HB_NO_BEYOND_64K
    case 2: return_trace (u.version2.sanitize (c));
#endif
    default: return_trace (true);
    }
  }

  bool subset (hb_subset_context_t *c) const
  {
    switch (u.version.major) {
    case 1: return u.version1.subset (c);
#ifndef HB_NO_BEYOND_64K
    case 2: return u.version2.subset (c);
#endif
    default: return false;
    }
  }

  bool has_glyph_classes () const
  {
    switch (u.version.major) {
    case 1: return u.version1.glyphClassDef != 0;
#ifndef HB_NO_BEYOND_64K
    case 2: return u.version2.glyphClassDef != 0;
#endif
    default: return false;
    }
  }
  const ClassDef &get_glyph_class_def () const
  {
    switch (u.version.major) {
    case 1: return this+u.version1.glyphClassDef;
#ifndef HB_NO_BEYOND_64K
    case 2: return this+u.version2.glyphClassDef;
#endif
    default: return Null(ClassDef);
    }
  }
  bool has_attach_list () const
  {
    switch (u.version.major) {
    case 1: return u.version1.attachList != 0;
#ifndef HB_NO_BEYOND_64K
    case 2: return u.version2.attachList != 0;
#endif
    default: return false;
    }
  }
  const AttachList &get_attach_list () const
  {
    switch (u.version.major) {
    case 1: return this+u.version1.attachList;
#ifndef HB_NO_BEYOND_64K
    case 2: return this+u.version2.attachList;
#endif
    default: return Null(AttachList);
    }
  }
  bool has_lig_carets () const
  {
    switch (u.version.major) {
    case 1: return u.version1.ligCaretList != 0;
#ifndef HB_NO_BEYOND_64K
    case 2: return u.version2.ligCaretList != 0;
#endif
    default: return false;
    }
  }
  const LigCaretList &get_lig_caret_list () const
  {
    switch (u.version.major) {
    case 1: return this+u.version1.ligCaretList;
#ifndef HB_NO_BEYOND_64K
    case 2: return this+u.version2.ligCaretList;
#endif
    default: return Null(LigCaretList);
    }
  }
  bool has_mark_attachment_types () const
  {
    switch (u.version.major) {
    case 1: return u.version1.markAttachClassDef != 0;
#ifndef HB_NO_BEYOND_64K
    case 2: return u.version2.markAttachClassDef != 0;
#endif
    default: return false;
    }
  }
  const ClassDef &get_mark_attach_class_def () const
  {
    switch (u.version.major) {
    case 1: return this+u.version1.markAttachClassDef;
#ifndef HB_NO_BEYOND_64K
    case 2: return this+u.version2.markAttachClassDef;
#endif
    default: return Null(ClassDef);
    }
  }
  bool has_mark_glyph_sets () const
  {
    switch (u.version.major) {
    case 1: return u.version.to_int () >= 0x00010002u && hb_barrier () && u.version1.markGlyphSetsDef != 0;
#ifndef HB_NO_BEYOND_64K
    case 2: return u.version2.markGlyphSetsDef != 0;
#endif
    default: return false;
    }
  }
  const MarkGlyphSets &get_mark_glyph_sets () const
  {
    switch (u.version.major) {
    case 1: return u.version.to_int () >= 0x00010002u && hb_barrier () ? this+u.version1.markGlyphSetsDef : Null(MarkGlyphSets);
#ifndef HB_NO_BEYOND_64K
    case 2: return this+u.version2.markGlyphSetsDef;
#endif
    default: return Null(MarkGlyphSets);
    }
  }
  bool has_var_store () const
  {
    switch (u.version.major) {
    case 1: return u.version.to_int () >= 0x00010003u && hb_barrier () && u.version1.varStore != 0;
#ifndef HB_NO_BEYOND_64K
    case 2: return u.version2.varStore != 0;
#endif
    default: return false;
    }
  }
  const ItemVariationStore &get_var_store () const
  {
    switch (u.version.major) {
    case 1: return u.version.to_int () >= 0x00010003u && hb_barrier () ? this+u.version1.varStore : Null(ItemVariationStore);
#ifndef HB_NO_BEYOND_64K
    case 2: return this+u.version2.varStore;
#endif
    default: return Null(ItemVariationStore);
    }
  }


  bool has_data () const { return u.version.to_int (); }
  unsigned int get_glyph_class (hb_codepoint_t glyph) const
  { return get_glyph_class_def ().get_class (glyph); }
  void get_glyphs_in_class (unsigned int klass, hb_set_t *glyphs) const
  { get_glyph_class_def ().collect_class (glyphs, klass); }

  unsigned int get_mark_attachment_type (hb_codepoint_t glyph) const
  { return get_mark_attach_class_def ().get_class (glyph); }

  unsigned int get_attach_points (hb_codepoint_t glyph_id,
				  unsigned int start_offset,
				  unsigned int *point_count /* IN/OUT */,
				  unsigned int *point_array /* OUT */) const
  { return get_attach_list ().get_attach_points (glyph_id, start_offset, point_count, point_array); }

  unsigned int get_lig_carets (hb_font_t *font,
			       hb_direction_t direction,
			       hb_codepoint_t glyph_id,
			       unsigned int start_offset,
			       unsigned int *caret_count /* IN/OUT */,
			       hb_position_t *caret_array /* OUT */) const
  { return get_lig_caret_list ().get_lig_carets (font,
						 direction, glyph_id, get_var_store(),
						 start_offset, caret_count, caret_array); }

  bool mark_set_covers (unsigned int set_index, hb_codepoint_t glyph_id) const
  { return get_mark_glyph_sets ().covers (set_index, glyph_id); }

  /* glyph_props is a 16-bit integer where the lower 8-bit have bits representing
   * glyph class and other bits, and high 8-bit the mark attachment type (if any).
   * Not to be confused with lookup_props which is very similar. */
  unsigned int get_glyph_props (hb_codepoint_t glyph) const
  {
    unsigned int klass = get_glyph_class (glyph);

    static_assert (((unsigned int) HB_OT_LAYOUT_GLYPH_PROPS_BASE_GLYPH == (unsigned int) LookupFlag::IgnoreBaseGlyphs), "");
    static_assert (((unsigned int) HB_OT_LAYOUT_GLYPH_PROPS_LIGATURE == (unsigned int) LookupFlag::IgnoreLigatures), "");
    static_assert (((unsigned int) HB_OT_LAYOUT_GLYPH_PROPS_MARK == (unsigned int) LookupFlag::IgnoreMarks), "");

    switch (klass) {
    default:			return HB_OT_LAYOUT_GLYPH_CLASS_UNCLASSIFIED;
    case BaseGlyph:		return HB_OT_LAYOUT_GLYPH_PROPS_BASE_GLYPH;
    case LigatureGlyph:		return HB_OT_LAYOUT_GLYPH_PROPS_LIGATURE;
    case MarkGlyph:
	  klass = get_mark_attachment_type (glyph);
	  return HB_OT_LAYOUT_GLYPH_PROPS_MARK | (klass << 8);
    }
  }

  HB_INTERNAL bool is_blocklisted (hb_blob_t *blob,
				   hb_face_t *face) const;

  struct accelerator_t
  {
    accelerator_t (hb_face_t *face)
    {
      table = hb_sanitize_context_t ().reference_table<GDEF> (face);
      if (unlikely (table->is_blocklisted (table.get_blob (), face)))
      {
	hb_blob_destroy (table.get_blob ());
	table = hb_blob_get_empty ();
      }

#ifndef HB_NO_GDEF_CACHE
      table->get_mark_glyph_sets ().collect_coverage (mark_glyph_set_digests);
#endif
    }
    ~accelerator_t () { table.destroy (); }

    unsigned int get_glyph_props (hb_codepoint_t glyph) const
    {
      unsigned v;

#ifndef HB_NO_GDEF_CACHE
      if (glyph_props_cache.get (glyph, &v))
        return v;
#endif

      v = table->get_glyph_props (glyph);

#ifndef HB_NO_GDEF_CACHE
      if (likely (table.get_blob ())) // Don't try setting if we are the null instance!
	glyph_props_cache.set (glyph, v);
#endif

      return v;

    }

    bool mark_set_covers (unsigned int set_index, hb_codepoint_t glyph_id) const
    {
      return
#ifndef HB_NO_GDEF_CACHE
	     mark_glyph_set_digests[set_index].may_have (glyph_id) &&
#endif
	     table->mark_set_covers (set_index, glyph_id);
    }

    hb_blob_ptr_t<GDEF> table;
#ifndef HB_NO_GDEF_CACHE
    hb_vector_t<hb_set_digest_t> mark_glyph_set_digests;
    mutable hb_cache_t<21, 3, 8> glyph_props_cache;
#endif
  };

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const
  { get_lig_caret_list ().collect_variation_indices (c); }

  protected:
  union {
  FixedVersion<>		version;	/* Version identifier */
  GDEFVersion1_2<SmallTypes>	version1;
#ifndef HB_NO_BEYOND_64K
  GDEFVersion1_2<MediumTypes>	version2;
#endif
  } u;
  public:
  DEFINE_SIZE_MIN (4);
};

struct GDEF_accelerator_t : GDEF::accelerator_t {
  GDEF_accelerator_t (hb_face_t *face) : GDEF::accelerator_t (face) {}
};

} /* namespace OT */


#endif /* OT_LAYOUT_GDEF_GDEF_HH */
