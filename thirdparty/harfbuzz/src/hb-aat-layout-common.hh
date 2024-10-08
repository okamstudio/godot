/*
 * Copyright © 2017  Google, Inc.
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

#ifndef HB_AAT_LAYOUT_COMMON_HH
#define HB_AAT_LAYOUT_COMMON_HH

#include "hb-aat-layout.hh"
#include "hb-aat-map.hh"
#include "hb-open-type.hh"

namespace OT {
struct GDEF;
};

namespace AAT {

using namespace OT;

#define HB_AAT_BUFFER_DIGEST_THRESHOLD 32

struct ankr;

struct hb_aat_apply_context_t :
       hb_dispatch_context_t<hb_aat_apply_context_t, bool, HB_DEBUG_APPLY>
{
  const char *get_name () { return "APPLY"; }
  template <typename T, typename ...Ts>
  return_t dispatch (const T &obj, Ts&&... ds)
  { return obj.apply (this, std::forward<Ts> (ds)...); }
  static return_t default_return_value () { return false; }
  bool stop_sublookup_iteration (return_t r) const { return r; }

  const hb_ot_shape_plan_t *plan;
  hb_font_t *font;
  hb_face_t *face;
  hb_buffer_t *buffer;
  hb_sanitize_context_t sanitizer;
  const ankr *ankr_table;
  const OT::GDEF *gdef_table;
  const hb_sorted_vector_t<hb_aat_map_t::range_flags_t> *range_flags = nullptr;
  hb_set_digest_t buffer_digest = hb_set_digest_t::full ();
  hb_set_digest_t machine_glyph_set = hb_set_digest_t::full ();
  hb_set_digest_t left_set = hb_set_digest_t::full ();
  hb_set_digest_t right_set = hb_set_digest_t::full ();
  hb_mask_t subtable_flags = 0;

  /* Unused. For debug tracing only. */
  unsigned int lookup_index;

  HB_INTERNAL hb_aat_apply_context_t (const hb_ot_shape_plan_t *plan_,
				      hb_font_t *font_,
				      hb_buffer_t *buffer_,
				      hb_blob_t *blob = const_cast<hb_blob_t *> (&Null (hb_blob_t)));

  HB_INTERNAL ~hb_aat_apply_context_t ();

  HB_INTERNAL void set_ankr_table (const AAT::ankr *ankr_table_);

  void set_lookup_index (unsigned int i) { lookup_index = i; }
};


/*
 * Lookup Table
 */

enum { DELETED_GLYPH = 0xFFFF };

template <typename T> struct Lookup;

template <typename T>
struct LookupFormat0
{
  friend struct Lookup<T>;

  private:
  const T* get_value (hb_codepoint_t glyph_id, unsigned int num_glyphs) const
  {
    if (unlikely (glyph_id >= num_glyphs)) return nullptr;
    return &arrayZ[glyph_id];
  }

  template <typename set_t>
  void collect_glyphs (set_t &glyphs, unsigned num_glyphs) const
  {
    glyphs.add_range (0, num_glyphs - 1);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (arrayZ.sanitize (c, c->get_num_glyphs ()));
  }
  bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    return_trace (arrayZ.sanitize (c, c->get_num_glyphs (), base));
  }

  protected:
  HBUINT16	format;		/* Format identifier--format = 0 */
  UnsizedArrayOf<T>
		arrayZ;		/* Array of lookup values, indexed by glyph index. */
  public:
  DEFINE_SIZE_UNBOUNDED (2);
};


template <typename T>
struct LookupSegmentSingle
{
  static constexpr unsigned TerminationWordCount = 2u;

  int cmp (hb_codepoint_t g) const
  { return g < first ? -1 : g <= last ? 0 : +1 ; }

  template <typename set_t>
  void collect_glyphs (set_t &glyphs) const
  {
    if (first == DELETED_GLYPH)
      return;
    glyphs.add_range (first, last);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && value.sanitize (c));
  }
  bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && value.sanitize (c, base));
  }

  HBGlyphID16	last;		/* Last GlyphID in this segment */
  HBGlyphID16	first;		/* First GlyphID in this segment */
  T		value;		/* The lookup value (only one) */
  public:
  DEFINE_SIZE_STATIC (4 + T::static_size);
};

