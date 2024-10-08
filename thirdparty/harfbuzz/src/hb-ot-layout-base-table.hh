/*
 * Copyright © 2016  Elie Roux <elie.roux@telecom-bretagne.eu>
 * Copyright © 2018  Google, Inc.
 * Copyright © 2018-2019  Ebrahim Byagowi
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
 * Google Author(s): Behdad Esfahbod
 */

#ifndef HB_OT_LAYOUT_BASE_TABLE_HH
#define HB_OT_LAYOUT_BASE_TABLE_HH

#include "hb-open-type.hh"
#include "hb-ot-layout-common.hh"

namespace OT {

/*
 * BASE -- Baseline
 * https://docs.microsoft.com/en-us/typography/opentype/spec/base
 */

struct BaseCoordFormat1
{
  hb_position_t get_coord (hb_font_t *font, hb_direction_t direction) const
  {
    return HB_DIRECTION_IS_HORIZONTAL (direction) ? font->em_scale_y (coordinate) : font->em_scale_x (coordinate);
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    return_trace ((bool) c->serializer->embed (*this));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  protected:
  HBUINT16	format;		/* Format identifier--format = 1 */
  FWORD		coordinate;	/* X or Y value, in design units */
  public:
  DEFINE_SIZE_STATIC (4);
};

struct BaseCoordFormat2
{
  hb_position_t get_coord (hb_font_t *font, hb_direction_t direction) const
  {
    /* TODO */
    return HB_DIRECTION_IS_HORIZONTAL (direction) ? font->em_scale_y (coordinate) : font->em_scale_x (coordinate);
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (*this);
    if (unlikely (!out)) return_trace (false);

    return_trace (c->serializer->check_assign (out->referenceGlyph,
                                               c->plan->glyph_map->get (referenceGlyph),
                                               HB_SERIALIZE_ERROR_INT_OVERFLOW));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  protected:
  HBUINT16	format;		/* Format identifier--format = 2 */
  FWORD		coordinate;	/* X or Y value, in design units */
  HBGlyphID16	referenceGlyph;	/* Glyph ID of control glyph */
  HBUINT16	coordPoint;	/* Index of contour point on the
				 * reference glyph */
  public:
  DEFINE_SIZE_STATIC (8);
};

struct BaseCoordFormat3
{
  hb_position_t get_coord (hb_font_t *font,
			   const ItemVariationStore &var_store,
			   hb_direction_t direction) const
  {
    const Device &device = this+deviceTable;

    return HB_DIRECTION_IS_HORIZONTAL (direction)
	 ? font->em_scale_y (coordinate) + device.get_y_delta (font, var_store)
	 : font->em_scale_x (coordinate) + device.get_x_delta (font, var_store);
  }

  void collect_variation_indices (hb_set_t& varidx_set /* OUT */) const
  {
    unsigned varidx = (this+deviceTable).get_variation_index ();
    varidx_set.add (varidx);
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (*this);
    if (unlikely (!out)) return_trace (false);

    if (!c->plan->pinned_at_default)
    {
      unsigned var_idx = (this+deviceTable).get_variation_index ();
      if (var_idx != VarIdx::NO_VARIATION)
      {
        hb_pair_t<unsigned, int> *v;
        if (!c->plan->base_variation_idx_map.has (var_idx, &v))
          return_trace (false);
        
        if (unlikely (!c->serializer->check_assign (out->coordinate, coordinate + hb_second (*v),
                                                    HB_SERIALIZE_ERROR_INT_OVERFLOW)))
          return_trace (false);
      }
    }
    return_trace (out->deviceTable.serialize_copy (c->serializer, deviceTable,
                                                   this, 0,
                                                   hb_serialize_context_t::Head,
                                                   &c->plan->base_variation_idx_map));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this) &&
			  deviceTable.sanitize (c, this)));
  }

  protected:
  HBUINT16	format;		/* Format identifier--format = 3 */
  FWORD		coordinate;	/* X or Y value, in design units */
  Offset16To<Device>
		deviceTable;	/* Offset to Device table for X or
				 * Y value, from beginning of
				 * BaseCoord table (may be NULL). */
  public:
  DEFINE_SIZE_STATIC (6);
};

struct BaseCoord
{
  bool has_data () const { return u.format; }

