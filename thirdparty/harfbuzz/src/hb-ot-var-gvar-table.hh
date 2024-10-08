/*
 * Copyright © 2019  Adobe Inc.
 * Copyright © 2019  Ebrahim Byagowi
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
 * Adobe Author(s): Michiharu Ariza
 */

#ifndef HB_OT_VAR_GVAR_TABLE_HH
#define HB_OT_VAR_GVAR_TABLE_HH

#include "hb-open-type.hh"
#include "hb-ot-var-common.hh"

/*
 * gvar -- Glyph Variation Table
 * https://docs.microsoft.com/en-us/typography/opentype/spec/gvar
 */
#define HB_OT_TAG_gvar HB_TAG('g','v','a','r')

namespace OT {

struct GlyphVariationData : TupleVariationData
{};

struct glyph_variations_t
{
  using tuple_variations_t = TupleVariationData::tuple_variations_t;
  hb_vector_t<tuple_variations_t> glyph_variations;

  hb_vector_t<char> compiled_shared_tuples;
  private:
  unsigned shared_tuples_count = 0;

  /* shared coords-> index map after instantiation */
  hb_hashmap_t<const hb_vector_t<char>*, unsigned> shared_tuples_idx_map;

  public:
  unsigned compiled_shared_tuples_count () const
  { return shared_tuples_count; }

  unsigned compiled_byte_size () const
  {
    unsigned byte_size = 0;
    for (const auto& _ : glyph_variations)
      byte_size += _.get_compiled_byte_size ();

    return byte_size;
  }

  bool create_from_glyphs_var_data (unsigned axis_count,
                                    const hb_array_t<const F2DOT14> shared_tuples,
                                    const hb_subset_plan_t *plan,
                                    const hb_hashmap_t<hb_codepoint_t, hb_bytes_t>& new_gid_var_data_map)
  {
    if (unlikely (!glyph_variations.alloc (plan->new_to_old_gid_list.length, true)))
      return false;

    auto it = hb_iter (plan->new_to_old_gid_list);
    for (auto &_ : it)
    {
      hb_codepoint_t new_gid = _.first;
      contour_point_vector_t *all_contour_points;
      if (!new_gid_var_data_map.has (new_gid) ||
          !plan->new_gid_contour_points_map.has (new_gid, &all_contour_points))
        return false;
      hb_bytes_t var_data = new_gid_var_data_map.get (new_gid);

      const GlyphVariationData* p = reinterpret_cast<const GlyphVariationData*> (var_data.arrayZ);
      hb_vector_t<unsigned> shared_indices;
      GlyphVariationData::tuple_iterator_t iterator;
      tuple_variations_t tuple_vars;

      /* in case variation data is empty, push an empty struct into the vector,
       * keep the vector in sync with the new_to_old_gid_list */
      if (!var_data || ! p->has_data () || !all_contour_points->length ||
          !GlyphVariationData::get_tuple_iterator (var_data, axis_count,
                                                   var_data.arrayZ,
                                                   shared_indices, &iterator))
      {
        glyph_variations.push (std::move (tuple_vars));
        continue;
      }

      bool is_composite_glyph = false;
      is_composite_glyph = plan->composite_new_gids.has (new_gid);

      if (!p->decompile_tuple_variations (all_contour_points->length, true /* is_gvar */,
                                          iterator, &(plan->axes_old_index_tag_map),
                                          shared_indices, shared_tuples,
                                          tuple_vars, /* OUT */
                                          is_composite_glyph))
        return false;
      glyph_variations.push (std::move (tuple_vars));
    }
    return !glyph_variations.in_error () && glyph_variations.length == plan->new_to_old_gid_list.length;
  }

  bool instantiate (const hb_subset_plan_t *plan)
  {
    unsigned count = plan->new_to_old_gid_list.length;
    bool iup_optimize = false;
    iup_optimize = plan->flags & HB_SUBSET_FLAGS_OPTIMIZE_IUP_DELTAS;
    for (unsigned i = 0; i < count; i++)
    {
      hb_codepoint_t new_gid = plan->new_to_old_gid_list[i].first;
      contour_point_vector_t *all_points;
      if (!plan->new_gid_contour_points_map.has (new_gid, &all_points))
        return false;
      if (!glyph_variations[i].instantiate (plan->axes_location, plan->axes_triple_distances, all_points, iup_optimize))
        return false;
    }
    return true;
  }

