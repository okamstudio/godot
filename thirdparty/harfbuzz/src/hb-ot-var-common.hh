/*
 * Copyright © 2021  Google, Inc.
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
 */

#ifndef HB_OT_VAR_COMMON_HH
#define HB_OT_VAR_COMMON_HH

#include "hb-ot-layout-common.hh"
#include "hb-priority-queue.hh"
#include "hb-subset-instancer-iup.hh"


namespace OT {


/* https://docs.microsoft.com/en-us/typography/opentype/spec/otvarcommonformats#tuplevariationheader */
struct TupleVariationHeader
{
  friend struct tuple_delta_t;
  unsigned get_size (unsigned axis_count) const
  { return min_size + get_all_tuples (axis_count).get_size (); }

  unsigned get_data_size () const { return varDataSize; }

  const TupleVariationHeader &get_next (unsigned axis_count) const
  { return StructAtOffset<TupleVariationHeader> (this, get_size (axis_count)); }

  bool unpack_axis_tuples (unsigned axis_count,
                           const hb_array_t<const F2DOT14> shared_tuples,
                           const hb_map_t *axes_old_index_tag_map,
                           hb_hashmap_t<hb_tag_t, Triple>& axis_tuples /* OUT */) const
  {
    const F2DOT14 *peak_tuple = nullptr;
    if (has_peak ())
      peak_tuple = get_peak_tuple (axis_count).arrayZ;
    else
    {
      unsigned int index = get_index ();
      if (unlikely ((index + 1) * axis_count > shared_tuples.length))
        return false;
      peak_tuple = shared_tuples.sub_array (axis_count * index, axis_count).arrayZ;
    }

    const F2DOT14 *start_tuple = nullptr;
    const F2DOT14 *end_tuple = nullptr;
    bool has_interm = has_intermediate ();

    if (has_interm)
    {
      start_tuple = get_start_tuple (axis_count).arrayZ;
      end_tuple = get_end_tuple (axis_count).arrayZ;
    }

    for (unsigned i = 0; i < axis_count; i++)
    {
      float peak = peak_tuple[i].to_float ();
      if (peak == 0.f) continue;

      hb_tag_t *axis_tag;
      if (!axes_old_index_tag_map->has (i, &axis_tag))
        return false;

      float start, end;
      if (has_interm)
      {
        start = start_tuple[i].to_float ();
        end = end_tuple[i].to_float ();
      }
      else
      {
        start = hb_min (peak, 0.f);
        end = hb_max (peak, 0.f);
      }
      axis_tuples.set (*axis_tag, Triple ((double) start, (double) peak, (double) end));
    }

    return true;
  }

  double calculate_scalar (hb_array_t<const int> coords, unsigned int coord_count,
			   const hb_array_t<const F2DOT14> shared_tuples,
			   const hb_vector_t<hb_pair_t<int,int>> *shared_tuple_active_idx = nullptr) const
  {
    const F2DOT14 *peak_tuple;

    unsigned start_idx = 0;
    unsigned end_idx = coord_count;
    unsigned step = 1;

    if (has_peak ())
      peak_tuple = get_peak_tuple (coord_count).arrayZ;
    else
    {
      unsigned int index = get_index ();
      if (unlikely ((index + 1) * coord_count > shared_tuples.length))
        return 0.0;
      peak_tuple = shared_tuples.sub_array (coord_count * index, coord_count).arrayZ;

      if (shared_tuple_active_idx)
      {
	if (unlikely (index >= shared_tuple_active_idx->length))
	  return 0.0;
	auto _ = (*shared_tuple_active_idx).arrayZ[index];
	if (_.second != -1)
	{
	  start_idx = _.first;
	  end_idx = _.second + 1;
	  step = _.second - _.first;
	}
	else if (_.first != -1)
	{
	  start_idx = _.first;
	  end_idx = start_idx + 1;
	}
      }
    }

    const F2DOT14 *start_tuple = nullptr;
    const F2DOT14 *end_tuple = nullptr;
    bool has_interm = has_intermediate ();
    if (has_interm)
    {
      start_tuple = get_start_tuple (coord_count).arrayZ;
      end_tuple = get_end_tuple (coord_count).arrayZ;
    }

    double scalar = 1.0;
    for (unsigned int i = start_idx; i < end_idx; i += step)
    {
      int peak = peak_tuple[i].to_int ();
      if (!peak) continue;

      int v = coords[i];
      if (v == peak) continue;

      if (has_interm)
      {
        int start = start_tuple[i].to_int ();
        int end = end_tuple[i].to_int ();
        if (unlikely (start > peak || peak > end ||
                      (start < 0 && end > 0 && peak))) continue;
        if (v < start || v > end) return 0.0;
        if (v < peak)
        { if (peak != start) scalar *= (double) (v - start) / (peak - start); }
        else
        { if (peak != end) scalar *= (double) (end - v) / (end - peak); }
      }
      else if (!v || v < hb_min (0, peak) || v > hb_max (0, peak)) return 0.0;
      else
        scalar *= (double) v / peak;
    }
    return scalar;
  }

  bool           has_peak () const { return tupleIndex & TuppleIndex::EmbeddedPeakTuple; }
  bool   has_intermediate () const { return tupleIndex & TuppleIndex::IntermediateRegion; }
  bool has_private_points () const { return tupleIndex & TuppleIndex::PrivatePointNumbers; }
  unsigned      get_index () const { return tupleIndex & TuppleIndex::TupleIndexMask; }

  protected:
  struct TuppleIndex : HBUINT16
  {
    enum Flags {
      EmbeddedPeakTuple   = 0x8000u,
      IntermediateRegion  = 0x4000u,
      PrivatePointNumbers = 0x2000u,
      TupleIndexMask      = 0x0FFFu
    };

    TuppleIndex& operator = (uint16_t i) { HBUINT16::operator= (i); return *this; }
    DEFINE_SIZE_STATIC (2);
  };

  hb_array_t<const F2DOT14> get_all_tuples (unsigned axis_count) const
  { return StructAfter<UnsizedArrayOf<F2DOT14>> (tupleIndex).as_array ((has_peak () + has_intermediate () * 2) * axis_count); }
  hb_array_t<const F2DOT14> get_peak_tuple (unsigned axis_count) const
  { return get_all_tuples (axis_count).sub_array (0, axis_count); }
  hb_array_t<const F2DOT14> get_start_tuple (unsigned axis_count) const
  { return get_all_tuples (axis_count).sub_array (has_peak () * axis_count, axis_count); }
  hb_array_t<const F2DOT14> get_end_tuple (unsigned axis_count) const
  { return get_all_tuples (axis_count).sub_array (has_peak () * axis_count + axis_count, axis_count); }

  HBUINT16      varDataSize;    /* The size in bytes of the serialized
                                 * data for this tuple variation table. */
  TuppleIndex   tupleIndex;     /* A packed field. The high 4 bits are flags (see below).
                                   The low 12 bits are an index into a shared tuple
                                   records array. */
  /* UnsizedArrayOf<F2DOT14> peakTuple - optional */
                                /* Peak tuple record for this tuple variation table — optional,
                                 * determined by flags in the tupleIndex value.
                                 *
                                 * Note that this must always be included in the 'cvar' table. */
  /* UnsizedArrayOf<F2DOT14> intermediateStartTuple - optional */
                                /* Intermediate start tuple record for this tuple variation table — optional,
                                   determined by flags in the tupleIndex value. */
  /* UnsizedArrayOf<F2DOT14> intermediateEndTuple - optional */
                                /* Intermediate end tuple record for this tuple variation table — optional,
                                 * determined by flags in the tupleIndex value. */
  public:
  DEFINE_SIZE_MIN (4);
};

struct tuple_delta_t
{
  static constexpr bool realloc_move = true;  // Watch out when adding new members!

  public:
  hb_hashmap_t<hb_tag_t, Triple> axis_tuples;

  /* indices_length = point_count, indice[i] = 1 means point i is referenced */
  hb_vector_t<bool> indices;

  hb_vector_t<double> deltas_x;
  /* empty for cvar tuples */
  hb_vector_t<double> deltas_y;

  /* compiled data: header and deltas
   * compiled point data is saved in a hashmap within tuple_variations_t cause
   * some point sets might be reused by different tuple variations */
  hb_vector_t<unsigned char> compiled_tuple_header;
  hb_vector_t<unsigned char> compiled_deltas;

  /* compiled peak coords, empty for non-gvar tuples */
  hb_vector_t<char> compiled_peak_coords;