  hb_position_t get_coord (hb_font_t            *font,
			   const ItemVariationStore &var_store,
			   hb_direction_t        direction) const
  {
    switch (u.format) {
    case 1: hb_barrier (); return u.format1.get_coord (font, direction);
    case 2: hb_barrier (); return u.format2.get_coord (font, direction);
    case 3: hb_barrier (); return u.format3.get_coord (font, var_store, direction);
    default:return 0;
    }
  }

  void collect_variation_indices (hb_set_t& varidx_set /* OUT */) const
  {
    switch (u.format) {
    case 3: hb_barrier (); u.format3.collect_variation_indices (varidx_set);
    default:return;
    }
  }

  template <typename context_t, typename ...Ts>
  typename context_t::return_t dispatch (context_t *c, Ts&&... ds) const
  {
    if (unlikely (!c->may_dispatch (this, &u.format))) return c->no_dispatch_return_value ();
    TRACE_DISPATCH (this, u.format);
    switch (u.format) {
    case 1: hb_barrier (); return_trace (c->dispatch (u.format1, std::forward<Ts> (ds)...));
    case 2: hb_barrier (); return_trace (c->dispatch (u.format2, std::forward<Ts> (ds)...));
    case 3: hb_barrier (); return_trace (c->dispatch (u.format3, std::forward<Ts> (ds)...));
    default:return_trace (c->default_return_value ());
    }
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!u.format.sanitize (c))) return_trace (false);
    hb_barrier ();
    switch (u.format) {
    case 1: hb_barrier (); return_trace (u.format1.sanitize (c));
    case 2: hb_barrier (); return_trace (u.format2.sanitize (c));
    case 3: hb_barrier (); return_trace (u.format3.sanitize (c));
    default:return_trace (false);
    }
  }

  protected:
  union {
  HBUINT16		format;
  BaseCoordFormat1	format1;
  BaseCoordFormat2	format2;
  BaseCoordFormat3	format3;
  } u;
  public:
  DEFINE_SIZE_UNION (2, format);
};

struct FeatMinMaxRecord
{
  int cmp (hb_tag_t key) const { return tag.cmp (key); }

  bool has_data () const { return tag; }

  hb_tag_t get_feature_tag () const { return tag; }

  void get_min_max (const BaseCoord **min, const BaseCoord **max) const
  {
    if (likely (min)) *min = &(this+minCoord);
    if (likely (max)) *max = &(this+maxCoord);
  }

  void collect_variation_indices (const hb_subset_plan_t* plan,
                                  const void *base,
                                  hb_set_t& varidx_set /* OUT */) const
  {
    if (!plan->layout_features.has (tag))
      return;

    (base+minCoord).collect_variation_indices (varidx_set);
    (base+maxCoord).collect_variation_indices (varidx_set);
  }

  bool subset (hb_subset_context_t *c,
               const void *base) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (*this);
    if (unlikely (!out)) return_trace (false);
    if (!(out->minCoord.serialize_subset (c, minCoord, base)))
      return_trace (false);

    return_trace (out->maxCoord.serialize_subset (c, maxCoord, base));
  }

  bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this) &&
			  minCoord.sanitize (c, base) &&
			  maxCoord.sanitize (c, base)));
  }

  protected:
  Tag		tag;		/* 4-byte feature identification tag--must
				 * match feature tag in FeatureList */
  Offset16To<BaseCoord>
		minCoord;	/* Offset to BaseCoord table that defines
				 * the minimum extent value, from beginning
				 * of MinMax table (may be NULL) */
  Offset16To<BaseCoord>
		maxCoord;	/* Offset to BaseCoord table that defines
				 * the maximum extent value, from beginning
				 * of MinMax table (may be NULL) */
  public:
  DEFINE_SIZE_STATIC (8);
};

struct MinMax
{
  void get_min_max (hb_tag_t          feature_tag,
		    const BaseCoord **min,
		    const BaseCoord **max) const
  {
    const FeatMinMaxRecord &minMaxCoord = featMinMaxRecords.bsearch (feature_tag);
    if (minMaxCoord.has_data ())
      minMaxCoord.get_min_max (min, max);
    else
    {
      if (likely (min)) *min = &(this+minCoord);
      if (likely (max)) *max = &(this+maxCoord);
    }
  }

