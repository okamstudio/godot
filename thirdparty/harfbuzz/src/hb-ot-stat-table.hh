/*
 * Copyright © 2018  Ebrahim Byagowi
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

#ifndef HB_OT_STAT_TABLE_HH
#define HB_OT_STAT_TABLE_HH

#include "hb-open-type.hh"
#include "hb-ot-layout-common.hh"

/*
 * STAT -- Style Attributes
 * https://docs.microsoft.com/en-us/typography/opentype/spec/stat
 */
#define HB_OT_TAG_STAT HB_TAG('S','T','A','T')


namespace OT {

enum
{
  OLDER_SIBLING_FONT_ATTRIBUTE = 0x0001,	/* If set, this axis value table
						 * provides axis value information
						 * that is applicable to other fonts
						 * within the same font family. This
						 * is used if the other fonts were
						 * released earlier and did not include
						 * information about values for some axis.
						 * If newer versions of the other
						 * fonts include the information
						 * themselves and are present,
						 * then this record is ignored. */
  ELIDABLE_AXIS_VALUE_NAME = 0x0002		/* If set, it indicates that the axis
						 * value represents the “normal” value
						 * for the axis and may be omitted when
						 * composing name strings. */
  // Reserved = 0xFFFC				/* Reserved for future use — set to zero. */
};

static bool axis_value_is_outside_axis_range (hb_tag_t axis_tag, float axis_value,
                                              const hb_hashmap_t<hb_tag_t, Triple> *user_axes_location)
{
  if (!user_axes_location->has (axis_tag))
    return false;

  double axis_value_double = static_cast<double>(axis_value);
  Triple axis_range = user_axes_location->get (axis_tag);
  return (axis_value_double < axis_range.minimum || axis_value_double > axis_range.maximum);
}

struct StatAxisRecord
{
  int cmp (hb_tag_t key) const { return tag.cmp (key); }

  hb_ot_name_id_t get_name_id () const { return nameID; }

  hb_tag_t get_axis_tag () const { return tag; }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this)));
  }

  protected:
  Tag		tag;		/* A tag identifying the axis of design variation. */
  NameID	nameID;		/* The name ID for entries in the 'name' table that
				 * provide a display string for this axis. */
  HBUINT16	ordering;	/* A value that applications can use to determine
				 * primary sorting of face names, or for ordering
				 * of descriptors when composing family or face names. */
  public:
  DEFINE_SIZE_STATIC (8);
};

struct AxisValueFormat1
{
  unsigned int get_axis_index () const { return axisIndex; }
  float get_value ()             const { return value.to_float (); }

  hb_ot_name_id_t get_value_name_id () const { return valueNameID; }

  hb_tag_t get_axis_tag (const hb_array_t<const StatAxisRecord> axis_records) const
  {
    unsigned axis_idx = get_axis_index ();
    return axis_records[axis_idx].get_axis_tag ();
  }

  bool keep_axis_value (const hb_array_t<const StatAxisRecord> axis_records,
                        const hb_hashmap_t<hb_tag_t, Triple> *user_axes_location) const
  {
    hb_tag_t axis_tag = get_axis_tag (axis_records);
    float axis_value = get_value ();

    return !axis_value_is_outside_axis_range (axis_tag, axis_value, user_axes_location);
  }

  bool subset (hb_subset_context_t *c,
               const hb_array_t<const StatAxisRecord> axis_records) const
  {
    TRACE_SUBSET (this);
    const hb_hashmap_t<hb_tag_t, Triple>* user_axes_location = &c->plan->user_axes_location;

    if (keep_axis_value (axis_records, user_axes_location))
      return_trace (c->serializer->embed (this));

    return_trace (false);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  protected:
  HBUINT16	format;		/* Format identifier — set to 1. */
  HBUINT16	axisIndex;	/* Zero-base index into the axis record array
				 * identifying the axis of design variation
				 * to which the axis value record applies.
				 * Must be less than designAxisCount. */
  HBUINT16	flags;		/* Flags — see below for details. */
  NameID	valueNameID;	/* The name ID for entries in the 'name' table
				 * that provide a display string for this
				 * attribute value. */
  F16DOT16	value;		/* A numeric value for this attribute value. */
  public:
  DEFINE_SIZE_STATIC (12);
};

struct AxisValueFormat2
{
  unsigned int get_axis_index () const { return axisIndex; }
  float get_value ()             const { return nominalValue.to_float (); }