template <typename T>
struct LookupFormat2
{
  friend struct Lookup<T>;

  private:
  const T* get_value (hb_codepoint_t glyph_id) const
  {
    const LookupSegmentSingle<T> *v = segments.bsearch (glyph_id);
    return v ? &v->value : nullptr;
  }

  template <typename set_t>
  void collect_glyphs (set_t &glyphs) const
  {
    unsigned count = segments.get_length ();
    for (unsigned int i = 0; i < count; i++)
      segments[i].collect_glyphs (glyphs);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (segments.sanitize (c));
  }
  bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    return_trace (segments.sanitize (c, base));
  }

  protected:
  HBUINT16	format;		/* Format identifier--format = 2 */
  VarSizedBinSearchArrayOf<LookupSegmentSingle<T>>
		segments;	/* The actual segments. These must already be sorted,
				 * according to the first word in each one (the last
				 * glyph in each segment). */
  public:
  DEFINE_SIZE_ARRAY (8, segments);
};

template <typename T>
struct LookupSegmentArray
{
  static constexpr unsigned TerminationWordCount = 2u;

  const T* get_value (hb_codepoint_t glyph_id, const void *base) const
  {
    return first <= glyph_id && glyph_id <= last ? &(base+valuesZ)[glyph_id - first] : nullptr;
  }

  template <typename set_t>
  void collect_glyphs (set_t &glyphs) const
  {
    if (first == DELETED_GLYPH)
      return;
    glyphs.add_range (first, last);
  }

  int cmp (hb_codepoint_t g) const
  { return g < first ? -1 : g <= last ? 0 : +1; }

  bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
		  first <= last &&
		  valuesZ.sanitize (c, base, last - first + 1));
  }
  template <typename ...Ts>
  bool sanitize (hb_sanitize_context_t *c, const void *base, Ts&&... ds) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
		  first <= last &&
		  valuesZ.sanitize (c, base, last - first + 1, std::forward<Ts> (ds)...));
  }

  HBGlyphID16	last;		/* Last GlyphID in this segment */
  HBGlyphID16	first;		/* First GlyphID in this segment */
  NNOffset16To<UnsizedArrayOf<T>>
		valuesZ;	/* A 16-bit offset from the start of
				 * the table to the data. */
  public:
  DEFINE_SIZE_STATIC (6);
};

template <typename T>
struct LookupFormat4
{
  friend struct Lookup<T>;

  private:
  const T* get_value (hb_codepoint_t glyph_id) const
  {
    const LookupSegmentArray<T> *v = segments.bsearch (glyph_id);
    return v ? v->get_value (glyph_id, this) : nullptr;
  }

  template <typename set_t>
  void collect_glyphs (set_t &glyphs) const
  {
    unsigned count = segments.get_length ();
    for (unsigned i = 0; i < count; i++)
      segments[i].collect_glyphs (glyphs);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (segments.sanitize (c, this));
  }
  bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    return_trace (segments.sanitize (c, this, base));
  }

  protected:
  HBUINT16	format;		/* Format identifier--format = 4 */
  VarSizedBinSearchArrayOf<LookupSegmentArray<T>>
		segments;	/* The actual segments. These must already be sorted,
				 * according to the first word in each one (the last
				 * glyph in each segment). */
  public:
  DEFINE_SIZE_ARRAY (8, segments);
};

template <typename T>
struct LookupSingle
{
  static constexpr unsigned TerminationWordCount = 1u;

  int cmp (hb_codepoint_t g) const { return glyph.cmp (g); }

  template <typename set_t>
  void collect_glyphs (set_t &glyphs) const
  {
    if (glyph == DELETED_GLYPH)
      return;
    glyphs.add (glyph);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && value.sanitize (c));
  }
  bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && value.sanitize (c, base));
  }

  HBGlyphID16	glyph;		/* Last GlyphID */
  T		value;		/* The lookup value (only one) */
  public:
  DEFINE_SIZE_STATIC (2 + T::static_size);
};

template <typename T>
struct LookupFormat6
{
  friend struct Lookup<T>;

