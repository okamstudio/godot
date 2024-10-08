/*
 * Copyright © 2007,2008,2009  Red Hat, Inc.
 * Copyright © 2010,2012  Google, Inc.
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

#ifndef HB_OT_LAYOUT_COMMON_HH
#define HB_OT_LAYOUT_COMMON_HH

#include "hb.hh"
#include "hb-ot-layout.hh"
#include "hb-open-type.hh"
#include "hb-set.hh"
#include "hb-bimap.hh"

#include "OT/Layout/Common/Coverage.hh"
#include "OT/Layout/types.hh"

// TODO(garretrieger): cleanup these after migration.
using OT::Layout::Common::Coverage;
using OT::Layout::Common::RangeRecord;
using OT::Layout::SmallTypes;
using OT::Layout::MediumTypes;


namespace OT {

template<typename Iterator>
static inline bool ClassDef_serialize (hb_serialize_context_t *c,
				       Iterator it);

static bool ClassDef_remap_and_serialize (
    hb_serialize_context_t *c,
    const hb_set_t &klasses,
    bool use_class_zero,
    hb_sorted_vector_t<hb_codepoint_pair_t> &glyph_and_klass, /* IN/OUT */
    hb_map_t *klass_map /*IN/OUT*/);

struct hb_collect_feature_substitutes_with_var_context_t
{
  const hb_map_t *axes_index_tag_map;
  const hb_hashmap_t<hb_tag_t, Triple> *axes_location;
  hb_hashmap_t<unsigned, hb::shared_ptr<hb_set_t>> *record_cond_idx_map;
  hb_hashmap_t<unsigned, const Feature*> *feature_substitutes_map;
  hb_set_t& catch_all_record_feature_idxes;

  // not stored in subset_plan
  hb_set_t *feature_indices;
  bool apply;
  bool variation_applied;
  bool universal;
  unsigned cur_record_idx;
  hb_hashmap_t<hb::shared_ptr<hb_map_t>, unsigned> *conditionset_map;
};

struct hb_prune_langsys_context_t
{
  hb_prune_langsys_context_t (const void         *table_,
                              hb_hashmap_t<unsigned, hb::unique_ptr<hb_set_t>> *script_langsys_map_,
                              const hb_map_t     *duplicate_feature_map_,
                              hb_set_t           *new_collected_feature_indexes_)
      :table (table_),
      script_langsys_map (script_langsys_map_),
      duplicate_feature_map (duplicate_feature_map_),
      new_feature_indexes (new_collected_feature_indexes_),
      script_count (0),langsys_feature_count (0) {}

  bool visitScript ()
  { return script_count++ < HB_MAX_SCRIPTS; }

  bool visitLangsys (unsigned feature_count)
  {
    langsys_feature_count += feature_count;
    return langsys_feature_count < HB_MAX_LANGSYS_FEATURE_COUNT;
  }

  public:
  const void *table;
  hb_hashmap_t<unsigned, hb::unique_ptr<hb_set_t>> *script_langsys_map;
  const hb_map_t     *duplicate_feature_map;
  hb_set_t           *new_feature_indexes;

  private:
  unsigned script_count;
  unsigned langsys_feature_count;
};

struct hb_subset_layout_context_t :
  hb_dispatch_context_t<hb_subset_layout_context_t, hb_empty_t, HB_DEBUG_SUBSET>
{
  const char *get_name () { return "SUBSET_LAYOUT"; }
  static return_t default_return_value () { return hb_empty_t (); }

  bool visitScript ()
  {
    return script_count++ < HB_MAX_SCRIPTS;
  }

  bool visitLangSys ()
  {
    return langsys_count++ < HB_MAX_LANGSYS;
  }

  bool visitFeatureIndex (int count)
  {
    feature_index_count += count;
    return feature_index_count < HB_MAX_FEATURE_INDICES;
  }

  bool visitLookupIndex()
  {
    lookup_index_count++;
    return lookup_index_count < HB_MAX_LOOKUP_VISIT_COUNT;
  }

  hb_subset_context_t *subset_context;
  const hb_tag_t table_tag;
  const hb_map_t *lookup_index_map;
  const hb_hashmap_t<unsigned, hb::unique_ptr<hb_set_t>> *script_langsys_map;
  const hb_map_t *feature_index_map;
  const hb_hashmap_t<unsigned, const Feature*> *feature_substitutes_map;
  hb_hashmap_t<unsigned, hb::shared_ptr<hb_set_t>> *feature_record_cond_idx_map;
  const hb_set_t *catch_all_record_feature_idxes;
  const hb_hashmap_t<unsigned, hb_pair_t<const void*, const void*>> *feature_idx_tag_map;

  unsigned cur_script_index;
  unsigned cur_feature_var_record_idx;

  hb_subset_layout_context_t (hb_subset_context_t *c_,
			      hb_tag_t tag_) :
				subset_context (c_),
				table_tag (tag_),
				cur_script_index (0xFFFFu),
				cur_feature_var_record_idx (0u),
				script_count (0),
				langsys_count (0),
				feature_index_count (0),
				lookup_index_count (0)
  {
    if (tag_ == HB_OT_TAG_GSUB)
    {
      lookup_index_map = &c_->plan->gsub_lookups;
      script_langsys_map = &c_->plan->gsub_langsys;
      feature_index_map = &c_->plan->gsub_features;
      feature_substitutes_map = &c_->plan->gsub_feature_substitutes_map;
      feature_record_cond_idx_map = c_->plan->user_axes_location.is_empty () ? nullptr : &c_->plan->gsub_feature_record_cond_idx_map;
      catch_all_record_feature_idxes = &c_->plan->gsub_old_features;
      feature_idx_tag_map = &c_->plan->gsub_old_feature_idx_tag_map;
    }
    else
    {
      lookup_index_map = &c_->plan->gpos_lookups;
      script_langsys_map = &c_->plan->gpos_langsys;
      feature_index_map = &c_->plan->gpos_features;
      feature_substitutes_map = &c_->plan->gpos_feature_substitutes_map;
      feature_record_cond_idx_map = c_->plan->user_axes_location.is_empty () ? nullptr : &c_->plan->gpos_feature_record_cond_idx_map;
      catch_all_record_feature_idxes = &c_->plan->gpos_old_features;
      feature_idx_tag_map = &c_->plan->gpos_old_feature_idx_tag_map;
    }
  }

  private:
  unsigned script_count;
  unsigned langsys_count;
  unsigned feature_index_count;
  unsigned lookup_index_count;
};

struct ItemVariationStore;
struct hb_collect_variation_indices_context_t :
       hb_dispatch_context_t<hb_collect_variation_indices_context_t>
{
  template <typename T>
  return_t dispatch (const T &obj) { obj.collect_variation_indices (this); return hb_empty_t (); }
  static return_t default_return_value () { return hb_empty_t (); }

  hb_set_t *layout_variation_indices;
  const hb_set_t *glyph_set;
  const hb_map_t *gpos_lookups;

  hb_collect_variation_indices_context_t (hb_set_t *layout_variation_indices_,
					  const hb_set_t *glyph_set_,
					  const hb_map_t *gpos_lookups_) :
					layout_variation_indices (layout_variation_indices_),
					glyph_set (glyph_set_),
					gpos_lookups (gpos_lookups_) {}
};

template<typename OutputArray>
struct subset_offset_array_t
{
  subset_offset_array_t (hb_subset_context_t *subset_context_,
			 OutputArray& out_,
			 const void *base_) : subset_context (subset_context_),
					      out (out_), base (base_) {}

  template <typename T>
  bool operator () (T&& offset)
  {
    auto snap = subset_context->serializer->snapshot ();
    auto *o = out.serialize_append (subset_context->serializer);
    if (unlikely (!o)) return false;
    bool ret = o->serialize_subset (subset_context, offset, base);
    if (!ret)
    {
      out.pop ();
      subset_context->serializer->revert (snap);
    }
    return ret;
  }

  private:
  hb_subset_context_t *subset_context;
  OutputArray &out;
  const void *base;
};


template<typename OutputArray, typename Arg>
struct subset_offset_array_arg_t
{
  subset_offset_array_arg_t (hb_subset_context_t *subset_context_,
			     OutputArray& out_,
			     const void *base_,
			     Arg &&arg_) : subset_context (subset_context_), out (out_),
					  base (base_), arg (arg_) {}

  template <typename T>
  bool operator () (T&& offset)
  {
    auto snap = subset_context->serializer->snapshot ();
    auto *o = out.serialize_append (subset_context->serializer);
    if (unlikely (!o)) return false;
    bool ret = o->serialize_subset (subset_context, offset, base, arg);
    if (!ret)
    {
      out.pop ();
      subset_context->serializer->revert (snap);
    }
    return ret;
  }

  private:
  hb_subset_context_t *subset_context;
  OutputArray &out;
  const void *base;
  Arg &&arg;
};

/*
 * Helper to subset an array of offsets. Subsets the thing pointed to by each offset
 * and discards the offset in the array if the subset operation results in an empty
 * thing.
 */
struct
{
  template<typename OutputArray>
  subset_offset_array_t<OutputArray>
  operator () (hb_subset_context_t *subset_context, OutputArray& out,
	       const void *base) const
  { return subset_offset_array_t<OutputArray> (subset_context, out, base); }

  /* Variant with one extra argument passed to serialize_subset */
  template<typename OutputArray, typename Arg>
  subset_offset_array_arg_t<OutputArray, Arg>
  operator () (hb_subset_context_t *subset_context, OutputArray& out,
	       const void *base, Arg &&arg) const
  { return subset_offset_array_arg_t<OutputArray, Arg> (subset_context, out, base, arg); }
}
HB_FUNCOBJ (subset_offset_array);

template<typename OutputArray>
struct subset_record_array_t
{
  subset_record_array_t (hb_subset_layout_context_t *c_, OutputArray* out_,
			 const void *base_) : subset_layout_context (c_),
					      out (out_), base (base_) {}

  template <typename T>
  void
  operator () (T&& record)
  {
    auto snap = subset_layout_context->subset_context->serializer->snapshot ();
    bool ret = record.subset (subset_layout_context, base);
    if (!ret) subset_layout_context->subset_context->serializer->revert (snap);
    else out->len++;
  }

  private:
  hb_subset_layout_context_t *subset_layout_context;
  OutputArray *out;
  const void *base;
};

template<typename OutputArray, typename Arg>
struct subset_record_array_arg_t
{
  subset_record_array_arg_t (hb_subset_layout_context_t *c_, OutputArray* out_,
			     const void *base_,
			     Arg &&arg_) : subset_layout_context (c_),
					   out (out_), base (base_), arg (arg_) {}

  template <typename T>
  void
  operator () (T&& record)
  {
    auto snap = subset_layout_context->subset_context->serializer->snapshot ();
    bool ret = record.subset (subset_layout_context, base, arg);
    if (!ret) subset_layout_context->subset_context->serializer->revert (snap);
    else out->len++;
  }

  private:
  hb_subset_layout_context_t *subset_layout_context;
  OutputArray *out;
  const void *base;
  Arg &&arg;
};

/*
 * Helper to subset a RecordList/record array. Subsets each Record in the array and
 * discards the record if the subset operation returns false.
 */
struct
{
  template<typename OutputArray>
  subset_record_array_t<OutputArray>
  operator () (hb_subset_layout_context_t *c, OutputArray* out,
	       const void *base) const
  { return subset_record_array_t<OutputArray> (c, out, base); }

  /* Variant with one extra argument passed to subset */
  template<typename OutputArray, typename Arg>
  subset_record_array_arg_t<OutputArray, Arg>
  operator () (hb_subset_layout_context_t *c, OutputArray* out,
               const void *base, Arg &&arg) const
  { return subset_record_array_arg_t<OutputArray, Arg> (c, out, base, arg); }
}
HB_FUNCOBJ (subset_record_array);


template<typename OutputArray>
struct serialize_math_record_array_t
{
  serialize_math_record_array_t (hb_serialize_context_t *serialize_context_,
                         OutputArray& out_,
                         const void *base_) : serialize_context (serialize_context_),
                                              out (out_), base (base_) {}

  template <typename T>
  bool operator () (T&& record)
  {
    if (!serialize_context->copy (record, base)) return false;
    out.len++;
    return true;
  }

  private:
  hb_serialize_context_t *serialize_context;
  OutputArray &out;
  const void *base;
};

/*
 * Helper to serialize an array of MATH records.
 */
struct
{
  template<typename OutputArray>
  serialize_math_record_array_t<OutputArray>
  operator () (hb_serialize_context_t *serialize_context, OutputArray& out,
               const void *base) const
  { return serialize_math_record_array_t<OutputArray> (serialize_context, out, base); }

}
HB_FUNCOBJ (serialize_math_record_array);

/*
 *
 * OpenType Layout Common Table Formats
 *
 */


/*
 * Script, ScriptList, LangSys, Feature, FeatureList, Lookup, LookupList
 */

struct IndexArray : Array16Of<Index>
{
  bool intersects (const hb_map_t *indexes) const
  { return hb_any (*this, indexes); }

  template <typename Iterator,
	    hb_requires (hb_is_iterator (Iterator))>
  void serialize (hb_serialize_context_t *c,
		  hb_subset_layout_context_t *l,
		  Iterator it)
  {
    if (!it) return;
    if (unlikely (!c->extend_min ((*this)))) return;

    for (const auto _ : it)
    {
      if (!l->visitLookupIndex()) break;

      Index i;
      i = _;
      c->copy (i);
      this->len++;
    }
  }

  unsigned int get_indexes (unsigned int start_offset,
			    unsigned int *_count /* IN/OUT */,
			    unsigned int *_indexes /* OUT */) const
  {
    if (_count)
    {
      + this->as_array ().sub_array (start_offset, _count)
      | hb_sink (hb_array (_indexes, *_count))
      ;
    }
    return this->len;
  }

  void add_indexes_to (hb_set_t* output /* OUT */) const
  {
    output->add_array (as_array ());
  }
};


/* https://docs.microsoft.com/en-us/typography/opentype/spec/features_pt#size */
struct FeatureParamsSize
{
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!c->check_struct (this))) return_trace (false);
    hb_barrier ();

    /* This subtable has some "history", if you will.  Some earlier versions of
     * Adobe tools calculated the offset of the FeatureParams subtable from the
     * beginning of the FeatureList table!  Now, that is dealt with in the
     * Feature implementation.  But we still need to be able to tell junk from
     * real data.  Note: We don't check that the nameID actually exists.
     *
     * Read Roberts wrote on 9/15/06 on opentype-list@indx.co.uk :
     *
     * Yes, it is correct that a new version of the AFDKO (version 2.0) will be
     * coming out soon, and that the makeotf program will build a font with a
     * 'size' feature that is correct by the specification.
     *
     * The specification for this feature tag is in the "OpenType Layout Tag
     * Registry". You can see a copy of this at:
     * https://docs.microsoft.com/en-us/typography/opentype/spec/features_pt#tag-size
     *
     * Here is one set of rules to determine if the 'size' feature is built
     * correctly, or as by the older versions of MakeOTF. You may be able to do
     * better.
     *
     * Assume that the offset to the size feature is according to specification,
     * and make the following value checks. If it fails, assume the size
     * feature is calculated as versions of MakeOTF before the AFDKO 2.0 built it.
     * If this fails, reject the 'size' feature. The older makeOTF's calculated the
     * offset from the beginning of the FeatureList table, rather than from the
     * beginning of the 'size' Feature table.
     *
     * If "design size" == 0:
     *     fails check
     *
     * Else if ("subfamily identifier" == 0 and
     *     "range start" == 0 and
     *     "range end" == 0 and
     *     "range start" == 0 and
     *     "menu name ID" == 0)
     *     passes check: this is the format used when there is a design size
     * specified, but there is no recommended size range.
     *
     * Else if ("design size" <  "range start" or
     *     "design size" >   "range end" or
     *     "range end" <= "range start" or
     *     "menu name ID"  < 256 or
     *     "menu name ID"  > 32767 or
     *     menu name ID is not a name ID which is actually in the name table)
     *     fails test
     * Else
     *     passes test.
     */

    if (!designSize)
      return_trace (false);
    else if (subfamilyID == 0 &&
	     subfamilyNameID == 0 &&
	     rangeStart == 0 &&
	     rangeEnd == 0)
      return_trace (true);
    else if (designSize < rangeStart ||
	     designSize > rangeEnd ||
	     subfamilyNameID < 256 ||
	     subfamilyNameID > 32767)
      return_trace (false);
    else
      return_trace (true);
  }

  void collect_name_ids (hb_set_t *nameids_to_retain /* OUT */) const
  { nameids_to_retain->add (subfamilyNameID); }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    return_trace ((bool) c->serializer->embed (*this));
  }

  HBUINT16	designSize;	/* Represents the design size in 720/inch
				 * units (decipoints).  The design size entry
				 * must be non-zero.  When there is a design
				 * size but no recommended size range, the
				 * rest of the array will consist of zeros. */
  HBUINT16	subfamilyID;	/* Has no independent meaning, but serves
				 * as an identifier that associates fonts
				 * in a subfamily. All fonts which share a
				 * Preferred or Font Family name and which
				 * differ only by size range shall have the
				 * same subfamily value, and no fonts which
				 * differ in weight or style shall have the
				 * same subfamily value. If this value is
				 * zero, the remaining fields in the array
				 * will be ignored. */
  NameID	subfamilyNameID;/* If the preceding value is non-zero, this
				 * value must be set in the range 256 - 32767
				 * (inclusive). It records the value of a
				 * field in the name table, which must
				 * contain English-language strings encoded
				 * in Windows Unicode and Macintosh Roman,
				 * and may contain additional strings
				 * localized to other scripts and languages.
				 * Each of these strings is the name an
				 * application should use, in combination
				 * with the family name, to represent the
				 * subfamily in a menu.  Applications will
				 * choose the appropriate version based on
				 * their selection criteria. */
  HBUINT16	rangeStart;	/* Large end of the recommended usage range
				 * (inclusive), stored in 720/inch units
				 * (decipoints). */
  HBUINT16	rangeEnd;	/* Small end of the recommended usage range
				   (exclusive), stored in 720/inch units
				 * (decipoints). */
  public:
  DEFINE_SIZE_STATIC (10);
};

/* https://docs.microsoft.com/en-us/typography/opentype/spec/features_pt#ssxx */
struct FeatureParamsStylisticSet
{
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    /* Right now minorVersion is at zero.  Which means, any table supports
     * the uiNameID field. */
    return_trace (c->check_struct (this));
  }

  void collect_name_ids (hb_set_t *nameids_to_retain /* OUT */) const
  { nameids_to_retain->add (uiNameID); }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    return_trace ((bool) c->serializer->embed (*this));
  }

  HBUINT16	version;	/* (set to 0): This corresponds to a “minor”
				 * version number. Additional data may be
				 * added to the end of this Feature Parameters
				 * table in the future. */

  NameID	uiNameID;	/* The 'name' table name ID that specifies a
				 * string (or strings, for multiple languages)
				 * for a user-interface label for this
				 * feature.  The values of uiLabelNameId and
				 * sampleTextNameId are expected to be in the
				 * font-specific name ID range (256-32767),
				 * though that is not a requirement in this
				 * Feature Parameters specification. The
				 * user-interface label for the feature can
				 * be provided in multiple languages. An
				 * English string should be included as a
				 * fallback. The string should be kept to a
				 * minimal length to fit comfortably with
				 * different application interfaces. */
  public:
  DEFINE_SIZE_STATIC (4);
};