  bool compile_bytes (const hb_map_t& axes_index_map,
                      const hb_map_t& axes_old_index_tag_map)
  {
    if (!compile_shared_tuples (axes_index_map, axes_old_index_tag_map))
      return false;
    for (tuple_variations_t& vars: glyph_variations)
      if (!vars.compile_bytes (axes_index_map, axes_old_index_tag_map,
                               true, /* use shared points*/
                               &shared_tuples_idx_map))
        return false;

    return true;
  }

  bool compile_shared_tuples (const hb_map_t& axes_index_map,
                              const hb_map_t& axes_old_index_tag_map)
  {
    /* key is pointer to compiled_peak_coords inside each tuple, hashing
     * function will always deref pointers first */
    hb_hashmap_t<const hb_vector_t<char>*, unsigned> coords_count_map;

    /* count the num of shared coords */
    for (tuple_variations_t& vars: glyph_variations)
    {
      for (tuple_delta_t& var : vars.tuple_vars)
      {
        if (!var.compile_peak_coords (axes_index_map, axes_old_index_tag_map))
          return false;
        unsigned* count;
        if (coords_count_map.has (&(var.compiled_peak_coords), &count))
          coords_count_map.set (&(var.compiled_peak_coords), *count + 1);
        else
          coords_count_map.set (&(var.compiled_peak_coords), 1);
      }
    }

    if (!coords_count_map || coords_count_map.in_error ())
      return false;

    /* add only those coords that are used more than once into the vector and sort */
    hb_vector_t<const hb_vector_t<char>*> shared_coords;
    if (unlikely (!shared_coords.alloc (coords_count_map.get_population ())))
      return false;

    for (const auto _ : coords_count_map.iter ())
    {
      if (_.second == 1) continue;
      shared_coords.push (_.first);
    }

    /* no shared tuples: no coords are used more than once */
    if (!shared_coords) return true;
    /* sorting based on the coords frequency first (high to low), then compare
     * the coords bytes */
    hb_qsort (shared_coords.arrayZ, shared_coords.length, sizeof (hb_vector_t<char>*), _cmp_coords, (void *) (&coords_count_map));

    /* build shared_coords->idx map and shared tuples byte array */

    shared_tuples_count = hb_min (0xFFFu + 1, shared_coords.length);
    unsigned len = shared_tuples_count * (shared_coords[0]->length);
    if (unlikely (!compiled_shared_tuples.alloc (len)))
      return false;

    for (unsigned i = 0; i < shared_tuples_count; i++)
    {
      shared_tuples_idx_map.set (shared_coords[i], i);
      /* add a concat() in hb_vector_t? */
      for (char c : shared_coords[i]->iter ())
        compiled_shared_tuples.push (c);
    }

    return true;
  }

  static int _cmp_coords (const void *pa, const void *pb, void *arg)
  {
    const hb_hashmap_t<const hb_vector_t<char>*, unsigned>* coords_count_map =
        reinterpret_cast<const hb_hashmap_t<const hb_vector_t<char>*, unsigned>*> (arg);

    /* shared_coords is hb_vector_t<const hb_vector_t<char>*> so casting pa/pb
     * to be a pointer to a pointer */
    const hb_vector_t<char>** a = reinterpret_cast<const hb_vector_t<char>**> (const_cast<void*>(pa));
    const hb_vector_t<char>** b = reinterpret_cast<const hb_vector_t<char>**> (const_cast<void*>(pb));

    bool has_a = coords_count_map->has (*a);
    bool has_b = coords_count_map->has (*b);

    if (has_a && has_b)
    {
      unsigned a_num = coords_count_map->get (*a);
      unsigned b_num = coords_count_map->get (*b);

      if (a_num != b_num)
        return b_num - a_num;

      return (*b)->as_array().cmp ((*a)->as_array ());
    }
    else if (has_a) return -1;
    else if (has_b) return 1;
    else return 0;
  }