  void collect_variation_indices (const hb_subset_plan_t* plan,
                                  hb_set_t& varidx_set /* OUT */) const
  {
    (this+minCoord).collect_variation_indices (varidx_set);
    (this+maxCoord).collect_variation_indices (varidx_set);
    for (const FeatMinMaxRecord& record : featMinMaxRecords)
      record.collect_variation_indices (plan, this, varidx_set);
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!out || !c->serializer->extend_min (out))) return_trace (false);

    if (!(out->minCoord.serialize_subset (c, minCoord, this)) ||
        !(out->maxCoord.serialize_subset (c, maxCoord, this)))
      return_trace (false);

    unsigned len = 0;
    for (const FeatMinMaxRecord& _ : featMinMaxRecords)
    {
      hb_tag_t feature_tag = _.get_feature_tag ();
      if (!c->plan->layout_features.has (feature_tag))
        continue;

      if (!_.subset (c, this)) return false;
      len++;
    }
    return_trace (c->serializer->check_assign (out->featMinMaxRecords.len, len,
                                               HB_SERIALIZE_ERROR_INT_OVERFLOW));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this) &&
			  minCoord.sanitize (c, this) &&
			  maxCoord.sanitize (c, this) &&
			  featMinMaxRecords.sanitize (c, this)));
  }

  protected:
  Offset16To<BaseCoord>
		minCoord;	/* Offset to BaseCoord table that defines
				 * minimum extent value, from the beginning
				 * of MinMax table (may be NULL) */
  Offset16To<BaseCoord>
		maxCoord;	/* Offset to BaseCoord table that defines
				 * maximum extent value, from the beginning
				 * of MinMax table (may be NULL) */
  SortedArray16Of<FeatMinMaxRecord>
		featMinMaxRecords;
				/* Array of FeatMinMaxRecords, in alphabetical
				 * order by featureTableTag */
  public:
  DEFINE_SIZE_ARRAY (6, featMinMaxRecords);
};

struct BaseValues
{
  const BaseCoord &get_base_coord (int baseline_tag_index) const
  {
    if (baseline_tag_index == -1) baseline_tag_index = defaultIndex;
    return this+baseCoords[baseline_tag_index];
  }

  void collect_variation_indices (hb_set_t& varidx_set /* OUT */) const
  {
    for (const auto& _ : baseCoords)
      (this+_).collect_variation_indices (varidx_set);
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!out || !c->serializer->extend_min (out))) return_trace (false);
    out->defaultIndex = defaultIndex;

    for (const auto& _ : baseCoords)
      if (!subset_offset_array (c, out->baseCoords, this) (_))
        return_trace (false);

    return_trace (bool (out->baseCoords));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this) &&
			  baseCoords.sanitize (c, this)));
  }

  protected:
  Index		defaultIndex;	/* Index number of default baseline for this
				 * script — equals index position of baseline tag
				 * in baselineTags array of the BaseTagList */
  Array16OfOffset16To<BaseCoord>
		baseCoords;	/* Number of BaseCoord tables defined — should equal
				 * baseTagCount in the BaseTagList
				 *
				 * Array of offsets to BaseCoord tables, from beginning of
				 * BaseValues table — order matches baselineTags array in
				 * the BaseTagList */
  public:
  DEFINE_SIZE_ARRAY (4, baseCoords);
};

struct BaseLangSysRecord
{
  int cmp (hb_tag_t key) const { return baseLangSysTag.cmp (key); }

  bool has_data () const { return baseLangSysTag; }

  const MinMax &get_min_max (const void* base) const { return base+minMax; }

  void collect_variation_indices (const void* base,
                                  const hb_subset_plan_t* plan,
                                  hb_set_t& varidx_set /* OUT */) const
  { (base+minMax).collect_variation_indices (plan, varidx_set); }

  bool subset (hb_subset_context_t *c,
               const void *base) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (*this);
    if (unlikely (!out)) return_trace (false);

    return_trace (out->minMax.serialize_subset (c, minMax, base));
  }

  bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this) &&
			  minMax.sanitize (c, base)));
  }

  protected:
  Tag		baseLangSysTag;	/* 4-byte language system identification tag */
  Offset16To<MinMax>
		minMax;		/* Offset to MinMax table, from beginning
				 * of BaseScript table */
  public:
  DEFINE_SIZE_STATIC (6);
};

