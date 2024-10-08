/*
 * Copyright © 2018  Google, Inc.
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
 * Google Author(s): Garret Rieger, Roderick Sheeter
 */

#include "hb-subset-plan.hh"
#include "hb-subset-accelerator.hh"
#include "hb-map.hh"
#include "hb-multimap.hh"
#include "hb-set.hh"

#include "hb-ot-cmap-table.hh"
#include "hb-ot-glyf-table.hh"
#include "hb-ot-layout-base-table.hh"
#include "hb-ot-layout-gdef-table.hh"
#include "hb-ot-layout-gpos-table.hh"
#include "hb-ot-layout-gsub-table.hh"
#include "hb-ot-cff1-table.hh"
#include "hb-ot-cff2-table.hh"
#include "OT/Color/COLR/COLR.hh"
#include "OT/Color/COLR/colrv1-closure.hh"
#include "OT/Color/CPAL/CPAL.hh"
#include "hb-ot-var-fvar-table.hh"
#include "hb-ot-var-avar-table.hh"
#include "hb-ot-stat-table.hh"
#include "hb-ot-math-table.hh"

using OT::Layout::GSUB;
using OT::Layout::GPOS;


hb_subset_accelerator_t::~hb_subset_accelerator_t ()
{
  if (cmap_cache && destroy_cmap_cache)
    destroy_cmap_cache ((void*) cmap_cache);

#ifndef HB_NO_SUBSET_CFF
  cff1_accel.fini ();
  cff2_accel.fini ();
#endif
  hb_face_destroy (source);
}


typedef hb_hashmap_t<unsigned, hb::unique_ptr<hb_set_t>> script_langsys_map;
#ifndef HB_NO_SUBSET_CFF
static inline bool
_add_cff_seac_components (const OT::cff1::accelerator_subset_t &cff,
			  hb_codepoint_t gid,
			  hb_set_t *gids_to_retain)
{
  hb_codepoint_t base_gid, accent_gid;
  if (cff.get_seac_components (gid, &base_gid, &accent_gid))
  {
    gids_to_retain->add (base_gid);
    gids_to_retain->add (accent_gid);
    return true;
  }
  return false;
}
#endif

static void
_remap_palette_indexes (const hb_set_t *palette_indexes,
			hb_map_t       *mapping /* OUT */)
{
  unsigned new_idx = 0;
  for (unsigned palette_index : palette_indexes->iter ())
  {
    if (palette_index == 0xFFFF)
    {
      mapping->set (palette_index, palette_index);
      continue;
    }
    mapping->set (palette_index, new_idx);
    new_idx++;
  }
}

static void
_remap_indexes (const hb_set_t *indexes,
		hb_map_t       *mapping /* OUT */)
{
  for (auto _ : + hb_enumerate (indexes->iter ()))
    mapping->set (_.second, _.first);

}

#ifndef HB_NO_SUBSET_LAYOUT

/*
 * Removes all tags from 'tags' that are not in filter. Additionally eliminates any duplicates.
 * Returns true if anything was removed (not including duplicates).
 */
static bool _filter_tag_list(hb_vector_t<hb_tag_t>* tags, /* IN/OUT */
                             const hb_set_t* filter)
{
  hb_vector_t<hb_tag_t> out;
  out.alloc (tags->get_size() + 1); // +1 is to allocate room for the null terminator.

  bool removed = false;
  hb_set_t visited;

  for (hb_tag_t tag : *tags)
  {
    if (!tag) continue;
    if (visited.has (tag)) continue;

    if (!filter->has (tag))
    {
      removed = true;
      continue;
    }

    visited.add (tag);
    out.push (tag);
  }

  // The collect function needs a null element to signal end of the array.
  out.push (HB_TAG_NONE);

  hb_swap (out, *tags);
  return removed;
}

template <typename T>
static void _collect_layout_indices (hb_subset_plan_t     *plan,
                                     const T&              table,
                                     hb_set_t		  *lookup_indices, /* OUT */
                                     hb_set_t		  *feature_indices, /* OUT */
                                     hb_hashmap_t<unsigned, hb::shared_ptr<hb_set_t>> *feature_record_cond_idx_map, /* OUT */
                                     hb_hashmap_t<unsigned, const OT::Feature*> *feature_substitutes_map, /* OUT */
                                     hb_set_t& catch_all_record_feature_idxes, /* OUT */
                                     hb_hashmap_t<unsigned, hb_pair_t<const void*, const void*>>& catch_all_record_idx_feature_map /* OUT */)
{
  unsigned num_features = table.get_feature_count ();
  hb_vector_t<hb_tag_t> features;
  if (!plan->check_success (features.resize (num_features))) return;
  table.get_feature_tags (0, &num_features, features.arrayZ);
  bool retain_all_features = !_filter_tag_list (&features, &plan->layout_features);

  unsigned num_scripts = table.get_script_count ();
  hb_vector_t<hb_tag_t> scripts;
  if (!plan->check_success (scripts.resize (num_scripts))) return;
  table.get_script_tags (0, &num_scripts, scripts.arrayZ);
  bool retain_all_scripts = !_filter_tag_list (&scripts, &plan->layout_scripts);

  if (!plan->check_success (!features.in_error ()) || !features
      || !plan->check_success (!scripts.in_error ()) || !scripts)
    return;

  hb_ot_layout_collect_features (plan->source,
                                 T::tableTag,
                                 retain_all_scripts ? nullptr : scripts.arrayZ,
                                 nullptr,
                                 retain_all_features ? nullptr : features.arrayZ,
                                 feature_indices);

#ifndef HB_NO_VAR
  // collect feature substitutes with variations
  if (!plan->user_axes_location.is_empty ())
  {
    hb_hashmap_t<hb::shared_ptr<hb_map_t>, unsigned> conditionset_map;
    OT::hb_collect_feature_substitutes_with_var_context_t c =
    {
      &plan->axes_old_index_tag_map,
      &plan->axes_location,
      feature_record_cond_idx_map,
      feature_substitutes_map,
      catch_all_record_feature_idxes,
      feature_indices,
      false,
      false,
      false,
      0,
      &conditionset_map
    };
    table.collect_feature_substitutes_with_variations (&c);
  }
#endif

  for (unsigned feature_index : *feature_indices)
  {
    const OT::Feature* f = &(table.get_feature (feature_index));
    const OT::Feature **p = nullptr;
    if (feature_substitutes_map->has (feature_index, &p))
      f = *p;

    f->add_lookup_indexes_to (lookup_indices);
  }

#ifndef HB_NO_VAR
  if (catch_all_record_feature_idxes)
  {
    for (unsigned feature_index : catch_all_record_feature_idxes)
    {
      const OT::Feature& f = table.get_feature (feature_index);
      f.add_lookup_indexes_to (lookup_indices);
      const void *tag = reinterpret_cast<const void*> (&(table.get_feature_list ().get_tag (feature_index)));
      catch_all_record_idx_feature_map.set (feature_index, hb_pair (&f, tag));
    }
  }

  // If all axes are pinned then all feature variations will be dropped so there's no need
  // to collect lookups from them.
  if (!plan->all_axes_pinned)
    table.feature_variation_collect_lookups (feature_indices,
                                             plan->user_axes_location.is_empty () ? nullptr: feature_record_cond_idx_map,
                                             lookup_indices);
#endif
}