  template<typename Iterator,
           hb_requires (hb_is_iterator (Iterator))>
  bool serialize_glyph_var_data (hb_serialize_context_t *c,
                                 Iterator it,
                                 bool long_offset,
                                 unsigned num_glyphs,
                                 char* glyph_var_data_offsets /* OUT: glyph var data offsets array */) const
  {
    TRACE_SERIALIZE (this);

    if (long_offset)
    {
      ((HBUINT32 *) glyph_var_data_offsets)[0] = 0;
      glyph_var_data_offsets += 4;
    }
    else
    {
      ((HBUINT16 *) glyph_var_data_offsets)[0] = 0;
      glyph_var_data_offsets += 2;
    }
    unsigned glyph_offset = 0;
    hb_codepoint_t last_gid = 0;
    unsigned idx = 0;

    TupleVariationData* cur_glyph = c->start_embed<TupleVariationData> ();
    if (!cur_glyph) return_trace (false);
    for (auto &_ : it)
    {
      hb_codepoint_t gid = _.first;
      if (long_offset)
        for (; last_gid < gid; last_gid++)
          ((HBUINT32 *) glyph_var_data_offsets)[last_gid] = glyph_offset;
      else
        for (; last_gid < gid; last_gid++)
          ((HBUINT16 *) glyph_var_data_offsets)[last_gid] = glyph_offset / 2;

      if (idx >= glyph_variations.length) return_trace (false);
      if (!cur_glyph->serialize (c, true, glyph_variations[idx])) return_trace (false);
      TupleVariationData* next_glyph = c->start_embed<TupleVariationData> ();
      glyph_offset += (char *) next_glyph - (char *) cur_glyph;

      if (long_offset)
        ((HBUINT32 *) glyph_var_data_offsets)[gid] = glyph_offset;
      else
        ((HBUINT16 *) glyph_var_data_offsets)[gid] = glyph_offset / 2;

      last_gid++;
      idx++;
      cur_glyph = next_glyph;
    }

    if (long_offset)
      for (; last_gid < num_glyphs; last_gid++)
        ((HBUINT32 *) glyph_var_data_offsets)[last_gid] = glyph_offset;
    else
      for (; last_gid < num_glyphs; last_gid++)
        ((HBUINT16 *) glyph_var_data_offsets)[last_gid] = glyph_offset / 2;
    return_trace (true);
  }
};

struct gvar
{
  static constexpr hb_tag_t tableTag = HB_OT_TAG_gvar;

  bool sanitize_shallow (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
		  (version.major == 1) &&
		  sharedTuples.sanitize (c, this, axisCount * sharedTupleCount) &&
		  (is_long_offset () ?
		     c->check_array (get_long_offset_array (), c->get_num_glyphs () + 1) :
		     c->check_array (get_short_offset_array (), c->get_num_glyphs () + 1)));
  }

  /* GlyphVariationData not sanitized here; must be checked while accessing each glyph variation data */
  bool sanitize (hb_sanitize_context_t *c) const
  { return sanitize_shallow (c); }

  bool decompile_glyph_variations (hb_subset_context_t *c,
                                   glyph_variations_t& glyph_vars /* OUT */) const
  {
    hb_hashmap_t<hb_codepoint_t, hb_bytes_t> new_gid_var_data_map;
    auto it = hb_iter (c->plan->new_to_old_gid_list);
    if (it->first == 0 && !(c->plan->flags & HB_SUBSET_FLAGS_NOTDEF_OUTLINE))
    {
      new_gid_var_data_map.set (0, hb_bytes_t ());
      it++;
    }

    for (auto &_ : it)
    {
      hb_codepoint_t new_gid = _.first;
      hb_codepoint_t old_gid = _.second;
      hb_bytes_t var_data_bytes = get_glyph_var_data_bytes (c->source_blob, glyphCountX, old_gid);
      new_gid_var_data_map.set (new_gid, var_data_bytes);
    }

    if (new_gid_var_data_map.in_error ()) return false;

    hb_array_t<const F2DOT14> shared_tuples = (this+sharedTuples).as_array ((unsigned) sharedTupleCount * (unsigned) axisCount);
    return glyph_vars.create_from_glyphs_var_data (axisCount, shared_tuples, c->plan, new_gid_var_data_map);
  }