  tuple_delta_t () = default;
  tuple_delta_t (const tuple_delta_t& o) = default;

  friend void swap (tuple_delta_t& a, tuple_delta_t& b) noexcept
  {
    hb_swap (a.axis_tuples, b.axis_tuples);
    hb_swap (a.indices, b.indices);
    hb_swap (a.deltas_x, b.deltas_x);
    hb_swap (a.deltas_y, b.deltas_y);
    hb_swap (a.compiled_tuple_header, b.compiled_tuple_header);
    hb_swap (a.compiled_deltas, b.compiled_deltas);
    hb_swap (a.compiled_peak_coords, b.compiled_peak_coords);
  }

  tuple_delta_t (tuple_delta_t&& o)  noexcept : tuple_delta_t ()
  { hb_swap (*this, o); }

  tuple_delta_t& operator = (tuple_delta_t&& o) noexcept
  {
    hb_swap (*this, o);
    return *this;
  }

  void remove_axis (hb_tag_t axis_tag)
  { axis_tuples.del (axis_tag); }

  bool set_tent (hb_tag_t axis_tag, Triple tent)
  { return axis_tuples.set (axis_tag, tent); }

  tuple_delta_t& operator += (const tuple_delta_t& o)
  {
    unsigned num = indices.length;
    for (unsigned i = 0; i < num; i++)
    {
      if (indices.arrayZ[i])
      {
        if (o.indices.arrayZ[i])
        {
          deltas_x[i] += o.deltas_x[i];
          if (deltas_y && o.deltas_y)
            deltas_y[i] += o.deltas_y[i];
        }
      }
      else
      {
        if (!o.indices.arrayZ[i]) continue;
        indices.arrayZ[i] = true;
        deltas_x[i] = o.deltas_x[i];
        if (deltas_y && o.deltas_y)
          deltas_y[i] = o.deltas_y[i];
      }
    }
    return *this;
  }

  tuple_delta_t& operator *= (double scalar)
  {
    if (scalar == 1.0)
      return *this;

    unsigned num = indices.length;
    if (deltas_y)
      for (unsigned i = 0; i < num; i++)
      {
	if (!indices.arrayZ[i]) continue;
	deltas_x[i] *= scalar;
	deltas_y[i] *= scalar;
      }
    else
      for (unsigned i = 0; i < num; i++)
      {
	if (!indices.arrayZ[i]) continue;
	deltas_x[i] *= scalar;
      }
    return *this;
  }

  hb_vector_t<tuple_delta_t> change_tuple_var_axis_limit (hb_tag_t axis_tag, Triple axis_limit,
                                                          TripleDistances axis_triple_distances) const
  {
    hb_vector_t<tuple_delta_t> out;
    Triple *tent;
    if (!axis_tuples.has (axis_tag, &tent))
    {
      out.push (*this);
      return out;
    }

    if ((tent->minimum < 0.0 && tent->maximum > 0.0) ||
        !(tent->minimum <= tent->middle && tent->middle <= tent->maximum))
      return out;

    if (tent->middle == 0.0)
    {
      out.push (*this);
      return out;
    }

    rebase_tent_result_t solutions = rebase_tent (*tent, axis_limit, axis_triple_distances);
    for (auto &t : solutions)
    {
      tuple_delta_t new_var = *this;
      if (t.second == Triple ())
        new_var.remove_axis (axis_tag);
      else
        new_var.set_tent (axis_tag, t.second);

      new_var *= t.first;
      out.push (std::move (new_var));
    }

    return out;
  }

  bool compile_peak_coords (const hb_map_t& axes_index_map,
                            const hb_map_t& axes_old_index_tag_map)
  {
    unsigned axis_count = axes_index_map.get_population ();
    if (unlikely (!compiled_peak_coords.alloc (axis_count * F2DOT14::static_size)))
      return false;

    unsigned orig_axis_count = axes_old_index_tag_map.get_population ();
    for (unsigned i = 0; i < orig_axis_count; i++)
    {
      if (!axes_index_map.has (i))
        continue;

      hb_tag_t axis_tag = axes_old_index_tag_map.get (i);
      Triple *coords;
      F2DOT14 peak_coord;
      if (axis_tuples.has (axis_tag, &coords))
        peak_coord.set_float (coords->middle);
      else
        peak_coord.set_int (0);

      /* push F2DOT14 value into char vector */
      int16_t val = peak_coord.to_int ();
      compiled_peak_coords.push (static_cast<char> (val >> 8));
      compiled_peak_coords.push (static_cast<char> (val & 0xFF));
    }

    return !compiled_peak_coords.in_error ();
  }

  /* deltas should be compiled already before we compile tuple
   * variation header cause we need to fill in the size of the
   * serialized data for this tuple variation */
  bool compile_tuple_var_header (const hb_map_t& axes_index_map,
                                 unsigned points_data_length,
                                 const hb_map_t& axes_old_index_tag_map,
                                 const hb_hashmap_t<const hb_vector_t<char>*, unsigned>* shared_tuples_idx_map)
  {
    /* compiled_deltas could be empty after iup delta optimization, we can skip
     * compiling this tuple and return true */
    if (!compiled_deltas) return true;

    unsigned cur_axis_count = axes_index_map.get_population ();
    /* allocate enough memory: 1 peak + 2 intermediate coords + fixed header size */
    unsigned alloc_len = 3 * cur_axis_count * (F2DOT14::static_size) + 4;
    if (unlikely (!compiled_tuple_header.resize (alloc_len))) return false;

    unsigned flag = 0;
    /* skip the first 4 header bytes: variationDataSize+tupleIndex */
    F2DOT14* p = reinterpret_cast<F2DOT14 *> (compiled_tuple_header.begin () + 4);
    F2DOT14* end = reinterpret_cast<F2DOT14 *> (compiled_tuple_header.end ());
    hb_array_t<F2DOT14> coords (p, end - p);

    /* encode peak coords */
    unsigned peak_count = 0;
    unsigned *shared_tuple_idx;
    if (shared_tuples_idx_map &&
        shared_tuples_idx_map->has (&compiled_peak_coords, &shared_tuple_idx))
    {
      flag = *shared_tuple_idx;
    }
    else
    {
      peak_count = encode_peak_coords(coords, flag, axes_index_map, axes_old_index_tag_map);
      if (!peak_count) return false;
    }

    /* encode interim coords, it's optional so returned num could be 0 */
    unsigned interim_count = encode_interm_coords (coords.sub_array (peak_count), flag, axes_index_map, axes_old_index_tag_map);

    /* pointdata length = 0 implies "use shared points" */
    if (points_data_length)
      flag |= TupleVariationHeader::TuppleIndex::PrivatePointNumbers;

    unsigned serialized_data_size = points_data_length + compiled_deltas.length;
    TupleVariationHeader *o = reinterpret_cast<TupleVariationHeader *> (compiled_tuple_header.begin ());
    o->varDataSize = serialized_data_size;
    o->tupleIndex = flag;

    unsigned total_header_len = 4 + (peak_count + interim_count) * (F2DOT14::static_size);
    return compiled_tuple_header.resize (total_header_len);
  }

  unsigned encode_peak_coords (hb_array_t<F2DOT14> peak_coords,
                               unsigned& flag,
                               const hb_map_t& axes_index_map,
                               const hb_map_t& axes_old_index_tag_map) const
  {
    unsigned orig_axis_count = axes_old_index_tag_map.get_population ();
    auto it = peak_coords.iter ();
    unsigned count = 0;
    for (unsigned i = 0; i < orig_axis_count; i++)
    {
      if (!axes_index_map.has (i)) /* axis pinned */
        continue;
      hb_tag_t axis_tag = axes_old_index_tag_map.get (i);
      Triple *coords;
      if (!axis_tuples.has (axis_tag, &coords))
        (*it).set_int (0);
      else
        (*it).set_float (coords->middle);
      it++;
      count++;
    }
    flag |= TupleVariationHeader::TuppleIndex::EmbeddedPeakTuple;
    return count;
  }