static inline void
_GSUBGPOS_find_duplicate_features (const OT::GSUBGPOS &g,
				   const hb_map_t *lookup_indices,
				   const hb_set_t *feature_indices,
				   const hb_hashmap_t<unsigned, const OT::Feature*> *feature_substitutes_map,
				   hb_map_t *duplicate_feature_map /* OUT */)
{
  if (feature_indices->is_empty ()) return;
  hb_hashmap_t<hb_tag_t, hb::unique_ptr<hb_set_t>> unique_features;
  //find out duplicate features after subset
  for (unsigned i : feature_indices->iter ())
  {
    hb_tag_t t = g.get_feature_tag (i);
    if (t == HB_MAP_VALUE_INVALID) continue;
    if (!unique_features.has (t))
    {
      if (unlikely (!unique_features.set (t, hb::unique_ptr<hb_set_t> {hb_set_create ()})))
	return;
      if (unique_features.has (t))
	unique_features.get (t)->add (i);
      duplicate_feature_map->set (i, i);
      continue;
    }

    bool found = false;

    hb_set_t* same_tag_features = unique_features.get (t);
    for (unsigned other_f_index : same_tag_features->iter ())
    {
      const OT::Feature* f = &(g.get_feature (i));
      const OT::Feature **p = nullptr;
      if (feature_substitutes_map->has (i, &p))
        f = *p;

      const OT::Feature* other_f = &(g.get_feature (other_f_index));
      if (feature_substitutes_map->has (other_f_index, &p))
        other_f = *p;

      auto f_iter =
      + hb_iter (f->lookupIndex)
      | hb_filter (lookup_indices)
      ;

      auto other_f_iter =
      + hb_iter (other_f->lookupIndex)
      | hb_filter (lookup_indices)
      ;

      bool is_equal = true;
      for (; f_iter && other_f_iter; f_iter++, other_f_iter++)
      {
	unsigned a = *f_iter;
	unsigned b = *other_f_iter;
	if (a != b) { is_equal = false; break; }
      }

      if (is_equal == false || f_iter || other_f_iter) continue;

      found = true;
      duplicate_feature_map->set (i, other_f_index);
      break;
    }

    if (found == false)
    {
      same_tag_features->add (i);
      duplicate_feature_map->set (i, i);
    }
  }
}

template <typename T>
static inline void
_closure_glyphs_lookups_features (hb_subset_plan_t   *plan,
				  hb_set_t	     *gids_to_retain,
				  hb_map_t	     *lookups,
				  hb_map_t	     *features,
				  script_langsys_map *langsys_map,
				  hb_hashmap_t<unsigned, hb::shared_ptr<hb_set_t>> *feature_record_cond_idx_map,
				  hb_hashmap_t<unsigned, const OT::Feature*> *feature_substitutes_map,
                                  hb_set_t &catch_all_record_feature_idxes,
                                  hb_hashmap_t<unsigned, hb_pair_t<const void*, const void*>>& catch_all_record_idx_feature_map)
{
  hb_blob_ptr_t<T> table = plan->source_table<T> ();
  hb_tag_t table_tag = table->tableTag;
  hb_set_t lookup_indices, feature_indices;
  _collect_layout_indices<T> (plan,
                              *table,
                              &lookup_indices,
                              &feature_indices,
                              feature_record_cond_idx_map,
                              feature_substitutes_map,
                              catch_all_record_feature_idxes,
                              catch_all_record_idx_feature_map);

  if (table_tag == HB_OT_TAG_GSUB && !(plan->flags & HB_SUBSET_FLAGS_NO_LAYOUT_CLOSURE))
    hb_ot_layout_lookups_substitute_closure (plan->source,
                                             &lookup_indices,
					     gids_to_retain);
  table->closure_lookups (plan->source,
			  gids_to_retain,
                          &lookup_indices);
  _remap_indexes (&lookup_indices, lookups);

  // prune features
  table->prune_features (lookups,
                         plan->user_axes_location.is_empty () ? nullptr : feature_record_cond_idx_map,
                         feature_substitutes_map,
                         &feature_indices);
  hb_map_t duplicate_feature_map;
  _GSUBGPOS_find_duplicate_features (*table, lookups, &feature_indices, feature_substitutes_map, &duplicate_feature_map);

  feature_indices.clear ();
  table->prune_langsys (&duplicate_feature_map, &plan->layout_scripts, langsys_map, &feature_indices);
  _remap_indexes (&feature_indices, features);

  table.destroy ();
}

#endif

#ifndef HB_NO_VAR
static inline void
_generate_varstore_inner_maps (const hb_set_t& varidx_set,
                               unsigned subtable_count,
                               hb_vector_t<hb_inc_bimap_t> &inner_maps /* OUT */)
{
  if (varidx_set.is_empty () || subtable_count == 0) return;

  if (unlikely (!inner_maps.resize (subtable_count))) return;
  for (unsigned idx : varidx_set)
  {
    uint16_t major = idx >> 16;
    uint16_t minor = idx & 0xFFFF;

    if (major >= subtable_count)
      continue;
    inner_maps[major].add (minor);
  }
}

static inline hb_font_t*
_get_hb_font_with_variations (const hb_subset_plan_t *plan)
{
  hb_font_t *font = hb_font_create (plan->source);

  hb_vector_t<hb_variation_t> vars;
  if (!vars.alloc (plan->user_axes_location.get_population ())) {
    hb_font_destroy (font);
    return nullptr;
  }

  for (auto _ : plan->user_axes_location)
  {
    hb_variation_t var;
    var.tag = _.first;
    var.value = _.second.middle;
    vars.push (var);
  }

#ifndef HB_NO_VAR
  hb_font_set_variations (font, vars.arrayZ, plan->user_axes_location.get_population ());
#endif
  return font;
}

static inline void
_remap_variation_indices (const OT::ItemVariationStore &var_store,
                          const hb_set_t &variation_indices,
                          const hb_vector_t<int>& normalized_coords,
                          bool calculate_delta, /* not pinned at default */
                          bool no_variations, /* all axes pinned */
                          hb_hashmap_t<unsigned, hb_pair_t<unsigned, int>> &variation_idx_delta_map /* OUT */)
{
  if (&var_store == &Null (OT::ItemVariationStore)) return;
  unsigned subtable_count = var_store.get_sub_table_count ();
  float *store_cache = var_store.create_cache ();

  unsigned new_major = 0, new_minor = 0;
  unsigned last_major = (variation_indices.get_min ()) >> 16;
  for (unsigned idx : variation_indices)
  {
    int delta = 0;
    if (calculate_delta)
      delta = roundf (var_store.get_delta (idx, normalized_coords.arrayZ,
                                           normalized_coords.length, store_cache));

    if (no_variations)
    {
      variation_idx_delta_map.set (idx, hb_pair_t<unsigned, int> (HB_OT_LAYOUT_NO_VARIATIONS_INDEX, delta));
      continue;
    }

    uint16_t major = idx >> 16;
    if (major >= subtable_count) break;
    if (major != last_major)
    {
      new_minor = 0;
      ++new_major;
    }

    unsigned new_idx = (new_major << 16) + new_minor;
    variation_idx_delta_map.set (idx, hb_pair_t<unsigned, int> (new_idx, delta));
    ++new_minor;
    last_major = major;
  }
  var_store.destroy_cache (store_cache);
}