struct BaseScript
{
  const MinMax &get_min_max (hb_tag_t language_tag) const
  {
    const BaseLangSysRecord& record = baseLangSysRecords.bsearch (language_tag);
    return record.has_data () ? record.get_min_max (this) : this+defaultMinMax;
  }

  const BaseCoord &get_base_coord (int baseline_tag_index) const
  { return (this+baseValues).get_base_coord (baseline_tag_index); }

  bool has_values () const { return baseValues; }
  bool has_min_max () const { return defaultMinMax; /* TODO What if only per-language is present? */ }

  void collect_variation_indices (const hb_subset_plan_t* plan,
                                  hb_set_t& varidx_set /* OUT */) const
  {
    (this+baseValues).collect_variation_indices (varidx_set);
    (this+defaultMinMax).collect_variation_indices (plan, varidx_set);
    
    for (const BaseLangSysRecord& _ : baseLangSysRecords)
      _.collect_variation_indices (this, plan, varidx_set);
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!out || !c->serializer->extend_min (out))) return_trace (false);

    if (baseValues && !out->baseValues.serialize_subset (c, baseValues, this))
      return_trace (false);

    if (defaultMinMax && !out->defaultMinMax.serialize_subset (c, defaultMinMax, this))
      return_trace (false);

    for (const auto& _ : baseLangSysRecords)
      if (!_.subset (c, this)) return_trace (false);

    return_trace (c->serializer->check_assign (out->baseLangSysRecords.len, baseLangSysRecords.len,
                                               HB_SERIALIZE_ERROR_INT_OVERFLOW));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this) &&
			  baseValues.sanitize (c, this) &&
			  defaultMinMax.sanitize (c, this) &&
			  baseLangSysRecords.sanitize (c, this)));
  }

  protected:
  Offset16To<BaseValues>
		baseValues;	/* Offset to BaseValues table, from beginning
				 * of BaseScript table (may be NULL) */
  Offset16To<MinMax>
		defaultMinMax;	/* Offset to MinMax table, from beginning of
				 * BaseScript table (may be NULL) */
  SortedArray16Of<BaseLangSysRecord>
		baseLangSysRecords;
				/* Number of BaseLangSysRecords
				 * defined — may be zero (0) */

  public:
  DEFINE_SIZE_ARRAY (6, baseLangSysRecords);
};

struct BaseScriptList;
struct BaseScriptRecord
{
  int cmp (hb_tag_t key) const { return baseScriptTag.cmp (key); }

  bool has_data () const { return baseScriptTag; }

  hb_tag_t get_script_tag () const { return baseScriptTag; }

  const BaseScript &get_base_script (const BaseScriptList *list) const
  { return list+baseScript; }

  void collect_variation_indices (const hb_subset_plan_t* plan,
                                  const void* list,
                                  hb_set_t& varidx_set /* OUT */) const
  {
    if (!plan->layout_scripts.has (baseScriptTag))
      return;

    (list+baseScript).collect_variation_indices (plan, varidx_set);
  }

  bool subset (hb_subset_context_t *c,
               const void *base) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (*this);
    if (unlikely (!out)) return_trace (false);

    return_trace (out->baseScript.serialize_subset (c, baseScript, base));
  }

  bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this) &&
			  baseScript.sanitize (c, base)));
  }

  protected:
  Tag		baseScriptTag;	/* 4-byte script identification tag */
  Offset16To<BaseScript>
		baseScript;	/* Offset to BaseScript table, from beginning
				 * of BaseScriptList */

  public:
  DEFINE_SIZE_STATIC (6);
};

struct BaseScriptList
{
  const BaseScript &get_base_script (hb_tag_t script) const
  {
    const BaseScriptRecord *record = &baseScriptRecords.bsearch (script);
    if (!record->has_data ()) record = &baseScriptRecords.bsearch (HB_TAG ('D','F','L','T'));
    return record->has_data () ? record->get_base_script (this) : Null (BaseScript);
  }