  /* if no need to encode intermediate coords, then just return p */
  unsigned encode_interm_coords (hb_array_t<F2DOT14> coords,
                                 unsigned& flag,
                                 const hb_map_t& axes_index_map,
                                 const hb_map_t& axes_old_index_tag_map) const
  {
    unsigned orig_axis_count = axes_old_index_tag_map.get_population ();
    unsigned cur_axis_count = axes_index_map.get_population ();

    auto start_coords_iter = coords.sub_array (0, cur_axis_count).iter ();
    auto end_coords_iter = coords.sub_array (cur_axis_count).iter ();
    bool encode_needed = false;
    unsigned count = 0;
    for (unsigned i = 0; i < orig_axis_count; i++)
    {
      if (!axes_index_map.has (i)) /* axis pinned */
        continue;
      hb_tag_t axis_tag = axes_old_index_tag_map.get (i);
      Triple *coords;
      float min_val = 0.f, val = 0.f, max_val = 0.f;
      if (axis_tuples.has (axis_tag, &coords))
      {
        min_val = coords->minimum;
        val = coords->middle;
        max_val = coords->maximum;
      }

      (*start_coords_iter).set_float (min_val);
      (*end_coords_iter).set_float (max_val);

      start_coords_iter++;
      end_coords_iter++;
      count += 2;
      if (min_val != hb_min (val, 0.f) || max_val != hb_max (val, 0.f))
        encode_needed = true;
    }

    if (encode_needed)
    {
      flag |= TupleVariationHeader::TuppleIndex::IntermediateRegion;
      return count;
    }
    return 0;
  }

  bool compile_deltas ()
  { return compile_deltas (indices, deltas_x, deltas_y, compiled_deltas); }

  static bool compile_deltas (const hb_vector_t<bool> &point_indices,
			      const hb_vector_t<double> &x_deltas,
			      const hb_vector_t<double> &y_deltas,
			      hb_vector_t<unsigned char> &compiled_deltas /* OUT */)
  {
    hb_vector_t<int> rounded_deltas;
    if (unlikely (!rounded_deltas.alloc (point_indices.length)))
      return false;

    for (unsigned i = 0; i < point_indices.length; i++)
    {
      if (!point_indices[i]) continue;
      int rounded_delta = (int) roundf (x_deltas.arrayZ[i]);
      rounded_deltas.push (rounded_delta);
    }

    if (!rounded_deltas) return true;
    /* allocate enough memories 5 * num_deltas */
    unsigned alloc_len = 5 * rounded_deltas.length;
    if (y_deltas)
      alloc_len *= 2;

    if (unlikely (!compiled_deltas.resize (alloc_len))) return false;

    unsigned encoded_len = compile_deltas (compiled_deltas, rounded_deltas);

    if (y_deltas)
    {
      /* reuse the rounded_deltas vector, check that y_deltas have the same num of deltas as x_deltas */
      unsigned j = 0;
      for (unsigned idx = 0; idx < point_indices.length; idx++)
      {
        if (!point_indices[idx]) continue;
        int rounded_delta = (int) roundf (y_deltas.arrayZ[idx]);

        if (j >= rounded_deltas.length) return false;

        rounded_deltas[j++] = rounded_delta;
      }

      if (j != rounded_deltas.length) return false;
      encoded_len += compile_deltas (compiled_deltas.as_array ().sub_array (encoded_len), rounded_deltas);
    }
    return compiled_deltas.resize (encoded_len);
  }

  static unsigned compile_deltas (hb_array_t<unsigned char> encoded_bytes,
				  hb_array_t<const int> deltas)
  {
    return TupleValues::compile (deltas, encoded_bytes);
  }

  bool calc_inferred_deltas (const contour_point_vector_t& orig_points)
  {
    unsigned point_count = orig_points.length;
    if (point_count != indices.length)
      return false;

    unsigned ref_count = 0;
    hb_vector_t<unsigned> end_points;

    for (unsigned i = 0; i < point_count; i++)
    {
      if (indices.arrayZ[i])
        ref_count++;
      if (orig_points.arrayZ[i].is_end_point)
        end_points.push (i);
    }
    /* all points are referenced, nothing to do */
    if (ref_count == point_count)
      return true;
    if (unlikely (end_points.in_error ())) return false;

    hb_set_t inferred_idxes;
    unsigned start_point = 0;
    for (unsigned end_point : end_points)
    {
      /* Check the number of unreferenced points in a contour. If no unref points or no ref points, nothing to do. */
      unsigned unref_count = 0;
      for (unsigned i = start_point; i < end_point + 1; i++)
        unref_count += indices.arrayZ[i];
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
          if (indices.arrayZ[i] && !indices.arrayZ[j]) break;
        }
        prev = j = i;
        for (;;)
        {
          i = j;
          j = next_index (i, start_point, end_point);
          if (!indices.arrayZ[i] && indices.arrayZ[j]) break;
        }
        next = j;
       /* Infer deltas for all unref points in the gap between prev and next */
        i = prev;
        for (;;)
        {
          i = next_index (i, start_point, end_point);
          if (i == next) break;
          deltas_x.arrayZ[i] = infer_delta ((double) orig_points.arrayZ[i].x,
                                            (double) orig_points.arrayZ[prev].x,
                                            (double) orig_points.arrayZ[next].x,
                                            deltas_x.arrayZ[prev], deltas_x.arrayZ[next]);
          deltas_y.arrayZ[i] = infer_delta ((double) orig_points.arrayZ[i].y,
                                            (double) orig_points.arrayZ[prev].y,
                                            (double) orig_points.arrayZ[next].y,
                                            deltas_y.arrayZ[prev], deltas_y.arrayZ[next]);
          inferred_idxes.add (i);
          if (--unref_count == 0) goto no_more_gaps;
        }
      }
    no_more_gaps:
      start_point = end_point + 1;
    }