  template<typename Iterator,
           hb_requires (hb_is_iterator (Iterator))>
  bool serialize (hb_serialize_context_t *c,
                  const glyph_variations_t& glyph_vars,
                  Iterator it,
                  unsigned axis_count,
                  unsigned num_glyphs,
                  bool force_long_offsets) const
  {
    TRACE_SERIALIZE (this);
    gvar *out = c->allocate_min<gvar> ();
    if (unlikely (!out)) return_trace (false);

    out->version.major = 1;
    out->version.minor = 0;
    out->axisCount = axis_count;
    out->glyphCountX = hb_min (0xFFFFu, num_glyphs);

    unsigned glyph_var_data_size = glyph_vars.compiled_byte_size ();
    /* According to the spec: If the short format (Offset16) is used for offsets,
     * the value stored is the offset divided by 2, so the maximum data size should
     * be 2 * 0xFFFFu, which is 0x1FFFEu */
    bool long_offset = glyph_var_data_size > 0x1FFFEu || force_long_offsets;
    out->flags = long_offset ? 1 : 0;

    HBUINT8 *glyph_var_data_offsets = c->allocate_size<HBUINT8> ((long_offset ? 4 : 2) * (num_glyphs + 1), false);
    if (!glyph_var_data_offsets) return_trace (false);

    /* shared tuples */
    unsigned shared_tuple_count = glyph_vars.compiled_shared_tuples_count ();
    out->sharedTupleCount = shared_tuple_count;

    if (!shared_tuple_count)
      out->sharedTuples = 0;
    else
    {
      hb_array_t<const char> shared_tuples = glyph_vars.compiled_shared_tuples.as_array ().copy (c);
      if (!shared_tuples.arrayZ) return_trace (false);
      out->sharedTuples = shared_tuples.arrayZ - (char *) out;
    }

    char *glyph_var_data = c->start_embed<char> ();
    if (!glyph_var_data) return_trace (false);
    out->dataZ = glyph_var_data - (char *) out;

    return_trace (glyph_vars.serialize_glyph_var_data (c, it, long_offset, num_glyphs,
                                                       (char *) glyph_var_data_offsets));
  }

  bool instantiate (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    glyph_variations_t glyph_vars;
    if (!decompile_glyph_variations (c, glyph_vars))
      return_trace (false);

    if (!glyph_vars.instantiate (c->plan)) return_trace (false);
    if (!glyph_vars.compile_bytes (c->plan->axes_index_map, c->plan->axes_old_index_tag_map))
      return_trace (false);

    unsigned axis_count = c->plan->axes_index_map.get_population ();
    unsigned num_glyphs = c->plan->num_output_glyphs ();
    auto it = hb_iter (c->plan->new_to_old_gid_list);

    bool force_long_offsets = false;
#ifdef HB_EXPERIMENTAL_API
    force_long_offsets = c->plan->flags & HB_SUBSET_FLAGS_IFTB_REQUIREMENTS;
#endif
    return_trace (serialize (c->serializer, glyph_vars, it, axis_count, num_glyphs, force_long_offsets));
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    if (c->plan->all_axes_pinned)
      return_trace (false);

    if (c->plan->normalized_coords)
      return_trace (instantiate (c));

    unsigned glyph_count = version.to_int () ? c->plan->source->get_num_glyphs () : 0;

    gvar *out = c->serializer->allocate_min<gvar> ();
    if (unlikely (!out)) return_trace (false);

    out->version.major = 1;
    out->version.minor = 0;
    out->axisCount = axisCount;
    out->sharedTupleCount = sharedTupleCount;

    unsigned int num_glyphs = c->plan->num_output_glyphs ();
    out->glyphCountX = hb_min (0xFFFFu, num_glyphs);

    auto it = hb_iter (c->plan->new_to_old_gid_list);
    if (it->first == 0 && !(c->plan->flags & HB_SUBSET_FLAGS_NOTDEF_OUTLINE))
      it++;
    unsigned int subset_data_size = 0;
    for (auto &_ : it)
    {
      hb_codepoint_t old_gid = _.second;
      subset_data_size += get_glyph_var_data_bytes (c->source_blob, glyph_count, old_gid).length;
    }

    /* According to the spec: If the short format (Offset16) is used for offsets,
     * the value stored is the offset divided by 2, so the maximum data size should
     * be 2 * 0xFFFFu, which is 0x1FFFEu */
    bool long_offset = subset_data_size > 0x1FFFEu;
#ifdef HB_EXPERIMENTAL_API
    long_offset = long_offset || (c->plan->flags & HB_SUBSET_FLAGS_IFTB_REQUIREMENTS);
#endif
    out->flags = long_offset ? 1 : 0;

    HBUINT8 *subset_offsets = c->serializer->allocate_size<HBUINT8> ((long_offset ? 4 : 2) * (num_glyphs + 1), false);
    if (!subset_offsets) return_trace (false);

    /* shared tuples */
    if (!sharedTupleCount || !sharedTuples)
      out->sharedTuples = 0;
    else
    {
      unsigned int shared_tuple_size = F2DOT14::static_size * axisCount * sharedTupleCount;
      F2DOT14 *tuples = c->serializer->allocate_size<F2DOT14> (shared_tuple_size);
      if (!tuples) return_trace (false);
      out->sharedTuples = (char *) tuples - (char *) out;
      hb_memcpy (tuples, this+sharedTuples, shared_tuple_size);
    }

    /* This ordering relative to the shared tuples array, which puts the glyphVariationData
       last in the table, is required when HB_SUBSET_FLAGS_IFTB_REQUIREMENTS is set */
    char *subset_data = c->serializer->allocate_size<char> (subset_data_size, false);
    if (!subset_data) return_trace (false);
    out->dataZ = subset_data - (char *) out;


    if (long_offset)
    {
      ((HBUINT32 *) subset_offsets)[0] = 0;
      subset_offsets += 4;
    }
    else
    {
      ((HBUINT16 *) subset_offsets)[0] = 0;
      subset_offsets += 2;
    }
    unsigned int glyph_offset = 0;

    hb_codepoint_t last = 0;
    it = hb_iter (c->plan->new_to_old_gid_list);
    if (it->first == 0 && !(c->plan->flags & HB_SUBSET_FLAGS_NOTDEF_OUTLINE))
      it++;
    for (auto &_ : it)
    {
      hb_codepoint_t gid = _.first;
      hb_codepoint_t old_gid = _.second;

      if (long_offset)
	for (; last < gid; last++)
	  ((HBUINT32 *) subset_offsets)[last] = glyph_offset;
      else
	for (; last < gid; last++)
	  ((HBUINT16 *) subset_offsets)[last] = glyph_offset / 2;

      hb_bytes_t var_data_bytes = get_glyph_var_data_bytes (c->source_blob,
							    glyph_count,
							    old_gid);

      hb_memcpy (subset_data, var_data_bytes.arrayZ, var_data_bytes.length);
      subset_data += var_data_bytes.length;
      glyph_offset += var_data_bytes.length;

      if (long_offset)
	((HBUINT32 *) subset_offsets)[gid] = glyph_offset;
      else
	((HBUINT16 *) subset_offsets)[gid] = glyph_offset / 2;

      last++; // Skip over gid
    }

    if (long_offset)
      for (; last < num_glyphs; last++)
	((HBUINT32 *) subset_offsets)[last] = glyph_offset;
    else
      for (; last < num_glyphs; last++)
	((HBUINT16 *) subset_offsets)[last] = glyph_offset / 2;

    return_trace (true);
  }