/* https://docs.microsoft.com/en-us/typography/opentype/spec/features_ae#cv01-cv99 */
struct FeatureParamsCharacterVariants
{
  unsigned
  get_characters (unsigned start_offset, unsigned *char_count, hb_codepoint_t *chars) const
  {
    if (char_count)
    {
      + characters.as_array ().sub_array (start_offset, char_count)
      | hb_sink (hb_array (chars, *char_count))
      ;
    }
    return characters.len;
  }

  unsigned get_size () const
  { return min_size + characters.len * HBUINT24::static_size; }

  void collect_name_ids (hb_set_t *nameids_to_retain /* OUT */) const
  {
    if (featUILableNameID) nameids_to_retain->add (featUILableNameID);
    if (featUITooltipTextNameID) nameids_to_retain->add (featUITooltipTextNameID);
    if (sampleTextNameID) nameids_to_retain->add (sampleTextNameID);

    if (!firstParamUILabelNameID || !numNamedParameters || numNamedParameters >= 0x7FFF)
      return;

    unsigned last_name_id = (unsigned) firstParamUILabelNameID + (unsigned) numNamedParameters - 1;
    nameids_to_retain->add_range (firstParamUILabelNameID, last_name_id);
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    return_trace ((bool) c->serializer->embed (*this));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  characters.sanitize (c));
  }

  HBUINT16	format;			/* Format number is set to 0. */
  NameID	featUILableNameID;	/* The ‘name’ table name ID that
					 * specifies a string (or strings,
					 * for multiple languages) for a
					 * user-interface label for this
					 * feature. (May be NULL.) */
  NameID	featUITooltipTextNameID;/* The ‘name’ table name ID that
					 * specifies a string (or strings,
					 * for multiple languages) that an
					 * application can use for tooltip
					 * text for this feature. (May be
					 * nullptr.) */
  NameID	sampleTextNameID;	/* The ‘name’ table name ID that
					 * specifies sample text that
					 * illustrates the effect of this
					 * feature. (May be NULL.) */
  HBUINT16	numNamedParameters;	/* Number of named parameters. (May
					 * be zero.) */
  NameID	firstParamUILabelNameID;/* The first ‘name’ table name ID
					 * used to specify strings for
					 * user-interface labels for the
					 * feature parameters. (Must be zero
					 * if numParameters is zero.) */
  Array16Of<HBUINT24>
		characters;		/* Array of the Unicode Scalar Value
					 * of the characters for which this
					 * feature provides glyph variants.
					 * (May be zero.) */
  public:
  DEFINE_SIZE_ARRAY (14, characters);
};

struct FeatureParams
{
  bool sanitize (hb_sanitize_context_t *c, hb_tag_t tag) const
  {
#ifdef HB_NO_LAYOUT_FEATURE_PARAMS
    return true;
#endif
    TRACE_SANITIZE (this);
    if (tag == HB_TAG ('s','i','z','e'))
      return_trace (u.size.sanitize (c));
    if ((tag & 0xFFFF0000u) == HB_TAG ('s','s','\0','\0')) /* ssXX */
      return_trace (u.stylisticSet.sanitize (c));
    if ((tag & 0xFFFF0000u) == HB_TAG ('c','v','\0','\0')) /* cvXX */
      return_trace (u.characterVariants.sanitize (c));
    return_trace (true);
  }

  void collect_name_ids (hb_tag_t tag, hb_set_t *nameids_to_retain /* OUT */) const
  {
#ifdef HB_NO_LAYOUT_FEATURE_PARAMS
    return;
#endif
    if (tag == HB_TAG ('s','i','z','e'))
      return (u.size.collect_name_ids (nameids_to_retain));
    if ((tag & 0xFFFF0000u) == HB_TAG ('s','s','\0','\0')) /* ssXX */
      return (u.stylisticSet.collect_name_ids (nameids_to_retain));
    if ((tag & 0xFFFF0000u) == HB_TAG ('c','v','\0','\0')) /* cvXX */
      return (u.characterVariants.collect_name_ids (nameids_to_retain));
  }

  bool subset (hb_subset_context_t *c, const Tag* tag) const
  {
    TRACE_SUBSET (this);
    if (!tag) return_trace (false);
    if (*tag == HB_TAG ('s','i','z','e'))
      return_trace (u.size.subset (c));
    if ((*tag & 0xFFFF0000u) == HB_TAG ('s','s','\0','\0')) /* ssXX */
      return_trace (u.stylisticSet.subset (c));
    if ((*tag & 0xFFFF0000u) == HB_TAG ('c','v','\0','\0')) /* cvXX */
      return_trace (u.characterVariants.subset (c));
    return_trace (false);
  }

#ifndef HB_NO_LAYOUT_FEATURE_PARAMS
  const FeatureParamsSize& get_size_params (hb_tag_t tag) const
  {
    if (tag == HB_TAG ('s','i','z','e'))
      return u.size;
    return Null (FeatureParamsSize);
  }
  const FeatureParamsStylisticSet& get_stylistic_set_params (hb_tag_t tag) const
  {
    if ((tag & 0xFFFF0000u) == HB_TAG ('s','s','\0','\0')) /* ssXX */
      return u.stylisticSet;
    return Null (FeatureParamsStylisticSet);
  }
  const FeatureParamsCharacterVariants& get_character_variants_params (hb_tag_t tag) const
  {
    if ((tag & 0xFFFF0000u) == HB_TAG ('c','v','\0','\0')) /* cvXX */
      return u.characterVariants;
    return Null (FeatureParamsCharacterVariants);
  }
#endif

  private:
  union {
  FeatureParamsSize			size;
  FeatureParamsStylisticSet		stylisticSet;
  FeatureParamsCharacterVariants	characterVariants;
  } u;
  public:
  DEFINE_SIZE_MIN (0);
};

struct Record_sanitize_closure_t {
  hb_tag_t tag;
  const void *list_base;
};

struct Feature
{
  unsigned int get_lookup_count () const
  { return lookupIndex.len; }
  hb_tag_t get_lookup_index (unsigned int i) const
  { return lookupIndex[i]; }
  unsigned int get_lookup_indexes (unsigned int start_index,
				   unsigned int *lookup_count /* IN/OUT */,
				   unsigned int *lookup_tags /* OUT */) const
  { return lookupIndex.get_indexes (start_index, lookup_count, lookup_tags); }
  void add_lookup_indexes_to (hb_set_t *lookup_indexes) const
  { lookupIndex.add_indexes_to (lookup_indexes); }

  const FeatureParams &get_feature_params () const
  { return this+featureParams; }

  bool intersects_lookup_indexes (const hb_map_t *lookup_indexes) const
  { return lookupIndex.intersects (lookup_indexes); }

  void collect_name_ids (hb_tag_t tag, hb_set_t *nameids_to_retain /* OUT */) const
  {
    if (featureParams)
      get_feature_params ().collect_name_ids (tag, nameids_to_retain);
  }

  bool subset (hb_subset_context_t         *c,
	       hb_subset_layout_context_t  *l,
	       const Tag                   *tag = nullptr) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    out->featureParams.serialize_subset (c, featureParams, this, tag);

    auto it =
    + hb_iter (lookupIndex)
    | hb_filter (l->lookup_index_map)
    | hb_map (l->lookup_index_map)
    ;

    out->lookupIndex.serialize (c->serializer, l, it);
    // The decision to keep or drop this feature is already made before we get here
    // so always retain it.
    return_trace (true);
  }

  bool sanitize (hb_sanitize_context_t *c,
		 const Record_sanitize_closure_t *closure = nullptr) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!(c->check_struct (this) && lookupIndex.sanitize (c))))
      return_trace (false);
    hb_barrier ();

    /* Some earlier versions of Adobe tools calculated the offset of the
     * FeatureParams subtable from the beginning of the FeatureList table!
     *
     * If sanitizing "failed" for the FeatureParams subtable, try it with the
     * alternative location.  We would know sanitize "failed" if old value
     * of the offset was non-zero, but it's zeroed now.
     *
     * Only do this for the 'size' feature, since at the time of the faulty
     * Adobe tools, only the 'size' feature had FeatureParams defined.
     */

    if (likely (featureParams.is_null ()))
      return_trace (true);

    unsigned int orig_offset = featureParams;
    if (unlikely (!featureParams.sanitize (c, this, closure ? closure->tag : HB_TAG_NONE)))
      return_trace (false);
    hb_barrier ();

    if (featureParams == 0 && closure &&
	closure->tag == HB_TAG ('s','i','z','e') &&
	closure->list_base && closure->list_base < this)
    {
      unsigned int new_offset_int = orig_offset -
				    (((char *) this) - ((char *) closure->list_base));

      Offset16To<FeatureParams> new_offset;
      /* Check that it would not overflow. */
      new_offset = new_offset_int;
      if (new_offset == new_offset_int &&
	  c->try_set (&featureParams, new_offset_int) &&
	  !featureParams.sanitize (c, this, closure ? closure->tag : HB_TAG_NONE))
	return_trace (false);
    }

    return_trace (true);
  }

  Offset16To<FeatureParams>
		 featureParams;	/* Offset to Feature Parameters table (if one
				 * has been defined for the feature), relative
				 * to the beginning of the Feature Table; = Null
				 * if not required */
  IndexArray	 lookupIndex;	/* Array of LookupList indices */
  public:
  DEFINE_SIZE_ARRAY_SIZED (4, lookupIndex);
};

template <typename Type>
struct Record
{
  int cmp (hb_tag_t a) const { return tag.cmp (a); }

  bool subset (hb_subset_layout_context_t *c, const void *base, const void *f_sub = nullptr) const
  {
    TRACE_SUBSET (this);
    auto *out = c->subset_context->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    if (!f_sub)
      return_trace (out->offset.serialize_subset (c->subset_context, offset, base, c, &tag));

    const Feature& f = *reinterpret_cast<const Feature *> (f_sub);
    auto *s = c->subset_context->serializer;
    s->push ();

    out->offset = 0;
    bool ret = f.subset (c->subset_context, c, &tag);
    if (ret)
      s->add_link (out->offset, s->pop_pack ());
    else
      s->pop_discard ();

    return_trace (ret);
  }

  bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    const Record_sanitize_closure_t closure = {tag, base};
    return_trace (c->check_struct (this) &&
		  offset.sanitize (c, base, &closure));
  }

  Tag           tag;            /* 4-byte Tag identifier */
  Offset16To<Type>
                offset;         /* Offset from beginning of object holding
                                 * the Record */
  public:
  DEFINE_SIZE_STATIC (6);
};

template <typename Type>
struct RecordArrayOf : SortedArray16Of<Record<Type>>
{
  const Offset16To<Type>& get_offset (unsigned int i) const
  { return (*this)[i].offset; }
  Offset16To<Type>& get_offset (unsigned int i)
  { return (*this)[i].offset; }
  const Tag& get_tag (unsigned int i) const
  { return (*this)[i].tag; }
  unsigned int get_tags (unsigned int start_offset,
                         unsigned int *record_count /* IN/OUT */,
                         hb_tag_t     *record_tags /* OUT */) const
  {
    if (record_count)
    {
      + this->as_array ().sub_array (start_offset, record_count)
      | hb_map (&Record<Type>::tag)
      | hb_sink (hb_array (record_tags, *record_count))
      ;
    }
    return this->len;
  }
  bool find_index (hb_tag_t tag, unsigned int *index) const
  {
    return this->bfind (tag, index, HB_NOT_FOUND_STORE, Index::NOT_FOUND_INDEX);
  }
};

template <typename Type>
struct RecordListOf : RecordArrayOf<Type>
{
  const Type& operator [] (unsigned int i) const
  { return this+this->get_offset (i); }

  bool subset (hb_subset_context_t *c,
               hb_subset_layout_context_t *l) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    + this->iter ()
    | hb_apply (subset_record_array (l, out, this))
    ;
    return_trace (true);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (RecordArrayOf<Type>::sanitize (c, this));
  }
};

struct RecordListOfFeature : RecordListOf<Feature>
{
  bool subset (hb_subset_context_t *c,
	       hb_subset_layout_context_t *l) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    + hb_enumerate (*this)
    | hb_filter (l->feature_index_map, hb_first)
    | hb_apply ([l, out, this] (const hb_pair_t<unsigned, const Record<Feature>&>& _)
                {
                  const Feature *f_sub = nullptr;
                  const Feature **f = nullptr;
                  if (l->feature_substitutes_map->has (_.first, &f))
                    f_sub = *f;

                  subset_record_array (l, out, this, f_sub) (_.second);
                })
    ;

    return_trace (true);
  }
};

typedef RecordListOf<Feature> FeatureList;


struct LangSys
{
  unsigned int get_feature_count () const
  { return featureIndex.len; }
  hb_tag_t get_feature_index (unsigned int i) const
  { return featureIndex[i]; }
  unsigned int get_feature_indexes (unsigned int start_offset,
				    unsigned int *feature_count /* IN/OUT */,
				    unsigned int *feature_indexes /* OUT */) const
  { return featureIndex.get_indexes (start_offset, feature_count, feature_indexes); }
  void add_feature_indexes_to (hb_set_t *feature_indexes) const
  { featureIndex.add_indexes_to (feature_indexes); }

  bool has_required_feature () const { return reqFeatureIndex != 0xFFFFu; }
  unsigned int get_required_feature_index () const
  {
    if (reqFeatureIndex == 0xFFFFu)
      return Index::NOT_FOUND_INDEX;
   return reqFeatureIndex;
  }

  LangSys* copy (hb_serialize_context_t *c) const
  {
    TRACE_SERIALIZE (this);
    return_trace (c->embed (*this));
  }

  bool compare (const LangSys& o, const hb_map_t *feature_index_map) const
  {
    if (reqFeatureIndex != o.reqFeatureIndex)
      return false;

    auto iter =
    + hb_iter (featureIndex)
    | hb_filter (feature_index_map)
    | hb_map (feature_index_map)
    ;

    auto o_iter =
    + hb_iter (o.featureIndex)
    | hb_filter (feature_index_map)
    | hb_map (feature_index_map)
    ;

    for (; iter && o_iter; iter++, o_iter++)
    {
      unsigned a = *iter;
      unsigned b = *o_iter;
      if (a != b) return false;
    }

    if (iter || o_iter) return false;

    return true;
  }

  void collect_features (hb_prune_langsys_context_t *c) const
  {
    if (!has_required_feature () && !get_feature_count ()) return;
    if (has_required_feature () &&
        c->duplicate_feature_map->has (reqFeatureIndex))
      c->new_feature_indexes->add (get_required_feature_index ());

    + hb_iter (featureIndex)
    | hb_filter (c->duplicate_feature_map)
    | hb_sink (c->new_feature_indexes)
    ;
  }

  bool subset (hb_subset_context_t        *c,
	       hb_subset_layout_context_t *l,
	       const Tag                  *tag = nullptr) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    const uint32_t *v;
    out->reqFeatureIndex = l->feature_index_map->has (reqFeatureIndex, &v) ? *v : 0xFFFFu;

    if (!l->visitFeatureIndex (featureIndex.len))
      return_trace (false);

    auto it =
    + hb_iter (featureIndex)
    | hb_filter (l->feature_index_map)
    | hb_map (l->feature_index_map)
    ;

    bool ret = bool (it);
    out->featureIndex.serialize (c->serializer, l, it);
    return_trace (ret);
  }

  bool sanitize (hb_sanitize_context_t *c,
		 const Record_sanitize_closure_t * = nullptr) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && featureIndex.sanitize (c));
  }

  Offset16	lookupOrderZ;	/* = Null (reserved for an offset to a
				 * reordering table) */
  HBUINT16	reqFeatureIndex;/* Index of a feature required for this
				 * language system--if no required features
				 * = 0xFFFFu */
  IndexArray	featureIndex;	/* Array of indices into the FeatureList */
  public:
  DEFINE_SIZE_ARRAY_SIZED (6, featureIndex);
};
DECLARE_NULL_NAMESPACE_BYTES (OT, LangSys);

struct Script
{
  unsigned int get_lang_sys_count () const
  { return langSys.len; }
  const Tag& get_lang_sys_tag (unsigned int i) const
  { return langSys.get_tag (i); }
  unsigned int get_lang_sys_tags (unsigned int start_offset,
				  unsigned int *lang_sys_count /* IN/OUT */,
				  hb_tag_t     *lang_sys_tags /* OUT */) const
  { return langSys.get_tags (start_offset, lang_sys_count, lang_sys_tags); }
  const LangSys& get_lang_sys (unsigned int i) const
  {
    if (i == Index::NOT_FOUND_INDEX) return get_default_lang_sys ();
    return this+langSys[i].offset;
  }
  bool find_lang_sys_index (hb_tag_t tag, unsigned int *index) const
  { return langSys.find_index (tag, index); }

  bool has_default_lang_sys () const           { return defaultLangSys != 0; }
  const LangSys& get_default_lang_sys () const { return this+defaultLangSys; }

  void prune_langsys (hb_prune_langsys_context_t *c,
                      unsigned script_index) const
  {
    if (!has_default_lang_sys () && !get_lang_sys_count ()) return;
    if (!c->visitScript ()) return;

    if (!c->script_langsys_map->has (script_index))
    {
      if (unlikely (!c->script_langsys_map->set (script_index, hb::unique_ptr<hb_set_t> {hb_set_create ()})))
	return;
    }

    if (has_default_lang_sys ())
    {
      //only collect features from non-redundant langsys
      const LangSys& d = get_default_lang_sys ();
      if (c->visitLangsys (d.get_feature_count ())) {
        d.collect_features (c);
      }

      for (auto _ : + hb_enumerate (langSys))
      {
        const LangSys& l = this+_.second.offset;
        if (!c->visitLangsys (l.get_feature_count ())) continue;
        if (l.compare (d, c->duplicate_feature_map)) continue;

        l.collect_features (c);
        c->script_langsys_map->get (script_index)->add (_.first);
      }
    }
    else
    {
      for (auto _ : + hb_enumerate (langSys))
      {
        const LangSys& l = this+_.second.offset;
        if (!c->visitLangsys (l.get_feature_count ())) continue;
        l.collect_features (c);
        c->script_langsys_map->get (script_index)->add (_.first);
      }
    }
  }