static inline void
_collect_layout_variation_indices (hb_subset_plan_t* plan)
{
  hb_blob_ptr_t<OT::GDEF> gdef = plan->source_table<OT::GDEF> ();
  hb_blob_ptr_t<GPOS> gpos = plan->source_table<GPOS> ();

  if (!gdef->has_data () || !gdef->has_var_store ())
  {
    gdef.destroy ();
    gpos.destroy ();
    return;
  }

  hb_set_t varidx_set;
  OT::hb_collect_variation_indices_context_t c (&varidx_set,
                                                &plan->_glyphset_gsub,
                                                &plan->gpos_lookups);
  gdef->collect_variation_indices (&c);

  if (hb_ot_layout_has_positioning (plan->source))
    gpos->collect_variation_indices (&c);

  _remap_variation_indices (gdef->get_var_store (),
                            varidx_set, plan->normalized_coords,
                            !plan->pinned_at_default,
                            plan->all_axes_pinned,
                            plan->layout_variation_idx_delta_map);

  unsigned subtable_count = gdef->get_var_store ().get_sub_table_count ();
  _generate_varstore_inner_maps (varidx_set, subtable_count, plan->gdef_varstore_inner_maps);

  gdef.destroy ();
  gpos.destroy ();
}

#ifndef HB_NO_BASE
static inline void
_collect_base_variation_indices (hb_subset_plan_t* plan)
{
  hb_blob_ptr_t<OT::BASE> base = plan->source_table<OT::BASE> ();
  if (!base->has_var_store ())
  {
    base.destroy ();
    return;
  }

  hb_set_t varidx_set;
  base->collect_variation_indices (plan, varidx_set);
  const OT::ItemVariationStore &var_store = base->get_var_store ();
  unsigned subtable_count = var_store.get_sub_table_count ();


  _remap_variation_indices (var_store, varidx_set,
                            plan->normalized_coords,
                            !plan->pinned_at_default,
                            plan->all_axes_pinned,
                            plan->base_variation_idx_map);
  _generate_varstore_inner_maps (varidx_set, subtable_count, plan->base_varstore_inner_maps);

  base.destroy ();
}

#endif
#endif

static inline void
_cmap_closure (hb_face_t	   *face,
	       const hb_set_t	   *unicodes,
	       hb_set_t		   *glyphset)
{
  OT::cmap::accelerator_t cmap (face);
  cmap.table->closure_glyphs (unicodes, glyphset);
}

#ifndef HB_NO_VAR
static void
_remap_colrv1_delta_set_index_indices (const OT::DeltaSetIndexMap &index_map,
                                       const hb_set_t &delta_set_idxes,
                                       hb_hashmap_t<unsigned, hb_pair_t<unsigned, int>> &variation_idx_delta_map, /* IN/OUT */
                                       hb_map_t &new_deltaset_idx_varidx_map /* OUT */)
{
  if (!index_map.get_map_count ())
    return;

  hb_hashmap_t<unsigned, hb_pair_t<unsigned, int>> delta_set_idx_delta_map;
  unsigned new_delta_set_idx = 0;
  for (unsigned delta_set_idx : delta_set_idxes)
  {
    unsigned var_idx = index_map.map (delta_set_idx);
    unsigned new_varidx = HB_OT_LAYOUT_NO_VARIATIONS_INDEX;
    int delta = 0;

    if (var_idx != HB_OT_LAYOUT_NO_VARIATIONS_INDEX)
    {
      hb_pair_t<unsigned, int> *new_varidx_delta;
      if (!variation_idx_delta_map.has (var_idx, &new_varidx_delta)) continue;

      new_varidx = hb_first (*new_varidx_delta);
      delta = hb_second (*new_varidx_delta);
    }

    new_deltaset_idx_varidx_map.set (new_delta_set_idx, new_varidx);
    delta_set_idx_delta_map.set (delta_set_idx, hb_pair_t<unsigned, int> (new_delta_set_idx, delta));
    new_delta_set_idx++;
  }
  variation_idx_delta_map = std::move (delta_set_idx_delta_map);
}
#endif

static void _colr_closure (hb_subset_plan_t* plan,
                           hb_set_t *glyphs_colred)
{
  OT::COLR::accelerator_t colr (plan->source);
  if (!colr.is_valid ()) return;

  hb_set_t palette_indices, layer_indices;
  // Collect all glyphs referenced by COLRv0
  hb_set_t glyphset_colrv0;
  for (hb_codepoint_t gid : *glyphs_colred)
    colr.closure_glyphs (gid, &glyphset_colrv0);

  glyphs_colred->union_ (glyphset_colrv0);

  //closure for COLRv1
  hb_set_t variation_indices, delta_set_indices;
  colr.closure_forV1 (glyphs_colred, &layer_indices, &palette_indices, &variation_indices, &delta_set_indices);

  colr.closure_V0palette_indices (glyphs_colred, &palette_indices);
  _remap_indexes (&layer_indices, &plan->colrv1_layers);
  _remap_palette_indexes (&palette_indices, &plan->colr_palettes);

#ifndef HB_NO_VAR
  if (!colr.has_var_store () || !variation_indices) return;

  const OT::ItemVariationStore &var_store = colr.get_var_store ();
  // generated inner_maps is used by ItemVariationStore serialize(), which is subset only
  unsigned subtable_count = var_store.get_sub_table_count ();
  _generate_varstore_inner_maps (variation_indices, subtable_count, plan->colrv1_varstore_inner_maps);

  /* colr variation indices mapping during planning phase:
   * generate colrv1_variation_idx_delta_map. When delta set index map is not
   * included, it's a mapping from varIdx-> (new varIdx,delta). Otherwise, it's
   * a mapping from old delta set idx-> (new delta set idx, delta). Mapping
   * delta set indices is the same as gid mapping.
   * Besides, we need to generate a delta set idx-> new var_idx map for updating
   * delta set index map if exists. This map will be updated again after
   * instancing. */
  if (!plan->all_axes_pinned)
  {
    _remap_variation_indices (var_store,
                              variation_indices,
                              plan->normalized_coords,
                              false, /* no need to calculate delta for COLR during planning */
                              plan->all_axes_pinned,
                              plan->colrv1_variation_idx_delta_map);

    if (colr.has_delta_set_index_map ())
      _remap_colrv1_delta_set_index_indices (colr.get_delta_set_index_map (),
                                             delta_set_indices,
                                             plan->colrv1_variation_idx_delta_map,
                                             plan->colrv1_new_deltaset_idx_varidx_map);
  }
#endif
}