    for (unsigned i = 0; i < point_count; i++)
    {
      /* if points are not referenced and deltas are not inferred, set to 0.
       * reference all points for gvar */
      if ( !indices[i])
      {
        if (!inferred_idxes.has (i))
        {
          deltas_x.arrayZ[i] = 0.0;
          deltas_y.arrayZ[i] = 0.0;
        }
        indices[i] = true;
      }
    }
    return true;
  }

  bool optimize (const contour_point_vector_t& contour_points,
                 bool is_composite,
                 double tolerance = 0.5 + 1e-10)
  {
    unsigned count = contour_points.length;
    if (deltas_x.length != count ||
        deltas_y.length != count)
      return false;

    hb_vector_t<bool> opt_indices;
    hb_vector_t<int> rounded_x_deltas, rounded_y_deltas;

    if (unlikely (!rounded_x_deltas.alloc (count) ||
                  !rounded_y_deltas.alloc (count)))
      return false;

    for (unsigned i = 0; i < count; i++)
    {
      int rounded_x_delta = (int) roundf (deltas_x.arrayZ[i]);
      int rounded_y_delta = (int) roundf (deltas_y.arrayZ[i]);
      rounded_x_deltas.push (rounded_x_delta);
      rounded_y_deltas.push (rounded_y_delta);
    }

    if (!iup_delta_optimize (contour_points, rounded_x_deltas, rounded_y_deltas, opt_indices, tolerance))
      return false;

    unsigned ref_count = 0;
    for (bool ref_flag : opt_indices)
       ref_count += ref_flag;

    if (ref_count == count) return true;

    hb_vector_t<double> opt_deltas_x, opt_deltas_y;
    bool is_comp_glyph_wo_deltas = (is_composite && ref_count == 0);
    if (is_comp_glyph_wo_deltas)
    {
      if (unlikely (!opt_deltas_x.resize (count) ||
                    !opt_deltas_y.resize (count)))
        return false;

      opt_indices.arrayZ[0] = true;
      for (unsigned i = 1; i < count; i++)
        opt_indices.arrayZ[i] = false;
    }

    hb_vector_t<unsigned char> opt_point_data;
    if (!compile_point_set (opt_indices, opt_point_data))
      return false;
    hb_vector_t<unsigned char> opt_deltas_data;
    if (!compile_deltas (opt_indices,
                         is_comp_glyph_wo_deltas ? opt_deltas_x : deltas_x,
                         is_comp_glyph_wo_deltas ? opt_deltas_y : deltas_y,
                         opt_deltas_data))
      return false;

    hb_vector_t<unsigned char> point_data;
    if (!compile_point_set (indices, point_data))
      return false;
    hb_vector_t<unsigned char> deltas_data;
    if (!compile_deltas (indices, deltas_x, deltas_y, deltas_data))
      return false;

    if (opt_point_data.length + opt_deltas_data.length < point_data.length + deltas_data.length)
    {
      indices.fini ();
      indices = std::move (opt_indices);

      if (is_comp_glyph_wo_deltas)
      {
        deltas_x.fini ();
        deltas_x = std::move (opt_deltas_x);

        deltas_y.fini ();
        deltas_y = std::move (opt_deltas_y);
      }
    }
    return !indices.in_error () && !deltas_x.in_error () && !deltas_y.in_error ();
  }

  static bool compile_point_set (const hb_vector_t<bool> &point_indices,
                                 hb_vector_t<unsigned char>& compiled_points /* OUT */)
  {
    unsigned num_points = 0;
    for (bool i : point_indices)
      if (i) num_points++;

    /* when iup optimization is enabled, num of referenced points could be 0 */
    if (!num_points) return true;

    unsigned indices_length = point_indices.length;
    /* If the points set consists of all points in the glyph, it's encoded with a
     * single zero byte */
    if (num_points == indices_length)
      return compiled_points.resize (1);

    /* allocate enough memories: 2 bytes for count + 3 bytes for each point */
    unsigned num_bytes = 2 + 3 *num_points;
    if (unlikely (!compiled_points.resize (num_bytes, false)))
      return false;

    unsigned pos = 0;
    /* binary data starts with the total number of reference points */
    if (num_points < 0x80)
      compiled_points.arrayZ[pos++] = num_points;
    else
    {
      compiled_points.arrayZ[pos++] = ((num_points >> 8) | 0x80);
      compiled_points.arrayZ[pos++] = num_points & 0xFF;
    }

    const unsigned max_run_length = 0x7F;
    unsigned i = 0;
    unsigned last_value = 0;
    unsigned num_encoded = 0;
    while (i < indices_length && num_encoded < num_points)
    {
      unsigned run_length = 0;
      unsigned header_pos = pos;
      compiled_points.arrayZ[pos++] = 0;

      bool use_byte_encoding = false;
      bool new_run = true;
      while (i < indices_length && num_encoded < num_points &&
             run_length <= max_run_length)
      {
        // find out next referenced point index
        while (i < indices_length && !point_indices[i])
          i++;

        if (i >= indices_length) break;

        unsigned cur_value = i;
        unsigned delta = cur_value - last_value;

        if (new_run)
        {
          use_byte_encoding = (delta <= 0xFF);
          new_run = false;
        }

        if (use_byte_encoding && delta > 0xFF)
          break;

        if (use_byte_encoding)
          compiled_points.arrayZ[pos++] = delta;
        else
        {
          compiled_points.arrayZ[pos++] = delta >> 8;
          compiled_points.arrayZ[pos++] = delta & 0xFF;
        }
        i++;
        last_value = cur_value;
        run_length++;
        num_encoded++;
      }

      if (use_byte_encoding)
        compiled_points.arrayZ[header_pos] = run_length - 1;
      else
        compiled_points.arrayZ[header_pos] = (run_length - 1) | 0x80;
    }
    return compiled_points.resize (pos, false);
  }

  static double infer_delta (double target_val, double prev_val, double next_val, double prev_delta, double next_delta)
  {
    if (prev_val == next_val)
      return (prev_delta == next_delta) ? prev_delta : 0.0;
    else if (target_val <= hb_min (prev_val, next_val))
      return (prev_val < next_val) ? prev_delta : next_delta;
    else if (target_val >= hb_max (prev_val, next_val))
      return (prev_val > next_val) ? prev_delta : next_delta;

    double r = (target_val - prev_val) / (next_val - prev_val);
    return prev_delta + r * (next_delta - prev_delta);
  }

  static unsigned int next_index (unsigned int i, unsigned int start, unsigned int end)
  { return (i >= end) ? start : (i + 1); }
};