  bool subset (hb_subset_context_t         *c,
	       hb_subset_layout_context_t  *l,
	       const Tag                   *tag) const
  {
    TRACE_SUBSET (this);
    if (!l->visitScript ()) return_trace (false);
    if (tag && !c->plan->layout_scripts.has (*tag))
      return false;

    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    bool defaultLang = false;
    if (has_default_lang_sys ())
    {
      c->serializer->push ();
      const LangSys& ls = this+defaultLangSys;
      bool ret = ls.subset (c, l);
      if (!ret && tag && *tag != HB_TAG ('D', 'F', 'L', 'T'))
      {
	c->serializer->pop_discard ();
	out->defaultLangSys = 0;
      }
      else
      {
	c->serializer->add_link (out->defaultLangSys, c->serializer->pop_pack ());
	defaultLang = true;
      }
    }

    const hb_set_t *active_langsys = l->script_langsys_map->get (l->cur_script_index);
    if (active_langsys)
    {
      + hb_enumerate (langSys)
      | hb_filter (active_langsys, hb_first)
      | hb_map (hb_second)
      | hb_filter ([=] (const Record<LangSys>& record) {return l->visitLangSys (); })
      | hb_apply (subset_record_array (l, &(out->langSys), this))
      ;
    }

    return_trace (bool (out->langSys.len) || defaultLang || l->table_tag == HB_OT_TAG_GSUB);
  }

  bool sanitize (hb_sanitize_context_t *c,
		 const Record_sanitize_closure_t * = nullptr) const
  {
    TRACE_SANITIZE (this);
    return_trace (defaultLangSys.sanitize (c, this) && langSys.sanitize (c, this));
  }

  protected:
  Offset16To<LangSys>
		defaultLangSys;	/* Offset to DefaultLangSys table--from
				 * beginning of Script table--may be Null */
  RecordArrayOf<LangSys>
		langSys;	/* Array of LangSysRecords--listed
				 * alphabetically by LangSysTag */
  public:
  DEFINE_SIZE_ARRAY_SIZED (4, langSys);
};

struct RecordListOfScript : RecordListOf<Script>
{
  bool subset (hb_subset_context_t *c,
               hb_subset_layout_context_t *l) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    for (auto _ : + hb_enumerate (*this))
    {
      auto snap = c->serializer->snapshot ();
      l->cur_script_index = _.first;
      bool ret = _.second.subset (l, this);
      if (!ret) c->serializer->revert (snap);
      else out->len++;
    }

    return_trace (true);
  }
};

typedef RecordListOfScript ScriptList;



struct LookupFlag : HBUINT16
{
  enum Flags {
    RightToLeft		= 0x0001u,
    IgnoreBaseGlyphs	= 0x0002u,
    IgnoreLigatures	= 0x0004u,
    IgnoreMarks		= 0x0008u,
    IgnoreFlags		= 0x000Eu,
    UseMarkFilteringSet	= 0x0010u,
    Reserved		= 0x00E0u,
    MarkAttachmentType	= 0xFF00u
  };
  public:
  DEFINE_SIZE_STATIC (2);
};

} /* namespace OT */
/* This has to be outside the namespace. */
HB_MARK_AS_FLAG_T (OT::LookupFlag::Flags);
namespace OT {

struct Lookup
{
  unsigned int get_subtable_count () const { return subTable.len; }

  template <typename TSubTable>
  const Array16OfOffset16To<TSubTable>& get_subtables () const
  { return reinterpret_cast<const Array16OfOffset16To<TSubTable> &> (subTable); }
  template <typename TSubTable>
  Array16OfOffset16To<TSubTable>& get_subtables ()
  { return reinterpret_cast<Array16OfOffset16To<TSubTable> &> (subTable); }

  template <typename TSubTable>
  const TSubTable& get_subtable (unsigned int i) const
  { return this+get_subtables<TSubTable> ()[i]; }
  template <typename TSubTable>
  TSubTable& get_subtable (unsigned int i)
  { return this+get_subtables<TSubTable> ()[i]; }

  unsigned int get_size () const
  {
    const HBUINT16 &markFilteringSet = StructAfter<const HBUINT16> (subTable);
    if (lookupFlag & LookupFlag::UseMarkFilteringSet)
      return (const char *) &StructAfter<const char> (markFilteringSet) - (const char *) this;
    return (const char *) &markFilteringSet - (const char *) this;
  }

  unsigned int get_type () const { return lookupType; }

  /* lookup_props is a 32-bit integer where the lower 16-bit is LookupFlag and
   * higher 16-bit is mark-filtering-set if the lookup uses one.
   * Not to be confused with glyph_props which is very similar. */
  uint32_t get_props () const
  {
    unsigned int flag = lookupFlag;
    if (unlikely (flag & LookupFlag::UseMarkFilteringSet))
    {
      const HBUINT16 &markFilteringSet = StructAfter<HBUINT16> (subTable);
      flag += (markFilteringSet << 16);
    }
    return flag;
  }

  template <typename TSubTable, typename context_t, typename ...Ts>
  typename context_t::return_t dispatch (context_t *c, Ts&&... ds) const
  {
    unsigned int lookup_type = get_type ();
    TRACE_DISPATCH (this, lookup_type);
    unsigned int count = get_subtable_count ();
    for (unsigned int i = 0; i < count; i++) {
      typename context_t::return_t r = get_subtable<TSubTable> (i).dispatch (c, lookup_type, std::forward<Ts> (ds)...);
      if (c->stop_sublookup_iteration (r))
	return_trace (r);
    }
    return_trace (c->default_return_value ());
  }

  bool serialize (hb_serialize_context_t *c,
		  unsigned int lookup_type,
		  uint32_t lookup_props,
		  unsigned int num_subtables)
  {
    TRACE_SERIALIZE (this);
    if (unlikely (!c->extend_min (this))) return_trace (false);
    lookupType = lookup_type;
    lookupFlag = lookup_props & 0xFFFFu;
    if (unlikely (!subTable.serialize (c, num_subtables))) return_trace (false);
    if (lookupFlag & LookupFlag::UseMarkFilteringSet)
    {
      if (unlikely (!c->extend (this))) return_trace (false);
      HBUINT16 &markFilteringSet = StructAfter<HBUINT16> (subTable);
      markFilteringSet = lookup_props >> 16;
    }
    return_trace (true);
  }

  template <typename TSubTable>
  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);
    out->lookupType = lookupType;
    out->lookupFlag = lookupFlag;

    const hb_set_t *glyphset = c->plan->glyphset_gsub ();
    unsigned int lookup_type = get_type ();
    + hb_iter (get_subtables <TSubTable> ())
    | hb_filter ([this, glyphset, lookup_type] (const Offset16To<TSubTable> &_) { return (this+_).intersects (glyphset, lookup_type); })
    | hb_apply (subset_offset_array (c, out->get_subtables<TSubTable> (), this, lookup_type))
    ;

    if (lookupFlag & LookupFlag::UseMarkFilteringSet)
    {
      const HBUINT16 &markFilteringSet = StructAfter<HBUINT16> (subTable);
      hb_codepoint_t *idx;
      if (!c->plan->used_mark_sets_map.has (markFilteringSet, &idx))
      {
        unsigned new_flag = lookupFlag;
        new_flag &= ~LookupFlag::UseMarkFilteringSet;
        out->lookupFlag = new_flag;
      }
      else
      {
        if (unlikely (!c->serializer->extend (out))) return_trace (false);
        HBUINT16 &outMarkFilteringSet = StructAfter<HBUINT16> (out->subTable);
        outMarkFilteringSet = *idx;
      }
    }

    // Always keep the lookup even if it's empty. The rest of layout subsetting depends on lookup
    // indices being consistent with those computed during planning. So if an empty lookup is
    // discarded during the subset phase it will invalidate all subsequent lookup indices.
    // Generally we shouldn't end up with an empty lookup as we pre-prune them during the planning
    // phase, but it can happen in rare cases such as when during closure subtable is considered
    // degenerate (see: https://github.com/harfbuzz/harfbuzz/issues/3853)
    return_trace (true);
  }

  template <typename TSubTable>
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (!(c->check_struct (this) && subTable.sanitize (c))) return_trace (false);
    hb_barrier ();

    unsigned subtables = get_subtable_count ();
    if (unlikely (!c->visit_subtables (subtables))) return_trace (false);

    if (lookupFlag & LookupFlag::UseMarkFilteringSet)
    {
      const HBUINT16 &markFilteringSet = StructAfter<HBUINT16> (subTable);
      if (!markFilteringSet.sanitize (c)) return_trace (false);
    }

    if (unlikely (!get_subtables<TSubTable> ().sanitize (c, this, get_type ())))
      return_trace (false);

    if (unlikely (get_type () == TSubTable::Extension && !c->get_edit_count ()))
    {
      hb_barrier ();

      /* The spec says all subtables of an Extension lookup should
       * have the same type, which shall not be the Extension type
       * itself (but we already checked for that).
       * This is specially important if one has a reverse type!
       *
       * We only do this if sanitizer edit_count is zero.  Otherwise,
       * some of the subtables might have become insane after they
       * were sanity-checked by the edits of subsequent subtables.
       * https://bugs.chromium.org/p/chromium/issues/detail?id=960331
       */
      unsigned int type = get_subtable<TSubTable> (0).u.extension.get_type ();
      for (unsigned int i = 1; i < subtables; i++)
	if (get_subtable<TSubTable> (i).u.extension.get_type () != type)
	  return_trace (false);
    }
    return_trace (true);
  }

  protected:
  HBUINT16	lookupType;		/* Different enumerations for GSUB and GPOS */
  HBUINT16	lookupFlag;		/* Lookup qualifiers */
  Array16Of<Offset16>
		subTable;		/* Array of SubTables */
/*HBUINT16	markFilteringSetX[HB_VAR_ARRAY];*//* Index (base 0) into GDEF mark glyph sets
					 * structure. This field is only present if bit
					 * UseMarkFilteringSet of lookup flags is set. */
  public:
  DEFINE_SIZE_ARRAY (6, subTable);
};

template <typename Types>
using LookupList = List16OfOffsetTo<Lookup, typename Types::HBUINT>;

template <typename TLookup, typename OffsetType>
struct LookupOffsetList : List16OfOffsetTo<TLookup, OffsetType>
{
  bool subset (hb_subset_context_t        *c,
	       hb_subset_layout_context_t *l) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (this);
    if (unlikely (!c->serializer->extend_min (out))) return_trace (false);

    + hb_enumerate (*this)
    | hb_filter (l->lookup_index_map, hb_first)
    | hb_map (hb_second)
    | hb_apply (subset_offset_array (c, *out, this))
    ;
    return_trace (true);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (List16OfOffset16To<TLookup>::sanitize (c, this));
  }
};


/*
 * Coverage Table
 */


static bool ClassDef_remap_and_serialize (hb_serialize_context_t *c,
					  const hb_set_t &klasses,
                                          bool use_class_zero,
                                          hb_sorted_vector_t<hb_codepoint_pair_t> &glyph_and_klass, /* IN/OUT */
					  hb_map_t *klass_map /*IN/OUT*/)
{
  if (!klass_map)
    return ClassDef_serialize (c, glyph_and_klass.iter ());

  /* any glyph not assigned a class value falls into Class zero (0),
   * if any glyph assigned to class 0, remapping must start with 0->0*/
  if (!use_class_zero)
    klass_map->set (0, 0);

  unsigned idx = klass_map->has (0) ? 1 : 0;
  for (const unsigned k: klasses)
  {
    if (klass_map->has (k)) continue;
    klass_map->set (k, idx);
    idx++;
  }


  for (unsigned i = 0; i < glyph_and_klass.length; i++)
  {
    hb_codepoint_t klass = glyph_and_klass[i].second;
    glyph_and_klass[i].second = klass_map->get (klass);
  }

  c->propagate_error (glyph_and_klass, klasses);
  return ClassDef_serialize (c, glyph_and_klass.iter ());
}

/*
 * Class Definition Table
 */

template <typename Types>
struct ClassDefFormat1_3
{
  friend struct ClassDef;

  private:
  unsigned int get_class (hb_codepoint_t glyph_id) const
  {
    return classValue[(unsigned int) (glyph_id - startGlyph)];
  }

  unsigned get_population () const
  {
    return classValue.len;
  }

  template<typename Iterator,
	   hb_requires (hb_is_sorted_source_of (Iterator, hb_codepoint_t))>
  bool serialize (hb_serialize_context_t *c,
		  Iterator it)
  {
    TRACE_SERIALIZE (this);
    if (unlikely (!c->extend_min (this))) return_trace (false);

    if (unlikely (!it))
    {
      classFormat = 1;
      startGlyph = 0;
      classValue.len = 0;
      return_trace (true);
    }

    hb_codepoint_t glyph_min = (*it).first;
    hb_codepoint_t glyph_max = + it
			       | hb_map (hb_first)
			       | hb_reduce (hb_max, 0u);
    unsigned glyph_count = glyph_max - glyph_min + 1;

    startGlyph = glyph_min;
    if (unlikely (!classValue.serialize (c, glyph_count))) return_trace (false);
    for (const hb_pair_t<hb_codepoint_t, uint32_t> gid_klass_pair : + it)
    {
      unsigned idx = gid_klass_pair.first - glyph_min;
      classValue[idx] = gid_klass_pair.second;
    }
    return_trace (true);
  }

  bool subset (hb_subset_context_t *c,
	       hb_map_t *klass_map = nullptr /*OUT*/,
               bool keep_empty_table = true,
               bool use_class_zero = true,
               const Coverage* glyph_filter = nullptr) const
  {
    TRACE_SUBSET (this);
    const hb_map_t &glyph_map = c->plan->glyph_map_gsub;

    hb_sorted_vector_t<hb_codepoint_pair_t> glyph_and_klass;
    hb_set_t orig_klasses;

    hb_codepoint_t start = startGlyph;
    hb_codepoint_t end   = start + classValue.len;

    for (const hb_codepoint_t gid : + hb_range (start, end))
    {
      hb_codepoint_t new_gid = glyph_map[gid];
      if (new_gid == HB_MAP_VALUE_INVALID) continue;
      if (glyph_filter && !glyph_filter->has(gid)) continue;

      unsigned klass = classValue[gid - start];
      if (!klass) continue;

      glyph_and_klass.push (hb_pair (new_gid, klass));
      orig_klasses.add (klass);
    }

    if (use_class_zero)
    {
      unsigned glyph_count = glyph_filter
			     ? hb_len (hb_iter (glyph_map.keys()) | hb_filter (glyph_filter))
			     : glyph_map.get_population ();
      use_class_zero = glyph_count <= glyph_and_klass.length;
    }
    if (!ClassDef_remap_and_serialize (c->serializer,
                                       orig_klasses,
                                       use_class_zero,
                                       glyph_and_klass,
                                       klass_map))
      return_trace (false);
    return_trace (keep_empty_table || (bool) glyph_and_klass);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && classValue.sanitize (c));
  }

  unsigned cost () const { return 1; }

  template <typename set_t>
  bool collect_coverage (set_t *glyphs) const
  {
    unsigned int start = 0;
    unsigned int count = classValue.len;
    for (unsigned int i = 0; i < count; i++)
    {
      if (classValue[i])
	continue;

      if (start != i)
	if (unlikely (!glyphs->add_range (startGlyph + start, startGlyph + i)))
	  return false;

      start = i + 1;
    }
    if (start != count)
      if (unlikely (!glyphs->add_range (startGlyph + start, startGlyph + count)))
	return false;

    return true;
  }

  template <typename set_t>
  bool collect_class (set_t *glyphs, unsigned klass) const
  {
    unsigned int count = classValue.len;
    for (unsigned int i = 0; i < count; i++)
      if (classValue[i] == klass) glyphs->add (startGlyph + i);
    return true;
  }

  bool intersects (const hb_set_t *glyphs) const
  {
    hb_codepoint_t start = startGlyph;
    hb_codepoint_t end = startGlyph + classValue.len;
    for (hb_codepoint_t iter = startGlyph - 1;
	 glyphs->next (&iter) && iter < end;)
      if (classValue[iter - start]) return true;
    return false;
  }
  bool intersects_class (const hb_set_t *glyphs, uint16_t klass) const
  {
    unsigned int count = classValue.len;
    if (klass == 0)
    {
      /* Match if there's any glyph that is not listed! */
      hb_codepoint_t g = HB_SET_VALUE_INVALID;
      if (!glyphs->next (&g)) return false;
      if (g < startGlyph) return true;
      g = startGlyph + count - 1;
      if (glyphs->next (&g)) return true;
      /* Fall through. */
    }
    /* TODO Speed up, using set overlap first? */
    /* TODO(iter) Rewrite as dagger. */
    const HBUINT16 *arr = classValue.arrayZ;
    for (unsigned int i = 0; i < count; i++)
      if (arr[i] == klass && glyphs->has (startGlyph + i))
	return true;
    return false;
  }

  void intersected_class_glyphs (const hb_set_t *glyphs, unsigned klass, hb_set_t *intersect_glyphs) const
  {
    unsigned count = classValue.len;
    if (klass == 0)
    {
      unsigned start_glyph = startGlyph;
      for (uint32_t g = HB_SET_VALUE_INVALID;
	   glyphs->next (&g) && g < start_glyph;)
	intersect_glyphs->add (g);

      for (uint32_t g = startGlyph + count - 1;
	   glyphs-> next (&g);)
	intersect_glyphs->add (g);

      return;
    }

    for (unsigned i = 0; i < count; i++)
      if (classValue[i] == klass && glyphs->has (startGlyph + i))
	intersect_glyphs->add (startGlyph + i);

#if 0
    /* The following implementation is faster asymptotically, but slower
     * in practice. */
    unsigned start_glyph = startGlyph;
    unsigned end_glyph = start_glyph + count;
    for (unsigned g = startGlyph - 1;
	 glyphs->next (&g) && g < end_glyph;)
      if (classValue.arrayZ[g - start_glyph] == klass)
        intersect_glyphs->add (g);
#endif
  }

  void intersected_classes (const hb_set_t *glyphs, hb_set_t *intersect_classes) const
  {
    if (glyphs->is_empty ()) return;
    hb_codepoint_t end_glyph = startGlyph + classValue.len - 1;
    if (glyphs->get_min () < startGlyph ||
        glyphs->get_max () > end_glyph)
      intersect_classes->add (0);

    for (const auto& _ : + hb_enumerate (classValue))
    {
      hb_codepoint_t g = startGlyph + _.first;
      if (glyphs->has (g))
        intersect_classes->add (_.second);
    }
  }

  protected:
  HBUINT16	classFormat;	/* Format identifier--format = 1 */
  typename Types::HBGlyphID
		 startGlyph;	/* First GlyphID of the classValueArray */
  typename Types::template ArrayOf<HBUINT16>
		classValue;	/* Array of Class Values--one per GlyphID */
  public:
  DEFINE_SIZE_ARRAY (2 + 2 * Types::size, classValue);
};

template <typename Types>
struct ClassDefFormat2_4
{
  friend struct ClassDef;