  protected:
  const hb_bytes_t get_glyph_var_data_bytes (hb_blob_t *blob,
					     unsigned glyph_count,
					     hb_codepoint_t glyph) const
  {
    unsigned start_offset = get_offset (glyph_count, glyph);
    unsigned end_offset = get_offset (glyph_count, glyph+1);
    if (unlikely (end_offset < start_offset)) return hb_bytes_t ();
    unsigned length = end_offset - start_offset;
    hb_bytes_t var_data = blob->as_bytes ().sub_array (((unsigned) dataZ) + start_offset, length);
    return likely (var_data.length >= GlyphVariationData::min_size) ? var_data : hb_bytes_t ();
  }

  bool is_long_offset () const { return flags & 1; }

  unsigned get_offset (unsigned glyph_count, unsigned i) const
  {
    if (unlikely (i > glyph_count)) return 0;
    hb_barrier ();
    return is_long_offset () ? get_long_offset_array ()[i] : get_short_offset_array ()[i] * 2;
  }

  const HBUINT32 * get_long_offset_array () const { return (const HBUINT32 *) &offsetZ; }
  const HBUINT16 *get_short_offset_array () const { return (const HBUINT16 *) &offsetZ; }

  public:
  struct accelerator_t
  {
    accelerator_t (hb_face_t *face)
    {
      table = hb_sanitize_context_t ().reference_table<gvar> (face);
      /* If sanitize failed, set glyphCount to 0. */
      glyphCount = table->version.to_int () ? face->get_num_glyphs () : 0;

      /* For shared tuples that only have one axis active, shared the index of
       * that axis as a cache. This will speed up caclulate_scalar() a lot
       * for fonts with lots of axes and many "monovar" tuples. */
      hb_array_t<const F2DOT14> shared_tuples = (table+table->sharedTuples).as_array (table->sharedTupleCount * table->axisCount);
      unsigned count = table->sharedTupleCount;
      if (unlikely (!shared_tuple_active_idx.resize (count, false))) return;
      unsigned axis_count = table->axisCount;
      for (unsigned i = 0; i < count; i++)
      {
	hb_array_t<const F2DOT14> tuple = shared_tuples.sub_array (axis_count * i, axis_count);
	int idx1 = -1, idx2 = -1;
	for (unsigned j = 0; j < axis_count; j++)
	{
	  const F2DOT14 &peak = tuple.arrayZ[j];
	  if (peak.to_int () != 0)
	  {
	    if (idx1 == -1)
	      idx1 = j;
	    else if (idx2 == -1)
	      idx2 = j;
	    else
	    {
	      idx1 = idx2 = -1;
	      break;
	    }
	  }
	}
	shared_tuple_active_idx.arrayZ[i] = {idx1, idx2};
      }
    }
    ~accelerator_t () { table.destroy (); }