  private:
  const T* get_value (hb_codepoint_t glyph_id) const
  {
    const LookupSingle<T> *v = entries.bsearch (glyph_id);
    return v ? &v->value : nullptr;
  }

  template <typename set_t>
  void collect_glyphs (set_t &glyphs) const
  {
    unsigned count = entries.get_length ();
    for (unsigned i = 0; i < count; i++)
      entries[i].collect_glyphs (glyphs);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (entries.sanitize (c));
  }
  bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    return_trace (entries.sanitize (c, base));
  }

  protected:
  HBUINT16	format;		/* Format identifier--format = 6 */
  VarSizedBinSearchArrayOf<LookupSingle<T>>
		entries;	/* The actual entries, sorted by glyph index. */
  public:
  DEFINE_SIZE_ARRAY (8, entries);
};

template <typename T>
struct LookupFormat8
{
  friend struct Lookup<T>;

  private:
  const T* get_value (hb_codepoint_t glyph_id) const
  {
    return firstGlyph <= glyph_id && glyph_id - firstGlyph < glyphCount ?
	   &valueArrayZ[glyph_id - firstGlyph] : nullptr;
  }

  template <typename set_t>
  void collect_glyphs (set_t &glyphs) const
  {
    if (unlikely (!glyphCount))
      return;
    if (firstGlyph == DELETED_GLYPH)
      return;
    glyphs.add_range (firstGlyph, firstGlyph + glyphCount - 1);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && valueArrayZ.sanitize (c, glyphCount));
  }
  bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && valueArrayZ.sanitize (c, glyphCount, base));
  }

  protected:
  HBUINT16	format;		/* Format identifier--format = 8 */
  HBGlyphID16	firstGlyph;	/* First glyph index included in the trimmed array. */
  HBUINT16	glyphCount;	/* Total number of glyphs (equivalent to the last
				 * glyph minus the value of firstGlyph plus 1). */
  UnsizedArrayOf<T>
		valueArrayZ;	/* The lookup values (indexed by the glyph index
				 * minus the value of firstGlyph). */
  public:
  DEFINE_SIZE_ARRAY (6, valueArrayZ);
};

template <typename T>
struct LookupFormat10
{
  friend struct Lookup<T>;

  private:
  const typename T::type get_value_or_null (hb_codepoint_t glyph_id) const
  {
    if (!(firstGlyph <= glyph_id && glyph_id - firstGlyph < glyphCount))
      return Null (T);

    const HBUINT8 *p = &valueArrayZ[(glyph_id - firstGlyph) * valueSize];

    unsigned int v = 0;
    unsigned int count = valueSize;
    for (unsigned int i = 0; i < count; i++)
      v = (v << 8) | *p++;

    return v;
  }

  template <typename set_t>
  void collect_glyphs (set_t &glyphs) const
  {
    if (unlikely (!glyphCount))
      return;
    if (firstGlyph == DELETED_GLYPH)
      return;
    glyphs.add_range (firstGlyph, firstGlyph + glyphCount - 1);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) &&
		  hb_barrier () &&
		  valueSize <= 4 &&
		  valueArrayZ.sanitize (c, glyphCount * valueSize));
  }

  protected:
  HBUINT16	format;		/* Format identifier--format = 8 */
  HBUINT16	valueSize;	/* Byte size of each value. */
  HBGlyphID16	firstGlyph;	/* First glyph index included in the trimmed array. */
  HBUINT16	glyphCount;	/* Total number of glyphs (equivalent to the last
				 * glyph minus the value of firstGlyph plus 1). */
  UnsizedArrayOf<HBUINT8>
		valueArrayZ;	/* The lookup values (indexed by the glyph index
				 * minus the value of firstGlyph). */
  public:
  DEFINE_SIZE_ARRAY (8, valueArrayZ);
};

template <typename T>
struct Lookup
{
  const T* get_value (hb_codepoint_t glyph_id, unsigned int num_glyphs) const
  {
    switch (u.format) {
    case 0: hb_barrier (); return u.format0.get_value (glyph_id, num_glyphs);
    case 2: hb_barrier (); return u.format2.get_value (glyph_id);
    case 4: hb_barrier (); return u.format4.get_value (glyph_id);
    case 6: hb_barrier (); return u.format6.get_value (glyph_id);
    case 8: hb_barrier (); return u.format8.get_value (glyph_id);
    default:return nullptr;
    }
  }