static inline void
_math_closure (hb_subset_plan_t *plan,
               hb_set_t         *glyphset)
{
  hb_blob_ptr_t<OT::MATH> math = plan->source_table<OT::MATH> ();
  if (math->has_data ())
    math->closure_glyphs (glyphset);
  math.destroy ();
}

static inline void
_remap_used_mark_sets (hb_subset_plan_t *plan,
                       hb_map_t& used_mark_sets_map)
{
  hb_blob_ptr_t<OT::GDEF> gdef = plan->source_table<OT::GDEF> ();

  if (!gdef->has_data () || !gdef->has_mark_glyph_sets ())
  {
    gdef.destroy ();
    return;
  }

  hb_set_t used_mark_sets;
  gdef->get_mark_glyph_sets ().collect_used_mark_sets (plan->_glyphset_gsub, used_mark_sets);
  gdef.destroy ();

  _remap_indexes (&used_mark_sets, &used_mark_sets_map);
}

static inline void
_remove_invalid_gids (hb_set_t *glyphs,
		      unsigned int num_glyphs)
{
  glyphs->del_range (num_glyphs, HB_SET_VALUE_INVALID);
}

template<bool GID_ALWAYS_EXISTS = false, typename I, typename F, typename G, hb_requires (hb_is_iterator (I))>
static void
_fill_unicode_and_glyph_map(hb_subset_plan_t *plan,
                            I unicode_iterator,
                            F unicode_to_gid_for_iterator,
                            G unicode_to_gid_general)
{
  for (hb_codepoint_t cp : unicode_iterator)
  {
    hb_codepoint_t gid = unicode_to_gid_for_iterator(cp);
    if (!GID_ALWAYS_EXISTS && gid == HB_MAP_VALUE_INVALID)
    {
      DEBUG_MSG(SUBSET, nullptr, "Drop U+%04X; no gid", cp);
      continue;
    }

    plan->codepoint_to_glyph->set (cp, gid);
    plan->unicode_to_new_gid_list.push (hb_pair (cp, gid));
  }
}

template<bool GID_ALWAYS_EXISTS = false, typename I, typename F, hb_requires (hb_is_iterator (I))>
static void
_fill_unicode_and_glyph_map(hb_subset_plan_t *plan,
                            I unicode_iterator,
                            F unicode_to_gid_for_iterator)
{
  _fill_unicode_and_glyph_map(plan, unicode_iterator, unicode_to_gid_for_iterator, unicode_to_gid_for_iterator);
}

static void
_populate_unicodes_to_retain (const hb_set_t *unicodes,
                              const hb_set_t *glyphs,
                              hb_subset_plan_t *plan)
{
  OT::cmap::accelerator_t cmap (plan->source);
  unsigned size_threshold = plan->source->get_num_glyphs ();
  if (glyphs->is_empty () && unicodes->get_population () < size_threshold)
  {

    const hb_map_t* unicode_to_gid = nullptr;
    if (plan->accelerator)
      unicode_to_gid = &plan->accelerator->unicode_to_gid;

    // This is approach to collection is faster, but can only be used  if glyphs
    // are not being explicitly added to the subset and the input unicodes set is
    // not excessively large (eg. an inverted set).
    plan->unicode_to_new_gid_list.alloc (unicodes->get_population ());
    if (!unicode_to_gid) {
      _fill_unicode_and_glyph_map(plan, unicodes->iter(), [&] (hb_codepoint_t cp) {
        hb_codepoint_t gid;
        if (!cmap.get_nominal_glyph (cp, &gid)) {
          return HB_MAP_VALUE_INVALID;
        }
        return gid;
      });
    } else {
      // Use in memory unicode to gid map it's faster then looking up from
      // the map. This code is mostly duplicated from above to avoid doing
      // conditionals on the presence of the unicode_to_gid map each
      // iteration.
      _fill_unicode_and_glyph_map(plan, unicodes->iter(), [&] (hb_codepoint_t cp) {
        return unicode_to_gid->get (cp);
      });
    }
  }
  else
  {
    // This approach is slower, but can handle adding in glyphs to the subset and will match
    // them with cmap entries.

    hb_map_t unicode_glyphid_map_storage;
    hb_set_t cmap_unicodes_storage;
    const hb_map_t* unicode_glyphid_map = &unicode_glyphid_map_storage;
    const hb_set_t* cmap_unicodes = &cmap_unicodes_storage;

    if (!plan->accelerator) {
      cmap.collect_mapping (&cmap_unicodes_storage, &unicode_glyphid_map_storage);
      plan->unicode_to_new_gid_list.alloc (hb_min(unicodes->get_population ()
                                                  + glyphs->get_population (),
                                                  cmap_unicodes->get_population ()));
    } else {
      unicode_glyphid_map = &plan->accelerator->unicode_to_gid;
      cmap_unicodes = &plan->accelerator->unicodes;
    }

    if (plan->accelerator &&
	unicodes->get_population () < cmap_unicodes->get_population () &&
	glyphs->get_population () < cmap_unicodes->get_population ())
    {
      plan->codepoint_to_glyph->alloc (unicodes->get_population () + glyphs->get_population ());

      auto &gid_to_unicodes = plan->accelerator->gid_to_unicodes;

      for (hb_codepoint_t gid : *glyphs)
      {
        auto unicodes = gid_to_unicodes.get (gid);
        _fill_unicode_and_glyph_map<true>(plan, unicodes, [&] (hb_codepoint_t cp) {
          return gid;
        },
        [&] (hb_codepoint_t cp) {
          return unicode_glyphid_map->get(cp);
        });
      }

      _fill_unicode_and_glyph_map(plan, unicodes->iter(), [&] (hb_codepoint_t cp) {
          /* Don't double-add entry. */
	if (plan->codepoint_to_glyph->has (cp))
          return HB_MAP_VALUE_INVALID;

        return unicode_glyphid_map->get(cp);
      },
      [&] (hb_codepoint_t cp) {
          return unicode_glyphid_map->get(cp);
      });

      plan->unicode_to_new_gid_list.qsort ();
    }
    else
    {
      plan->codepoint_to_glyph->alloc (cmap_unicodes->get_population ());
      hb_codepoint_t first = HB_SET_VALUE_INVALID, last = HB_SET_VALUE_INVALID;
      for (; cmap_unicodes->next_range (&first, &last); )
      {
        _fill_unicode_and_glyph_map(plan, hb_range(first, last + 1), [&] (hb_codepoint_t cp) {
          hb_codepoint_t gid = (*unicode_glyphid_map)[cp];
	  if (!unicodes->has (cp) && !glyphs->has (gid))
	    return HB_MAP_VALUE_INVALID;
          return gid;
        },
        [&] (hb_codepoint_t cp) {
          return unicode_glyphid_map->get(cp);
        });
      }
    }

    /* Add gids which where requested, but not mapped in cmap */
    unsigned num_glyphs = plan->source->get_num_glyphs ();
    hb_codepoint_t first = HB_SET_VALUE_INVALID, last = HB_SET_VALUE_INVALID;
    for (; glyphs->next_range (&first, &last); )
    {
      if (first >= num_glyphs)
	break;
      if (last >= num_glyphs)
        last = num_glyphs - 1;
      plan->_glyphset_gsub.add_range (first, last);
    }
  }

  auto &arr = plan->unicode_to_new_gid_list;
  if (arr.length)
  {
    plan->unicodes.add_sorted_array (&arr.arrayZ->first, arr.length, sizeof (*arr.arrayZ));
    plan->_glyphset_gsub.add_array (&arr.arrayZ->second, arr.length, sizeof (*arr.arrayZ));
  }
}