struct TupleVariationData
{
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    // here check on min_size only, TupleVariationHeader and var data will be
    // checked while accessing through iterator.
    return_trace (c->check_struct (this));
  }

  unsigned get_size (unsigned axis_count) const
  {
    unsigned total_size = min_size;
    unsigned count = tupleVarCount.get_count ();
    const TupleVariationHeader *tuple_var_header = &(get_tuple_var_header());
    for (unsigned i = 0; i < count; i++)
    {
      total_size += tuple_var_header->get_size (axis_count) + tuple_var_header->get_data_size ();
      tuple_var_header = &tuple_var_header->get_next (axis_count);
    }

    return total_size;
  }

  const TupleVariationHeader &get_tuple_var_header (void) const
  { return StructAfter<TupleVariationHeader> (data); }

  struct tuple_iterator_t;
  struct tuple_variations_t
  {
    hb_vector_t<tuple_delta_t> tuple_vars;

    private:
    /* referenced point set->compiled point data map */
    hb_hashmap_t<const hb_vector_t<bool>*, hb_vector_t<char>> point_data_map;
    /* referenced point set-> count map, used in finding shared points */
    hb_hashmap_t<const hb_vector_t<bool>*, unsigned> point_set_count_map;

    /* empty for non-gvar tuples.
     * shared_points_bytes is a pointer to some value in the point_data_map,
     * which will be freed during map destruction. Save it for serialization, so
     * no need to do find_shared_points () again */
    hb_vector_t<char> *shared_points_bytes = nullptr;

    /* total compiled byte size as TupleVariationData format, initialized to its
     * min_size: 4 */
    unsigned compiled_byte_size = 4;

    /* for gvar iup delta optimization: whether this is a composite glyph */
    bool is_composite = false;

    public:
    tuple_variations_t () = default;
    tuple_variations_t (const tuple_variations_t&) = delete;
    tuple_variations_t& operator=(const tuple_variations_t&) = delete;
    tuple_variations_t (tuple_variations_t&&) = default;
    tuple_variations_t& operator=(tuple_variations_t&&) = default;
    ~tuple_variations_t () = default;

    explicit operator bool () const { return bool (tuple_vars); }
    unsigned get_var_count () const
    {
      unsigned count = 0;
      /* when iup delta opt is enabled, compiled_deltas could be empty and we
       * should skip this tuple */
      for (auto& tuple: tuple_vars)
        if (tuple.compiled_deltas) count++;

      if (shared_points_bytes && shared_points_bytes->length)
        count |= TupleVarCount::SharedPointNumbers;
      return count;
    }

    unsigned get_compiled_byte_size () const
    { return compiled_byte_size; }

    bool create_from_tuple_var_data (tuple_iterator_t iterator,
                                     unsigned tuple_var_count,
                                     unsigned point_count,
                                     bool is_gvar,
                                     const hb_map_t *axes_old_index_tag_map,
                                     const hb_vector_t<unsigned> &shared_indices,
                                     const hb_array_t<const F2DOT14> shared_tuples,
                                     bool is_composite_glyph)
    {
      do
      {
        const HBUINT8 *p = iterator.get_serialized_data ();
        unsigned int length = iterator.current_tuple->get_data_size ();
        if (unlikely (!iterator.var_data_bytes.check_range (p, length)))
          return false;

        hb_hashmap_t<hb_tag_t, Triple> axis_tuples;
        if (!iterator.current_tuple->unpack_axis_tuples (iterator.get_axis_count (), shared_tuples, axes_old_index_tag_map, axis_tuples)
            || axis_tuples.is_empty ())
          return false;

        hb_vector_t<unsigned> private_indices;
        bool has_private_points = iterator.current_tuple->has_private_points ();
        const HBUINT8 *end = p + length;
        if (has_private_points &&
            !TupleVariationData::decompile_points (p, private_indices, end))
          return false;

        const hb_vector_t<unsigned> &indices = has_private_points ? private_indices : shared_indices;
        bool apply_to_all = (indices.length == 0);
        unsigned num_deltas = apply_to_all ? point_count : indices.length;

        hb_vector_t<int> deltas_x;

        if (unlikely (!deltas_x.resize (num_deltas, false) ||
                      !TupleVariationData::decompile_deltas (p, deltas_x, end)))
          return false;

        hb_vector_t<int> deltas_y;
        if (is_gvar)
        {
          if (unlikely (!deltas_y.resize (num_deltas, false) ||
                        !TupleVariationData::decompile_deltas (p, deltas_y, end)))
            return false;
        }

        tuple_delta_t var;
        var.axis_tuples = std::move (axis_tuples);
        if (unlikely (!var.indices.resize (point_count) ||
                      !var.deltas_x.resize (point_count, false)))
          return false;

        if (is_gvar && unlikely (!var.deltas_y.resize (point_count, false)))
          return false;

        for (unsigned i = 0; i < num_deltas; i++)
        {
          unsigned idx = apply_to_all ? i : indices[i];
          if (idx >= point_count) continue;
          var.indices[idx] = true;
          var.deltas_x[idx] = deltas_x[i];
          if (is_gvar)
            var.deltas_y[idx] = deltas_y[i];
        }
        tuple_vars.push (std::move (var));
      } while (iterator.move_to_next ());

      is_composite = is_composite_glyph;
      return true;
    }

    bool create_from_item_var_data (const VarData &var_data,
                                    const hb_vector_t<hb_hashmap_t<hb_tag_t, Triple>>& regions,
                                    const hb_map_t& axes_old_index_tag_map,
                                    unsigned& item_count,
                                    const hb_inc_bimap_t* inner_map = nullptr)
    {
      /* NULL offset, to keep original varidx valid, just return */
      if (&var_data == &Null (VarData))
        return true;

      unsigned num_regions = var_data.get_region_index_count ();
      if (!tuple_vars.alloc (num_regions)) return false;

      item_count = inner_map ? inner_map->get_population () : var_data.get_item_count ();
      if (!item_count) return true;
      unsigned row_size = var_data.get_row_size ();
      const HBUINT8 *delta_bytes = var_data.get_delta_bytes ();

      for (unsigned r = 0; r < num_regions; r++)
      {
        /* In VarData, deltas are organized in rows, convert them into
         * column(region) based tuples, resize deltas_x first */
        tuple_delta_t tuple;
        if (!tuple.deltas_x.resize (item_count, false) ||
            !tuple.indices.resize (item_count, false))
          return false;

        for (unsigned i = 0; i < item_count; i++)
        {
          tuple.indices.arrayZ[i] = true;
          tuple.deltas_x.arrayZ[i] = var_data.get_item_delta_fast (inner_map ? inner_map->backward (i) : i,
                                                                   r, delta_bytes, row_size);
        }

        unsigned region_index = var_data.get_region_index (r);
        if (region_index >= regions.length) return false;
        tuple.axis_tuples = regions.arrayZ[region_index];

        tuple_vars.push (std::move (tuple));
      }
      return !tuple_vars.in_error ();
    }

    private:
    static int _cmp_axis_tag (const void *pa, const void *pb)
    {
      const hb_tag_t *a = (const hb_tag_t*) pa;
      const hb_tag_t *b = (const hb_tag_t*) pb;
      return (int)(*a) - (int)(*b);
    }

    bool change_tuple_variations_axis_limits (const hb_hashmap_t<hb_tag_t, Triple>& normalized_axes_location,
                                              const hb_hashmap_t<hb_tag_t, TripleDistances>& axes_triple_distances)
    {
      /* sort axis_tag/axis_limits, make result deterministic */
      hb_vector_t<hb_tag_t> axis_tags;
      if (!axis_tags.alloc (normalized_axes_location.get_population ()))
        return false;
      for (auto t : normalized_axes_location.keys ())
        axis_tags.push (t);

      axis_tags.qsort (_cmp_axis_tag);
      for (auto axis_tag : axis_tags)
      {
        Triple *axis_limit;
        if (!normalized_axes_location.has (axis_tag, &axis_limit))
          return false;
        TripleDistances axis_triple_distances{1.0, 1.0};
        if (axes_triple_distances.has (axis_tag))
          axis_triple_distances = axes_triple_distances.get (axis_tag);

        hb_vector_t<tuple_delta_t> new_vars;
        for (const tuple_delta_t& var : tuple_vars)
        {
          hb_vector_t<tuple_delta_t> out = var.change_tuple_var_axis_limit (axis_tag, *axis_limit, axis_triple_distances);
          if (!out) continue;

          unsigned new_len = new_vars.length + out.length;

          if (unlikely (!new_vars.alloc (new_len, false)))
            return false;

          for (unsigned i = 0; i < out.length; i++)
            new_vars.push (std::move (out[i]));
        }
        tuple_vars.fini ();
        tuple_vars = std::move (new_vars);
      }
      return true;
    }

    /* merge tuple variations with overlapping tents, if iup delta optimization
     * is enabled, add default deltas to contour_points */
    bool merge_tuple_variations (contour_point_vector_t* contour_points = nullptr)
    {
      hb_vector_t<tuple_delta_t> new_vars;
      hb_hashmap_t<const hb_hashmap_t<hb_tag_t, Triple>*, unsigned> m;
      unsigned i = 0;
      for (const tuple_delta_t& var : tuple_vars)
      {
        /* if all axes are pinned, drop the tuple variation */
        if (var.axis_tuples.is_empty ())
        {
          /* if iup_delta_optimize is enabled, add deltas to contour coords */
          if (contour_points && !contour_points->add_deltas (var.deltas_x,
                                                             var.deltas_y,
                                                             var.indices))
            return false;
          continue;
        }

        unsigned *idx;
        if (m.has (&(var.axis_tuples), &idx))
        {
          new_vars[*idx] += var;
        }
        else
        {
          new_vars.push (var);
          if (!m.set (&(var.axis_tuples), i))
            return false;
          i++;
        }
      }
      tuple_vars.fini ();
      tuple_vars = std::move (new_vars);
      return true;
    }

    /* compile all point set and store byte data in a point_set->hb_bytes_t hashmap,
     * also update point_set->count map, which will be used in finding shared
     * point set*/
    bool compile_all_point_sets ()
    {
      for (const auto& tuple: tuple_vars)
      {
        const hb_vector_t<bool>* points_set = &(tuple.indices);
        if (point_data_map.has (points_set))
        {
          unsigned *count;
          if (unlikely (!point_set_count_map.has (points_set, &count) ||
                        !point_set_count_map.set (points_set, (*count) + 1)))
            return false;
          continue;
        }

        hb_vector_t<unsigned char> compiled_point_data;
        if (!tuple_delta_t::compile_point_set (*points_set, compiled_point_data))
          return false;

        if (!point_data_map.set (points_set, std::move (compiled_point_data)) ||
            !point_set_count_map.set (points_set, 1))
          return false;
      }
      return true;
    }

    /* find shared points set which saves most bytes */
    void find_shared_points ()
    {
      unsigned max_saved_bytes = 0;

      for (const auto& _ : point_data_map.iter_ref ())
      {
        const hb_vector_t<bool>* points_set = _.first;
        unsigned data_length = _.second.length;
        if (!data_length) continue;
        unsigned *count;
        if (unlikely (!point_set_count_map.has (points_set, &count) ||
                      *count <= 1))
        {
          shared_points_bytes = nullptr;
          return;
        }

        unsigned saved_bytes = data_length * ((*count) -1);
        if (saved_bytes > max_saved_bytes)
        {
          max_saved_bytes = saved_bytes;
          shared_points_bytes = &(_.second);
        }
      }
    }

    bool calc_inferred_deltas (const contour_point_vector_t& contour_points)
    {
      for (tuple_delta_t& var : tuple_vars)
        if (!var.calc_inferred_deltas (contour_points))
          return false;

      return true;
    }

    bool iup_optimize (const contour_point_vector_t& contour_points)
    {
      for (tuple_delta_t& var : tuple_vars)
      {
        if (!var.optimize (contour_points, is_composite))
          return false;
      }
      return true;
    }

    public:
    bool instantiate (const hb_hashmap_t<hb_tag_t, Triple>& normalized_axes_location,
                      const hb_hashmap_t<hb_tag_t, TripleDistances>& axes_triple_distances,
                      contour_point_vector_t* contour_points = nullptr,
                      bool optimize = false)
    {
      if (!tuple_vars) return true;
      if (!change_tuple_variations_axis_limits (normalized_axes_location, axes_triple_distances))
        return false;
      /* compute inferred deltas only for gvar */
      if (contour_points)
        if (!calc_inferred_deltas (*contour_points))
          return false;

      /* if iup delta opt is on, contour_points can't be null */
      if (optimize && !contour_points)
        return false;

      if (!merge_tuple_variations (optimize ? contour_points : nullptr))
        return false;

      if (optimize && !iup_optimize (*contour_points)) return false;
      return !tuple_vars.in_error ();
    }

    bool compile_bytes (const hb_map_t& axes_index_map,
                        const hb_map_t& axes_old_index_tag_map,
                        bool use_shared_points,
                        const hb_hashmap_t<const hb_vector_t<char>*, unsigned>* shared_tuples_idx_map = nullptr)
    {
      // compile points set and store data in hashmap
      if (!compile_all_point_sets ())
        return false;

      if (use_shared_points)
      {
        find_shared_points ();
        if (shared_points_bytes)
          compiled_byte_size += shared_points_bytes->length;
      }
      // compile delta and tuple var header for each tuple variation
      for (auto& tuple: tuple_vars)
      {
        const hb_vector_t<bool>* points_set = &(tuple.indices);
        hb_vector_t<char> *points_data;
        if (unlikely (!point_data_map.has (points_set, &points_data)))
          return false;

        /* when iup optimization is enabled, num of referenced points could be 0
         * and thus the compiled points bytes is empty, we should skip compiling
         * this tuple */
        if (!points_data->length)
          continue;
        if (!tuple.compile_deltas ())
          return false;

        unsigned points_data_length = (points_data != shared_points_bytes) ? points_data->length : 0;
        if (!tuple.compile_tuple_var_header (axes_index_map, points_data_length, axes_old_index_tag_map,
                                             shared_tuples_idx_map))
          return false;
        compiled_byte_size += tuple.compiled_tuple_header.length + points_data_length + tuple.compiled_deltas.length;
      }
      return true;
    }

    bool serialize_var_headers (hb_serialize_context_t *c, unsigned& total_header_len) const
    {
      TRACE_SERIALIZE (this);
      for (const auto& tuple: tuple_vars)
      {
        tuple.compiled_tuple_header.as_array ().copy (c);
        if (c->in_error ()) return_trace (false);
        total_header_len += tuple.compiled_tuple_header.length;
      }
      return_trace (true);
    }

    bool serialize_var_data (hb_serialize_context_t *c, bool is_gvar) const
    {
      TRACE_SERIALIZE (this);
      if (is_gvar && shared_points_bytes)
      {
        hb_bytes_t s (shared_points_bytes->arrayZ, shared_points_bytes->length);
        s.copy (c);
      }

      for (const auto& tuple: tuple_vars)
      {
        const hb_vector_t<bool>* points_set = &(tuple.indices);
        hb_vector_t<char> *point_data;
        if (!point_data_map.has (points_set, &point_data))
          return_trace (false);

        if (!is_gvar || point_data != shared_points_bytes)
        {
          hb_bytes_t s (point_data->arrayZ, point_data->length);
          s.copy (c);
        }

        tuple.compiled_deltas.as_array ().copy (c);
        if (c->in_error ()) return_trace (false);
      }

      /* padding for gvar */
      if (is_gvar && (compiled_byte_size % 2))
      {
        HBUINT8 pad;
        pad = 0;
        if (!c->embed (pad)) return_trace (false);
      }
      return_trace (true);
    }
  };

  struct tuple_iterator_t
  {
    unsigned get_axis_count () const { return axis_count; }

    void init (hb_bytes_t var_data_bytes_, unsigned int axis_count_, const void *table_base_)
    {
      var_data_bytes = var_data_bytes_;
      var_data = var_data_bytes_.as<TupleVariationData> ();
      index = 0;
      axis_count = axis_count_;
      current_tuple = &var_data->get_tuple_var_header ();
      data_offset = 0;
      table_base = table_base_;
    }

    bool get_shared_indices (hb_vector_t<unsigned int> &shared_indices /* OUT */)
    {
      if (var_data->has_shared_point_numbers ())
      {
        const HBUINT8 *base = &(table_base+var_data->data);
        const HBUINT8 *p = base;
        if (!decompile_points (p, shared_indices, (const HBUINT8 *) (var_data_bytes.arrayZ + var_data_bytes.length))) return false;
        data_offset = p - base;
      }
      return true;
    }

    bool is_valid () const
    {
      return (index < var_data->tupleVarCount.get_count ()) &&
             var_data_bytes.check_range (current_tuple, TupleVariationHeader::min_size) &&
             var_data_bytes.check_range (current_tuple, hb_max (current_tuple->get_data_size (),
                                                                current_tuple->get_size (axis_count)));
    }

    bool move_to_next ()
    {
      data_offset += current_tuple->get_data_size ();
      current_tuple = &current_tuple->get_next (axis_count);
      index++;
      return is_valid ();
    }

    const HBUINT8 *get_serialized_data () const
    { return &(table_base+var_data->data) + data_offset; }

    private:
    const TupleVariationData *var_data;
    unsigned int index;
    unsigned int axis_count;
    unsigned int data_offset;
    const void *table_base;

    public:
    hb_bytes_t var_data_bytes;
    const TupleVariationHeader *current_tuple;
  };

  static bool get_tuple_iterator (hb_bytes_t var_data_bytes, unsigned axis_count,
                                  const void *table_base,
                                  hb_vector_t<unsigned int> &shared_indices /* OUT */,
                                  tuple_iterator_t *iterator /* OUT */)
  {
    iterator->init (var_data_bytes, axis_count, table_base);
    if (!iterator->get_shared_indices (shared_indices))
      return false;
    return iterator->is_valid ();
  }

  bool has_shared_point_numbers () const { return tupleVarCount.has_shared_point_numbers (); }

  static bool decompile_points (const HBUINT8 *&p /* IN/OUT */,
				hb_vector_t<unsigned int> &points /* OUT */,
				const HBUINT8 *end)
  {
    enum packed_point_flag_t
    {
      POINTS_ARE_WORDS     = 0x80,
      POINT_RUN_COUNT_MASK = 0x7F
    };

    if (unlikely (p + 1 > end)) return false;

    unsigned count = *p++;
    if (count & POINTS_ARE_WORDS)
    {
      if (unlikely (p + 1 > end)) return false;
      count = ((count & POINT_RUN_COUNT_MASK) << 8) | *p++;
    }
    if (unlikely (!points.resize (count, false))) return false;

    unsigned n = 0;
    unsigned i = 0;
    while (i < count)
    {
      if (unlikely (p + 1 > end)) return false;
      unsigned control = *p++;
      unsigned run_count = (control & POINT_RUN_COUNT_MASK) + 1;
      unsigned stop = i + run_count;
      if (unlikely (stop > count)) return false;
      if (control & POINTS_ARE_WORDS)
      {
        if (unlikely (p + run_count * HBUINT16::static_size > end)) return false;
        for (; i < stop; i++)
        {
          n += *(const HBUINT16 *)p;
          points.arrayZ[i] = n;
          p += HBUINT16::static_size;
        }
      }
      else
      {
        if (unlikely (p + run_count > end)) return false;
        for (; i < stop; i++)
        {
          n += *p++;
          points.arrayZ[i] = n;
        }
      }
    }
    return true;
  }

  template <typename T>
  static bool decompile_deltas (const HBUINT8 *&p /* IN/OUT */,
				hb_vector_t<T> &deltas /* IN/OUT */,
				const HBUINT8 *end,
				bool consume_all = false)
  {
    return TupleValues::decompile (p, deltas, end, consume_all);
  }

  bool has_data () const { return tupleVarCount; }

  bool decompile_tuple_variations (unsigned point_count,
                                   bool is_gvar,
                                   tuple_iterator_t iterator,
                                   const hb_map_t *axes_old_index_tag_map,
                                   const hb_vector_t<unsigned> &shared_indices,
                                   const hb_array_t<const F2DOT14> shared_tuples,
                                   tuple_variations_t& tuple_variations, /* OUT */
                                   bool is_composite_glyph = false) const
  {
    return tuple_variations.create_from_tuple_var_data (iterator, tupleVarCount,
                                                        point_count, is_gvar,
                                                        axes_old_index_tag_map,
                                                        shared_indices,
                                                        shared_tuples,
                                                        is_composite_glyph);
  }

  bool serialize (hb_serialize_context_t *c,
                  bool is_gvar,
                  const tuple_variations_t& tuple_variations) const
  {
    TRACE_SERIALIZE (this);
    /* empty tuple variations, just return and skip serialization. */
    if (!tuple_variations) return_trace (true);

    auto *out = c->start_embed (this);
    if (unlikely (!c->extend_min (out))) return_trace (false);

    if (!c->check_assign (out->tupleVarCount, tuple_variations.get_var_count (),
                          HB_SERIALIZE_ERROR_INT_OVERFLOW)) return_trace (false);

    unsigned total_header_len = 0;

    if (!tuple_variations.serialize_var_headers (c, total_header_len))
      return_trace (false);

    unsigned data_offset = min_size + total_header_len;
    if (!is_gvar) data_offset += 4;
    if (!c->check_assign (out->data, data_offset, HB_SERIALIZE_ERROR_INT_OVERFLOW)) return_trace (false);

    return tuple_variations.serialize_var_data (c, is_gvar);
  }

  protected:
  struct TupleVarCount : HBUINT16
  {
    friend struct tuple_variations_t;
    bool has_shared_point_numbers () const { return ((*this) & SharedPointNumbers); }
    unsigned int get_count () const { return (*this) & CountMask; }
    TupleVarCount& operator = (uint16_t i) { HBUINT16::operator= (i); return *this; }
    explicit operator bool () const { return get_count (); }

    protected:
    enum Flags
    {
      SharedPointNumbers= 0x8000u,
      CountMask         = 0x0FFFu
    };
    public:
    DEFINE_SIZE_STATIC (2);
  };

  TupleVarCount tupleVarCount;  /* A packed field. The high 4 bits are flags, and the
                                 * low 12 bits are the number of tuple variation tables
                                 * for this glyph. The number of tuple variation tables
                                 * can be any number between 1 and 4095. */
  Offset16To<HBUINT8>
                data;           /* Offset from the start of the base table
                                 * to the serialized data. */
  /* TupleVariationHeader tupleVariationHeaders[] *//* Array of tuple variation headers. */
  public:
  DEFINE_SIZE_MIN (4);
};