  const typename T::type get_value_or_null (hb_codepoint_t glyph_id, unsigned int num_glyphs) const
  {
    switch (u.format) {
      /* Format 10 cannot return a pointer. */
      case 10: hb_barrier (); return u.format10.get_value_or_null (glyph_id);
      default:
      const T *v = get_value (glyph_id, num_glyphs);
      return v ? *v : Null (T);
    }
  }

  template <typename set_t>
  void collect_glyphs (set_t &glyphs, unsigned int num_glyphs) const
  {
    switch (u.format) {
    case 0: hb_barrier (); u.format0.collect_glyphs (glyphs, num_glyphs); return;
    case 2: hb_barrier (); u.format2.collect_glyphs (glyphs); return;
    case 4: hb_barrier (); u.format4.collect_glyphs (glyphs); return;
    case 6: hb_barrier (); u.format6.collect_glyphs (glyphs); return;
    case 8: hb_barrier (); u.format8.collect_glyphs (glyphs); return;
    case 10: hb_barrier (); u.format10.collect_glyphs (glyphs); return;
    default:return;
    }
  }

  typename T::type get_class (hb_codepoint_t glyph_id,
			      unsigned int num_glyphs,
			      unsigned int outOfRange) const
  {
    const T *v = get_value (glyph_id, num_glyphs);
    return v ? *v : outOfRange;
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (!u.format.sanitize (c)) return_trace (false);
    hb_barrier ();
    switch (u.format) {
    case 0: hb_barrier (); return_trace (u.format0.sanitize (c));
    case 2: hb_barrier (); return_trace (u.format2.sanitize (c));
    case 4: hb_barrier (); return_trace (u.format4.sanitize (c));
    case 6: hb_barrier (); return_trace (u.format6.sanitize (c));
    case 8: hb_barrier (); return_trace (u.format8.sanitize (c));
    case 10: hb_barrier (); return_trace (u.format10.sanitize (c));
    default:return_trace (true);
    }
  }
  bool sanitize (hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    if (!u.format.sanitize (c)) return_trace (false);
    hb_barrier ();
    switch (u.format) {
    case 0: hb_barrier (); return_trace (u.format0.sanitize (c, base));
    case 2: hb_barrier (); return_trace (u.format2.sanitize (c, base));
    case 4: hb_barrier (); return_trace (u.format4.sanitize (c, base));
    case 6: hb_barrier (); return_trace (u.format6.sanitize (c, base));
    case 8: hb_barrier (); return_trace (u.format8.sanitize (c, base));
    case 10: return_trace (false); /* We don't support format10 here currently. */
    default:return_trace (true);
    }
  }

  protected:
  union {
  HBUINT16		format;		/* Format identifier */
  LookupFormat0<T>	format0;
  LookupFormat2<T>	format2;
  LookupFormat4<T>	format4;
  LookupFormat6<T>	format6;
  LookupFormat8<T>	format8;
  LookupFormat10<T>	format10;
  } u;
  public:
  DEFINE_SIZE_UNION (2, format);
};
DECLARE_NULL_NAMESPACE_BYTES_TEMPLATE1 (AAT, Lookup, 2);

/*
 * (Extended) State Table
 */

template <typename T>
struct Entry
{
  // This does seem like it's ever called.
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    /* Note, we don't recurse-sanitize data because we don't access it.
     * That said, in our DEFINE_SIZE_STATIC we access T::static_size,
     * which ensures that data has a simple sanitize(). To be determined
     * if I need to remove that as well.
     *
     * HOWEVER! Because we are a template, our DEFINE_SIZE_STATIC
     * assertion wouldn't be checked, hence the line below. */
    static_assert (T::static_size, "");

    return_trace (c->check_struct (this));
  }

  public:
  HBUINT16	newState;	/* Byte offset from beginning of state table
				 * to the new state. Really?!?! Or just state
				 * number?  The latter in morx for sure. */
  HBUINT16	flags;		/* Table specific. */
  T		data;		/* Optional offsets to per-glyph tables. */
  public:
  DEFINE_SIZE_STATIC (4 + T::static_size);
};