  hb_ot_name_id_t get_value_name_id () const { return valueNameID; }

  hb_tag_t get_axis_tag (const hb_array_t<const StatAxisRecord> axis_records) const
  {
    unsigned axis_idx = get_axis_index ();
    return axis_records[axis_idx].get_axis_tag ();
  }

  bool keep_axis_value (const hb_array_t<const StatAxisRecord> axis_records,
                        const hb_hashmap_t<hb_tag_t, Triple> *user_axes_location) const
  {
    hb_tag_t axis_tag = get_axis_tag (axis_records);
    float axis_value = get_value ();

    return !axis_value_is_outside_axis_range (axis_tag, axis_value, user_axes_location);
  }

  bool subset (hb_subset_context_t *c,
               const hb_array_t<const StatAxisRecord> axis_records) const
  {
    TRACE_SUBSET (this);
    const hb_hashmap_t<hb_tag_t, Triple>* user_axes_location = &c->plan->user_axes_location;

    if (keep_axis_value (axis_records, user_axes_location))
      return_trace (c->serializer->embed (this));

    return_trace (false);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  protected:
  HBUINT16	format;		/* Format identifier — set to 2. */
  HBUINT16	axisIndex;	/* Zero-base index into the axis record array
				 * identifying the axis of design variation
				 * to which the axis value record applies.
				 * Must be less than designAxisCount. */
  HBUINT16	flags;		/* Flags — see below for details. */
  NameID	valueNameID;	/* The name ID for entries in the 'name' table
				 * that provide a display string for this
				 * attribute value. */
  F16DOT16	nominalValue;	/* A numeric value for this attribute value. */
  F16DOT16	rangeMinValue;	/* The minimum value for a range associated
				 * with the specified name ID. */
  F16DOT16	rangeMaxValue;	/* The maximum value for a range associated
				 * with the specified name ID. */
  public:
  DEFINE_SIZE_STATIC (20);
};

struct AxisValueFormat3
{
  unsigned int get_axis_index () const { return axisIndex; }
  float get_value ()             const { return value.to_float (); }

  hb_ot_name_id_t get_value_name_id () const { return valueNameID; }

  hb_tag_t get_axis_tag (const hb_array_t<const StatAxisRecord> axis_records) const
  {
    unsigned axis_idx = get_axis_index ();
    return axis_records[axis_idx].get_axis_tag ();
  }

  bool keep_axis_value (const hb_array_t<const StatAxisRecord> axis_records,
                        const hb_hashmap_t<hb_tag_t, Triple> *user_axes_location) const
  {
    hb_tag_t axis_tag = get_axis_tag (axis_records);
    float axis_value = get_value ();

    return !axis_value_is_outside_axis_range (axis_tag, axis_value, user_axes_location);
  }