using tuple_variations_t = TupleVariationData::tuple_variations_t;
struct item_variations_t
{
  using region_t = const hb_hashmap_t<hb_tag_t, Triple>*;
  private:
  /* each subtable is decompiled into a tuple_variations_t, in which all tuples
   * have the same num of deltas (rows) */
  hb_vector_t<tuple_variations_t> vars;

  /* num of retained rows for each subtable, there're 2 cases when var_data is empty:
   * 1. retained item_count is zero
   * 2. regions is empty and item_count is non-zero.
   * when converting to tuples, both will be dropped because the tuple is empty,
   * however, we need to retain 2. as all-zero rows to keep original varidx
   * valid, so we need a way to remember the num of rows for each subtable */
  hb_vector_t<unsigned> var_data_num_rows;

  /* original region list, decompiled from item varstore, used when rebuilding
   * region list after instantiation */
  hb_vector_t<hb_hashmap_t<hb_tag_t, Triple>> orig_region_list;

  /* region list: vector of Regions, maintain the original order for the regions
   * that existed before instantiate (), append the new regions at the end.
   * Regions are stored in each tuple already, save pointers only.
   * When converting back to item varstore, unused regions will be pruned */
  hb_vector_t<region_t> region_list;

  /* region -> idx map after instantiation and pruning unused regions */
  hb_hashmap_t<region_t, unsigned> region_map;