template <>
struct Entry<void>
{
  // This does seem like it's ever called.
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  public:
  HBUINT16	newState;	/* Byte offset from beginning of state table to the new state. */
  HBUINT16	flags;		/* Table specific. */
  public:
  DEFINE_SIZE_STATIC (4);
};

enum Class
{
  CLASS_END_OF_TEXT = 0,
  CLASS_OUT_OF_BOUNDS = 1,
  CLASS_DELETED_GLYPH = 2,
  CLASS_END_OF_LINE = 3,
};

template <typename Types, typename Extra>
struct StateTable
{
  typedef typename Types::HBUINT HBUINT;
  typedef typename Types::HBUSHORT HBUSHORT;
  typedef typename Types::ClassTypeNarrow ClassType;

  enum State
  {
    STATE_START_OF_TEXT = 0,
    STATE_START_OF_LINE = 1,
  };

  template <typename set_t>
  void collect_glyphs (set_t &glyphs, unsigned num_glyphs) const
  {
    (this+classTable).collect_glyphs (glyphs, num_glyphs);
  }

  int new_state (unsigned int newState) const
  { return Types::extended ? newState : ((int) newState - (int) stateArrayTable) / (int) nClasses; }

  template <typename set_t>
  unsigned int get_class (hb_codepoint_t glyph_id,
			  unsigned int num_glyphs,
			  const set_t &glyphs) const
  {
    if (unlikely (glyph_id == DELETED_GLYPH)) return CLASS_DELETED_GLYPH;
    if (!glyphs[glyph_id]) return CLASS_OUT_OF_BOUNDS;
    return (this+classTable).get_class (glyph_id, num_glyphs, CLASS_OUT_OF_BOUNDS);
  }

  const Entry<Extra> *get_entries () const
  { return (this+entryTable).arrayZ; }

  const Entry<Extra> &get_entry (int state, unsigned int klass) const
  {
    if (unlikely (klass >= nClasses))
      klass = CLASS_OUT_OF_BOUNDS;

    const HBUSHORT *states = (this+stateArrayTable).arrayZ;
    const Entry<Extra> *entries = (this+entryTable).arrayZ;

    unsigned int entry = states[state * nClasses + klass];
    DEBUG_MSG (APPLY, nullptr, "e%u", entry);

    return entries[entry];
  }