    private:

    static float infer_delta (const hb_array_t<contour_point_t> points,
			      const hb_array_t<contour_point_t> deltas,
			      unsigned int target, unsigned int prev, unsigned int next,
			      float contour_point_t::*m)
    {
      float target_val = points.arrayZ[target].*m;
      float prev_val = points.arrayZ[prev].*m;
      float next_val = points.arrayZ[next].*m;
      float prev_delta =  deltas.arrayZ[prev].*m;
      float next_delta =  deltas.arrayZ[next].*m;

      if (prev_val == next_val)
	return (prev_delta == next_delta) ? prev_delta : 0.f;
      else if (target_val <= hb_min (prev_val, next_val))
	return (prev_val < next_val) ? prev_delta : next_delta;
      else if (target_val >= hb_max (prev_val, next_val))
	return (prev_val > next_val) ? prev_delta : next_delta;

      /* linear interpolation */
      float r = (target_val - prev_val) / (next_val - prev_val);
      return prev_delta + r * (next_delta - prev_delta);
    }

    static unsigned int next_index (unsigned int i, unsigned int start, unsigned int end)
    { return (i >= end) ? start : (i + 1); }

    public:
    bool apply_deltas_to_points (hb_codepoint_t glyph,
				 hb_array_t<const int> coords,
				 const hb_array_t<contour_point_t> points,
				 bool phantom_only = false) const
    {
      if (unlikely (glyph >= glyphCount)) return true;

      hb_bytes_t var_data_bytes = table->get_glyph_var_data_bytes (table.get_blob (), glyphCount, glyph);
      if (!var_data_bytes.as<GlyphVariationData> ()->has_data ()) return true;
      hb_vector_t<unsigned int> shared_indices;
      GlyphVariationData::tuple_iterator_t iterator;
      if (!GlyphVariationData::get_tuple_iterator (var_data_bytes, table->axisCount,
						   var_data_bytes.arrayZ,
						   shared_indices, &iterator))
	return true; /* so isn't applied at all */

      /* Save original points for inferred delta calculation */
      contour_point_vector_t orig_points_vec; // Populated lazily
      auto orig_points = orig_points_vec.as_array ();

      /* flag is used to indicate referenced point */
      contour_point_vector_t deltas_vec; // Populated lazily
      auto deltas = deltas_vec.as_array ();

      hb_vector_t<unsigned> end_points; // Populated lazily

      unsigned num_coords = table->axisCount;
      hb_array_t<const F2DOT14> shared_tuples = (table+table->sharedTuples).as_array (table->sharedTupleCount * num_coords);

      hb_vector_t<unsigned int> private_indices;
      hb_vector_t<int> x_deltas;
      hb_vector_t<int> y_deltas;
      unsigned count = points.length;
      bool flush = false;
      do
      {
	float scalar = iterator.current_tuple->calculate_scalar (coords, num_coords, shared_tuples,
								 &shared_tuple_active_idx);
	if (scalar == 0.f) continue;
	const HBUINT8 *p = iterator.get_serialized_data ();
	unsigned int length = iterator.current_tuple->get_data_size ();
	if (unlikely (!iterator.var_data_bytes.check_range (p, length)))
	  return false;

	if (!deltas)
	{
	  if (unlikely (!deltas_vec.resize (count, false))) return false;
	  deltas = deltas_vec.as_array ();
	  hb_memset (deltas.arrayZ + (phantom_only ? count - 4 : 0), 0,
		     (phantom_only ? 4 : count) * sizeof (deltas[0]));
	}

	const HBUINT8 *end = p + length;

	bool has_private_points = iterator.current_tuple->has_private_points ();
	if (has_private_points &&
	    !GlyphVariationData::decompile_points (p, private_indices, end))
	  return false;
	const hb_array_t<unsigned int> &indices = has_private_points ? private_indices : shared_indices;

	bool apply_to_all = (indices.length == 0);
	unsigned int num_deltas = apply_to_all ? points.length : indices.length;
	if (unlikely (!x_deltas.resize (num_deltas, false))) return false;
	if (unlikely (!GlyphVariationData::decompile_deltas (p, x_deltas, end))) return false;
	if (unlikely (!y_deltas.resize (num_deltas, false))) return false;
	if (unlikely (!GlyphVariationData::decompile_deltas (p, y_deltas, end))) return false;

	if (!apply_to_all)
	{
	  if (!orig_points && !phantom_only)
	  {
	    orig_points_vec.extend (points);
	    if (unlikely (orig_points_vec.in_error ())) return false;
	    orig_points = orig_points_vec.as_array ();
	  }

	  if (flush)
	  {
	    for (unsigned int i = phantom_only ? count - 4 : 0; i < count; i++)
	      points.arrayZ[i].translate (deltas.arrayZ[i]);
	    flush = false;

	  }
	  hb_memset (deltas.arrayZ + (phantom_only ? count - 4 : 0), 0,
		     (phantom_only ? 4 : count) * sizeof (deltas[0]));
	}

	if (HB_OPTIMIZE_SIZE_VAL)
	{
	  for (unsigned int i = 0; i < num_deltas; i++)
	  {
	    unsigned int pt_index;
	    if (apply_to_all)
	      pt_index = i;
	    else
	    {
	      pt_index = indices[i];
	      if (unlikely (pt_index >= deltas.length)) continue;
	    }
	    if (phantom_only && pt_index < count - 4) continue;
	    auto &delta = deltas.arrayZ[pt_index];
	    delta.flag = 1;	/* this point is referenced, i.e., explicit deltas specified */
	    delta.x += x_deltas.arrayZ[i] * scalar;
	    delta.y += y_deltas.arrayZ[i] * scalar;
	  }
	}
	else
	{
	  /* Ouch. Four cases... for optimization. */
	  if (scalar != 1.0f)
	  {
	    if (apply_to_all)
	      for (unsigned int i = phantom_only ? count - 4 : 0; i < count; i++)
	      {
		unsigned int pt_index = i;
		auto &delta = deltas.arrayZ[pt_index];
		delta.x += x_deltas.arrayZ[i] * scalar;
		delta.y += y_deltas.arrayZ[i] * scalar;
	      }
	    else
	      for (unsigned int i = 0; i < num_deltas; i++)
	      {
		unsigned int pt_index = indices[i];
		if (unlikely (pt_index >= deltas.length)) continue;
		if (phantom_only && pt_index < count - 4) continue;
		auto &delta = deltas.arrayZ[pt_index];
		delta.flag = 1;	/* this point is referenced, i.e., explicit deltas specified */
		delta.x += x_deltas.arrayZ[i] * scalar;
		delta.y += y_deltas.arrayZ[i] * scalar;
	      }
	  }
	  else
	  {
	    if (apply_to_all)
	      for (unsigned int i = phantom_only ? count - 4 : 0; i < count; i++)
	      {
		unsigned int pt_index = i;
		auto &delta = deltas.arrayZ[pt_index];
		delta.x += x_deltas.arrayZ[i];
		delta.y += y_deltas.arrayZ[i];
	      }
	    else
	      for (unsigned int i = 0; i < num_deltas; i++)
	      {
		unsigned int pt_index = indices[i];
		if (unlikely (pt_index >= deltas.length)) continue;
		if (phantom_only && pt_index < count - 4) continue;
		auto &delta = deltas.arrayZ[pt_index];
		delta.flag = 1;	/* this point is referenced, i.e., explicit deltas specified */
		delta.x += x_deltas.arrayZ[i];
		delta.y += y_deltas.arrayZ[i];
	      }
	  }
	}

	/* infer deltas for unreferenced points */
	if (!apply_to_all && !phantom_only)
	{
	  if (!end_points)
	  {
	    for (unsigned i = 0; i < count; ++i)
	      if (points.arrayZ[i].is_end_point)
		end_points.push (i);
	    if (unlikely (end_points.in_error ())) return false;
	  }

	  unsigned start_point = 0;
	  for (unsigned end_point : end_points)
	  {
	    /* Check the number of unreferenced points in a contour. If no unref points or no ref points, nothing to do. */
	    unsigned unref_count = 0;
	    for (unsigned i = start_point; i < end_point + 1; i++)
	      unref_count += deltas.arrayZ[i].flag;
	    unref_count = (end_point - start_point + 1) - unref_count;

	    unsigned j = start_point;
	    if (unref_count == 0 || unref_count > end_point - start_point)
	      goto no_more_gaps;

	    for (;;)
	    {
	      /* Locate the next gap of unreferenced points between two referenced points prev and next.
	       * Note that a gap may wrap around at left (start_point) and/or at right (end_point).
	       */
	      unsigned int prev, next, i;
	      for (;;)
	      {
		i = j;
		j = next_index (i, start_point, end_point);
		if (deltas.arrayZ[i].flag && !deltas.arrayZ[j].flag) break;
	      }
	      prev = j = i;
	      for (;;)
	      {
		i = j;
		j = next_index (i, start_point, end_point);
		if (!deltas.arrayZ[i].flag && deltas.arrayZ[j].flag) break;
	      }
	      next = j;
	      /* Infer deltas for all unref points in the gap between prev and next */
	      i = prev;
	      for (;;)
	      {
		i = next_index (i, start_point, end_point);
		if (i == next) break;
		deltas.arrayZ[i].x = infer_delta (orig_points, deltas, i, prev, next, &contour_point_t::x);
		deltas.arrayZ[i].y = infer_delta (orig_points, deltas, i, prev, next, &contour_point_t::y);
		if (--unref_count == 0) goto no_more_gaps;
	      }
	    }
	  no_more_gaps:
	    start_point = end_point + 1;
	  }
	}

	flush = true;

      } while (iterator.move_to_next ());

      if (flush)
      {
	for (unsigned int i = phantom_only ? count - 4 : 0; i < count; i++)
	  points.arrayZ[i].translate (deltas.arrayZ[i]);
      }

      return true;
    }