static unsigned
_glyf_add_gid_and_children (const OT::glyf_accelerator_t &glyf,
			    hb_codepoint_t gid,
			    hb_set_t *gids_to_retain,
			    int operation_count,
			    unsigned depth = 0)
{
  /* Check if is already visited */
  if (gids_to_retain->has (gid)) return operation_count;

  gids_to_retain->add (gid);

  if (unlikely (depth++ > HB_MAX_NESTING_LEVEL)) return operation_count;
  if (unlikely (--operation_count < 0)) return operation_count;

  auto glyph = glyf.glyph_for_gid (gid);

  for (auto &item : glyph.get_composite_iterator ())
    operation_count =
      _glyf_add_gid_and_children (glyf,
				  item.get_gid (),
				  gids_to_retain,
				  operation_count,
				  depth);

  return operation_count;
}

static void
_nameid_closure (hb_subset_plan_t* plan,
		 hb_set_t* drop_tables)
{
#ifndef HB_NO_STYLE
  plan->source->table.STAT->collect_name_ids (&plan->user_axes_location, &plan->name_ids);
#endif
#ifndef HB_NO_VAR
  if (!plan->all_axes_pinned)
    plan->source->table.fvar->collect_name_ids (&plan->user_axes_location, &plan->axes_old_index_tag_map, &plan->name_ids);
#endif
#ifndef HB_NO_COLOR
  if (!drop_tables->has (HB_OT_TAG_CPAL))
    plan->source->table.CPAL->collect_name_ids (&plan->colr_palettes, &plan->name_ids);
#endif

#ifndef HB_NO_SUBSET_LAYOUT
  if (!drop_tables->has (HB_OT_TAG_GPOS))
  {
    hb_blob_ptr_t<GPOS> gpos = plan->source_table<GPOS> ();
    gpos->collect_name_ids (&plan->gpos_features, &plan->name_ids);
    gpos.destroy ();
  }
  if (!drop_tables->has (HB_OT_TAG_GSUB))
  {
    hb_blob_ptr_t<GSUB> gsub = plan->source_table<GSUB> ();
    gsub->collect_name_ids (&plan->gsub_features, &plan->name_ids);
    gsub.destroy ();
  }
#endif
}

static void
_populate_gids_to_retain (hb_subset_plan_t* plan,
		          hb_set_t* drop_tables)
{
  OT::glyf_accelerator_t glyf (plan->source);
#ifndef HB_NO_SUBSET_CFF
  // Note: we cannot use inprogress_accelerator here, since it has not been
  // created yet. So in case of preprocessed-face (and otherwise), we do an
  // extra sanitize pass here, which is not ideal.
  OT::cff1::accelerator_subset_t stack_cff (plan->accelerator ? nullptr : plan->source);
  const OT::cff1::accelerator_subset_t *cff (plan->accelerator ? plan->accelerator->cff1_accel.get () : &stack_cff);
#endif

  plan->_glyphset_gsub.add (0); // Not-def

  _cmap_closure (plan->source, &plan->unicodes, &plan->_glyphset_gsub);

#ifndef HB_NO_SUBSET_LAYOUT
  if (!drop_tables->has (HB_OT_TAG_GSUB))
    // closure all glyphs/lookups/features needed for GSUB substitutions.
    _closure_glyphs_lookups_features<GSUB> (
        plan,
        &plan->_glyphset_gsub,
        &plan->gsub_lookups,
        &plan->gsub_features,
        &plan->gsub_langsys,
        &plan->gsub_feature_record_cond_idx_map,
        &plan->gsub_feature_substitutes_map,
        plan->gsub_old_features,
        plan->gsub_old_feature_idx_tag_map);

  if (!drop_tables->has (HB_OT_TAG_GPOS))
    _closure_glyphs_lookups_features<GPOS> (
        plan,
        &plan->_glyphset_gsub,
        &plan->gpos_lookups,
        &plan->gpos_features,
        &plan->gpos_langsys,
        &plan->gpos_feature_record_cond_idx_map,
        &plan->gpos_feature_substitutes_map,
        plan->gpos_old_features,
        plan->gpos_old_feature_idx_tag_map);
#endif
  _remove_invalid_gids (&plan->_glyphset_gsub, plan->source->get_num_glyphs ());

  plan->_glyphset_mathed = plan->_glyphset_gsub;
  if (!drop_tables->has (HB_OT_TAG_MATH))
  {
    _math_closure (plan, &plan->_glyphset_mathed);
    _remove_invalid_gids (&plan->_glyphset_mathed, plan->source->get_num_glyphs ());
  }

  hb_set_t cur_glyphset = plan->_glyphset_mathed;
  if (!drop_tables->has (HB_OT_TAG_COLR))
  {
    _colr_closure (plan, &cur_glyphset);
    _remove_invalid_gids (&cur_glyphset, plan->source->get_num_glyphs ());
  }

  plan->_glyphset_colred = cur_glyphset;

  // XXX TODO VARC closure / subset

  _nameid_closure (plan, drop_tables);
  /* Populate a full set of glyphs to retain by adding all referenced
   * composite glyphs. */
  if (glyf.has_data ())
    for (hb_codepoint_t gid : cur_glyphset)
      _glyf_add_gid_and_children (glyf, gid, &plan->_glyphset,
				  cur_glyphset.get_population () * HB_MAX_COMPOSITE_OPERATIONS_PER_GLYPH);
  else
    plan->_glyphset.union_ (cur_glyphset);
#ifndef HB_NO_SUBSET_CFF
  if (!plan->accelerator || plan->accelerator->has_seac)
  {
    bool has_seac = false;
    if (cff->is_valid ())
      for (hb_codepoint_t gid : cur_glyphset)
	if (_add_cff_seac_components (*cff, gid, &plan->_glyphset))
	  has_seac = true;
    plan->has_seac = has_seac;
  }
#endif

  _remove_invalid_gids (&plan->_glyphset, plan->source->get_num_glyphs ());

#ifndef HB_NO_VAR
  if (!drop_tables->has (HB_OT_TAG_GDEF))
    _collect_layout_variation_indices (plan);
#endif
}