  private:
  unsigned int get_class (hb_codepoint_t glyph_id) const
  {
    return rangeRecord.bsearch (glyph_id).value;
  }

  unsigned get_population () const
  {
    typename Types::large_int ret = 0;
    for (const auto &r : rangeRecord)
      ret += r.get_population ();
    return ret > UINT_MAX ? UINT_MAX : (unsigned) ret;
  }

  template<typename Iterator,
	   hb_requires (hb_is_sorted_source_of (Iterator, hb_codepoint_t))>
  bool serialize (hb_serialize_context_t *c,
		  Iterator it)
  {
    TRACE_SERIALIZE (this);
    if (unlikely (!c->extend_min (this))) return_trace (false);

    if (unlikely (!it))
    {
      classFormat = 2;
      rangeRecord.len = 0;
      return_trace (true);
    }

    unsigned unsorted = false;
    unsigned num_ranges = 1;
    hb_codepoint_t prev_gid = (*it).first;
    unsigned prev_klass = (*it).second;

    RangeRecord<Types> range_rec;
    range_rec.first = prev_gid;
    range_rec.last = prev_gid;
    range_rec.value = prev_klass;

    auto *record = c->copy (range_rec);
    if (unlikely (!record)) return_trace (false);

    for (const auto gid_klass_pair : + (++it))
    {
      hb_codepoint_t cur_gid = gid_klass_pair.first;
      unsigned cur_klass = gid_klass_pair.second;

      if (cur_gid != prev_gid + 1 ||
	  cur_klass != prev_klass)
      {

	if (unlikely (cur_gid < prev_gid))
	  unsorted = true;

	if (unlikely (!record)) break;
	record->last = prev_gid;
	num_ranges++;

	range_rec.first = cur_gid;
	range_rec.last = cur_gid;
	range_rec.value = cur_klass;

	record = c->copy (range_rec);
      }

      prev_klass = cur_klass;
      prev_gid = cur_gid;
    }

    if (unlikely (c->in_error ())) return_trace (false);

    if (likely (record)) record->last = prev_gid;
    rangeRecord.len = num_ranges;

    if (unlikely (unsorted))
      rangeRecord.as_array ().qsort (RangeRecord<Types>::cmp_range);

    return_trace (true);
  }

  bool subset (hb_subset_context_t *c,
	       hb_map_t *klass_map = nullptr /*OUT*/,
               bool keep_empty_table = true,
               bool use_class_zero = true,
               const Coverage* glyph_filter = nullptr) const
  {
    TRACE_SUBSET (this);
    const hb_map_t &glyph_map = c->plan->glyph_map_gsub;
    const hb_set_t &glyph_set = *c->plan->glyphset_gsub ();

    hb_sorted_vector_t<hb_codepoint_pair_t> glyph_and_klass;
    hb_set_t orig_klasses;

    if (glyph_set.get_population () * hb_bit_storage ((unsigned) rangeRecord.len) / 2
	< get_population ())
    {
      for (hb_codepoint_t g : glyph_set)
      {
	unsigned klass = get_class (g);
	if (!klass) continue;
	hb_codepoint_t new_gid = glyph_map[g];
	if (new_gid == HB_MAP_VALUE_INVALID) continue;
	if (glyph_filter && !glyph_filter->has (g)) continue;
	glyph_and_klass.push (hb_pair (new_gid, klass));
	orig_klasses.add (klass);
      }
    }
    else
    {
      unsigned num_source_glyphs = c->plan->source->get_num_glyphs ();
      for (auto &range : rangeRecord)
      {
	unsigned klass = range.value;
	if (!klass) continue;
	hb_codepoint_t start = range.first;
	hb_codepoint_t end   = hb_min (range.last + 1, num_source_glyphs);
	for (hb_codepoint_t g = start; g < end; g++)
	{
	  hb_codepoint_t new_gid = glyph_map[g];
	  if (new_gid == HB_MAP_VALUE_INVALID) continue;
	  if (glyph_filter && !glyph_filter->has (g)) continue;

	  glyph_and_klass.push (hb_pair (new_gid, klass));
	  orig_klasses.add (klass);
	}
      }
    }

    const hb_set_t& glyphset = *c->plan->glyphset_gsub ();
    unsigned glyph_count = glyph_filter
                           ? hb_len (hb_iter (glyphset) | hb_filter (glyph_filter))
                           : glyph_map.get_population ();
    use_class_zero = use_class_zero && glyph_count <= glyph_and_klass.length;
    if (!ClassDef_remap_and_serialize (c->serializer,
                                       orig_klasses,
                                       use_class_zero,
                                       glyph_and_klass,
                                       klass_map))
      return_trace (false);
    return_trace (keep_empty_table || (bool) glyph_and_klass);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (rangeRecord.sanitize (c));
  }

  unsigned cost () const { return hb_bit_storage ((unsigned) rangeRecord.len); /* bsearch cost */ }

  template <typename set_t>
  bool collect_coverage (set_t *glyphs) const
  {
    for (auto &range : rangeRecord)
      if (range.value)
	if (unlikely (!range.collect_coverage (glyphs)))
	  return false;
    return true;
  }

  template <typename set_t>
  bool collect_class (set_t *glyphs, unsigned int klass) const
  {
    for (auto &range : rangeRecord)
    {
      if (range.value == klass)
	if (unlikely (!range.collect_coverage (glyphs)))
	  return false;
    }
    return true;
  }

  bool intersects (const hb_set_t *glyphs) const
  {
    if (rangeRecord.len > glyphs->get_population () * hb_bit_storage ((unsigned) rangeRecord.len) / 2)
    {
      for (auto g : *glyphs)
        if (get_class (g))
	  return true;
      return false;
    }

    return hb_any (+ hb_iter (rangeRecord)
                   | hb_map ([glyphs] (const RangeRecord<Types> &range) { return range.intersects (*glyphs) && range.value; }));
  }
  bool intersects_class (const hb_set_t *glyphs, uint16_t klass) const
  {
    if (klass == 0)
    {
      /* Match if there's any glyph that is not listed! */
      hb_codepoint_t g = HB_SET_VALUE_INVALID;
      hb_codepoint_t last = HB_SET_VALUE_INVALID;
      auto it = hb_iter (rangeRecord);
      for (auto &range : it)
      {
        if (it->first == last + 1)
	{
	  it++;
	  continue;
	}

	if (!glyphs->next (&g))
	  break;
	if (g < range.first)
	  return true;
	g = range.last;
	last = g;
      }
      if (g != HB_SET_VALUE_INVALID && glyphs->next (&g))
	return true;
      /* Fall through. */
    }
    for (const auto &range : rangeRecord)
      if (range.value == klass && range.intersects (*glyphs))
	return true;
    return false;
  }

  void intersected_class_glyphs (const hb_set_t *glyphs, unsigned klass, hb_set_t *intersect_glyphs) const
  {
    if (klass == 0)
    {
      hb_codepoint_t g = HB_SET_VALUE_INVALID;
      for (auto &range : rangeRecord)
      {
	if (!glyphs->next (&g))
	  goto done;
	while (g < range.first)
	{
	  intersect_glyphs->add (g);
	  if (!glyphs->next (&g))
	    goto done;
        }
        g = range.last;
      }
      while (glyphs->next (&g))
	intersect_glyphs->add (g);
      done:

      return;
    }

    unsigned count = rangeRecord.len;
    if (count > glyphs->get_population () * hb_bit_storage (count) * 8)
    {
      for (auto g : *glyphs)
      {
        unsigned i;
        if (rangeRecord.as_array ().bfind (g, &i) &&
	    rangeRecord.arrayZ[i].value == klass)
	  intersect_glyphs->add (g);
      }
      return;
    }

    for (auto &range : rangeRecord)
    {
      if (range.value != klass) continue;

      unsigned end = range.last + 1;
      for (hb_codepoint_t g = range.first - 1;
	   glyphs->next (&g) && g < end;)
	intersect_glyphs->add (g);
    }
  }

  void intersected_classes (const hb_set_t *glyphs, hb_set_t *intersect_classes) const
  {
    if (glyphs->is_empty ()) return;

    hb_codepoint_t g = HB_SET_VALUE_INVALID;
    for (auto &range : rangeRecord)
    {
      if (!glyphs->next (&g))
        break;
      if (g < range.first)
      {
        intersect_classes->add (0);
        break;
      }
      g = range.last;
    }
    if (g != HB_SET_VALUE_INVALID && glyphs->next (&g))
      intersect_classes->add (0);

    for (const auto& range : rangeRecord)
      if (range.intersects (*glyphs))
        intersect_classes->add (range.value);
  }

  protected:
  HBUINT16	classFormat;	/* Format identifier--format = 2 */
  typename Types::template SortedArrayOf<RangeRecord<Types>>
		rangeRecord;	/* Array of glyph ranges--ordered by
				 * Start GlyphID */
  public:
  DEFINE_SIZE_ARRAY (2 + Types::size, rangeRecord);
};

struct ClassDef
{
  /* Has interface. */
  unsigned operator [] (hb_codepoint_t k) const { return get (k); }
  bool has (hb_codepoint_t k) const { return (*this)[k]; }
  /* Projection. */
  hb_codepoint_t operator () (hb_codepoint_t k) const { return get (k); }

  unsigned int get (hb_codepoint_t k) const { return get_class (k); }
  unsigned int get_class (hb_codepoint_t glyph_id) const
  {
    switch (u.format) {
    case 1: hb_barrier (); return u.format1.get_class (glyph_id);
    case 2: hb_barrier (); return u.format2.get_class (glyph_id);
#ifndef HB_NO_BEYOND_64K
    case 3: hb_barrier (); return u.format3.get_class (glyph_id);
    case 4: hb_barrier (); return u.format4.get_class (glyph_id);
#endif
    default:return 0;
    }
  }

  unsigned get_population () const
  {
    switch (u.format) {
    case 1: hb_barrier (); return u.format1.get_population ();
    case 2: hb_barrier (); return u.format2.get_population ();
#ifndef HB_NO_BEYOND_64K
    case 3: hb_barrier (); return u.format3.get_population ();
    case 4: hb_barrier (); return u.format4.get_population ();
#endif
    default:return NOT_COVERED;
    }
  }

  template<typename Iterator,
	   hb_requires (hb_is_sorted_source_of (Iterator, hb_codepoint_t))>
  bool serialize (hb_serialize_context_t *c, Iterator it_with_class_zero)
  {
    TRACE_SERIALIZE (this);
    if (unlikely (!c->extend_min (this))) return_trace (false);

    auto it = + it_with_class_zero | hb_filter (hb_second);

    unsigned format = 2;
    hb_codepoint_t glyph_max = 0;
    if (likely (it))
    {
      hb_codepoint_t glyph_min = (*it).first;
      glyph_max = glyph_min;

      unsigned num_glyphs = 0;
      unsigned num_ranges = 1;
      hb_codepoint_t prev_gid = glyph_min;
      unsigned prev_klass = (*it).second;

      for (const auto gid_klass_pair : it)
      {
	hb_codepoint_t cur_gid = gid_klass_pair.first;
	unsigned cur_klass = gid_klass_pair.second;
        num_glyphs++;
	if (cur_gid == glyph_min) continue;
        if (cur_gid > glyph_max) glyph_max = cur_gid;
	if (cur_gid != prev_gid + 1 ||
	    cur_klass != prev_klass)
	  num_ranges++;

	prev_gid = cur_gid;
	prev_klass = cur_klass;
      }

      if (num_glyphs && 1 + (glyph_max - glyph_min + 1) <= num_ranges * 3)
	format = 1;
    }

#ifndef HB_NO_BEYOND_64K
    if (glyph_max > 0xFFFFu)
      u.format += 2;
    if (unlikely (glyph_max > 0xFFFFFFu))
#else
    if (unlikely (glyph_max > 0xFFFFu))
#endif
    {
      c->check_success (false, HB_SERIALIZE_ERROR_INT_OVERFLOW);
      return_trace (false);
    }

    u.format = format;

    switch (u.format)
    {
    case 1: hb_barrier (); return_trace (u.format1.serialize (c, it));
    case 2: hb_barrier (); return_trace (u.format2.serialize (c, it));
#ifndef HB_NO_BEYOND_64K
    case 3: hb_barrier (); return_trace (u.format3.serialize (c, it));
    case 4: hb_barrier (); return_trace (u.format4.serialize (c, it));
#endif
    default:return_trace (false);
    }
  }

  bool subset (hb_subset_context_t *c,
	       hb_map_t *klass_map = nullptr /*OUT*/,
               bool keep_empty_table = true,
               bool use_class_zero = true,
               const Coverage* glyph_filter = nullptr) const
  {
    TRACE_SUBSET (this);
    switch (u.format) {
    case 1: hb_barrier (); return_trace (u.format1.subset (c, klass_map, keep_empty_table, use_class_zero, glyph_filter));
    case 2: hb_barrier (); return_trace (u.format2.subset (c, klass_map, keep_empty_table, use_class_zero, glyph_filter));
#ifndef HB_NO_BEYOND_64K
    case 3: hb_barrier (); return_trace (u.format3.subset (c, klass_map, keep_empty_table, use_class_zero, glyph_filter));
    case 4: hb_barrier (); return_trace (u.format4.subset (c, klass_map, keep_empty_table, use_class_zero, glyph_filter));
#endif
    default:return_trace (false);
    }
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (!u.format.sanitize (c)) return_trace (false);
    hb_barrier ();
    switch (u.format) {
    case 1: hb_barrier (); return_trace (u.format1.sanitize (c));
    case 2: hb_barrier (); return_trace (u.format2.sanitize (c));
#ifndef HB_NO_BEYOND_64K
    case 3: hb_barrier (); return_trace (u.format3.sanitize (c));
    case 4: hb_barrier (); return_trace (u.format4.sanitize (c));
#endif
    default:return_trace (true);
    }
  }

  unsigned cost () const
  {
    switch (u.format) {
    case 1: hb_barrier (); return u.format1.cost ();
    case 2: hb_barrier (); return u.format2.cost ();
#ifndef HB_NO_BEYOND_64K
    case 3: hb_barrier (); return u.format3.cost ();
    case 4: hb_barrier (); return u.format4.cost ();
#endif
    default:return 0u;
    }
  }

  /* Might return false if array looks unsorted.
   * Used for faster rejection of corrupt data. */
  template <typename set_t>
  bool collect_coverage (set_t *glyphs) const
  {
    switch (u.format) {
    case 1: hb_barrier (); return u.format1.collect_coverage (glyphs);
    case 2: hb_barrier (); return u.format2.collect_coverage (glyphs);
#ifndef HB_NO_BEYOND_64K
    case 3: hb_barrier (); return u.format3.collect_coverage (glyphs);
    case 4: hb_barrier (); return u.format4.collect_coverage (glyphs);
#endif
    default:return false;
    }
  }

  /* Might return false if array looks unsorted.
   * Used for faster rejection of corrupt data. */
  template <typename set_t>
  bool collect_class (set_t *glyphs, unsigned int klass) const
  {
    switch (u.format) {
    case 1: hb_barrier (); return u.format1.collect_class (glyphs, klass);
    case 2: hb_barrier (); return u.format2.collect_class (glyphs, klass);
#ifndef HB_NO_BEYOND_64K
    case 3: hb_barrier (); return u.format3.collect_class (glyphs, klass);
    case 4: hb_barrier (); return u.format4.collect_class (glyphs, klass);
#endif
    default:return false;
    }
  }

  bool intersects (const hb_set_t *glyphs) const
  {
    switch (u.format) {
    case 1: hb_barrier (); return u.format1.intersects (glyphs);
    case 2: hb_barrier (); return u.format2.intersects (glyphs);
#ifndef HB_NO_BEYOND_64K
    case 3: hb_barrier (); return u.format3.intersects (glyphs);
    case 4: hb_barrier (); return u.format4.intersects (glyphs);
#endif
    default:return false;
    }
  }
  bool intersects_class (const hb_set_t *glyphs, unsigned int klass) const
  {
    switch (u.format) {
    case 1: hb_barrier (); return u.format1.intersects_class (glyphs, klass);
    case 2: hb_barrier (); return u.format2.intersects_class (glyphs, klass);
#ifndef HB_NO_BEYOND_64K
    case 3: hb_barrier (); return u.format3.intersects_class (glyphs, klass);
    case 4: hb_barrier (); return u.format4.intersects_class (glyphs, klass);
#endif
    default:return false;
    }
  }

  void intersected_class_glyphs (const hb_set_t *glyphs, unsigned klass, hb_set_t *intersect_glyphs) const
  {
    switch (u.format) {
    case 1: hb_barrier (); return u.format1.intersected_class_glyphs (glyphs, klass, intersect_glyphs);
    case 2: hb_barrier (); return u.format2.intersected_class_glyphs (glyphs, klass, intersect_glyphs);
#ifndef HB_NO_BEYOND_64K
    case 3: hb_barrier (); return u.format3.intersected_class_glyphs (glyphs, klass, intersect_glyphs);
    case 4: hb_barrier (); return u.format4.intersected_class_glyphs (glyphs, klass, intersect_glyphs);
#endif
    default:return;
    }
  }

  void intersected_classes (const hb_set_t *glyphs, hb_set_t *intersect_classes) const
  {
    switch (u.format) {
    case 1: hb_barrier (); return u.format1.intersected_classes (glyphs, intersect_classes);
    case 2: hb_barrier (); return u.format2.intersected_classes (glyphs, intersect_classes);
#ifndef HB_NO_BEYOND_64K
    case 3: hb_barrier (); return u.format3.intersected_classes (glyphs, intersect_classes);
    case 4: hb_barrier (); return u.format4.intersected_classes (glyphs, intersect_classes);
#endif
    default:return;
    }
  }


  protected:
  union {
  HBUINT16			format;		/* Format identifier */
  ClassDefFormat1_3<SmallTypes>	format1;
  ClassDefFormat2_4<SmallTypes>	format2;
#ifndef HB_NO_BEYOND_64K
  ClassDefFormat1_3<MediumTypes>format3;
  ClassDefFormat2_4<MediumTypes>format4;
#endif
  } u;
  public:
  DEFINE_SIZE_UNION (2, format);
};

template<typename Iterator>
static inline bool ClassDef_serialize (hb_serialize_context_t *c,
				       Iterator it)
{ return (c->start_embed<ClassDef> ()->serialize (c, it)); }


/*
 * Item Variation Store
 */

/* ported from fonttools (class _Encoding) */
struct delta_row_encoding_t
{
  /* each byte represents a region, value is one of 0/1/2/4, which means bytes
   * needed for this region */
  hb_vector_t<uint8_t> chars;
  unsigned width = 0;
  hb_vector_t<uint8_t> columns;
  unsigned overhead = 0;
  hb_vector_t<const hb_vector_t<int>*> items;

  delta_row_encoding_t () = default;
  delta_row_encoding_t (hb_vector_t<uint8_t>&& chars_,
                        const hb_vector_t<int>* row = nullptr) :
                        delta_row_encoding_t ()