  bool subset (hb_subset_context_t *c,
               const hb_array_t<const StatAxisRecord> axis_records) const
  {
    TRACE_SUBSET (this);
    const hb_hashmap_t<hb_tag_t, Triple>* user_axes_location = &c->plan->user_axes_location;

    if (keep_axis_value (axis_records, user_axes_location))
      return_trace (c->serializer->embed (this));

    return_trace (false);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  protected:
  HBUINT16	format;		/* Format identifier — set to 3. */
  HBUINT16	axisIndex;	/* Zero-base index into the axis record array
				 * identifying the axis of design variation
				 * to which the axis value record applies.
				 * Must be less than designAxisCount. */
  HBUINT16	flags;		/* Flags — see below for details. */
  NameID	valueNameID;	/* The name ID for entries in the 'name' table
				 * that provide a display string for this
				 * attribute value. */
  F16DOT16	value;		/* A numeric value for this attribute value. */
  F16DOT16	linkedValue;	/* The numeric value for a style-linked mapping
				 * from this value. */
  public:
  DEFINE_SIZE_STATIC (16);
};

struct AxisValueRecord
{
  unsigned int get_axis_index () const { return axisIndex; }
  float get_value ()             const { return value.to_float (); }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  protected:
  HBUINT16	axisIndex;	/* Zero-base index into the axis record array
				 * identifying the axis to which this value
				 * applies. Must be less than designAxisCount. */
  F16DOT16	value;		/* A numeric value for this attribute value. */
  public:
  DEFINE_SIZE_STATIC (6);
};

struct AxisValueFormat4
{
  const AxisValueRecord &get_axis_record (unsigned int axis_index) const
  { return axisValues.as_array (axisCount)[axis_index]; }

  bool keep_axis_value (const hb_array_t<const StatAxisRecord> axis_records,
                        const hb_hashmap_t<hb_tag_t, Triple> *user_axes_location) const
  {
    hb_array_t<const AxisValueRecord> axis_value_records = axisValues.as_array (axisCount);

    for (const auto& rec : axis_value_records)
    {
      unsigned axis_idx = rec.get_axis_index ();
      float axis_value = rec.get_value ();
      hb_tag_t axis_tag = axis_records[axis_idx].get_axis_tag ();

      if (axis_value_is_outside_axis_range (axis_tag, axis_value, user_axes_location))
        return false;
    }

    return true;
  }

  bool subset (hb_subset_context_t *c,
               const hb_array_t<const StatAxisRecord> axis_records) const
  {
    TRACE_SUBSET (this);
    const hb_hashmap_t<hb_tag_t, Triple> *user_axes_location = &c->plan->user_axes_location;
    if (!keep_axis_value (axis_records, user_axes_location))
      return_trace (false);

    unsigned total_size = min_size + axisCount * AxisValueRecord::static_size;
    auto *out = c->serializer->allocate_size<AxisValueFormat4> (total_size);
    if (unlikely (!out)) return_trace (false);
    hb_memcpy (out, this, total_size);
    return_trace (true);
  }

  hb_ot_name_id_t get_value_name_id () const { return valueNameID; }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this) &&
			  hb_barrier () &&
                          axisValues.sanitize (c, axisCount)));
  }

  protected:
  HBUINT16	format;		/* Format identifier — set to 4. */
  HBUINT16	axisCount;	/* The total number of axes contributing to
				 * this axis-values combination. */
  HBUINT16	flags;		/* Flags — see below for details. */
  NameID	valueNameID;	/* The name ID for entries in the 'name' table
				 * that provide a display string for this
				 * attribute value. */
  UnsizedArrayOf<AxisValueRecord>
		axisValues;	/* Array of AxisValue records that provide the
				 * combination of axis values, one for each
				 * contributing axis. */
  public:
  DEFINE_SIZE_ARRAY (8, axisValues);
};

struct AxisValue
{
  float get_value (unsigned int axis_index) const
  {
    switch (u.format)
    {
    case 1: hb_barrier (); return u.format1.get_value ();
    case 2: hb_barrier (); return u.format2.get_value ();
    case 3: hb_barrier (); return u.format3.get_value ();
    case 4: hb_barrier (); return u.format4.get_axis_record (axis_index).get_value ();
    default:return 0.f;
    }
  }

  unsigned int get_axis_index () const
  {
    switch (u.format)
    {
    case 1: hb_barrier (); return u.format1.get_axis_index ();
    case 2: hb_barrier (); return u.format2.get_axis_index ();
    case 3: hb_barrier (); return u.format3.get_axis_index ();
    /* case 4: Makes more sense for variable fonts which are handled by fvar in hb-style */
    default:return -1;
    }
  }