  void collect_variation_indices (const hb_subset_plan_t* plan,
                                  hb_set_t& varidx_set /* OUT */) const
  {
    for (const BaseScriptRecord& _ : baseScriptRecords)
      _.collect_variation_indices (plan, this, varidx_set);
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!out || !c->serializer->extend_min (out))) return_trace (false);

    unsigned len = 0;
    for (const BaseScriptRecord& _ : baseScriptRecords)
    {
      hb_tag_t script_tag = _.get_script_tag ();
      if (!c->plan->layout_scripts.has (script_tag))
        continue;

      if (!_.subset (c, this)) return false;
      len++;
    }
    return_trace (c->serializer->check_assign (out->baseScriptRecords.len, len,
                                               HB_SERIALIZE_ERROR_INT_OVERFLOW));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  baseScriptRecords.sanitize (c, this));
  }

  protected:
  SortedArray16Of<BaseScriptRecord>
			baseScriptRecords;

  public:
  DEFINE_SIZE_ARRAY (2, baseScriptRecords);
};

struct Axis
{
  bool get_baseline (hb_tag_t          baseline_tag,
		     hb_tag_t          script_tag,
		     hb_tag_t          language_tag,
		     const BaseCoord **coord) const
  {
    const BaseScript &base_script = (this+baseScriptList).get_base_script (script_tag);
    if (!base_script.has_values ())
    {
      *coord = nullptr;
      return false;
    }

    if (likely (coord))
    {
      unsigned int tag_index = 0;
      if (!(this+baseTagList).bfind (baseline_tag, &tag_index))
      {
        *coord = nullptr;
        return false;
      }
      *coord = &base_script.get_base_coord (tag_index);
    }

    return true;
  }

  bool get_min_max (hb_tag_t          script_tag,
		    hb_tag_t          language_tag,
		    hb_tag_t          feature_tag,
		    const BaseCoord **min_coord,
		    const BaseCoord **max_coord) const
  {
    const BaseScript &base_script = (this+baseScriptList).get_base_script (script_tag);
    if (!base_script.has_min_max ())
    {
      *min_coord = *max_coord = nullptr;
      return false;
    }

    base_script.get_min_max (language_tag).get_min_max (feature_tag, min_coord, max_coord);

    return true;
  }

  void collect_variation_indices (const hb_subset_plan_t* plan,
                                  hb_set_t& varidx_set /* OUT */) const
  { (this+baseScriptList).collect_variation_indices (plan, varidx_set); }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (*this);
    if (unlikely (!out)) return_trace (false);

    out->baseTagList.serialize_copy (c->serializer, baseTagList, this);
    return_trace (out->baseScriptList.serialize_subset (c, baseScriptList, this));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this) &&
			  baseTagList.sanitize (c, this) &&
			  baseScriptList.sanitize (c, this)));
  }

  protected:
  Offset16To<SortedArray16Of<Tag>>
		baseTagList;	/* Offset to BaseTagList table, from beginning
				 * of Axis table (may be NULL)
				 * Array of 4-byte baseline identification tags — must
				 * be in alphabetical order */
  Offset16To<BaseScriptList>
		baseScriptList;	/* Offset to BaseScriptList table, from beginning
				 * of Axis table
				 * Array of BaseScriptRecords, in alphabetical order
				 * by baseScriptTag */

  public:
  DEFINE_SIZE_STATIC (4);
};

struct BASE
{
  static constexpr hb_tag_t tableTag = HB_OT_TAG_BASE;

  const Axis &get_axis (hb_direction_t direction) const
  { return HB_DIRECTION_IS_VERTICAL (direction) ? this+vAxis : this+hAxis; }

  bool has_var_store () const
  { return version.to_int () >= 0x00010001u && varStore != 0; }

  const ItemVariationStore &get_var_store () const
  { return version.to_int () < 0x00010001u ? Null (ItemVariationStore) : this+varStore; }

  void collect_variation_indices (const hb_subset_plan_t* plan,
                                  hb_set_t& varidx_set /* OUT */) const
  {
    (this+hAxis).collect_variation_indices (plan, varidx_set);
    (this+vAxis).collect_variation_indices (plan, varidx_set);
  }