    unsigned int get_axis_count () const { return table->axisCount; }

    private:
    hb_blob_ptr_t<gvar> table;
    unsigned glyphCount;
    hb_vector_t<hb_pair_t<int, int>> shared_tuple_active_idx;
  };

  protected:
  FixedVersion<>version;	/* Version number of the glyph variations table
				 * Set to 0x00010000u. */
  HBUINT16	axisCount;	/* The number of variation axes for this font. This must be
				 * the same number as axisCount in the 'fvar' table. */
  HBUINT16	sharedTupleCount;
				/* The number of shared tuple records. Shared tuple records
				 * can be referenced within glyph variation data tables for
				 * multiple glyphs, as opposed to other tuple records stored
				 * directly within a glyph variation data table. */
  NNOffset32To<UnsizedArrayOf<F2DOT14>>
		sharedTuples;	/* Offset from the start of this table to the shared tuple records.
				 * Array of tuple records shared across all glyph variation data tables. */
  HBUINT16	glyphCountX;	/* The number of glyphs in this font. This must match the number of
				 * glyphs stored elsewhere in the font. */
  HBUINT16	flags;		/* Bit-field that gives the format of the offset array that follows.
				 * If bit 0 is clear, the offsets are uint16; if bit 0 is set, the
				 * offsets are uint32. */
  Offset32To<GlyphVariationData>
		dataZ;		/* Offset from the start of this table to the array of
				 * GlyphVariationData tables. */
  UnsizedArrayOf<HBUINT8>
		offsetZ;	/* Offsets from the start of the GlyphVariationData array
				 * to each GlyphVariationData table. */
  public:
  DEFINE_SIZE_ARRAY (20, offsetZ);
};

struct gvar_accelerator_t : gvar::accelerator_t {
  gvar_accelerator_t (hb_face_t *face) : gvar::accelerator_t (face) {}
};

} /* namespace OT */

#endif /* HB_OT_VAR_GVAR_TABLE_HH */