  {
    chars = std::move (chars_);
    width = get_width ();
    columns = get_columns ();
    overhead = get_chars_overhead (columns);
    if (row) items.push (row);
  }

  bool is_empty () const
  { return !items; }

  static hb_vector_t<uint8_t> get_row_chars (const hb_vector_t<int>& row)
  {
    hb_vector_t<uint8_t> ret;
    if (!ret.alloc (row.length)) return ret;

    bool long_words = false;

    /* 0/1/2 byte encoding */
    for (int i = row.length - 1; i >= 0; i--)
    {
      int v =  row.arrayZ[i];
      if (v == 0)
        ret.push (0);
      else if (v > 32767 || v < -32768)
      {
        long_words = true;
        break;
      }
      else if (v > 127 || v < -128)
        ret.push (2);
      else
        ret.push (1);
    }

    if (!long_words)
      return ret;

    /* redo, 0/2/4 bytes encoding */
    ret.reset ();
    for (int i = row.length - 1; i >= 0; i--)
    {
      int v =  row.arrayZ[i];
      if (v == 0)
        ret.push (0);
      else if (v > 32767 || v < -32768)
        ret.push (4);
      else
        ret.push (2);
    }
    return ret;
  }

  inline unsigned get_width ()
  {
    unsigned ret = + hb_iter (chars)
                   | hb_reduce (hb_add, 0u)
                   ;
    return ret;
  }

  hb_vector_t<uint8_t> get_columns ()
  {
    hb_vector_t<uint8_t> cols;
    cols.alloc (chars.length);
    for (auto v : chars)
    {
      uint8_t flag = v ? 1 : 0;
      cols.push (flag);
    }
    return cols;
  }

  static inline unsigned get_chars_overhead (const hb_vector_t<uint8_t>& cols)
  {
    unsigned c = 4 + 6; // 4 bytes for LOffset, 6 bytes for VarData header
    unsigned cols_bit_count = 0;
    for (auto v : cols)
      if (v) cols_bit_count++;
    return c + cols_bit_count * 2;
  }

  unsigned get_gain () const
  {
    int count = items.length;
    return hb_max (0, (int) overhead - count);
  }

  int gain_from_merging (const delta_row_encoding_t& other_encoding) const
  {
    int combined_width = 0;
    for (unsigned i = 0; i < chars.length; i++)
      combined_width += hb_max (chars.arrayZ[i], other_encoding.chars.arrayZ[i]);

    hb_vector_t<uint8_t> combined_columns;
    combined_columns.alloc (columns.length);
    for (unsigned i = 0; i < columns.length; i++)
      combined_columns.push (columns.arrayZ[i] | other_encoding.columns.arrayZ[i]);

    int combined_overhead = get_chars_overhead (combined_columns);
    int combined_gain = (int) overhead + (int) other_encoding.overhead - combined_overhead
                        - (combined_width - (int) width) * items.length
                        - (combined_width - (int) other_encoding.width) * other_encoding.items.length;

    return combined_gain;
  }

  static int cmp (const void *pa, const void *pb)
  {
    const delta_row_encoding_t *a = (const delta_row_encoding_t *)pa;
    const delta_row_encoding_t *b = (const delta_row_encoding_t *)pb;

    int gain_a = a->get_gain ();
    int gain_b = b->get_gain ();

    if (gain_a != gain_b)
      return gain_a - gain_b;

    return (b->chars).as_array ().cmp ((a->chars).as_array ());
  }

  static int cmp_width (const void *pa, const void *pb)
  {
    const delta_row_encoding_t *a = (const delta_row_encoding_t *)pa;
    const delta_row_encoding_t *b = (const delta_row_encoding_t *)pb;

    if (a->width != b->width)
      return (int) a->width - (int) b->width;

    return (b->chars).as_array ().cmp ((a->chars).as_array ());
  }

  bool add_row (const hb_vector_t<int>* row)
  { return items.push (row); }
};

struct VarRegionAxis
{
  float evaluate (int coord) const
  {
    int peak = peakCoord.to_int ();
    if (peak == 0 || coord == peak)
      return 1.f;
    else if (coord == 0) // Faster
      return 0.f;

    int start = startCoord.to_int (), end = endCoord.to_int ();

    /* TODO Move these to sanitize(). */
    if (unlikely (start > peak || peak > end))
      return 1.f;
    if (unlikely (start < 0 && end > 0 && peak != 0))
      return 1.f;

    if (coord <= start || end <= coord)
      return 0.f;

    /* Interpolate */
    if (coord < peak)
      return float (coord - start) / (peak - start);
    else
      return float (end - coord) / (end - peak);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  bool serialize (hb_serialize_context_t *c) const
  {
    TRACE_SERIALIZE (this);
    return_trace (c->embed (this));
  }

  public:
  F2DOT14	startCoord;
  F2DOT14	peakCoord;
  F2DOT14	endCoord;
  public:
  DEFINE_SIZE_STATIC (6);
};
struct SparseVarRegionAxis
{
  float evaluate (const int *coords, unsigned int coord_len) const
  {
    unsigned i = axisIndex;
    int coord = i < coord_len ? coords[i] : 0;
    return axis.evaluate (coord);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  bool serialize (hb_serialize_context_t *c) const
  {
    TRACE_SERIALIZE (this);
    return_trace (c->embed (this));
  }

  public:
  HBUINT16 axisIndex;
  VarRegionAxis axis;
  public:
  DEFINE_SIZE_STATIC (8);
};

#define REGION_CACHE_ITEM_CACHE_INVALID 2.f

struct VarRegionList
{
  using cache_t = float;

  float evaluate (unsigned int region_index,
		  const int *coords, unsigned int coord_len,
		  cache_t *cache = nullptr) const
  {
    if (unlikely (region_index >= regionCount))
      return 0.;

    float *cached_value = nullptr;
    if (cache)
    {
      cached_value = &(cache[region_index]);
      if (likely (*cached_value != REGION_CACHE_ITEM_CACHE_INVALID))
	return *cached_value;
    }

    const VarRegionAxis *axes = axesZ.arrayZ + (region_index * axisCount);

    float v = 1.;
    unsigned int count = axisCount;
    for (unsigned int i = 0; i < count; i++)
    {
      int coord = i < coord_len ? coords[i] : 0;
      float factor = axes[i].evaluate (coord);
      if (factor == 0.f)
      {
        if (cache)
	  *cached_value = 0.;
	return 0.;
      }
      v *= factor;
    }

    if (cache)
      *cached_value = v;
    return v;
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
		  axesZ.sanitize (c, axisCount * regionCount));
  }

  bool serialize (hb_serialize_context_t *c,
                  const hb_vector_t<hb_tag_t>& axis_tags,
                  const hb_vector_t<const hb_hashmap_t<hb_tag_t, Triple>*>& regions)
  {
    TRACE_SERIALIZE (this);
    unsigned axis_count = axis_tags.length;
    unsigned region_count = regions.length;
    if (!axis_count || !region_count) return_trace (false);
    if (unlikely (hb_unsigned_mul_overflows (axis_count * region_count,
                                             VarRegionAxis::static_size))) return_trace (false);
    if (unlikely (!c->extend_min (this))) return_trace (false);
    axisCount = axis_count;
    regionCount = region_count;

    for (unsigned r = 0; r < region_count; r++)
    {
      const auto& region = regions[r];
      for (unsigned i = 0; i < axis_count; i++)
      {
        hb_tag_t tag = axis_tags.arrayZ[i];
        VarRegionAxis var_region_rec;
        Triple *coords;
        if (region->has (tag, &coords))
        {
          var_region_rec.startCoord.set_float (coords->minimum);
          var_region_rec.peakCoord.set_float (coords->middle);
          var_region_rec.endCoord.set_float (coords->maximum);
        }
        else
        {
          var_region_rec.startCoord.set_int (0);
          var_region_rec.peakCoord.set_int (0);
          var_region_rec.endCoord.set_int (0);
        }
        if (!var_region_rec.serialize (c))
          return_trace (false);
      }
    }
    return_trace (true);
  }

  bool serialize (hb_serialize_context_t *c, const VarRegionList *src, const hb_inc_bimap_t &region_map)
  {
    TRACE_SERIALIZE (this);
    if (unlikely (!c->extend_min (this))) return_trace (false);
    axisCount = src->axisCount;
    regionCount = region_map.get_population ();
    if (unlikely (hb_unsigned_mul_overflows (axisCount * regionCount,
					     VarRegionAxis::static_size))) return_trace (false);
    if (unlikely (!c->extend (this))) return_trace (false);
    unsigned int region_count = src->regionCount;
    for (unsigned int r = 0; r < regionCount; r++)
    {
      unsigned int backward = region_map.backward (r);
      if (backward >= region_count) return_trace (false);
      hb_memcpy (&axesZ[axisCount * r], &src->axesZ[axisCount * backward], VarRegionAxis::static_size * axisCount);
    }

    return_trace (true);
  }

  bool get_var_region (unsigned region_index,
                       const hb_map_t& axes_old_index_tag_map,
                       hb_hashmap_t<hb_tag_t, Triple>& axis_tuples /* OUT */) const
  {
    if (region_index >= regionCount) return false;
    const VarRegionAxis* axis_region = axesZ.arrayZ + (region_index * axisCount);
    for (unsigned i = 0; i < axisCount; i++)
    {
      hb_tag_t *axis_tag;
      if (!axes_old_index_tag_map.has (i, &axis_tag))
        return false;

      float min_val = axis_region->startCoord.to_float ();
      float def_val = axis_region->peakCoord.to_float ();
      float max_val = axis_region->endCoord.to_float ();

      if (def_val != 0.f)
        axis_tuples.set (*axis_tag, Triple ((double) min_val, (double) def_val, (double) max_val));
      axis_region++;
    }
    return !axis_tuples.in_error ();
  }

  bool get_var_regions (const hb_map_t& axes_old_index_tag_map,
                        hb_vector_t<hb_hashmap_t<hb_tag_t, Triple>>& regions /* OUT */) const
  {
    if (!regions.alloc (regionCount))
      return false;

    for (unsigned i = 0; i < regionCount; i++)
    {
      hb_hashmap_t<hb_tag_t, Triple> axis_tuples;
      if (!get_var_region (i, axes_old_index_tag_map, axis_tuples))
        return false;
      regions.push (std::move (axis_tuples));
    }
    return !regions.in_error ();
  }

  unsigned int get_size () const { return min_size + VarRegionAxis::static_size * axisCount * regionCount; }

  public:
  HBUINT16	axisCount;
  HBUINT15	regionCount;
  protected:
  UnsizedArrayOf<VarRegionAxis>
		axesZ;
  public:
  DEFINE_SIZE_ARRAY (4, axesZ);
};

struct SparseVariationRegion : Array16Of<SparseVarRegionAxis>
{
  float evaluate (const int *coords, unsigned int coord_len) const
  {
    float v = 1.f;
    unsigned int count = len;
    for (unsigned int i = 0; i < count; i++)
    {
      float factor = arrayZ[i].evaluate (coords, coord_len);
      if (factor == 0.f)
	return 0.;
      v *= factor;
    }
    return v;
  }
};

struct SparseVarRegionList
{
  using cache_t = float;

  float evaluate (unsigned int region_index,
		  const int *coords, unsigned int coord_len,
		  cache_t *cache = nullptr) const
  {
    if (unlikely (region_index >= regions.len))
      return 0.;

    float *cached_value = nullptr;
    if (cache)
    {
      cached_value = &(cache[region_index]);
      if (likely (*cached_value != REGION_CACHE_ITEM_CACHE_INVALID))
	return *cached_value;
    }

    const SparseVariationRegion &region = this+regions[region_index];

    float v = region.evaluate (coords, coord_len);

    if (cache)
      *cached_value = v;
    return v;
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (regions.sanitize (c, this));
  }

  public:
  Array16Of<Offset32To<SparseVariationRegion>>
		regions;
  public:
  DEFINE_SIZE_ARRAY (2, regions);
};


struct VarData
{
  unsigned int get_item_count () const
  { return itemCount; }

  unsigned int get_region_index_count () const
  { return regionIndices.len; }

  unsigned get_region_index (unsigned i) const
  { return i >= regionIndices.len ? -1 : regionIndices[i]; }

  unsigned int get_row_size () const
  { return (wordCount () + regionIndices.len) * (longWords () ? 2 : 1); }

  unsigned int get_size () const
  { return min_size
	 - regionIndices.min_size + regionIndices.get_size ()
	 + itemCount * get_row_size ();
  }

  float get_delta (unsigned int inner,
		   const int *coords, unsigned int coord_count,
		   const VarRegionList &regions,
		   VarRegionList::cache_t *cache = nullptr) const
  {
    if (unlikely (inner >= itemCount))
      return 0.;

   unsigned int count = regionIndices.len;
   bool is_long = longWords ();
   unsigned word_count = wordCount ();
   unsigned int scount = is_long ? count : word_count;
   unsigned int lcount = is_long ? word_count : 0;

   const HBUINT8 *bytes = get_delta_bytes ();
   const HBUINT8 *row = bytes + inner * get_row_size ();

   float delta = 0.;
   unsigned int i = 0;

   const HBINT32 *lcursor = reinterpret_cast<const HBINT32 *> (row);
   for (; i < lcount; i++)
   {
     float scalar = regions.evaluate (regionIndices.arrayZ[i], coords, coord_count, cache);
     delta += scalar * *lcursor++;
   }
   const HBINT16 *scursor = reinterpret_cast<const HBINT16 *> (lcursor);
   for (; i < scount; i++)
   {
     float scalar = regions.evaluate (regionIndices.arrayZ[i], coords, coord_count, cache);
     delta += scalar * *scursor++;
   }
   const HBINT8 *bcursor = reinterpret_cast<const HBINT8 *> (scursor);
   for (; i < count; i++)
   {
     float scalar = regions.evaluate (regionIndices.arrayZ[i], coords, coord_count, cache);
     delta += scalar * *bcursor++;
   }

   return delta;
  }

  void get_region_scalars (const int *coords, unsigned int coord_count,
			   const VarRegionList &regions,
			   float *scalars /*OUT */,
			   unsigned int num_scalars) const
  {
    unsigned count = hb_min (num_scalars, regionIndices.len);
    for (unsigned int i = 0; i < count; i++)
      scalars[i] = regions.evaluate (regionIndices.arrayZ[i], coords, coord_count);
    for (unsigned int i = count; i < num_scalars; i++)
      scalars[i] = 0.f;
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  regionIndices.sanitize (c) &&
		  hb_barrier () &&
		  wordCount () <= regionIndices.len &&
		  c->check_range (get_delta_bytes (),
				  itemCount,
				  get_row_size ()));
  }

  bool serialize (hb_serialize_context_t *c,
                  bool has_long,
                  const hb_vector_t<const hb_vector_t<int>*>& rows)
  {
    TRACE_SERIALIZE (this);
    if (unlikely (!c->extend_min (this))) return_trace (false);
    unsigned row_count = rows.length;
    itemCount = row_count;

    int min_threshold = has_long ? -65536 : -128;
    int max_threshold = has_long ? +65535 : +127;
    enum delta_size_t { kZero=0, kNonWord, kWord };
    hb_vector_t<delta_size_t> delta_sz;
    unsigned num_regions = rows[0]->length;
    if (!delta_sz.resize (num_regions))
      return_trace (false);

    unsigned word_count = 0;
    for (unsigned r = 0; r < num_regions; r++)
    {
      for (unsigned i = 0; i < row_count; i++)
      {
        int delta = rows[i]->arrayZ[r];
        if (delta < min_threshold || delta > max_threshold)
        {
          delta_sz[r] = kWord;
          word_count++;
          break;
        }
        else if (delta != 0)
        {
          delta_sz[r] = kNonWord;
        }
      }
    }

    /* reorder regions: words and then non-words*/
    unsigned word_index = 0;
    unsigned non_word_index = word_count;
    hb_map_t ri_map;
    for (unsigned r = 0; r < num_regions; r++)
    {
      if (!delta_sz[r]) continue;
      unsigned new_r = (delta_sz[r] == kWord)? word_index++ : non_word_index++;
      if (!ri_map.set (new_r, r))
        return_trace (false);
    }

    wordSizeCount = word_count | (has_long ? 0x8000u /* LONG_WORDS */ : 0);

    unsigned ri_count = ri_map.get_population ();
    regionIndices.len = ri_count;
    if (unlikely (!c->extend (this))) return_trace (false);

    for (unsigned r = 0; r < ri_count; r++)
    {
      hb_codepoint_t *idx;
      if (!ri_map.has (r, &idx))
        return_trace (false);
      regionIndices[r] = *idx;
    }

    HBUINT8 *delta_bytes = get_delta_bytes ();
    unsigned row_size = get_row_size ();
    for (unsigned int i = 0; i < row_count; i++)
    {
      for (unsigned int r = 0; r < ri_count; r++)
      {
        int delta = rows[i]->arrayZ[ri_map[r]];
        set_item_delta_fast (i, r, delta, delta_bytes, row_size);
      }
    }
    return_trace (true);
  }

  bool serialize (hb_serialize_context_t *c,
		  const VarData *src,
		  const hb_inc_bimap_t &inner_map,
		  const hb_inc_bimap_t &region_map)
  {
    TRACE_SERIALIZE (this);
    if (unlikely (!c->extend_min (this))) return_trace (false);
    itemCount = inner_map.get_next_value ();

    /* Optimize word count */
    unsigned ri_count = src->regionIndices.len;
    enum delta_size_t { kZero=0, kNonWord, kWord };
    hb_vector_t<delta_size_t> delta_sz;
    hb_vector_t<unsigned int> ri_map;	/* maps new index to old index */
    delta_sz.resize (ri_count);
    ri_map.resize (ri_count);
    unsigned int new_word_count = 0;
    unsigned int r;

    const HBUINT8 *src_delta_bytes = src->get_delta_bytes ();
    unsigned src_row_size = src->get_row_size ();
    unsigned src_word_count = src->wordCount ();
    bool     src_long_words = src->longWords ();

    bool has_long = false;
    if (src_long_words)
    {
      for (r = 0; r < src_word_count; r++)
      {
        for (unsigned old_gid : inner_map.keys())
	{
	  int32_t delta = src->get_item_delta_fast (old_gid, r, src_delta_bytes, src_row_size);
	  if (delta < -65536 || 65535 < delta)
	  {
	    has_long = true;
	    break;
	  }
        }
      }
    }

    signed min_threshold = has_long ? -65536 : -128;
    signed max_threshold = has_long ? +65535 : +127;
    for (r = 0; r < ri_count; r++)
    {
      bool short_circuit = src_long_words == has_long && src_word_count <= r;

      delta_sz[r] = kZero;
      for (unsigned old_gid : inner_map.keys())
      {
	int32_t delta = src->get_item_delta_fast (old_gid, r, src_delta_bytes, src_row_size);
	if (delta < min_threshold || max_threshold < delta)
	{
	  delta_sz[r] = kWord;
	  new_word_count++;
	  break;
	}
	else if (delta != 0)
	{
	  delta_sz[r] = kNonWord;
	  if (short_circuit)
	    break;
	}
      }
    }

    unsigned int word_index = 0;
    unsigned int non_word_index = new_word_count;
    unsigned int new_ri_count = 0;
    for (r = 0; r < ri_count; r++)
      if (delta_sz[r])
      {
	unsigned new_r = (delta_sz[r] == kWord)? word_index++ : non_word_index++;
	ri_map[new_r] = r;
	new_ri_count++;
      }

    wordSizeCount = new_word_count | (has_long ? 0x8000u /* LONG_WORDS */ : 0);

    regionIndices.len = new_ri_count;

    if (unlikely (!c->extend (this))) return_trace (false);

    for (r = 0; r < new_ri_count; r++)
      regionIndices[r] = region_map[src->regionIndices[ri_map[r]]];

    HBUINT8 *delta_bytes = get_delta_bytes ();
    unsigned row_size = get_row_size ();
    unsigned count = itemCount;
    for (unsigned int i = 0; i < count; i++)
    {
      unsigned int old = inner_map.backward (i);
      for (unsigned int r = 0; r < new_ri_count; r++)
	set_item_delta_fast (i, r,
			     src->get_item_delta_fast (old, ri_map[r],
						       src_delta_bytes, src_row_size),
			     delta_bytes, row_size);
    }

    return_trace (true);
  }