  hb_ot_name_id_t get_value_name_id () const
  {
    switch (u.format)
    {
    case 1: hb_barrier (); return u.format1.get_value_name_id ();
    case 2: hb_barrier (); return u.format2.get_value_name_id ();
    case 3: hb_barrier (); return u.format3.get_value_name_id ();
    case 4: hb_barrier (); return u.format4.get_value_name_id ();
    default:return HB_OT_NAME_ID_INVALID;
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
    default:return_trace (c->default_return_value ());
    }
  }

  bool keep_axis_value (const hb_array_t<const StatAxisRecord> axis_records,
                        hb_hashmap_t<hb_tag_t, Triple> *user_axes_location) const
  {
    switch (u.format)
    {
    case 1: hb_barrier (); return u.format1.keep_axis_value (axis_records, user_axes_location);
    case 2: hb_barrier (); return u.format2.keep_axis_value (axis_records, user_axes_location);
    case 3: hb_barrier (); return u.format3.keep_axis_value (axis_records, user_axes_location);
    case 4: hb_barrier (); return u.format4.keep_axis_value (axis_records, user_axes_location);
    default:return false;
    }
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!c->check_struct (this)))
      return_trace (false);
    hb_barrier ();

    switch (u.format)
    {
    case 1: hb_barrier (); return_trace (u.format1.sanitize (c));
    case 2: hb_barrier (); return_trace (u.format2.sanitize (c));
    case 3: hb_barrier (); return_trace (u.format3.sanitize (c));
    case 4: hb_barrier (); return_trace (u.format4.sanitize (c));
    default:return_trace (true);
    }
  }

  protected:
  union
  {
  HBUINT16		format;
  AxisValueFormat1	format1;
  AxisValueFormat2	format2;
  AxisValueFormat3	format3;
  AxisValueFormat4	format4;
  } u;
  public:
  DEFINE_SIZE_UNION (2, format);
};

struct AxisValueOffsetArray: UnsizedArrayOf<Offset16To<AxisValue>>
{
  bool subset (hb_subset_context_t *c,
               unsigned axisValueCount,
               unsigned& count,
               const hb_array_t<const StatAxisRecord> axis_records) const
  {
    TRACE_SUBSET (this);

    auto axisValueOffsets = as_array (axisValueCount);
    count = 0;
    for (const auto& offset : axisValueOffsets)
    {
      if (!offset) continue;
      auto o_snap = c->serializer->snapshot ();
      auto *o = c->serializer->embed (offset);
      if (!o) return_trace (false);
      if (!o->serialize_subset (c, offset, this, axis_records))
      {
        c->serializer->revert (o_snap);
        continue;
      }
      count++;
    }

    return_trace (count);
  }
};

struct STAT
{
  static constexpr hb_tag_t tableTag = HB_OT_TAG_STAT;

  bool has_data () const { return version.to_int (); }

  bool get_value (hb_tag_t tag, float *value) const
  {
    unsigned int axis_index;
    if (!get_design_axes ().lfind (tag, &axis_index)) return false;

    hb_array_t<const Offset16To<AxisValue>> axis_values = get_axis_value_offsets ();
    for (unsigned int i = 0; i < axis_values.length; i++)
    {
      const AxisValue& axis_value = this+offsetToAxisValueOffsets+axis_values[i];
      if (axis_value.get_axis_index () == axis_index)
      {
	if (value)
	  *value = axis_value.get_value (axis_index);
	return true;
      }
    }
    return false;
  }

  unsigned get_design_axis_count () const { return designAxisCount; }

  hb_ot_name_id_t get_axis_record_name_id (unsigned axis_record_index) const
  {
    if (unlikely (axis_record_index >= designAxisCount)) return HB_OT_NAME_ID_INVALID;
    const StatAxisRecord &axis_record = get_design_axes ()[axis_record_index];
    return axis_record.get_name_id ();
  }

  unsigned get_axis_value_count () const { return axisValueCount; }

  hb_ot_name_id_t get_axis_value_name_id (unsigned axis_value_index) const
  {
    if (unlikely (axis_value_index >= axisValueCount)) return HB_OT_NAME_ID_INVALID;
    const AxisValue &axis_value = (this + get_axis_value_offsets ()[axis_value_index]);
    return axis_value.get_value_name_id ();
  }