  /* all delta rows after instantiation */
  hb_vector_t<hb_vector_t<int>> delta_rows;
  /* final optimized vector of encoding objects used to assemble the varstore */
  hb_vector_t<delta_row_encoding_t> encodings;

  /* old varidxes -> new var_idxes map */
  hb_map_t varidx_map;

  /* has long words */
  bool has_long = false;

  public:
  bool has_long_word () const
  { return has_long; }

  const hb_vector_t<region_t>& get_region_list () const
  { return region_list; }

  const hb_vector_t<delta_row_encoding_t>& get_vardata_encodings () const
  { return encodings; }

  const hb_map_t& get_varidx_map () const
  { return varidx_map; }

  bool instantiate (const ItemVariationStore& varStore,
                    const hb_subset_plan_t *plan,
                    bool optimize=true,
                    bool use_no_variation_idx=true,
                    const hb_array_t <const hb_inc_bimap_t> inner_maps = hb_array_t<const hb_inc_bimap_t> ())
  {
    if (!create_from_item_varstore (varStore, plan->axes_old_index_tag_map, inner_maps))
      return false;
    if (!instantiate_tuple_vars (plan->axes_location, plan->axes_triple_distances))
      return false;
    return as_item_varstore (optimize, use_no_variation_idx);
  }

  /* keep below APIs public only for unit test: test-item-varstore */
  bool create_from_item_varstore (const ItemVariationStore& varStore,
                                  const hb_map_t& axes_old_index_tag_map,
                                  const hb_array_t <const hb_inc_bimap_t> inner_maps = hb_array_t<const hb_inc_bimap_t> ())
  {
    const VarRegionList& regionList = varStore.get_region_list ();
    if (!regionList.get_var_regions (axes_old_index_tag_map, orig_region_list))
      return false;

    unsigned num_var_data = varStore.get_sub_table_count ();
    if (inner_maps && inner_maps.length != num_var_data) return false;
    if (!vars.alloc (num_var_data) ||
        !var_data_num_rows.alloc (num_var_data)) return false;

    for (unsigned i = 0; i < num_var_data; i++)
    {
      if (inner_maps && !inner_maps.arrayZ[i].get_population ())
          continue;
      tuple_variations_t var_data_tuples;
      unsigned item_count = 0;
      if (!var_data_tuples.create_from_item_var_data (varStore.get_sub_table (i),
                                                      orig_region_list,
                                                      axes_old_index_tag_map,
                                                      item_count,
                                                      inner_maps ? &(inner_maps.arrayZ[i]) : nullptr))
        return false;

      var_data_num_rows.push (item_count);
      vars.push (std::move (var_data_tuples));
    }
    return !vars.in_error () && !var_data_num_rows.in_error () && vars.length == var_data_num_rows.length;
  }

  bool instantiate_tuple_vars (const hb_hashmap_t<hb_tag_t, Triple>& normalized_axes_location,
                               const hb_hashmap_t<hb_tag_t, TripleDistances>& axes_triple_distances)
  {
    for (tuple_variations_t& tuple_vars : vars)
      if (!tuple_vars.instantiate (normalized_axes_location, axes_triple_distances))
        return false;

    if (!build_region_list ()) return false;
    return true;
  }

  bool build_region_list ()
  {
    /* scan all tuples and collect all unique regions, prune unused regions */
    hb_hashmap_t<region_t, unsigned> all_regions;
    hb_hashmap_t<region_t, unsigned> used_regions;

    /* use a vector when inserting new regions, make result deterministic */
    hb_vector_t<region_t> all_unique_regions;
    for (const tuple_variations_t& sub_table : vars)
    {
      for (const tuple_delta_t& tuple : sub_table.tuple_vars)
      {
        region_t r = &(tuple.axis_tuples);
        if (!used_regions.has (r))
        {
          bool all_zeros = true;
          for (float d : tuple.deltas_x)
          {
            int delta = (int) roundf (d);
            if (delta != 0)
            {
              all_zeros = false;
              break;
            }
          }
          if (!all_zeros)
          {
            if (!used_regions.set (r, 1))
              return false;
          }
        }
        if (all_regions.has (r))
          continue;
        if (!all_regions.set (r, 1))
          return false;
        all_unique_regions.push (r);
      }
    }

    /* regions are empty means no variation data, return true */
    if (!all_regions || !all_unique_regions) return true;

    if (!region_list.alloc (all_regions.get_population ()))
      return false;

    unsigned idx = 0;
    /* append the original regions that pre-existed */
    for (const auto& r : orig_region_list)
    {
      if (!all_regions.has (&r) || !used_regions.has (&r))
        continue;

      region_list.push (&r);
      if (!region_map.set (&r, idx))
        return false;
      all_regions.del (&r);
      idx++;
    }

    /* append the new regions at the end */
    for (const auto& r: all_unique_regions)
    {
      if (!all_regions.has (r) || !used_regions.has (r))
        continue;
      region_list.push (r);
      if (!region_map.set (r, idx))
        return false;
      all_regions.del (r);
      idx++;
    }
    return (!region_list.in_error ()) && (!region_map.in_error ());
  }

  /* main algorithm ported from fonttools VarStore_optimize() method, optimize
   * varstore by default */

  struct combined_gain_idx_tuple_t
  {
    int gain;
    unsigned idx_1;
    unsigned idx_2;

    combined_gain_idx_tuple_t () = default;
    combined_gain_idx_tuple_t (int gain_, unsigned i, unsigned j)
        :gain (gain_), idx_1 (i), idx_2 (j) {}

    bool operator < (const combined_gain_idx_tuple_t& o)
    {
      if (gain != o.gain)
        return gain < o.gain;

      if (idx_1 != o.idx_1)
        return idx_1 < o.idx_1;

      return idx_2 < o.idx_2;
    }

    bool operator <= (const combined_gain_idx_tuple_t& o)
    {
      if (*this < o) return true;
      return gain == o.gain && idx_1 == o.idx_1 && idx_2 == o.idx_2;
    }
  };