  void collect_region_refs (hb_set_t &region_indices, const hb_inc_bimap_t &inner_map) const
  {
    const HBUINT8 *delta_bytes = get_delta_bytes ();
    unsigned row_size = get_row_size ();

    for (unsigned int r = 0; r < regionIndices.len; r++)
    {
      unsigned int region = regionIndices.arrayZ[r];
      if (region_indices.has (region)) continue;
      for (hb_codepoint_t old_gid : inner_map.keys())
	if (get_item_delta_fast (old_gid, r, delta_bytes, row_size) != 0)
	{
	  region_indices.add (region);
	  break;
	}
    }
  }

  public:
  const HBUINT8 *get_delta_bytes () const
  { return &StructAfter<HBUINT8> (regionIndices); }

  protected:
  HBUINT8 *get_delta_bytes ()
  { return &StructAfter<HBUINT8> (regionIndices); }

  public:
  int32_t get_item_delta_fast (unsigned int item, unsigned int region,
			       const HBUINT8 *delta_bytes, unsigned row_size) const
  {
    if (unlikely (item >= itemCount || region >= regionIndices.len)) return 0;

    const HBINT8 *p = (const HBINT8 *) delta_bytes + item * row_size;
    unsigned word_count = wordCount ();
    bool is_long = longWords ();
    if (is_long)
    {
      if (region < word_count)
	return ((const HBINT32 *) p)[region];
      else
	return ((const HBINT16 *)(p + HBINT32::static_size * word_count))[region - word_count];
    }
    else
    {
      if (region < word_count)
	return ((const HBINT16 *) p)[region];
      else
	return (p + HBINT16::static_size * word_count)[region - word_count];
    }
  }
  int32_t get_item_delta (unsigned int item, unsigned int region) const
  {
     return get_item_delta_fast (item, region,
				 get_delta_bytes (),
				 get_row_size ());
  }

  protected:
  void set_item_delta_fast (unsigned int item, unsigned int region, int32_t delta,
			    HBUINT8 *delta_bytes, unsigned row_size)
  {
    HBINT8 *p = (HBINT8 *) delta_bytes + item * row_size;
    unsigned word_count = wordCount ();
    bool is_long = longWords ();
    if (is_long)
    {
      if (region < word_count)
	((HBINT32 *) p)[region] = delta;
      else
	((HBINT16 *)(p + HBINT32::static_size * word_count))[region - word_count] = delta;
    }
    else
    {
      if (region < word_count)
	((HBINT16 *) p)[region] = delta;
      else
	(p + HBINT16::static_size * word_count)[region - word_count] = delta;
    }
  }
  void set_item_delta (unsigned int item, unsigned int region, int32_t delta)
  {
    set_item_delta_fast (item, region, delta,
			 get_delta_bytes (),
			 get_row_size ());
  }

  bool longWords () const { return wordSizeCount & 0x8000u /* LONG_WORDS */; }
  unsigned wordCount () const { return wordSizeCount & 0x7FFFu /* WORD_DELTA_COUNT_MASK */; }

  protected:
  HBUINT16		itemCount;
  HBUINT16		wordSizeCount;
  Array16Of<HBUINT16>	regionIndices;
/*UnsizedArrayOf<HBUINT8>bytesX;*/
  public:
  DEFINE_SIZE_ARRAY (6, regionIndices);
};

struct MultiVarData
{
  unsigned int get_size () const
  { return min_size
	 - regionIndices.min_size + regionIndices.get_size ()
	 + StructAfter<CFF2Index> (regionIndices).get_size ();
  }

  void get_delta (unsigned int inner,
		  const int *coords, unsigned int coord_count,
		  const SparseVarRegionList &regions,
		  hb_array_t<float> out,
		  SparseVarRegionList::cache_t *cache = nullptr) const
  {
    auto &deltaSets = StructAfter<decltype (deltaSetsX)> (regionIndices);

    auto values_iter = deltaSets[inner];

    unsigned regionCount = regionIndices.len;
    unsigned count = out.length;
    for (unsigned regionIndex = 0; regionIndex < regionCount; regionIndex++)
    {
      float scalar = regions.evaluate (regionIndices.arrayZ[regionIndex],
				       coords, coord_count,
				       cache);
      if (scalar == 1.f)
	for (unsigned i = 0; i < count; i++)
	  out.arrayZ[i] += *values_iter++;
      else if (scalar)
	for (unsigned i = 0; i < count; i++)
	  out.arrayZ[i] += *values_iter++ * scalar;
      else
        values_iter += count;
    }
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (format.sanitize (c) &&
		  hb_barrier () &&
		  format == 1 &&
		  regionIndices.sanitize (c) &&
		  hb_barrier () &&
		  StructAfter<decltype (deltaSetsX)> (regionIndices).sanitize (c));
  }

  protected:
  HBUINT8	      format; // 1
  Array16Of<HBUINT16> regionIndices;
  TupleList	      deltaSetsX;
  public:
  DEFINE_SIZE_MIN (8);
};

struct ItemVariationStore
{
  friend struct item_variations_t;
  using cache_t = VarRegionList::cache_t;

  cache_t *create_cache () const
  {
#ifdef HB_NO_VAR
    return nullptr;
#endif
    auto &r = this+regions;
    unsigned count = r.regionCount;

    float *cache = (float *) hb_malloc (sizeof (float) * count);
    if (unlikely (!cache)) return nullptr;

    for (unsigned i = 0; i < count; i++)
      cache[i] = REGION_CACHE_ITEM_CACHE_INVALID;

    return cache;
  }

  static void destroy_cache (cache_t *cache) { hb_free (cache); }

  private:
  float get_delta (unsigned int outer, unsigned int inner,
		   const int *coords, unsigned int coord_count,
		   VarRegionList::cache_t *cache = nullptr) const
  {
#ifdef HB_NO_VAR
    return 0.f;
#endif

    if (unlikely (outer >= dataSets.len))
      return 0.f;

    return (this+dataSets[outer]).get_delta (inner,
					     coords, coord_count,
					     this+regions,
					     cache);
  }

  public:
  float get_delta (unsigned int index,
		   const int *coords, unsigned int coord_count,
		   VarRegionList::cache_t *cache = nullptr) const
  {
    unsigned int outer = index >> 16;
    unsigned int inner = index & 0xFFFF;
    return get_delta (outer, inner, coords, coord_count, cache);
  }
  float get_delta (unsigned int index,
		   hb_array_t<const int> coords,
		   VarRegionList::cache_t *cache = nullptr) const
  {
    return get_delta (index,
		      coords.arrayZ, coords.length,
		      cache);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
#ifdef HB_NO_VAR
    return true;
#endif

    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
		  format == 1 &&
		  regions.sanitize (c, this) &&
		  dataSets.sanitize (c, this));
  }

  bool serialize (hb_serialize_context_t *c,
                  bool has_long,
                  const hb_vector_t<hb_tag_t>& axis_tags,
                  const hb_vector_t<const hb_hashmap_t<hb_tag_t, Triple>*>& region_list,
                  const hb_vector_t<delta_row_encoding_t>& vardata_encodings)
  {
    TRACE_SERIALIZE (this);
#ifdef HB_NO_VAR
    return_trace (false);
#endif
    if (unlikely (!c->extend_min (this))) return_trace (false);

    format = 1;
    if (!regions.serialize_serialize (c, axis_tags, region_list))
      return_trace (false);

    unsigned num_var_data = vardata_encodings.length;
    if (!num_var_data) return_trace (false);
    if (unlikely (!c->check_assign (dataSets.len, num_var_data,
                                    HB_SERIALIZE_ERROR_INT_OVERFLOW)))
      return_trace (false);

    if (unlikely (!c->extend (dataSets))) return_trace (false);
    for (unsigned i = 0; i < num_var_data; i++)
      if (!dataSets[i].serialize_serialize (c, has_long, vardata_encodings[i].items))
        return_trace (false);

    return_trace (true);
  }

  bool serialize (hb_serialize_context_t *c,
		  const ItemVariationStore *src,
		  const hb_array_t <const hb_inc_bimap_t> &inner_maps)
  {
    TRACE_SERIALIZE (this);
#ifdef HB_NO_VAR
    return_trace (false);
#endif

    if (unlikely (!c->extend_min (this))) return_trace (false);

    unsigned int set_count = 0;
    for (unsigned int i = 0; i < inner_maps.length; i++)
      if (inner_maps[i].get_population ())
	set_count++;

    format = 1;

    const auto &src_regions = src+src->regions;

    hb_set_t region_indices;
    for (unsigned int i = 0; i < inner_maps.length; i++)
      (src+src->dataSets[i]).collect_region_refs (region_indices, inner_maps[i]);

    if (region_indices.in_error ())
      return_trace (false);

    region_indices.del_range ((src_regions).regionCount, hb_set_t::INVALID);

    /* TODO use constructor when our data-structures support that. */
    hb_inc_bimap_t region_map;
    + hb_iter (region_indices)
    | hb_apply ([&region_map] (unsigned _) { region_map.add(_); })
    ;
    if (region_map.in_error())
      return_trace (false);

    if (unlikely (!regions.serialize_serialize (c, &src_regions, region_map)))
      return_trace (false);

    dataSets.len = set_count;
    if (unlikely (!c->extend (dataSets))) return_trace (false);

    /* TODO: The following code could be simplified when
     * List16OfOffset16To::subset () can take a custom param to be passed to VarData::serialize () */
    unsigned int set_index = 0;
    for (unsigned int i = 0; i < inner_maps.length; i++)
    {
      if (!inner_maps[i].get_population ()) continue;
      if (unlikely (!dataSets[set_index++]
		     .serialize_serialize (c, &(src+src->dataSets[i]), inner_maps[i], region_map)))
	return_trace (false);
    }

    return_trace (true);
  }

  ItemVariationStore *copy (hb_serialize_context_t *c) const
  {
    TRACE_SERIALIZE (this);
    auto *out = c->start_embed (this);
    if (unlikely (!out)) return_trace (nullptr);

    hb_vector_t <hb_inc_bimap_t> inner_maps;
    unsigned count = dataSets.len;
    for (unsigned i = 0; i < count; i++)
    {
      hb_inc_bimap_t *map = inner_maps.push ();
      if (!c->propagate_error(inner_maps))
        return_trace(nullptr);
      auto &data = this+dataSets[i];

      unsigned itemCount = data.get_item_count ();
      for (unsigned j = 0; j < itemCount; j++)
	map->add (j);
    }

    if (unlikely (!out->serialize (c, this, inner_maps))) return_trace (nullptr);

    return_trace (out);
  }

  bool subset (hb_subset_context_t *c, const hb_array_t<const hb_inc_bimap_t> &inner_maps) const
  {
    TRACE_SUBSET (this);
#ifdef HB_NO_VAR
    return_trace (false);
#endif

    ItemVariationStore *varstore_prime = c->serializer->start_embed<ItemVariationStore> ();
    if (unlikely (!varstore_prime)) return_trace (false);

    varstore_prime->serialize (c->serializer, this, inner_maps);

    return_trace (
        !c->serializer->in_error()
        && varstore_prime->dataSets);
  }

  unsigned int get_region_index_count (unsigned int major) const
  {
#ifdef HB_NO_VAR
    return 0;
#endif
    return (this+dataSets[major]).get_region_index_count ();
  }

  void get_region_scalars (unsigned int major,
			   const int *coords, unsigned int coord_count,
			   float *scalars /*OUT*/,
			   unsigned int num_scalars) const
  {
#ifdef HB_NO_VAR
    for (unsigned i = 0; i < num_scalars; i++)
      scalars[i] = 0.f;
    return;
#endif

    (this+dataSets[major]).get_region_scalars (coords, coord_count,
					       this+regions,
					       &scalars[0], num_scalars);
  }

  unsigned int get_sub_table_count () const
   {
#ifdef HB_NO_VAR
     return 0;
#endif
     return dataSets.len;
   }

  const VarData& get_sub_table (unsigned i) const
  {
#ifdef HB_NO_VAR
     return Null (VarData);
#endif
     return this+dataSets[i];
  }

  const VarRegionList& get_region_list () const
  {
#ifdef HB_NO_VAR
     return Null (VarRegionList);
#endif
     return this+regions;
  }

  protected:
  HBUINT16				format;
  Offset32To<VarRegionList>		regions;
  Array16OfOffset32To<VarData>		dataSets;
  public:
  DEFINE_SIZE_ARRAY_SIZED (8, dataSets);
};

struct MultiItemVariationStore
{
  using cache_t = SparseVarRegionList::cache_t;

  cache_t *create_cache () const
  {
#ifdef HB_NO_VAR
    return nullptr;
#endif
    auto &r = this+regions;
    unsigned count = r.regions.len;

    float *cache = (float *) hb_malloc (sizeof (float) * count);
    if (unlikely (!cache)) return nullptr;

    for (unsigned i = 0; i < count; i++)
      cache[i] = REGION_CACHE_ITEM_CACHE_INVALID;

    return cache;
  }

  static void destroy_cache (cache_t *cache) { hb_free (cache); }

  private:
  void get_delta (unsigned int outer, unsigned int inner,
		  const int *coords, unsigned int coord_count,
		  hb_array_t<float> out,
		  VarRegionList::cache_t *cache = nullptr) const
  {
#ifdef HB_NO_VAR
    return;
#endif

    if (unlikely (outer >= dataSets.len))
      return;

    return (this+dataSets[outer]).get_delta (inner,
					     coords, coord_count,
					     this+regions,
					     out,
					     cache);
  }

  public:
  void get_delta (unsigned int index,
		  const int *coords, unsigned int coord_count,
		  hb_array_t<float> out,
		  VarRegionList::cache_t *cache = nullptr) const
  {
    unsigned int outer = index >> 16;
    unsigned int inner = index & 0xFFFF;
    get_delta (outer, inner, coords, coord_count, out, cache);
  }
  void get_delta (unsigned int index,
		  hb_array_t<const int> coords,
		  hb_array_t<float> out,
		  VarRegionList::cache_t *cache = nullptr) const
  {
    return get_delta (index,
		      coords.arrayZ, coords.length,
		      out,
		      cache);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
#ifdef HB_NO_VAR
    return true;
#endif

    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
		  format == 1 &&
		  regions.sanitize (c, this) &&
		  dataSets.sanitize (c, this));
  }

  protected:
  HBUINT16				format; // 1
  Offset32To<SparseVarRegionList>	regions;
  Array16OfOffset32To<MultiVarData>	dataSets;
  public:
  DEFINE_SIZE_ARRAY_SIZED (8, dataSets);
};

#undef REGION_CACHE_ITEM_CACHE_INVALID

template <typename MapCountT>
struct DeltaSetIndexMapFormat01
{
  friend struct DeltaSetIndexMap;

  unsigned get_size () const
  { return min_size + mapCount * get_width (); }

  private:
  DeltaSetIndexMapFormat01* copy (hb_serialize_context_t *c) const
  {
    TRACE_SERIALIZE (this);
    return_trace (c->embed (this));
  }

  template <typename T>
  bool serialize (hb_serialize_context_t *c, const T &plan)
  {
    unsigned int width = plan.get_width ();
    unsigned int inner_bit_count = plan.get_inner_bit_count ();
    const hb_array_t<const uint32_t> output_map = plan.get_output_map ();

    TRACE_SERIALIZE (this);
    if (unlikely (output_map.length && ((((inner_bit_count-1)&~0xF)!=0) || (((width-1)&~0x3)!=0))))
      return_trace (false);
    if (unlikely (!c->extend_min (this))) return_trace (false);

    entryFormat = ((width-1)<<4)|(inner_bit_count-1);
    mapCount = output_map.length;
    HBUINT8 *p = c->allocate_size<HBUINT8> (width * output_map.length);
    if (unlikely (!p)) return_trace (false);
    for (unsigned int i = 0; i < output_map.length; i++)
    {
      unsigned int v = output_map.arrayZ[i];
      if (v)
      {
	unsigned int outer = v >> 16;
	unsigned int inner = v & 0xFFFF;
	unsigned int u = (outer << inner_bit_count) | inner;
	for (unsigned int w = width; w > 0;)
	{
	  p[--w] = u;
	  u >>= 8;
	}
      }
      p += width;
    }
    return_trace (true);
  }

  uint32_t map (unsigned int v) const /* Returns 16.16 outer.inner. */
  {
    /* If count is zero, pass value unchanged.  This takes
     * care of direct mapping for advance map. */
    if (!mapCount)
      return v;

    if (v >= mapCount)
      v = mapCount - 1;

    unsigned int u = 0;
    { /* Fetch it. */
      unsigned int w = get_width ();
      const HBUINT8 *p = mapDataZ.arrayZ + w * v;
      for (; w; w--)
        u = (u << 8) + *p++;
    }

    { /* Repack it. */
      unsigned int n = get_inner_bit_count ();
      unsigned int outer = u >> n;
      unsigned int inner = u & ((1 << n) - 1);
      u = (outer<<16) | inner;
    }

    return u;
  }

  unsigned get_map_count () const       { return mapCount; }
  unsigned get_width () const           { return ((entryFormat >> 4) & 3) + 1; }
  unsigned get_inner_bit_count () const { return (entryFormat & 0xF) + 1; }


  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
                  c->check_range (mapDataZ.arrayZ,
                                  mapCount,
                                  get_width ()));
  }

  protected:
  HBUINT8       format;         /* Format identifier--format = 0 */
  HBUINT8       entryFormat;    /* A packed field that describes the compressed
                                 * representation of delta-set indices. */
  MapCountT     mapCount;       /* The number of mapping entries. */
  UnsizedArrayOf<HBUINT8>
                mapDataZ;       /* The delta-set index mapping data. */

  public:
  DEFINE_SIZE_ARRAY (2+MapCountT::static_size, mapDataZ);
};