  void collect_name_ids (hb_hashmap_t<hb_tag_t, Triple> *user_axes_location,
                         hb_set_t *nameids_to_retain /* OUT */) const
  {
    if (!has_data ()) return;

    + get_design_axes ()
    | hb_map (&StatAxisRecord::get_name_id)
    | hb_sink (nameids_to_retain)
    ;

    auto designAxes = get_design_axes ();

    + get_axis_value_offsets ()
    | hb_map (hb_add (&(this + offsetToAxisValueOffsets)))
    | hb_filter ([&] (const AxisValue& _)
                 { return _.keep_axis_value (designAxes, user_axes_location); })
    | hb_map (&AxisValue::get_value_name_id)
    | hb_sink (nameids_to_retain)
    ;

    nameids_to_retain->add (elidedFallbackNameID);
  }

  bool subset (hb_subset_context_t *c) const
  {
    TRACE_SUBSET (this);
    STAT *out = c->serializer->embed (this);
    if (unlikely (!out)) return_trace (false);

    auto designAxes = get_design_axes ();
    for (unsigned i = 0; i < (unsigned)designAxisCount; i++)
      if (unlikely (!c->serializer->embed (designAxes[i])))
          return_trace (false);

    if (designAxisCount)
      c->serializer->check_assign (out->designAxesOffset, this->get_size (),
                                   HB_SERIALIZE_ERROR_INT_OVERFLOW);

    unsigned count = 0;
    out->offsetToAxisValueOffsets.serialize_subset (c, offsetToAxisValueOffsets, this,
                                                    axisValueCount, count, designAxes);
    return_trace (c->serializer->check_assign (out->axisValueCount, count, HB_SERIALIZE_ERROR_INT_OVERFLOW));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this) &&
			  hb_barrier () &&
			  version.major == 1 &&
			  version.minor > 0 &&
			  designAxesOffset.sanitize (c, this, designAxisCount) &&
			  offsetToAxisValueOffsets.sanitize (c, this, axisValueCount, &(this+offsetToAxisValueOffsets))));
  }

  protected:
  hb_array_t<const StatAxisRecord> const get_design_axes () const
  { return (this+designAxesOffset).as_array (designAxisCount); }

  hb_array_t<const Offset16To<AxisValue>> const get_axis_value_offsets () const
  { return (this+offsetToAxisValueOffsets).as_array (axisValueCount); }


  protected:
  FixedVersion<>version;	/* Version of the stat table
				 * initially set to 0x00010002u */
  HBUINT16	designAxisSize;	/* The size in bytes of each axis record. */
  HBUINT16	designAxisCount;/* The number of design axis records. In a
				 * font with an 'fvar' table, this value must be
				 * greater than or equal to the axisCount value
				 * in the 'fvar' table. In all fonts, must
				 * be greater than zero if axisValueCount
				 * is greater than zero. */
  NNOffset32To<UnsizedArrayOf<StatAxisRecord>>
		designAxesOffset;
				/* Offset in bytes from the beginning of
				 * the STAT table to the start of the design
				 * axes array. If designAxisCount is zero,
				 * set to zero; if designAxisCount is greater
				 * than zero, must be greater than zero. */
  HBUINT16	axisValueCount;	/* The number of axis value tables. */
  NNOffset32To<AxisValueOffsetArray>
		offsetToAxisValueOffsets;
				/* Offset in bytes from the beginning of
				 * the STAT table to the start of the design
				 * axes value offsets array. If axisValueCount
				 * is zero, set to zero; if axisValueCount is
				 * greater than zero, must be greater than zero. */
  NameID	elidedFallbackNameID;
				/* Name ID used as fallback when projection of
				 * names into a particular font model produces
				 * a subfamily name containing only elidable
				 * elements. */
  public:
  DEFINE_SIZE_STATIC (20);
};


} /* namespace OT */


#endif /* HB_OT_STAT_TABLE_HH */