  bool sanitize (hb_sanitize_context_t *c,
		 unsigned int *num_entries_out = nullptr) const
  {
    TRACE_SANITIZE (this);
    if (unlikely (!(c->check_struct (this) &&
		    hb_barrier () &&
		    nClasses >= 4 /* Ensure pre-defined classes fit.  */ &&
		    classTable.sanitize (c, this)))) return_trace (false);

    const HBUSHORT *states = (this+stateArrayTable).arrayZ;
    const Entry<Extra> *entries = (this+entryTable).arrayZ;

    unsigned int num_classes = nClasses;
    if (unlikely (hb_unsigned_mul_overflows (num_classes, states[0].static_size)))
      return_trace (false);
    unsigned int row_stride = num_classes * states[0].static_size;

    /* Apple 'kern' table has this peculiarity:
     *
     * "Because the stateTableOffset in the state table header is (strictly
     * speaking) redundant, some 'kern' tables use it to record an initial
     * state where that should not be StartOfText. To determine if this is
     * done, calculate what the stateTableOffset should be. If it's different
     * from the actual stateTableOffset, use it as the initial state."
     *
     * We implement this by calling the initial state zero, but allow *negative*
     * states if the start state indeed was not the first state.  Since the code
     * is shared, this will also apply to 'mort' table.  The 'kerx' / 'morx'
     * tables are not affected since those address states by index, not offset.
     */

    int min_state = 0;
    int max_state = 0;
    unsigned int num_entries = 0;

    int state_pos = 0;
    int state_neg = 0;
    unsigned int entry = 0;
    while (min_state < state_neg || state_pos <= max_state)
    {
      if (min_state < state_neg)
      {
	/* Negative states. */
	if (unlikely (hb_unsigned_mul_overflows (min_state, num_classes)))
	  return_trace (false);
	if (unlikely (!c->check_range (&states[min_state * num_classes],
				       -min_state,
				       row_stride)))
	  return_trace (false);
	if ((c->max_ops -= state_neg - min_state) <= 0)
	  return_trace (false);
	{ /* Sweep new states. */
	  const HBUSHORT *stop = &states[min_state * num_classes];
	  if (unlikely (stop > states))
	    return_trace (false);
	  for (const HBUSHORT *p = states; stop < p; p--)
	    num_entries = hb_max (num_entries, *(p - 1) + 1u);
	  state_neg = min_state;
	}
      }

      if (state_pos <= max_state)
      {
	/* Positive states. */
	if (unlikely (!c->check_range (states,
				       max_state + 1,
				       row_stride)))
	  return_trace (false);
	if ((c->max_ops -= max_state - state_pos + 1) <= 0)
	  return_trace (false);
	{ /* Sweep new states. */
	  if (unlikely (hb_unsigned_mul_overflows ((max_state + 1), num_classes)))
	    return_trace (false);
	  const HBUSHORT *stop = &states[(max_state + 1) * num_classes];
	  if (unlikely (stop < states))
	    return_trace (false);
	  for (const HBUSHORT *p = &states[state_pos * num_classes]; p < stop; p++)
	    num_entries = hb_max (num_entries, *p + 1u);
	  state_pos = max_state + 1;
	}
      }

      if (unlikely (!c->check_array (entries, num_entries)))
	return_trace (false);
      if ((c->max_ops -= num_entries - entry) <= 0)
	return_trace (false);
      { /* Sweep new entries. */
	const Entry<Extra> *stop = &entries[num_entries];
	for (const Entry<Extra> *p = &entries[entry]; p < stop; p++)
	{
	  int newState = new_state (p->newState);
	  min_state = hb_min (min_state, newState);
	  max_state = hb_max (max_state, newState);
	}
	entry = num_entries;
      }
    }

    if (num_entries_out)
      *num_entries_out = num_entries;

    return_trace (true);
  }

  protected:
  HBUINT	nClasses;	/* Number of classes, which is the number of indices
				 * in a single line in the state array. */
  NNOffsetTo<ClassType, HBUINT>
		classTable;	/* Offset to the class table. */
  NNOffsetTo<UnsizedArrayOf<HBUSHORT>, HBUINT>
		stateArrayTable;/* Offset to the state array. */
  NNOffsetTo<UnsizedArrayOf<Entry<Extra>>, HBUINT>
		entryTable;	/* Offset to the entry array. */

  public:
  DEFINE_SIZE_STATIC (4 * sizeof (HBUINT));
};

template <typename HBUCHAR>
struct ClassTable
{
  unsigned int get_class (hb_codepoint_t glyph_id, unsigned int outOfRange) const
  {
    unsigned int i = glyph_id - firstGlyph;
    return i >= classArray.len ? outOfRange : classArray.arrayZ[i];
  }
  unsigned int get_class (hb_codepoint_t glyph_id,
			  unsigned int num_glyphs HB_UNUSED,
			  unsigned int outOfRange) const
  {
    return get_class (glyph_id, outOfRange);
  }

  template <typename set_t>
  void collect_glyphs (set_t &glyphs, unsigned num_glyphs) const
  {
    for (unsigned i = 0; i < classArray.len; i++)
      if (classArray.arrayZ[i] != CLASS_OUT_OF_BOUNDS)
	glyphs.add (firstGlyph + i);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && classArray.sanitize (c));
  }
  protected:
  HBGlyphID16		firstGlyph;	/* First glyph index included in the trimmed array. */
  Array16Of<HBUCHAR>	classArray;	/* The class codes (indexed by glyph index minus
					 * firstGlyph). */
  public:
  DEFINE_SIZE_ARRAY (4, classArray);
};