  bool as_item_varstore (bool optimize=true, bool use_no_variation_idx=true)
  {
    /* return true if no variation data */
    if (!region_list) return true;
    unsigned num_cols = region_list.length;
    /* pre-alloc a 2D vector for all sub_table's VarData rows */
    unsigned total_rows = 0;
    for (unsigned major = 0; major < var_data_num_rows.length; major++)
      total_rows += var_data_num_rows[major];

    if (!delta_rows.resize (total_rows)) return false;
    /* init all rows to [0]*num_cols */
    for (unsigned i = 0; i < total_rows; i++)
      if (!(delta_rows[i].resize (num_cols))) return false;

    /* old VarIdxes -> full encoding_row mapping */
    hb_hashmap_t<unsigned, const hb_vector_t<int>*> front_mapping;
    unsigned start_row = 0;
    hb_vector_t<delta_row_encoding_t> encoding_objs;
    hb_hashmap_t<hb_vector_t<uint8_t>, unsigned> chars_idx_map;

    /* delta_rows map, used for filtering out duplicate rows */
    hb_hashmap_t<const hb_vector_t<int>*, unsigned> delta_rows_map;
    for (unsigned major = 0; major < vars.length; major++)
    {
      /* deltas are stored in tuples(column based), convert them back into items
       * (row based) delta */
      const tuple_variations_t& tuples = vars[major];
      unsigned num_rows = var_data_num_rows[major];
      for (const tuple_delta_t& tuple: tuples.tuple_vars)
      {
        if (tuple.deltas_x.length != num_rows)
          return false;

        /* skip unused regions */
        unsigned *col_idx;
        if (!region_map.has (&(tuple.axis_tuples), &col_idx))
          continue;

        for (unsigned i = 0; i < num_rows; i++)
        {
          int rounded_delta = roundf (tuple.deltas_x[i]);
          delta_rows[start_row + i][*col_idx] += rounded_delta;
          if ((!has_long) && (rounded_delta < -65536 || rounded_delta > 65535))
            has_long = true;
        }
      }

      if (!optimize)
      {
        /* assemble a delta_row_encoding_t for this subtable, skip optimization so
         * chars is not initialized, we only need delta rows for serialization */
        delta_row_encoding_t obj;
        for (unsigned r = start_row; r < start_row + num_rows; r++)
          obj.add_row (&(delta_rows.arrayZ[r]));

        encodings.push (std::move (obj));
        start_row += num_rows;
        continue;
      }

      for (unsigned minor = 0; minor < num_rows; minor++)
      {
        const hb_vector_t<int>& row = delta_rows[start_row + minor];
        if (use_no_variation_idx)
        {
          bool all_zeros = true;
          for (int delta : row)
          {
            if (delta != 0)
            {
              all_zeros = false;
              break;
            }
          }
          if (all_zeros)
            continue;
        }

        if (!front_mapping.set ((major<<16) + minor, &row))
          return false;

        hb_vector_t<uint8_t> chars = delta_row_encoding_t::get_row_chars (row);
        if (!chars) return false;

        if (delta_rows_map.has (&row))
          continue;

        delta_rows_map.set (&row, 1);
        unsigned *obj_idx;
        if (chars_idx_map.has (chars, &obj_idx))
        {
          delta_row_encoding_t& obj = encoding_objs[*obj_idx];
          if (!obj.add_row (&row))
            return false;
        }
        else
        {
          if (!chars_idx_map.set (chars, encoding_objs.length))
            return false;
          delta_row_encoding_t obj (std::move (chars), &row);
          encoding_objs.push (std::move (obj));
        }
      }

      start_row += num_rows;
    }

    /* return directly if no optimization, maintain original VariationIndex so
     * varidx_map would be empty */
    if (!optimize) return !encodings.in_error ();

    /* sort encoding_objs */
    encoding_objs.qsort ();

    /* main algorithm: repeatedly pick 2 best encodings to combine, and combine
     * them */
    hb_priority_queue_t<combined_gain_idx_tuple_t> queue;
    unsigned num_todos = encoding_objs.length;
    for (unsigned i = 0; i < num_todos; i++)
    {
      for (unsigned j = i + 1; j < num_todos; j++)
      {
        int combining_gain = encoding_objs.arrayZ[i].gain_from_merging (encoding_objs.arrayZ[j]);
        if (combining_gain > 0)
          queue.insert (combined_gain_idx_tuple_t (-combining_gain, i, j), 0);
      }
    }

    hb_set_t removed_todo_idxes;
    while (queue)
    {
      auto t = queue.pop_minimum ().first;
      unsigned i = t.idx_1;
      unsigned j = t.idx_2;

      if (removed_todo_idxes.has (i) || removed_todo_idxes.has (j))
        continue;

      delta_row_encoding_t& encoding = encoding_objs.arrayZ[i];
      delta_row_encoding_t& other_encoding = encoding_objs.arrayZ[j];

      removed_todo_idxes.add (i);
      removed_todo_idxes.add (j);

      hb_vector_t<uint8_t> combined_chars;
      if (!combined_chars.alloc (encoding.chars.length))
        return false;

      for (unsigned idx = 0; idx < encoding.chars.length; idx++)
      {
        uint8_t v = hb_max (encoding.chars.arrayZ[idx], other_encoding.chars.arrayZ[idx]);
        combined_chars.push (v);
      }

      delta_row_encoding_t combined_encoding_obj (std::move (combined_chars));
      for (const auto& row : hb_concat (encoding.items, other_encoding.items))
        combined_encoding_obj.add_row (row);

      for (unsigned idx = 0; idx < encoding_objs.length; idx++)
      {
        if (removed_todo_idxes.has (idx)) continue;

        const delta_row_encoding_t& obj = encoding_objs.arrayZ[idx];
        if (obj.chars == combined_chars)
        {
          for (const auto& row : obj.items)
            combined_encoding_obj.add_row (row);

          removed_todo_idxes.add (idx);
          continue;
        }

        int combined_gain = combined_encoding_obj.gain_from_merging (obj);
        if (combined_gain > 0)
          queue.insert (combined_gain_idx_tuple_t (-combined_gain, idx, encoding_objs.length), 0);
      }

      encoding_objs.push (std::move (combined_encoding_obj));
    }

    int num_final_encodings = (int) encoding_objs.length - (int) removed_todo_idxes.get_population ();
    if (num_final_encodings <= 0) return false;

    if (!encodings.alloc (num_final_encodings)) return false;
    for (unsigned i = 0; i < encoding_objs.length; i++)
    {
      if (removed_todo_idxes.has (i)) continue;
      encodings.push (std::move (encoding_objs.arrayZ[i]));
    }

    /* sort again based on width, make result deterministic */
    encodings.qsort (delta_row_encoding_t::cmp_width);

    return compile_varidx_map (front_mapping);
  }

  private:
  /* compile varidx_map for one VarData subtable (index specified by major) */
  bool compile_varidx_map (const hb_hashmap_t<unsigned, const hb_vector_t<int>*>& front_mapping)
  {
    /* full encoding_row -> new VarIdxes mapping */
    hb_hashmap_t<const hb_vector_t<int>*, unsigned> back_mapping;

    for (unsigned major = 0; major < encodings.length; major++)
    {
      delta_row_encoding_t& encoding = encodings[major];
      /* just sanity check, this shouldn't happen */
      if (encoding.is_empty ())
        return false;

      unsigned num_rows = encoding.items.length;

      /* sort rows, make result deterministic */
      encoding.items.qsort (_cmp_row);

      /* compile old to new var_idxes mapping */
      for (unsigned minor = 0; minor < num_rows; minor++)
      {
        unsigned new_varidx = (major << 16) + minor;
        back_mapping.set (encoding.items.arrayZ[minor], new_varidx);
      }
    }

    for (auto _ : front_mapping.iter ())
    {
      unsigned old_varidx = _.first;
      unsigned *new_varidx;
      if (back_mapping.has (_.second, &new_varidx))
        varidx_map.set (old_varidx, *new_varidx);
      else
        varidx_map.set (old_varidx, HB_OT_LAYOUT_NO_VARIATIONS_INDEX);
    }
    return !varidx_map.in_error ();
  }

  static int _cmp_row (const void *pa, const void *pb)
  {
    /* compare pointers of vectors(const hb_vector_t<int>*) that represent a row */
    const hb_vector_t<int>** a = (const hb_vector_t<int>**) pa;
    const hb_vector_t<int>** b = (const hb_vector_t<int>**) pb;

    for (unsigned i = 0; i < (*b)->length; i++)
    {
      int va = (*a)->arrayZ[i];
      int vb = (*b)->arrayZ[i];
      if (va != vb)
        return va < vb ? -1 : 1;
    }
    return 0;
  }
};


} /* namespace OT */


#endif /* HB_OT_VAR_COMMON_HH */