  bool subset_varstore (hb_subset_context_t *c,
                        BASE *out /* OUT */) const
  {
    TRACE_SUBSET (this);
    if (!c->serializer->allocate_size<Offset32To<ItemVariationStore>> (Offset32To<ItemVariationStore>::static_size))
        return_trace (false);
    if (!c->plan->normalized_coords)
      return_trace (out->varStore.serialize_subset (c, varStore, this, c->plan->base_varstore_inner_maps.as_array ()));

    if (c->plan->all_axes_pinned)
      return_trace (true);

    item_variations_t item_vars;
    if (!item_vars.instantiate (this+varStore, c->plan, true, true,
                                c->plan->base_varstore_inner_maps.as_array ()))
      return_trace (false);

    if (!out->varStore.serialize_serialize (c->serializer,
                                            item_vars.has_long_word (),
                                            c->plan->axis_tags,
                                            item_vars.get_region_list (),
                                            item_vars.get_vardata_encodings ()))
      return_trace (false);

    const hb_map_t &varidx_map = item_vars.get_varidx_map ();
    /* base_variation_idx_map in the plan is old_varidx->(varidx, delta)
     * mapping, new varidx is generated for subsetting, we need to remap this
     * after instancing */
    for (auto _ : c->plan->base_variation_idx_map.iter_ref ())
    {
      uint32_t varidx = _.second.first;
      uint32_t *new_varidx;
      if (varidx_map.has (varidx, &new_varidx))
        _.second.first = *new_varidx;
      else
        _.second.first = HB_OT_LAYOUT_NO_VARIATIONS_INDEX;
    }
    return_trace (true);
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!out || !c->serializer->extend_min (out))) return_trace (false);

    out->version = version;
    if (has_var_store () && !subset_varstore (c, out))
        return_trace (false);

    if (hAxis && !out->hAxis.serialize_subset (c, hAxis, this))
      return_trace (false);

    if (vAxis && !out->vAxis.serialize_subset (c, vAxis, this))
      return_trace (false);

    return_trace (true);
  }

  bool get_baseline (hb_font_t      *font,
		     hb_tag_t        baseline_tag,
		     hb_direction_t  direction,
		     hb_tag_t        script_tag,
		     hb_tag_t        language_tag,
		     hb_position_t  *base) const
  {
    const BaseCoord *base_coord = nullptr;
    if (unlikely (!get_axis (direction).get_baseline (baseline_tag, script_tag, language_tag, &base_coord) ||
		  !base_coord || !base_coord->has_data ()))
      return false;

    if (likely (base))
      *base = base_coord->get_coord (font, get_var_store (), direction);

    return true;
  }

  bool get_min_max (hb_font_t      *font,
		    hb_direction_t  direction,
		    hb_tag_t        script_tag,
		    hb_tag_t        language_tag,
		    hb_tag_t        feature_tag,
		    hb_position_t  *min,
		    hb_position_t  *max) const
  {
    const BaseCoord *min_coord, *max_coord;
    if (!get_axis (direction).get_min_max (script_tag, language_tag, feature_tag,
					   &min_coord, &max_coord))
      return false;

    const ItemVariationStore &var_store = get_var_store ();
    if (likely (min && min_coord)) *min = min_coord->get_coord (font, var_store, direction);
    if (likely (max && max_coord)) *max = max_coord->get_coord (font, var_store, direction);
    return true;
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this) &&
			  hb_barrier () &&
			  likely (version.major == 1) &&
			  hAxis.sanitize (c, this) &&
			  vAxis.sanitize (c, this) &&
			  (version.to_int () < 0x00010001u || varStore.sanitize (c, this))));
  }

  protected:
  FixedVersion<>version;	/* Version of the BASE table */
  Offset16To<Axis>hAxis;		/* Offset to horizontal Axis table, from beginning
				 * of BASE table (may be NULL) */
  Offset16To<Axis>vAxis;		/* Offset to vertical Axis table, from beginning
				 * of BASE table (may be NULL) */
  Offset32To<ItemVariationStore>
		varStore;	/* Offset to the table of Item Variation
				 * Store--from beginning of BASE
				 * header (may be NULL).  Introduced
				 * in version 0x00010001. */
  public:
  DEFINE_SIZE_MIN (8);
};


} /* namespace OT */


#endif /* HB_OT_LAYOUT_BASE_TABLE_HH */