struct DeltaSetIndexMap
{
  template <typename T>
  bool serialize (hb_serialize_context_t *c, const T &plan)
  {
    TRACE_SERIALIZE (this);
    unsigned length = plan.get_output_map ().length;
    u.format = length <= 0xFFFF ? 0 : 1;
    switch (u.format) {
    case 0: hb_barrier (); return_trace (u.format0.serialize (c, plan));
    case 1: hb_barrier (); return_trace (u.format1.serialize (c, plan));
    default:return_trace (false);
    }
  }

  uint32_t map (unsigned v) const
  {
    switch (u.format) {
    case 0: hb_barrier (); return (u.format0.map (v));
    case 1: hb_barrier (); return (u.format1.map (v));
    default:return v;
    }
  }

  unsigned get_map_count () const
  {
    switch (u.format) {
    case 0: hb_barrier (); return u.format0.get_map_count ();
    case 1: hb_barrier (); return u.format1.get_map_count ();
    default:return 0;
    }
  }

  unsigned get_width () const
  {
    switch (u.format) {
    case 0: hb_barrier (); return u.format0.get_width ();
    case 1: hb_barrier (); return u.format1.get_width ();
    default:return 0;
    }
  }

  unsigned get_inner_bit_count () const
  {
    switch (u.format) {
    case 0: hb_barrier (); return u.format0.get_inner_bit_count ();
    case 1: hb_barrier (); return u.format1.get_inner_bit_count ();
    default:return 0;
    }
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (!u.format.sanitize (c)) return_trace (false);
    hb_barrier ();
    switch (u.format) {
    case 0: hb_barrier (); return_trace (u.format0.sanitize (c));
    case 1: hb_barrier (); return_trace (u.format1.sanitize (c));
    default:return_trace (true);
    }
  }

  DeltaSetIndexMap* copy (hb_serialize_context_t *c) const
  {
    TRACE_SERIALIZE (this);
    switch (u.format) {
    case 0: hb_barrier (); return_trace (reinterpret_cast<DeltaSetIndexMap *> (u.format0.copy (c)));
    case 1: hb_barrier (); return_trace (reinterpret_cast<DeltaSetIndexMap *> (u.format1.copy (c)));
    default:return_trace (nullptr);
    }
  }

  protected:
  union {
  HBUINT8                            format;         /* Format identifier */
  DeltaSetIndexMapFormat01<HBUINT16> format0;
  DeltaSetIndexMapFormat01<HBUINT32> format1;
  } u;
  public:
  DEFINE_SIZE_UNION (1, format);
};


struct ItemVarStoreInstancer
{
  ItemVarStoreInstancer (const ItemVariationStore *varStore,
			 const DeltaSetIndexMap *varIdxMap,
			 hb_array_t<const int> coords,
			 VarRegionList::cache_t *cache = nullptr) :
    varStore (varStore), varIdxMap (varIdxMap), coords (coords), cache (cache)
  {
    if (!varStore)
      varStore = &Null(ItemVariationStore);
  }

  operator bool () const { return varStore && bool (coords); }

  float operator[] (uint32_t varIdx) const
  { return (*this) (varIdx); }

  float operator() (uint32_t varIdx, unsigned short offset = 0) const
  {
    if (varIdxMap)
      varIdx = varIdxMap->map (VarIdx::add (varIdx, offset));
    else
      varIdx += offset;
    return coords ? varStore->get_delta (varIdx, coords, cache) : 0.f;
  }

  const ItemVariationStore *varStore;
  const DeltaSetIndexMap *varIdxMap;
  hb_array_t<const int> coords;
  VarRegionList::cache_t *cache;
};

struct MultiItemVarStoreInstancer
{
  MultiItemVarStoreInstancer (const MultiItemVariationStore *varStore,
			      const DeltaSetIndexMap *varIdxMap,
			      hb_array_t<const int> coords,
			      SparseVarRegionList::cache_t *cache = nullptr) :
    varStore (varStore), varIdxMap (varIdxMap), coords (coords), cache (cache)
  {
    if (!varStore)
      varStore = &Null(MultiItemVariationStore);
  }

  operator bool () const { return varStore && bool (coords); }

  float operator[] (uint32_t varIdx) const
  {
    float v = 0;
    (*this) (hb_array (&v, 1), varIdx);
    return v;
  }

  void operator() (hb_array_t<float> out, uint32_t varIdx, unsigned short offset = 0) const
  {
    if (coords)
    {
      if (varIdxMap)
	varIdx = varIdxMap->map (VarIdx::add (varIdx, offset));
      else
	varIdx += offset;
      varStore->get_delta (varIdx, coords, out, cache);
    }
    else
      for (unsigned i = 0; i < out.length; i++)
        out.arrayZ[i] = 0.f;
  }

  const MultiItemVariationStore *varStore;
  const DeltaSetIndexMap *varIdxMap;
  hb_array_t<const int> coords;
  SparseVarRegionList::cache_t *cache;
};


/*
 * Feature Variations
 */
enum Cond_with_Var_flag_t
{
  KEEP_COND_WITH_VAR = 0,
  KEEP_RECORD_WITH_VAR = 1,
  DROP_COND_WITH_VAR = 2,
  DROP_RECORD_WITH_VAR = 3,
};

struct Condition;

template <typename Instancer>
static bool
_hb_recurse_condition_evaluate (const struct Condition &condition,
				const int *coords,
				unsigned int coord_len,
				Instancer *instancer);

struct ConditionAxisRange
{
  friend struct Condition;

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    const hb_map_t *index_map = &c->plan->axes_index_map;
    if (index_map->is_empty ()) return_trace (true);

    const hb_map_t& axes_old_index_tag_map = c->plan->axes_old_index_tag_map;
    hb_codepoint_t *axis_tag;
    if (!axes_old_index_tag_map.has (axisIndex, &axis_tag) ||
        !index_map->has (axisIndex))
      return_trace (false);

    const hb_hashmap_t<hb_tag_t, Triple>& normalized_axes_location = c->plan->axes_location;
    Triple axis_limit{-1.0, 0.0, 1.0};
    Triple *normalized_limit;
    if (normalized_axes_location.has (*axis_tag, &normalized_limit))
      axis_limit = *normalized_limit;

    const hb_hashmap_t<hb_tag_t, TripleDistances>& axes_triple_distances = c->plan->axes_triple_distances;
    TripleDistances axis_triple_distances{1.0, 1.0};
    TripleDistances *triple_dists;
    if (axes_triple_distances.has (*axis_tag, &triple_dists))
      axis_triple_distances = *triple_dists;

    float normalized_min = renormalizeValue ((double) filterRangeMinValue.to_float (), axis_limit, axis_triple_distances, false);
    float normalized_max = renormalizeValue ((double) filterRangeMaxValue.to_float (), axis_limit, axis_triple_distances, false);
    out->filterRangeMinValue.set_float (normalized_min);
    out->filterRangeMaxValue.set_float (normalized_max);

    return_trace (c->serializer->check_assign (out->axisIndex, index_map->get (axisIndex),
                                               HB_SERIALIZE_ERROR_INT_OVERFLOW));
  }

  private:
  Cond_with_Var_flag_t keep_with_variations (hb_collect_feature_substitutes_with_var_context_t *c,
                                             hb_map_t *condition_map /* OUT */) const
  {
    //invalid axis index, drop the entire record
    if (!c->axes_index_tag_map->has (axisIndex))
      return DROP_RECORD_WITH_VAR;

    hb_tag_t axis_tag = c->axes_index_tag_map->get (axisIndex);

    Triple axis_range (-1.0, 0.0, 1.0);
    Triple *axis_limit;
    bool axis_set_by_user = false;
    if (c->axes_location->has (axis_tag, &axis_limit))
    {
      axis_range = *axis_limit;
      axis_set_by_user = true;
    }

    float axis_min_val = axis_range.minimum;
    float axis_default_val = axis_range.middle;
    float axis_max_val = axis_range.maximum;

    float filter_min_val = filterRangeMinValue.to_float ();
    float filter_max_val = filterRangeMaxValue.to_float ();

    if (axis_default_val < filter_min_val ||
        axis_default_val > filter_max_val)
      c->apply = false;

    //condition not met, drop the entire record
    if (axis_min_val > filter_max_val || axis_max_val < filter_min_val ||
        filter_min_val > filter_max_val)
      return DROP_RECORD_WITH_VAR;

    //condition met and axis pinned, drop the condition
    if (axis_set_by_user && axis_range.is_point ())
      return DROP_COND_WITH_VAR;

    if (filter_max_val != axis_max_val || filter_min_val != axis_min_val)
    {
      // add axisIndex->value into the hashmap so we can check if the record is
      // unique with variations
      int16_t int_filter_max_val = filterRangeMaxValue.to_int ();
      int16_t int_filter_min_val = filterRangeMinValue.to_int ();
      hb_codepoint_t val = (int_filter_max_val << 16) + int_filter_min_val;

      condition_map->set (axisIndex, val);
      return KEEP_COND_WITH_VAR;
    }
    return KEEP_RECORD_WITH_VAR;
  }

  template <typename Instancer>
  bool evaluate (const int *coords, unsigned int coord_len,
		 Instancer *instancer HB_UNUSED) const
  {
    int coord = axisIndex < coord_len ? coords[axisIndex] : 0;
    return filterRangeMinValue.to_int () <= coord && coord <= filterRangeMaxValue.to_int ();
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  protected:
  HBUINT16	format;		/* Format identifier--format = 1 */
  HBUINT16	axisIndex;
  F2DOT14	filterRangeMinValue;
  F2DOT14	filterRangeMaxValue;
  public:
  DEFINE_SIZE_STATIC (8);
};

struct ConditionValue
{
  friend struct Condition;

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    // TODO(subset)
    return_trace (false);
  }

  private:
  template <typename Instancer>
  bool evaluate (const int *coords, unsigned int coord_len,
		 Instancer *instancer) const
  {
    signed value = defaultValue;
    value += (*instancer)[varIdx];
    return value > 0;
  }

  bool subset (hb_subset_context_t *c,
               hb_subset_layout_context_t *l,
               bool insert_catch_all) const
  {
    TRACE_SUBSET (this);
    // TODO(subset)
    return_trace (false);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  protected:
  HBUINT16	format;		/* Format identifier--format = 2 */
  HBINT16	defaultValue;   /* Value at default instance. */
  VarIdx	varIdx;		/* Variation index */
  public:
  DEFINE_SIZE_STATIC (8);
};

struct ConditionAnd
{
  friend struct Condition;

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    // TODO(subset)
    return_trace (false);
  }

  private:
  template <typename Instancer>
  bool evaluate (const int *coords, unsigned int coord_len,
		 Instancer *instancer) const
  {
    unsigned int count = conditions.len;
    for (unsigned int i = 0; i < count; i++)
      if (!_hb_recurse_condition_evaluate (this+conditions.arrayZ[i],
					   coords, coord_len,
					   instancer))
	return false;
    return true;
  }

  bool subset (hb_subset_context_t *c,
               hb_subset_layout_context_t *l,
               bool insert_catch_all) const
  {
    TRACE_SUBSET (this);
    // TODO(subset)
    return_trace (false);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (conditions.sanitize (c, this));
  }

  protected:
  HBUINT16	format;		/* Format identifier--format = 3 */
  Array8OfOffset24To<struct Condition>	conditions;
  public:
  DEFINE_SIZE_ARRAY (3, conditions);
};

struct ConditionOr
{
  friend struct Condition;

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    // TODO(subset)
    return_trace (false);
  }

  private:
  template <typename Instancer>
  bool evaluate (const int *coords, unsigned int coord_len,
		 Instancer *instancer) const
  {
    unsigned int count = conditions.len;
    for (unsigned int i = 0; i < count; i++)
      if (_hb_recurse_condition_evaluate (this+conditions.arrayZ[i],
					  coords, coord_len,
					  instancer))
	return true;
    return false;
  }

  bool subset (hb_subset_context_t *c,
               hb_subset_layout_context_t *l,
               bool insert_catch_all) const
  {
    TRACE_SUBSET (this);
    // TODO(subset)
    return_trace (false);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (conditions.sanitize (c, this));
  }

  protected:
  HBUINT16	format;		/* Format identifier--format = 4 */
  Array8OfOffset24To<struct Condition>	conditions;
  public:
  DEFINE_SIZE_ARRAY (3, conditions);
};

struct ConditionNegate
{
  friend struct Condition;

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    // TODO(subset)
    return_trace (false);
  }

  private:
  template <typename Instancer>
  bool evaluate (const int *coords, unsigned int coord_len,
		 Instancer *instancer) const
  {
    return !_hb_recurse_condition_evaluate (this+condition,
					    coords, coord_len,
					    instancer);
  }

  bool subset (hb_subset_context_t *c,
               hb_subset_layout_context_t *l,
               bool insert_catch_all) const
  {
    TRACE_SUBSET (this);
    // TODO(subset)
    return_trace (false);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (condition.sanitize (c, this));
  }

  protected:
  HBUINT16	format;		/* Format identifier--format = 5 */
  Offset24To<struct Condition>	condition;
  public:
  DEFINE_SIZE_STATIC (5);
};

struct Condition
{
  template <typename Instancer>
  bool evaluate (const int *coords, unsigned int coord_len,
		 Instancer *instancer) const
  {
    switch (u.format) {
    case 1: hb_barrier (); return u.format1.evaluate (coords, coord_len, instancer);
    case 2: hb_barrier (); return u.format2.evaluate (coords, coord_len, instancer);
    case 3: hb_barrier (); return u.format3.evaluate (coords, coord_len, instancer);
    case 4: hb_barrier (); return u.format4.evaluate (coords, coord_len, instancer);
    case 5: hb_barrier (); return u.format5.evaluate (coords, coord_len, instancer);
    default:return false;
    }
  }

  Cond_with_Var_flag_t keep_with_variations (hb_collect_feature_substitutes_with_var_context_t *c,
                                             hb_map_t *condition_map /* OUT */) const
  {
    switch (u.format) {
    case 1: hb_barrier (); return u.format1.keep_with_variations (c, condition_map);
    // TODO(subset)
    default: c->apply = false; return KEEP_COND_WITH_VAR;
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
    case 4: hb_barrier (); return_trace (c->dispatch (u.format4, std::forward<Ts> (ds)...));
    case 5: hb_barrier (); return_trace (c->dispatch (u.format5, std::forward<Ts> (ds)...));
    default:return_trace (c->default_return_value ());
    }
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (!u.format.sanitize (c)) return_trace (false);
    hb_barrier ();
    switch (u.format) {
    case 1: hb_barrier (); return_trace (u.format1.sanitize (c));
    case 2: hb_barrier (); return_trace (u.format2.sanitize (c));
    case 3: hb_barrier (); return_trace (u.format3.sanitize (c));
    case 4: hb_barrier (); return_trace (u.format4.sanitize (c));
    case 5: hb_barrier (); return_trace (u.format5.sanitize (c));
    default:return_trace (true);
    }
  }

  protected:
  union {
  HBUINT16		format;		/* Format identifier */
  ConditionAxisRange	format1;
  ConditionValue	format2;
  ConditionAnd		format3;
  ConditionOr		format4;
  ConditionNegate	format5;
  } u;
  public:
  DEFINE_SIZE_UNION (2, format);
};

template <typename Instancer>
bool
_hb_recurse_condition_evaluate (const struct Condition &condition,
				const int *coords,
				unsigned int coord_len,
				Instancer *instancer)
{
  return condition.evaluate (coords, coord_len, instancer);
}

struct ConditionList
{
  const Condition& operator[] (unsigned i) const
  { return this+conditions[i]; }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (conditions.sanitize (c, this));
  }

  protected:
  Array32OfOffset32To<Condition> conditions;
  public:
  DEFINE_SIZE_ARRAY (4, conditions);
};

struct ConditionSet
{
  bool evaluate (const int *coords, unsigned int coord_len,
		 ItemVarStoreInstancer *instancer) const
  {
    unsigned int count = conditions.len;
    for (unsigned int i = 0; i < count; i++)
      if (!(this+conditions.arrayZ[i]).evaluate (coords, coord_len, instancer))
	return false;
    return true;
  }

  void keep_with_variations (hb_collect_feature_substitutes_with_var_context_t *c) const
  {
    hb_map_t *condition_map = hb_map_create ();
    if (unlikely (!condition_map)) return;
    hb::shared_ptr<hb_map_t> p {condition_map};

    hb_set_t *cond_set = hb_set_create ();
    if (unlikely (!cond_set)) return;
    hb::shared_ptr<hb_set_t> s {cond_set};

    c->apply = true;
    bool should_keep = false;
    unsigned num_kept_cond = 0, cond_idx = 0;
    for (const auto& offset : conditions)
    {
      Cond_with_Var_flag_t ret = (this+offset).keep_with_variations (c, condition_map);
      // condition is not met or condition out of range, drop the entire record
      if (ret == DROP_RECORD_WITH_VAR)
        return;

      if (ret == KEEP_COND_WITH_VAR)
      {
        should_keep = true;
        cond_set->add (cond_idx);
        num_kept_cond++;
      }

      if (ret == KEEP_RECORD_WITH_VAR)
        should_keep = true;

      cond_idx++;
    }

    if (!should_keep) return;

    //check if condition_set is unique with variations
    if (c->conditionset_map->has (p))
      //duplicate found, drop the entire record
      return;

    c->conditionset_map->set (p, 1);
    c->record_cond_idx_map->set (c->cur_record_idx, s);
    if (should_keep && num_kept_cond == 0)
      c->universal = true;
  }

  bool subset (hb_subset_context_t *c,
               hb_subset_layout_context_t *l,
               bool insert_catch_all) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (this);
    if (unlikely (!out || !c->serializer->extend_min (out))) return_trace (false);

    if (insert_catch_all) return_trace (true);

    hb_set_t *retained_cond_set = nullptr;
    if (l->feature_record_cond_idx_map != nullptr)
      retained_cond_set = l->feature_record_cond_idx_map->get (l->cur_feature_var_record_idx);

    unsigned int count = conditions.len;
    for (unsigned int i = 0; i < count; i++)
    {
      if (retained_cond_set != nullptr && !retained_cond_set->has (i))
        continue;
      subset_offset_array (c, out->conditions, this) (conditions[i]);
    }

    return_trace (bool (out->conditions));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (conditions.sanitize (c, this));
  }

  protected:
  Array16OfOffset32To<Condition>	conditions;
  public:
  DEFINE_SIZE_ARRAY (2, conditions);
};

struct FeatureTableSubstitutionRecord
{
  friend struct FeatureTableSubstitution;