struct SubtableGlyphCoverage
{
  bool sanitize (hb_sanitize_context_t *c, unsigned subtable_count) const
  {
    TRACE_SANITIZE (this);

    if (unlikely (!c->check_array (&subtableOffsets, subtable_count)))
      return_trace (false);

    unsigned bytes = (c->get_num_glyphs () + CHAR_BIT - 1) / CHAR_BIT;
    for (unsigned i = 0; i < subtable_count; i++)
    {
      uint32_t offset = (uint32_t) subtableOffsets[i];
      if (offset == 0 || offset == 0xFFFFFFFF)
        continue;
      if (unlikely (!subtableOffsets[i].sanitize (c, this, bytes)))
        return_trace (false);
    }

    return_trace (true);
  }
  protected:
  UnsizedArrayOf<NNOffset32To<UnsizedArrayOf<HBUINT8>>> subtableOffsets;
					    /* Array of offsets from the beginning of the
					     * subtable glyph coverage table to the glyph
					     * coverage bitfield for a given subtable; there
					     * is one offset for each subtable in the chain */
  /* UnsizedArrayOf<HBUINT8> coverageBitfields; *//* The individual coverage bitfields. */
  public:
  DEFINE_SIZE_ARRAY (0, subtableOffsets);
};

struct ObsoleteTypes
{
  static constexpr bool extended = false;
  typedef HBUINT16 HBUINT;
  typedef HBUINT8 HBUSHORT;
  typedef ClassTable<HBUINT8> ClassTypeNarrow;
  typedef ClassTable<HBUINT16> ClassTypeWide;

  template <typename T>
  static unsigned int offsetToIndex (unsigned int offset,
				     const void *base,
				     const T *array)
  {
    /* https://github.com/harfbuzz/harfbuzz/issues/3483 */
    /* If offset is less than base, return an offset that would
     * result in an address half a 32bit address-space away,
     * to make sure sanitize fails even on 32bit builds. */
    if (unlikely (offset < unsigned ((const char *) array - (const char *) base)))
      return INT_MAX / T::static_size;

    /* https://github.com/harfbuzz/harfbuzz/issues/2816 */
    return (offset - unsigned ((const char *) array - (const char *) base)) / T::static_size;
  }
  template <typename T>
  static unsigned int byteOffsetToIndex (unsigned int offset,
					 const void *base,
					 const T *array)
  {
    return offsetToIndex (offset, base, array);
  }
  template <typename T>
  static unsigned int wordOffsetToIndex (unsigned int offset,
					 const void *base,
					 const T *array)
  {
    return offsetToIndex (2 * offset, base, array);
  }
};
struct ExtendedTypes
{
  static constexpr bool extended = true;
  typedef HBUINT32 HBUINT;
  typedef HBUINT16 HBUSHORT;
  typedef Lookup<HBUINT16> ClassTypeNarrow;
  typedef Lookup<HBUINT16> ClassTypeWide;

  template <typename T>
  static unsigned int offsetToIndex (unsigned int offset,
				     const void *base HB_UNUSED,
				     const T *array HB_UNUSED)
  {
    return offset;
  }
  template <typename T>
  static unsigned int byteOffsetToIndex (unsigned int offset,
					 const void *base HB_UNUSED,
					 const T *array HB_UNUSED)
  {
    return offset / 2;
  }
  template <typename T>
  static unsigned int wordOffsetToIndex (unsigned int offset,
					 const void *base HB_UNUSED,
					 const T *array HB_UNUSED)
  {
    return offset;
  }
};

template <typename Types, typename EntryData>
struct StateTableDriver
{
  using StateTableT = StateTable<Types, EntryData>;
  using EntryT = Entry<EntryData>;

  StateTableDriver (const StateTableT &machine_,
		    hb_face_t *face_) :
	      machine (machine_),
	      num_glyphs (face_->get_num_glyphs ()) {}

  template <typename context_t>
  bool is_idempotent_on_all_out_of_bounds (context_t *c, hb_aat_apply_context_t *ac)
  {
    const auto entry = machine.get_entry (StateTableT::STATE_START_OF_TEXT, CLASS_OUT_OF_BOUNDS);
    return !c->is_actionable (ac->buffer, this, entry) &&
	    machine.new_state (entry.newState) == StateTableT::STATE_START_OF_TEXT;
  }