static void
_create_glyph_map_gsub (const hb_set_t* glyph_set_gsub,
                        const hb_map_t* glyph_map,
                        hb_map_t* out)
{
  out->alloc (glyph_set_gsub->get_population ());
  + hb_iter (glyph_set_gsub)
  | hb_map ([&] (hb_codepoint_t gid) {
    return hb_codepoint_pair_t (gid, glyph_map->get (gid));
  })
  | hb_sink (out)
  ;
}

static bool
_create_old_gid_to_new_gid_map (const hb_face_t *face,
				bool		 retain_gids,
				const hb_set_t	*all_gids_to_retain,
                                const hb_map_t  *requested_glyph_map,
				hb_map_t	*glyph_map, /* OUT */
				hb_map_t	*reverse_glyph_map, /* OUT */
				hb_sorted_vector_t<hb_codepoint_pair_t> *new_to_old_gid_list /* OUT */,
				unsigned int	*num_glyphs /* OUT */)
{
  unsigned pop = all_gids_to_retain->get_population ();
  reverse_glyph_map->alloc (pop);
  glyph_map->alloc (pop);
  new_to_old_gid_list->alloc (pop);

  if (*requested_glyph_map)
  {
    hb_set_t new_gids(requested_glyph_map->values());
    if (new_gids.get_population() != requested_glyph_map->get_population())
    {
      DEBUG_MSG (SUBSET, nullptr, "The provided custom glyph mapping is not unique.");
      return false;
    }

    if (retain_gids)
    {
      DEBUG_MSG (SUBSET, nullptr,
        "HB_SUBSET_FLAGS_RETAIN_GIDS cannot be set if "
        "a custom glyph mapping has been provided.");
      return false;
    }

    hb_codepoint_t max_glyph = 0;
    hb_set_t remaining;
    for (auto old_gid : all_gids_to_retain->iter ())
    {
      if (old_gid == 0) {
	new_to_old_gid_list->push (hb_pair<hb_codepoint_t, hb_codepoint_t> (0u, 0u));
        continue;
      }

      hb_codepoint_t* new_gid;
      if (!requested_glyph_map->has (old_gid, &new_gid))
      {
        remaining.add(old_gid);
        continue;
      }

      if (*new_gid > max_glyph)
        max_glyph = *new_gid;
      new_to_old_gid_list->push (hb_pair (*new_gid, old_gid));
    }
    new_to_old_gid_list->qsort ();

    // Anything that wasn't mapped by the requested mapping should
    // be placed after the requested mapping.
    for (auto old_gid : remaining)
      new_to_old_gid_list->push (hb_pair (++max_glyph, old_gid));

    *num_glyphs = max_glyph + 1;
  }
  else if (!retain_gids)
  {
    + hb_enumerate (hb_iter (all_gids_to_retain), (hb_codepoint_t) 0)
    | hb_sink (new_to_old_gid_list)
    ;
    *num_glyphs = new_to_old_gid_list->length;
  }
  else
  {
    + hb_iter (all_gids_to_retain)
    | hb_map ([] (hb_codepoint_t _) {
		return hb_codepoint_pair_t (_, _);
	      })
    | hb_sink (new_to_old_gid_list)
    ;

    hb_codepoint_t max_glyph = HB_SET_VALUE_INVALID;
    hb_set_previous (all_gids_to_retain, &max_glyph);

    *num_glyphs = max_glyph + 1;
  }

  reverse_glyph_map->alloc (reverse_glyph_map->get_population () + new_to_old_gid_list->length);
  + hb_iter (new_to_old_gid_list)
  | hb_sink (reverse_glyph_map)
  ;
  glyph_map->alloc (glyph_map->get_population () + new_to_old_gid_list->length);
  + hb_iter (new_to_old_gid_list)
  | hb_map (&hb_codepoint_pair_t::reverse)
  | hb_sink (glyph_map)
  ;

  return true;
}

#ifndef HB_NO_VAR
static void
_normalize_axes_location (hb_face_t *face, hb_subset_plan_t *plan)
{
  if (plan->user_axes_location.is_empty ())
    return;

  hb_array_t<const OT::AxisRecord> axes = face->table.fvar->get_axes ();
  plan->normalized_coords.resize (axes.length);

  bool has_avar = face->table.avar->has_data ();
  const OT::SegmentMaps *seg_maps = nullptr;
  unsigned avar_axis_count = 0;
  if (has_avar)
  {
    seg_maps = face->table.avar->get_segment_maps ();
    avar_axis_count = face->table.avar->get_axis_count();
  }

  bool axis_not_pinned = false;
  unsigned old_axis_idx = 0, new_axis_idx = 0;
  for (const auto& axis : axes)
  {
    hb_tag_t axis_tag = axis.get_axis_tag ();
    plan->axes_old_index_tag_map.set (old_axis_idx, axis_tag);

    if (!plan->user_axes_location.has (axis_tag) ||
        !plan->user_axes_location.get (axis_tag).is_point ())
    {
      axis_not_pinned = true;
      plan->axes_index_map.set (old_axis_idx, new_axis_idx);
      plan->axis_tags.push (axis_tag);
      new_axis_idx++;
    }

    Triple *axis_range;
    if (plan->user_axes_location.has (axis_tag, &axis_range))
    {
      plan->axes_triple_distances.set (axis_tag, axis.get_triple_distances ());

      int normalized_min = axis.normalize_axis_value (axis_range->minimum);
      int normalized_default = axis.normalize_axis_value (axis_range->middle);
      int normalized_max = axis.normalize_axis_value (axis_range->maximum);

      if (has_avar && old_axis_idx < avar_axis_count)
      {
        normalized_min = seg_maps->map (normalized_min);
        normalized_default = seg_maps->map (normalized_default);
        normalized_max = seg_maps->map (normalized_max);
      }
      plan->axes_location.set (axis_tag, Triple (static_cast<double> (normalized_min / 16384.0),
                                                 static_cast<double> (normalized_default / 16384.0),
                                                 static_cast<double> (normalized_max / 16384.0)));

      if (normalized_default != 0)
        plan->pinned_at_default = false;

      plan->normalized_coords[old_axis_idx] = normalized_default;
    }

    old_axis_idx++;

    if (has_avar && old_axis_idx < avar_axis_count)
      seg_maps = &StructAfter<OT::SegmentMaps> (*seg_maps);
  }
  plan->all_axes_pinned = !axis_not_pinned;
}