  void collect_lookups (const void *base, hb_set_t *lookup_indexes /* OUT */) const
  {
    return (base+feature).add_lookup_indexes_to (lookup_indexes);
  }

  void closure_features (const void *base,
			 const hb_map_t *lookup_indexes,
			 hb_set_t       *feature_indexes /* OUT */) const
  {
    if ((base+feature).intersects_lookup_indexes (lookup_indexes))
      feature_indexes->add (featureIndex);
  }

  void collect_feature_substitutes_with_variations (hb_hashmap_t<unsigned, const Feature*> *feature_substitutes_map,
                                                    hb_set_t& catch_all_record_feature_idxes,
                                                    const hb_set_t *feature_indices,
                                                    const void *base) const
  {
    if (feature_indices->has (featureIndex))
    {
      feature_substitutes_map->set (featureIndex, &(base+feature));
      catch_all_record_feature_idxes.add (featureIndex);
    }
  }

  bool serialize (hb_subset_layout_context_t *c,
                  unsigned feature_index,
                  const Feature *f, const Tag *tag)
  {
    TRACE_SERIALIZE (this);
    hb_serialize_context_t *s = c->subset_context->serializer;
    if (unlikely (!s->extend_min (this))) return_trace (false);

    uint32_t *new_feature_idx;
    if (!c->feature_index_map->has (feature_index, &new_feature_idx))
      return_trace (false);

    if (!s->check_assign (featureIndex, *new_feature_idx, HB_SERIALIZE_ERROR_INT_OVERFLOW))
      return_trace (false);

    s->push ();
    bool ret = f->subset (c->subset_context, c, tag);
    if (ret) s->add_link (feature, s->pop_pack ());
    else s->pop_discard ();

    return_trace (ret);
  }

  bool subset (hb_subset_layout_context_t *c, const void *base) const
  {
    TRACE_SUBSET (this);
    uint32_t *new_feature_index;
    if (!c->feature_index_map->has (featureIndex, &new_feature_index))
      return_trace (false);

    auto *out = c->subset_context->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    out->featureIndex = *new_feature_index;
    return_trace (out->feature.serialize_subset (c->subset_context, feature, base, c));
  }

  bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && feature.sanitize (c, base));
  }

  protected:
  HBUINT16		featureIndex;
  Offset32To<Feature>	feature;
  public:
  DEFINE_SIZE_STATIC (6);
};

struct FeatureTableSubstitution
{
  const Feature *find_substitute (unsigned int feature_index) const
  {
    unsigned int count = substitutions.len;
    for (unsigned int i = 0; i < count; i++)
    {
      const FeatureTableSubstitutionRecord &record = substitutions.arrayZ[i];
      if (record.featureIndex == feature_index)
	return &(this+record.feature);
    }
    return nullptr;
  }

  void collect_lookups (const hb_set_t *feature_indexes,
			hb_set_t       *lookup_indexes /* OUT */) const
  {
    + hb_iter (substitutions)
    | hb_filter (feature_indexes, &FeatureTableSubstitutionRecord::featureIndex)
    | hb_apply ([this, lookup_indexes] (const FeatureTableSubstitutionRecord& r)
		{ r.collect_lookups (this, lookup_indexes); })
    ;
  }

  void closure_features (const hb_map_t *lookup_indexes,
			 hb_set_t       *feature_indexes /* OUT */) const
  {
    for (const FeatureTableSubstitutionRecord& record : substitutions)
      record.closure_features (this, lookup_indexes, feature_indexes);
  }

  bool intersects_features (const hb_map_t *feature_index_map) const
  {
    for (const FeatureTableSubstitutionRecord& record : substitutions)
    {
      if (feature_index_map->has (record.featureIndex)) return true;
    }
    return false;
  }

  void collect_feature_substitutes_with_variations (hb_collect_feature_substitutes_with_var_context_t *c) const
  {
    for (const FeatureTableSubstitutionRecord& record : substitutions)
      record.collect_feature_substitutes_with_variations (c->feature_substitutes_map,
                                                          c->catch_all_record_feature_idxes,
                                                          c->feature_indices, this);
  }

  bool subset (hb_subset_context_t        *c,
	       hb_subset_layout_context_t *l,
               bool insert_catch_all) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!out || !c->serializer->extend_min (out))) return_trace (false);

    out->version.major = version.major;
    out->version.minor = version.minor;

    if (insert_catch_all)
    {
      for (unsigned feature_index : *(l->catch_all_record_feature_idxes))
      {
        hb_pair_t<const void*, const void*> *p;
        if (!l->feature_idx_tag_map->has (feature_index, &p))
          return_trace (false);
        auto *o = out->substitutions.serialize_append (c->serializer);
        if (!o->serialize (l, feature_index,
                           reinterpret_cast<const Feature*> (p->first),
                           reinterpret_cast<const Tag*> (p->second)))
          return_trace (false);
      }
      return_trace (true);
    }

    + substitutions.iter ()
    | hb_apply (subset_record_array (l, &(out->substitutions), this))
    ;

    return_trace (bool (out->substitutions));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (version.sanitize (c) &&
		  hb_barrier () &&
		  likely (version.major == 1) &&
		  substitutions.sanitize (c, this));
  }

  protected:
  FixedVersion<>	version;	/* Version--0x00010000u */
  Array16Of<FeatureTableSubstitutionRecord>
			substitutions;
  public:
  DEFINE_SIZE_ARRAY (6, substitutions);
};

struct FeatureVariationRecord
{
  friend struct FeatureVariations;

  void collect_lookups (const void     *base,
			const hb_set_t *feature_indexes,
			hb_set_t       *lookup_indexes /* OUT */) const
  {
    return (base+substitutions).collect_lookups (feature_indexes, lookup_indexes);
  }

  void closure_features (const void     *base,
			 const hb_map_t *lookup_indexes,
			 hb_set_t       *feature_indexes /* OUT */) const
  {
    (base+substitutions).closure_features (lookup_indexes, feature_indexes);
  }

  bool intersects_features (const void *base, const hb_map_t *feature_index_map) const
  {
    return (base+substitutions).intersects_features (feature_index_map);
  }

  void collect_feature_substitutes_with_variations (hb_collect_feature_substitutes_with_var_context_t *c,
                                                    const void *base) const
  {
    (base+conditions).keep_with_variations (c);
    if (c->apply && !c->variation_applied)
    {
      (base+substitutions).collect_feature_substitutes_with_variations (c);
      c->variation_applied = true; // set variations only once
    }
  }

  bool subset (hb_subset_layout_context_t *c, const void *base,
               bool insert_catch_all = false) const
  {
    TRACE_SUBSET (this);
    auto *out = c->subset_context->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    out->conditions.serialize_subset (c->subset_context, conditions, base, c, insert_catch_all);
    out->substitutions.serialize_subset (c->subset_context, substitutions, base, c, insert_catch_all);

    return_trace (true);
  }

  bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    return_trace (conditions.sanitize (c, base) &&
		  substitutions.sanitize (c, base));
  }

  protected:
  Offset32To<ConditionSet>
			conditions;
  Offset32To<FeatureTableSubstitution>
			substitutions;
  public:
  DEFINE_SIZE_STATIC (8);
};

struct FeatureVariations
{
  static constexpr unsigned NOT_FOUND_INDEX = 0xFFFFFFFFu;

  bool find_index (const int *coords, unsigned int coord_len,
		   unsigned int *index,
		   ItemVarStoreInstancer *instancer) const
  {
    unsigned int count = varRecords.len;
    for (unsigned int i = 0; i < count; i++)
    {
      const FeatureVariationRecord &record = varRecords.arrayZ[i];
      if ((this+record.conditions).evaluate (coords, coord_len, instancer))
      {
	*index = i;
	return true;
      }
    }
    *index = NOT_FOUND_INDEX;
    return false;
  }

  const Feature *find_substitute (unsigned int variations_index,
				  unsigned int feature_index) const
  {
    const FeatureVariationRecord &record = varRecords[variations_index];
    return (this+record.substitutions).find_substitute (feature_index);
  }

  void collect_feature_substitutes_with_variations (hb_collect_feature_substitutes_with_var_context_t *c) const
  {
    unsigned int count = varRecords.len;
    for (unsigned int i = 0; i < count; i++)
    {
      c->cur_record_idx = i;
      varRecords[i].collect_feature_substitutes_with_variations (c, this);
      if (c->universal)
        break;
    }
    if (c->universal || c->record_cond_idx_map->is_empty ())
      c->catch_all_record_feature_idxes.reset ();
  }

  FeatureVariations* copy (hb_serialize_context_t *c) const
  {
    TRACE_SERIALIZE (this);
    return_trace (c->embed (*this));
  }

  void collect_lookups (const hb_set_t *feature_indexes,
			const hb_hashmap_t<unsigned, hb::shared_ptr<hb_set_t>> *feature_record_cond_idx_map,
			hb_set_t       *lookup_indexes /* OUT */) const
  {
    unsigned count = varRecords.len;
    for (unsigned int i = 0; i < count; i++)
    {
      if (feature_record_cond_idx_map &&
          !feature_record_cond_idx_map->has (i))
        continue;
      varRecords[i].collect_lookups (this, feature_indexes, lookup_indexes);
    }
  }

  void closure_features (const hb_map_t *lookup_indexes,
			 const hb_hashmap_t<unsigned, hb::shared_ptr<hb_set_t>> *feature_record_cond_idx_map,
			 hb_set_t       *feature_indexes /* OUT */) const
  {
    unsigned int count = varRecords.len;
    for (unsigned int i = 0; i < count; i++)
    {
      if (feature_record_cond_idx_map != nullptr &&
          !feature_record_cond_idx_map->has (i))
        continue;
      varRecords[i].closure_features (this, lookup_indexes, feature_indexes);
    }
  }

  bool subset (hb_subset_context_t *c,
	       hb_subset_layout_context_t *l) const
  {
    TRACE_SUBSET (this);
    auto *out = c->serializer->start_embed (*this);
    if (unlikely (!out || !c->serializer->extend_min (out))) return_trace (false);

    out->version.major = version.major;
    out->version.minor = version.minor;

    int keep_up_to = -1;
    for (int i = varRecords.len - 1; i >= 0; i--) {
      if (varRecords[i].intersects_features (this, l->feature_index_map)) {
        keep_up_to = i;
        break;
      }
    }

    unsigned count = (unsigned) (keep_up_to + 1);
    for (unsigned i = 0; i < count; i++)
    {
      if (l->feature_record_cond_idx_map != nullptr &&
          !l->feature_record_cond_idx_map->has (i))
        continue;

      l->cur_feature_var_record_idx = i;
      subset_record_array (l, &(out->varRecords), this) (varRecords[i]);
    }

    if (out->varRecords.len && !l->catch_all_record_feature_idxes->is_empty ())
    {
      bool insert_catch_all_record = true;
      subset_record_array (l, &(out->varRecords), this, insert_catch_all_record) (varRecords[0]);
    }

    return_trace (bool (out->varRecords));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (version.sanitize (c) &&
		  hb_barrier () &&
		  likely (version.major == 1) &&
		  varRecords.sanitize (c, this));
  }

  protected:
  FixedVersion<>	version;	/* Version--0x00010000u */
  Array32Of<FeatureVariationRecord>
			varRecords;
  public:
  DEFINE_SIZE_ARRAY_SIZED (8, varRecords);
};


/*
 * Device Tables
 */

struct HintingDevice
{
  friend struct Device;

  private:

  hb_position_t get_x_delta (hb_font_t *font) const
  { return get_delta (font->x_ppem, font->x_scale); }

  hb_position_t get_y_delta (hb_font_t *font) const
  { return get_delta (font->y_ppem, font->y_scale); }

  public:

  unsigned int get_size () const
  {
    unsigned int f = deltaFormat;
    if (unlikely (f < 1 || f > 3 || startSize > endSize)) return 3 * HBUINT16::static_size;
    return HBUINT16::static_size * (4 + ((endSize - startSize) >> (4 - f)));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && c->check_range (this, this->get_size ()));
  }

  HintingDevice* copy (hb_serialize_context_t *c) const
  {
    TRACE_SERIALIZE (this);
    return_trace (c->embed<HintingDevice> (this));
  }

  private:

  int get_delta (unsigned int ppem, int scale) const
  {
    if (!ppem) return 0;

    int pixels = get_delta_pixels (ppem);

    if (!pixels) return 0;

    return (int) (pixels * (int64_t) scale / ppem);
  }
  int get_delta_pixels (unsigned int ppem_size) const
  {
    unsigned int f = deltaFormat;
    if (unlikely (f < 1 || f > 3))
      return 0;

    if (ppem_size < startSize || ppem_size > endSize)
      return 0;

    unsigned int s = ppem_size - startSize;

    unsigned int byte = deltaValueZ[s >> (4 - f)];
    unsigned int bits = (byte >> (16 - (((s & ((1 << (4 - f)) - 1)) + 1) << f)));
    unsigned int mask = (0xFFFFu >> (16 - (1 << f)));

    int delta = bits & mask;

    if ((unsigned int) delta >= ((mask + 1) >> 1))
      delta -= mask + 1;

    return delta;
  }

  protected:
  HBUINT16	startSize;		/* Smallest size to correct--in ppem */
  HBUINT16	endSize;		/* Largest size to correct--in ppem */
  HBUINT16	deltaFormat;		/* Format of DeltaValue array data: 1, 2, or 3
					 * 1	Signed 2-bit value, 8 values per uint16
					 * 2	Signed 4-bit value, 4 values per uint16
					 * 3	Signed 8-bit value, 2 values per uint16
					 */
  UnsizedArrayOf<HBUINT16>
		deltaValueZ;		/* Array of compressed data */
  public:
  DEFINE_SIZE_ARRAY (6, deltaValueZ);
};

struct VariationDevice
{
  friend struct Device;

  private:

  hb_position_t get_x_delta (hb_font_t *font,
			     const ItemVariationStore &store,
			     ItemVariationStore::cache_t *store_cache = nullptr) const
  { return font->em_scalef_x (get_delta (font, store, store_cache)); }

  hb_position_t get_y_delta (hb_font_t *font,
			     const ItemVariationStore &store,
			     ItemVariationStore::cache_t *store_cache = nullptr) const
  { return font->em_scalef_y (get_delta (font, store, store_cache)); }

  VariationDevice* copy (hb_serialize_context_t *c,
                         const hb_hashmap_t<unsigned, hb_pair_t<unsigned, int>> *layout_variation_idx_delta_map) const
  {
    TRACE_SERIALIZE (this);
    if (!layout_variation_idx_delta_map) return_trace (nullptr);

    hb_pair_t<unsigned, int> *v;
    if (!layout_variation_idx_delta_map->has (varIdx, &v))
      return_trace (nullptr);

    c->start_zerocopy (this->static_size);
    auto *out = c->embed (this);
    if (unlikely (!out)) return_trace (nullptr);

    if (!c->check_assign (out->varIdx, hb_first (*v), HB_SERIALIZE_ERROR_INT_OVERFLOW))
      return_trace (nullptr);
    return_trace (out);
  }

  void collect_variation_index (hb_collect_variation_indices_context_t *c) const
  { c->layout_variation_indices->add (varIdx); }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  private:

  float get_delta (hb_font_t *font,
		   const ItemVariationStore &store,
		   ItemVariationStore::cache_t *store_cache = nullptr) const
  {
    return store.get_delta (varIdx, font->coords, font->num_coords, (ItemVariationStore::cache_t *) store_cache);
  }

  protected:
  VarIdx	varIdx;		/* Variation index */
  HBUINT16	deltaFormat;	/* Format identifier for this table: 0x0x8000 */
  public:
  DEFINE_SIZE_STATIC (6);
};

struct DeviceHeader
{
  protected:
  HBUINT16		reserved1;
  HBUINT16		reserved2;
  public:
  HBUINT16		format;		/* Format identifier */
  public:
  DEFINE_SIZE_STATIC (6);
};

struct Device
{
  hb_position_t get_x_delta (hb_font_t *font,
			     const ItemVariationStore &store=Null (ItemVariationStore),
			     ItemVariationStore::cache_t *store_cache = nullptr) const
  {
    switch (u.b.format)
    {
#ifndef HB_NO_HINTING
    case 1: case 2: case 3:
      return u.hinting.get_x_delta (font);
#endif
#ifndef HB_NO_VAR
    case 0x8000:
      return u.variation.get_x_delta (font, store, store_cache);
#endif
    default:
      return 0;
    }
  }
  hb_position_t get_y_delta (hb_font_t *font,
			     const ItemVariationStore &store=Null (ItemVariationStore),
			     ItemVariationStore::cache_t *store_cache = nullptr) const
  {
    switch (u.b.format)
    {
    case 1: case 2: case 3:
#ifndef HB_NO_HINTING
      return u.hinting.get_y_delta (font);
#endif
#ifndef HB_NO_VAR
    case 0x8000:
      return u.variation.get_y_delta (font, store, store_cache);
#endif
    default:
      return 0;
    }
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (!u.b.format.sanitize (c)) return_trace (false);
    switch (u.b.format) {
#ifndef HB_NO_HINTING
    case 1: case 2: case 3:
      return_trace (u.hinting.sanitize (c));
#endif
#ifndef HB_NO_VAR
    case 0x8000:
      return_trace (u.variation.sanitize (c));
#endif
    default:
      return_trace (true);
    }
  }

  Device* copy (hb_serialize_context_t *c,
                const hb_hashmap_t<unsigned, hb_pair_t<unsigned, int>> *layout_variation_idx_delta_map=nullptr) const
  {
    TRACE_SERIALIZE (this);
    switch (u.b.format) {
#ifndef HB_NO_HINTING
    case 1:
    case 2:
    case 3:
      return_trace (reinterpret_cast<Device *> (u.hinting.copy (c)));
#endif
#ifndef HB_NO_VAR
    case 0x8000:
      return_trace (reinterpret_cast<Device *> (u.variation.copy (c, layout_variation_idx_delta_map)));
#endif
    default:
      return_trace (nullptr);
    }
  }

  void collect_variation_indices (hb_collect_variation_indices_context_t *c) const
  {
    switch (u.b.format) {
#ifndef HB_NO_HINTING
    case 1:
    case 2:
    case 3:
      return;
#endif
#ifndef HB_NO_VAR
    case 0x8000:
      u.variation.collect_variation_index (c);
      return;
#endif
    default:
      return;
    }
  }

  unsigned get_variation_index () const
  {
    switch (u.b.format) {
#ifndef HB_NO_VAR
    case 0x8000:
      return u.variation.varIdx;
#endif
    default:
      return HB_OT_LAYOUT_NO_VARIATIONS_INDEX;
    }
  }

  protected:
  union {
  DeviceHeader		b;
  HintingDevice		hinting;
#ifndef HB_NO_VAR
  VariationDevice	variation;
#endif
  } u;
  public:
  DEFINE_SIZE_UNION (6, b);
};


} /* namespace OT */


#endif /* HB_OT_LAYOUT_COMMON_HH */