  template <typename context_t>
  void drive (context_t *c, hb_aat_apply_context_t *ac)
  {
    hb_buffer_t *buffer = ac->buffer;

    if (!c->in_place)
      buffer->clear_output ();

    int state = StateTableT::STATE_START_OF_TEXT;
    // If there's only one range, we already checked the flag.
    auto *last_range = ac->range_flags && (ac->range_flags->length > 1) ? &(*ac->range_flags)[0] : nullptr;
    for (buffer->idx = 0; buffer->successful;)
    {
      /* This block is copied in NoncontextualSubtable::apply. Keep in sync. */
      if (last_range)
      {
	auto *range = last_range;
	if (buffer->idx < buffer->len)
	{
	  unsigned cluster = buffer->cur().cluster;
	  while (cluster < range->cluster_first)
	    range--;
	  while (cluster > range->cluster_last)
	    range++;


	  last_range = range;
	}
	if (!(range->flags & ac->subtable_flags))
	{
	  if (buffer->idx == buffer->len || unlikely (!buffer->successful))
	    break;

	  state = StateTableT::STATE_START_OF_TEXT;
	  (void) buffer->next_glyph ();
	  continue;
	}
      }

      unsigned int klass = likely (buffer->idx < buffer->len) ?
			   machine.get_class (buffer->cur().codepoint, num_glyphs, ac->machine_glyph_set) :
			   (unsigned) CLASS_END_OF_TEXT;
      DEBUG_MSG (APPLY, nullptr, "c%u at %u", klass, buffer->idx);
      const EntryT &entry = machine.get_entry (state, klass);
      const int next_state = machine.new_state (entry.newState);

      /* Conditions under which it's guaranteed safe-to-break before current glyph:
       *
       * 1. There was no action in this transition; and
       *
       * 2. If we break before current glyph, the results will be the same. That
       *    is guaranteed if:
       *
       *    2a. We were already in start-of-text state; or
       *
       *    2b. We are epsilon-transitioning to start-of-text state; or
       *
       *    2c. Starting from start-of-text state seeing current glyph:
       *
       *        2c'. There won't be any actions; and
       *
       *        2c". We would end up in the same state that we were going to end up
       *             in now, including whether epsilon-transitioning.
       *
       *    and
       *
       * 3. If we break before current glyph, there won't be any end-of-text action
       *    after previous glyph.
       *
       * This triples the transitions we need to look up, but is worth returning
       * granular unsafe-to-break results. See eg.:
       *
       *   https://github.com/harfbuzz/harfbuzz/issues/2860
       */

      const auto is_safe_to_break_extra = [&]()
      {
          /* 2c. */
          const auto &wouldbe_entry = machine.get_entry(StateTableT::STATE_START_OF_TEXT, klass);

          /* 2c'. */
          if (c->is_actionable (buffer, this, wouldbe_entry))
	    return false;

          /* 2c". */
          return next_state == machine.new_state(wouldbe_entry.newState)
              && (entry.flags & context_t::DontAdvance) == (wouldbe_entry.flags & context_t::DontAdvance);
      };

      const auto is_safe_to_break = [&]()
      {
          /* 1. */
          if (c->is_actionable (buffer, this, entry))
              return false;

          /* 2. */
          // This one is meh, I know...
          const auto ok =
                 state == StateTableT::STATE_START_OF_TEXT
              || ((entry.flags & context_t::DontAdvance) && next_state == StateTableT::STATE_START_OF_TEXT)
              || is_safe_to_break_extra();
          if (!ok)
              return false;

          /* 3. */
          return !c->is_actionable (buffer, this, machine.get_entry (state, CLASS_END_OF_TEXT));
      };

      if (!is_safe_to_break () && buffer->backtrack_len () && buffer->idx < buffer->len)
	buffer->unsafe_to_break_from_outbuffer (buffer->backtrack_len () - 1, buffer->idx + 1);

      c->transition (buffer, this, entry);

      state = next_state;
      DEBUG_MSG (APPLY, nullptr, "s%d", state);

      if (buffer->idx == buffer->len || unlikely (!buffer->successful))
	break;

      if (!(entry.flags & context_t::DontAdvance) || buffer->max_ops-- <= 0)
	(void) buffer->next_glyph ();
    }

    if (!c->in_place)
      buffer->sync ();
  }

  public:
  const StateTableT &machine;
  unsigned int num_glyphs;
};


} /* namespace AAT */


#endif /* HB_AAT_LAYOUT_COMMON_HH */