static void
_update_instance_metrics_map_from_cff2 (hb_subset_plan_t *plan)
{
  if (!plan->normalized_coords) return;
  OT::cff2::accelerator_t cff2 (plan->source);
  if (!cff2.is_valid ()) return;

  hb_font_t *font = _get_hb_font_with_variations (plan);
  if (unlikely (!plan->check_success (font != nullptr)))
  {
    hb_font_destroy (font);
    return;
  }

  hb_glyph_extents_t extents = {0x7FFF, -0x7FFF};
  OT::hmtx_accelerator_t _hmtx (plan->source);
  float *hvar_store_cache = nullptr;
  if (_hmtx.has_data () && _hmtx.var_table.get_length ())
    hvar_store_cache = _hmtx.var_table->get_var_store ().create_cache ();

  OT::vmtx_accelerator_t _vmtx (plan->source);
  float *vvar_store_cache = nullptr;
  if (_vmtx.has_data () && _vmtx.var_table.get_length ())
    vvar_store_cache = _vmtx.var_table->get_var_store ().create_cache ();

  for (auto p : *plan->glyph_map)
  {
    hb_codepoint_t old_gid = p.first;
    hb_codepoint_t new_gid = p.second;
    if (!cff2.get_extents (font, old_gid, &extents)) continue;
    bool has_bounds_info = true;
    if (extents.x_bearing == 0 && extents.width == 0 &&
        extents.height == 0 && extents.y_bearing == 0)
      has_bounds_info = false;

    if (has_bounds_info)
    {
      plan->head_maxp_info.xMin = hb_min (plan->head_maxp_info.xMin, extents.x_bearing);
      plan->head_maxp_info.xMax = hb_max (plan->head_maxp_info.xMax, extents.x_bearing + extents.width);
      plan->head_maxp_info.yMax = hb_max (plan->head_maxp_info.yMax, extents.y_bearing);
      plan->head_maxp_info.yMin = hb_min (plan->head_maxp_info.yMin, extents.y_bearing + extents.height);
    }

    if (_hmtx.has_data ())
    {
      int hori_aw = _hmtx.get_advance_without_var_unscaled (old_gid);
      if (_hmtx.var_table.get_length ())
        hori_aw += (int) roundf (_hmtx.var_table->get_advance_delta_unscaled (old_gid, font->coords, font->num_coords,
                                                                              hvar_store_cache));
      int lsb = extents.x_bearing;
      if (!has_bounds_info)
      {
        if (!_hmtx.get_leading_bearing_without_var_unscaled (old_gid, &lsb))
          continue;
      }
      plan->hmtx_map.set (new_gid, hb_pair ((unsigned) hori_aw, lsb));
      plan->bounds_width_vec[new_gid] = extents.width;
    }

    if (_vmtx.has_data ())
    {
      int vert_aw = _vmtx.get_advance_without_var_unscaled (old_gid);
      if (_vmtx.var_table.get_length ())
        vert_aw += (int) roundf (_vmtx.var_table->get_advance_delta_unscaled (old_gid, font->coords, font->num_coords,
                                                                              vvar_store_cache));

      int tsb = extents.y_bearing;
      if (!has_bounds_info)
      {
        if (!_vmtx.get_leading_bearing_without_var_unscaled (old_gid, &tsb))
          continue;
      }
      plan->vmtx_map.set (new_gid, hb_pair ((unsigned) vert_aw, tsb));
      plan->bounds_height_vec[new_gid] = extents.height;
    }
  }
  hb_font_destroy (font);
  if (hvar_store_cache)
    _hmtx.var_table->get_var_store ().destroy_cache (hvar_store_cache);
  if (vvar_store_cache)
    _vmtx.var_table->get_var_store ().destroy_cache (vvar_store_cache);
}

static bool
_get_instance_glyphs_contour_points (hb_subset_plan_t *plan)
{
  /* contour_points vector only needed for updating gvar table (infer delta and
   * iup delta optimization) during partial instancing */
  if (plan->user_axes_location.is_empty () || plan->all_axes_pinned)
    return true;

  OT::glyf_accelerator_t glyf (plan->source);

  for (auto &_ : plan->new_to_old_gid_list)
  {
    hb_codepoint_t new_gid = _.first;
    contour_point_vector_t all_points;
    if (new_gid == 0 && !(plan->flags & HB_SUBSET_FLAGS_NOTDEF_OUTLINE))
    {
      if (unlikely (!plan->new_gid_contour_points_map.set (new_gid, all_points)))
        return false;
      continue;
    }

    hb_codepoint_t old_gid = _.second;
    auto glyph = glyf.glyph_for_gid (old_gid);
    if (unlikely (!glyph.get_all_points_without_var (plan->source, all_points)))
      return false;
    if (unlikely (!plan->new_gid_contour_points_map.set (new_gid, all_points)))
      return false;

    /* composite new gids are only needed by iup delta optimization */
    if ((plan->flags & HB_SUBSET_FLAGS_OPTIMIZE_IUP_DELTAS) && glyph.is_composite ())
      plan->composite_new_gids.add (new_gid);
  }
  return true;
}
#endif

hb_subset_plan_t::hb_subset_plan_t (hb_face_t *face,
				    const hb_subset_input_t *input)
{
  successful = true;
  flags = input->flags;

  unicode_to_new_gid_list.init ();

  name_ids = *input->sets.name_ids;
  name_languages = *input->sets.name_languages;
  layout_features = *input->sets.layout_features;
  layout_scripts = *input->sets.layout_scripts;
  glyphs_requested = *input->sets.glyphs;
  drop_tables = *input->sets.drop_tables;
  no_subset_tables = *input->sets.no_subset_tables;
  source = hb_face_reference (face);
  dest = hb_face_builder_create ();

  codepoint_to_glyph = hb_map_create ();
  glyph_map = hb_map_create ();
  reverse_glyph_map = hb_map_create ();

  gsub_insert_catch_all_feature_variation_rec = false;
  gpos_insert_catch_all_feature_variation_rec = false;
  gdef_varstore_inner_maps.init ();

  user_axes_location = input->axes_location;
  all_axes_pinned = false;
  pinned_at_default = true;
  has_gdef_varstore = false;

#ifdef HB_EXPERIMENTAL_API
  for (auto _ : input->name_table_overrides)
  {
    hb_bytes_t name_bytes = _.second;
    unsigned len = name_bytes.length;
    char *name_str = (char *) hb_malloc (len);
    if (unlikely (!check_success (name_str)))
      break;

    hb_memcpy (name_str, name_bytes.arrayZ, len);
    name_table_overrides.set (_.first, hb_bytes_t (name_str, len));
  }
#endif

  void* accel = hb_face_get_user_data(face, hb_subset_accelerator_t::user_data_key());

  attach_accelerator_data = input->attach_accelerator_data;
  force_long_loca = input->force_long_loca;
#ifdef HB_EXPERIMENTAL_API
  force_long_loca = force_long_loca || (flags & HB_SUBSET_FLAGS_IFTB_REQUIREMENTS);
#endif

  if (accel)
    accelerator = (hb_subset_accelerator_t*) accel;

  if (unlikely (in_error ()))
    return;

#ifndef HB_NO_VAR
  _normalize_axes_location (face, this);
#endif

  _populate_unicodes_to_retain (input->sets.unicodes, input->sets.glyphs, this);

  _populate_gids_to_retain (this, input->sets.drop_tables);
  if (unlikely (in_error ()))
    return;

  if (!check_success(_create_old_gid_to_new_gid_map(
          face,
          input->flags & HB_SUBSET_FLAGS_RETAIN_GIDS,
          &_glyphset,
          &input->glyph_map,
          glyph_map,
          reverse_glyph_map,
	  &new_to_old_gid_list,
          &_num_output_glyphs))) {
    return;
  }

  _create_glyph_map_gsub (
      &_glyphset_gsub,
      glyph_map,
      &glyph_map_gsub);

  // Now that we have old to new gid map update the unicode to new gid list.
  for (unsigned i = 0; i < unicode_to_new_gid_list.length; i++)
  {
    // Use raw array access for performance.
    unicode_to_new_gid_list.arrayZ[i].second =
        glyph_map->get(unicode_to_new_gid_list.arrayZ[i].second);
  }

  bounds_width_vec.resize (_num_output_glyphs, false);
  for (auto &v : bounds_width_vec)
    v = 0xFFFFFFFF;
  bounds_height_vec.resize (_num_output_glyphs, false);
  for (auto &v : bounds_height_vec)
    v = 0xFFFFFFFF;

  if (!drop_tables.has (HB_OT_TAG_GDEF))
    _remap_used_mark_sets (this, used_mark_sets_map);

#ifndef HB_NO_VAR
#ifndef HB_NO_BASE
  if (!drop_tables.has (HB_OT_TAG_BASE))
    _collect_base_variation_indices (this);
#endif
#endif

  if (unlikely (in_error ()))
    return;

#ifndef HB_NO_VAR
  _update_instance_metrics_map_from_cff2 (this);
  if (!check_success (_get_instance_glyphs_contour_points (this)))
      return;
#endif

  if (attach_accelerator_data)
  {
    inprogress_accelerator =
      hb_subset_accelerator_t::create (source,
				       *codepoint_to_glyph,
                                       unicodes,
				       has_seac);

    check_success (inprogress_accelerator);
  }

#define HB_SUBSET_PLAN_MEMBER(Type, Name) check_success (!Name.in_error ());
#include "hb-subset-plan-member-list.hh"
#undef HB_SUBSET_PLAN_MEMBER
}

hb_subset_plan_t::~hb_subset_plan_t()
{
  hb_face_destroy (dest);

  hb_map_destroy (codepoint_to_glyph);
  hb_map_destroy (glyph_map);
  hb_map_destroy (reverse_glyph_map);
#ifndef HB_NO_SUBSET_CFF
  cff1_accel.fini ();
  cff2_accel.fini ();
#endif
  hb_face_destroy (source);

#ifdef HB_EXPERIMENTAL_API
  for (auto _ : name_table_overrides.iter_ref ())
    _.second.fini ();
#endif

  if (inprogress_accelerator)
    hb_subset_accelerator_t::destroy ((void*) inprogress_accelerator);
}


/**
 * hb_subset_plan_create_or_fail:
 * @face: font face to create the plan for.
 * @input: a #hb_subset_input_t input.
 *
 * Computes a plan for subsetting the supplied face according
 * to a provided input. The plan describes
 * which tables and glyphs should be retained.
 *
 * Return value: (transfer full): New subset plan. Destroy with
 * hb_subset_plan_destroy(). If there is a failure creating the plan
 * nullptr will be returned.
 *
 * Since: 4.0.0
 **/
hb_subset_plan_t *
hb_subset_plan_create_or_fail (hb_face_t	 *face,
                               const hb_subset_input_t *input)
{
  hb_subset_plan_t *plan;
  if (unlikely (!(plan = hb_object_create<hb_subset_plan_t> (face, input))))
    return nullptr;

  if (unlikely (plan->in_error ()))
  {
    hb_subset_plan_destroy (plan);
    return nullptr;
  }

  return plan;
}

/**
 * hb_subset_plan_destroy:
 * @plan: a #hb_subset_plan_t
 *
 * Decreases the reference count on @plan, and if it reaches zero, destroys
 * @plan, freeing all memory.
 *
 * Since: 4.0.0
 **/
void
hb_subset_plan_destroy (hb_subset_plan_t *plan)
{
  if (!hb_object_destroy (plan)) return;

  hb_free (plan);
}

/**
 * hb_subset_plan_old_to_new_glyph_mapping:
 * @plan: a subsetting plan.
 *
 * Returns the mapping between glyphs in the original font to glyphs in the
 * subset that will be produced by @plan
 *
 * Return value: (transfer none):
 * A pointer to the #hb_map_t of the mapping.
 *
 * Since: 4.0.0
 **/
hb_map_t *
hb_subset_plan_old_to_new_glyph_mapping (const hb_subset_plan_t *plan)
{
  return plan->glyph_map;
}

/**
 * hb_subset_plan_new_to_old_glyph_mapping:
 * @plan: a subsetting plan.
 *
 * Returns the mapping between glyphs in the subset that will be produced by
 * @plan and the glyph in the original font.
 *
 * Return value: (transfer none):
 * A pointer to the #hb_map_t of the mapping.
 *
 * Since: 4.0.0
 **/
hb_map_t *
hb_subset_plan_new_to_old_glyph_mapping (const hb_subset_plan_t *plan)
{
  return plan->reverse_glyph_map;
}

/**
 * hb_subset_plan_unicode_to_old_glyph_mapping:
 * @plan: a subsetting plan.
 *
 * Returns the mapping between codepoints in the original font and the
 * associated glyph id in the original font.
 *
 * Return value: (transfer none):
 * A pointer to the #hb_map_t of the mapping.
 *
 * Since: 4.0.0
 **/
hb_map_t *
hb_subset_plan_unicode_to_old_glyph_mapping (const hb_subset_plan_t *plan)
{
  return plan->codepoint_to_glyph;
}

/**
 * hb_subset_plan_reference: (skip)
 * @plan: a #hb_subset_plan_t object.
 *
 * Increases the reference count on @plan.
 *
 * Return value: @plan.
 *
 * Since: 4.0.0
 **/
hb_subset_plan_t *
hb_subset_plan_reference (hb_subset_plan_t *plan)
{
  return hb_object_reference (plan);
}

/**
 * hb_subset_plan_set_user_data: (skip)
 * @plan: a #hb_subset_plan_t object.
 * @key: The user-data key to set
 * @data: A pointer to the user data
 * @destroy: (nullable): A callback to call when @data is not needed anymore
 * @replace: Whether to replace an existing data with the same key
 *
 * Attaches a user-data key/data pair to the given subset plan object.
 *
 * Return value: `true` if success, `false` otherwise
 *
 * Since: 4.0.0
 **/
hb_bool_t
hb_subset_plan_set_user_data (hb_subset_plan_t   *plan,
                              hb_user_data_key_t *key,
                              void               *data,
                              hb_destroy_func_t   destroy,
                              hb_bool_t	          replace)
{
  return hb_object_set_user_data (plan, key, data, destroy, replace);
}

/**
 * hb_subset_plan_get_user_data: (skip)
 * @plan: a #hb_subset_plan_t object.
 * @key: The user-data key to query
 *
 * Fetches the user data associated with the specified key,
 * attached to the specified subset plan object.
 *
 * Return value: (transfer none): A pointer to the user data
 *
 * Since: 4.0.0
 **/
void *
hb_subset_plan_get_user_data (const hb_subset_plan_t *plan,
                              hb_user_data_key_t     *key)
{
  return hb_object_get_user_data (plan, key);
}
