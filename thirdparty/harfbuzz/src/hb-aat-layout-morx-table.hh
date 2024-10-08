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

#ifndef HB_AAT_LAYOUT_MORX_TABLE_HH
#define HB_AAT_LAYOUT_MORX_TABLE_HH

#include "hb-open-type.hh"
#include "hb-aat-layout-common.hh"
#include "hb-ot-layout-common.hh"
#include "hb-ot-layout-gdef-table.hh"
#include "hb-aat-map.hh"

/*
 * morx -- Extended Glyph Metamorphosis
 * https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6morx.html
 * https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6mort.html
 */
#define HB_AAT_TAG_morx HB_TAG('m','o','r','x')
#define HB_AAT_TAG_mort HB_TAG('m','o','r','t')


namespace AAT {

using namespace OT;

template <typename Types>
struct RearrangementSubtable
{
  typedef typename Types::HBUINT HBUINT;

  typedef void EntryData;

  struct driver_context_t
  {
    static constexpr bool in_place = true;
    enum Flags
    {
      MarkFirst		= 0x8000,	/* If set, make the current glyph the first
					 * glyph to be rearranged. */
      DontAdvance	= 0x4000,	/* If set, don't advance to the next glyph
					 * before going to the new state. This means
					 * that the glyph index doesn't change, even
					 * if the glyph at that index has changed. */
      MarkLast		= 0x2000,	/* If set, make the current glyph the last
					 * glyph to be rearranged. */
      Reserved		= 0x1FF0,	/* These bits are reserved and should be set to 0. */
      Verb		= 0x000F,	/* The type of rearrangement specified. */
    };

    driver_context_t (const RearrangementSubtable *table HB_UNUSED) :
	ret (false),
	start (0), end (0) {}

    bool is_actionable (hb_buffer_t *buffer HB_UNUSED,
			StateTableDriver<Types, EntryData> *driver HB_UNUSED,
			const Entry<EntryData> &entry) const
    {
      return (entry.flags & Verb) && start < end;
    }
    void transition (hb_buffer_t *buffer,
		     StateTableDriver<Types, EntryData> *driver,
		     const Entry<EntryData> &entry)
    {
      unsigned int flags = entry.flags;

      if (flags & MarkFirst)
	start = buffer->idx;

      if (flags & MarkLast)
	end = hb_min (buffer->idx + 1, buffer->len);

      if ((flags & Verb) && start < end)
      {
	/* The following map has two nibbles, for start-side
	 * and end-side. Values of 0,1,2 mean move that many
	 * to the other side. Value of 3 means move 2 and
	 * flip them. */
	const unsigned char map[16] =
	{
	  0x00,	/* 0	no change */
	  0x10,	/* 1	Ax => xA */
	  0x01,	/* 2	xD => Dx */
	  0x11,	/* 3	AxD => DxA */
	  0x20,	/* 4	ABx => xAB */
	  0x30,	/* 5	ABx => xBA */
	  0x02,	/* 6	xCD => CDx */
	  0x03,	/* 7	xCD => DCx */
	  0x12,	/* 8	AxCD => CDxA */
	  0x13,	/* 9	AxCD => DCxA */
	  0x21,	/* 10	ABxD => DxAB */
	  0x31,	/* 11	ABxD => DxBA */
	  0x22,	/* 12	ABxCD => CDxAB */
	  0x32,	/* 13	ABxCD => CDxBA */
	  0x23,	/* 14	ABxCD => DCxAB */
	  0x33,	/* 15	ABxCD => DCxBA */
	};

	unsigned int m = map[flags & Verb];
	unsigned int l = hb_min (2u, m >> 4);
	unsigned int r = hb_min (2u, m & 0x0F);
	bool reverse_l = 3 == (m >> 4);
	bool reverse_r = 3 == (m & 0x0F);

	if (end - start >= l + r && end-start <= HB_MAX_CONTEXT_LENGTH)
	{
	  buffer->merge_clusters (start, hb_min (buffer->idx + 1, buffer->len));
	  buffer->merge_clusters (start, end);

	  hb_glyph_info_t *info = buffer->info;
	  hb_glyph_info_t buf[4];

	  hb_memcpy (buf, info + start, l * sizeof (buf[0]));
	  hb_memcpy (buf + 2, info + end - r, r * sizeof (buf[0]));

	  if (l != r)
	    memmove (info + start + r, info + start + l, (end - start - l - r) * sizeof (buf[0]));

	  hb_memcpy (info + start, buf + 2, r * sizeof (buf[0]));
	  hb_memcpy (info + end - l, buf, l * sizeof (buf[0]));
	  if (reverse_l)
	  {
	    buf[0] = info[end - 1];
	    info[end - 1] = info[end - 2];
	    info[end - 2] = buf[0];
	  }
	  if (reverse_r)
	  {
	    buf[0] = info[start];
	    info[start] = info[start + 1];
	    info[start + 1] = buf[0];
	  }
	}
      }
    }

    public:
    bool ret;
    private:
    unsigned int start;
    unsigned int end;
  };

  bool apply (hb_aat_apply_context_t *c) const
  {
    TRACE_APPLY (this);

    driver_context_t dc (this);

    StateTableDriver<Types, EntryData> driver (machine, c->face);

    if (driver.is_idempotent_on_all_out_of_bounds (&dc, c) &&
	!c->buffer_digest.may_have (c->machine_glyph_set))
      return_trace (false);

    driver.drive (&dc, c);

    return_trace (dc.ret);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (machine.sanitize (c));
  }

  public:
  StateTable<Types, EntryData>	machine;
  public:
  DEFINE_SIZE_STATIC ((StateTable<Types, EntryData>::static_size));
};

template <typename Types>
struct ContextualSubtable
{
  typedef typename Types::HBUINT HBUINT;

  struct EntryData
  {
    HBUINT16	markIndex;	/* Index of the substitution table for the
				 * marked glyph (use 0xFFFF for none). */
    HBUINT16	currentIndex;	/* Index of the substitution table for the
				 * current glyph (use 0xFFFF for none). */
    public:
    DEFINE_SIZE_STATIC (4);
  };

  struct driver_context_t
  {
    static constexpr bool in_place = true;
    enum Flags
    {
      SetMark		= 0x8000,	/* If set, make the current glyph the marked glyph. */
      DontAdvance	= 0x4000,	/* If set, don't advance to the next glyph before
					 * going to the new state. */
      Reserved		= 0x3FFF,	/* These bits are reserved and should be set to 0. */
    };

    driver_context_t (const ContextualSubtable *table_,
			     hb_aat_apply_context_t *c_) :
	ret (false),
	c (c_),
	gdef (*c->gdef_table),
	mark_set (false),
	has_glyph_classes (gdef.has_glyph_classes ()),
	mark (0),
	table (table_),
	subs (table+table->substitutionTables) {}

    bool is_actionable (hb_buffer_t *buffer,
			StateTableDriver<Types, EntryData> *driver,
			const Entry<EntryData> &entry) const
    {
      if (buffer->idx == buffer->len && !mark_set)
	return false;

      return entry.data.markIndex != 0xFFFF || entry.data.currentIndex != 0xFFFF;
    }
    void transition (hb_buffer_t *buffer,
		     StateTableDriver<Types, EntryData> *driver,
		     const Entry<EntryData> &entry)
    {
      /* Looks like CoreText applies neither mark nor current substitution for
       * end-of-text if mark was not explicitly set. */
      if (buffer->idx == buffer->len && !mark_set)
	return;

      const HBGlyphID16 *replacement;

      replacement = nullptr;
      if (Types::extended)
      {
	if (entry.data.markIndex != 0xFFFF)
	{
	  const Lookup<HBGlyphID16> &lookup = subs[entry.data.markIndex];
	  replacement = lookup.get_value (buffer->info[mark].codepoint, driver->num_glyphs);
	}
      }
      else
      {
	unsigned int offset = entry.data.markIndex + buffer->info[mark].codepoint;
	const UnsizedArrayOf<HBGlyphID16> &subs_old = (const UnsizedArrayOf<HBGlyphID16> &) subs;
	replacement = &subs_old[Types::wordOffsetToIndex (offset, table, subs_old.arrayZ)];
	if (!(replacement->sanitize (&c->sanitizer) &&
	      hb_barrier () &&
	      *replacement))
	  replacement = nullptr;
      }
      if (replacement)
      {
	buffer->unsafe_to_break (mark, hb_min (buffer->idx + 1, buffer->len));
	buffer->info[mark].codepoint = *replacement;
	c->buffer_digest.add (*replacement);
	if (has_glyph_classes)
	  _hb_glyph_info_set_glyph_props (&buffer->info[mark],
					  gdef.get_glyph_props (*replacement));
	ret = true;
      }

      replacement = nullptr;
      unsigned int idx = hb_min (buffer->idx, buffer->len - 1);
      if (Types::extended)
      {
	if (entry.data.currentIndex != 0xFFFF)
	{
	  const Lookup<HBGlyphID16> &lookup = subs[entry.data.currentIndex];
	  replacement = lookup.get_value (buffer->info[idx].codepoint, driver->num_glyphs);
	}
      }
      else
      {
	unsigned int offset = entry.data.currentIndex + buffer->info[idx].codepoint;
	const UnsizedArrayOf<HBGlyphID16> &subs_old = (const UnsizedArrayOf<HBGlyphID16> &) subs;
	replacement = &subs_old[Types::wordOffsetToIndex (offset, table, subs_old.arrayZ)];
	if (!(replacement->sanitize (&c->sanitizer) &&
	      hb_barrier () &&
	      *replacement))
	  replacement = nullptr;
      }
      if (replacement)
      {
	buffer->info[idx].codepoint = *replacement;
	c->buffer_digest.add (*replacement);
	if (has_glyph_classes)
	  _hb_glyph_info_set_glyph_props (&buffer->info[idx],
					  gdef.get_glyph_props (*replacement));
	ret = true;
      }

      if (entry.flags & SetMark)
      {
	mark_set = true;
	mark = buffer->idx;
      }
    }

    public:
    bool ret;
    private:
    hb_aat_apply_context_t *c;
    const OT::GDEF &gdef;
    bool mark_set;
    bool has_glyph_classes;
    unsigned int mark;
    const ContextualSubtable *table;
    const UnsizedListOfOffset16To<Lookup<HBGlyphID16>, HBUINT, void, false> &subs;
  };

  bool apply (hb_aat_apply_context_t *c) const
  {
    TRACE_APPLY (this);

    driver_context_t dc (this, c);

    StateTableDriver<Types, EntryData> driver (machine, c->face);

    if (driver.is_idempotent_on_all_out_of_bounds (&dc, c) &&
	!c->buffer_digest.may_have (c->machine_glyph_set))
      return_trace (false);

    driver.drive (&dc, c);

    return_trace (dc.ret);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);

    unsigned int num_entries = 0;
    if (unlikely (!machine.sanitize (c, &num_entries))) return_trace (false);
    hb_barrier ();

    if (!Types::extended)
      return_trace (substitutionTables.sanitize (c, this, 0));

    unsigned int num_lookups = 0;

    const Entry<EntryData> *entries = machine.get_entries ();
    for (unsigned int i = 0; i < num_entries; i++)
    {
      const EntryData &data = entries[i].data;

      if (data.markIndex != 0xFFFF)
	num_lookups = hb_max (num_lookups, 1u + data.markIndex);
      if (data.currentIndex != 0xFFFF)
	num_lookups = hb_max (num_lookups, 1u + data.currentIndex);
    }

    return_trace (substitutionTables.sanitize (c, this, num_lookups));
  }

  public:
  StateTable<Types, EntryData>
		machine;
  protected:
  NNOffsetTo<UnsizedListOfOffset16To<Lookup<HBGlyphID16>, HBUINT, void, false>, HBUINT>
		substitutionTables;
  public:
  DEFINE_SIZE_STATIC ((StateTable<Types, EntryData>::static_size + HBUINT::static_size));
};


template <bool extended>
struct LigatureEntry;

template <>
struct LigatureEntry<true>
{
  enum Flags
  {
    SetComponent	= 0x8000,	/* Push this glyph onto the component stack for
					 * eventual processing. */
    DontAdvance		= 0x4000,	/* Leave the glyph pointer at this glyph for the
					   next iteration. */
    PerformAction	= 0x2000,	/* Use the ligActionIndex to process a ligature
					 * group. */
    Reserved		= 0x1FFF,	/* These bits are reserved and should be set to 0. */
  };

  struct EntryData
  {
    HBUINT16	ligActionIndex;	/* Index to the first ligActionTable entry
				 * for processing this group, if indicated
				 * by the flags. */
    public:
    DEFINE_SIZE_STATIC (2);
  };

  static bool performAction (const Entry<EntryData> &entry)
  { return entry.flags & PerformAction; }

  static unsigned int ligActionIndex (const Entry<EntryData> &entry)
  { return entry.data.ligActionIndex; }
};
template <>
struct LigatureEntry<false>
{
  enum Flags
  {
    SetComponent	= 0x8000,	/* Push this glyph onto the component stack for
					 * eventual processing. */
    DontAdvance		= 0x4000,	/* Leave the glyph pointer at this glyph for the
					   next iteration. */
    Offset		= 0x3FFF,	/* Byte offset from beginning of subtable to the
					 * ligature action list. This value must be a
					 * multiple of 4. */
  };

  typedef void EntryData;

  static bool performAction (const Entry<EntryData> &entry)
  { return entry.flags & Offset; }

  static unsigned int ligActionIndex (const Entry<EntryData> &entry)
  { return entry.flags & Offset; }
};


template <typename Types>
struct LigatureSubtable
{
  typedef typename Types::HBUINT HBUINT;

  typedef LigatureEntry<Types::extended> LigatureEntryT;
  typedef typename LigatureEntryT::EntryData EntryData;

  struct driver_context_t
  {
    static constexpr bool in_place = false;
    enum
    {
      DontAdvance	= LigatureEntryT::DontAdvance,
    };
    enum LigActionFlags
    {
      LigActionLast	= 0x80000000,	/* This is the last action in the list. This also
					 * implies storage. */
      LigActionStore	= 0x40000000,	/* Store the ligature at the current cumulated index
					 * in the ligature table in place of the marked
					 * (i.e. currently-popped) glyph. */
      LigActionOffset	= 0x3FFFFFFF,	/* A 30-bit value which is sign-extended to 32-bits
					 * and added to the glyph ID, resulting in an index
					 * into the component table. */
    };

    driver_context_t (const LigatureSubtable *table_,
		      hb_aat_apply_context_t *c_) :
	ret (false),
	c (c_),
	table (table_),
	ligAction (table+table->ligAction),
	component (table+table->component),
	ligature (table+table->ligature),
	match_length (0) {}

    bool is_actionable (hb_buffer_t *buffer HB_UNUSED,
			StateTableDriver<Types, EntryData> *driver HB_UNUSED,
			const Entry<EntryData> &entry) const
    {
      return LigatureEntryT::performAction (entry);
    }
    void transition (hb_buffer_t *buffer,
		     StateTableDriver<Types, EntryData> *driver,
		     const Entry<EntryData> &entry)
    {
      DEBUG_MSG (APPLY, nullptr, "Ligature transition at %u", buffer->idx);
      if (entry.flags & LigatureEntryT::SetComponent)
      {
	/* Never mark same index twice, in case DontAdvance was used... */
	if (match_length && match_positions[(match_length - 1u) % ARRAY_LENGTH (match_positions)] == buffer->out_len)
	  match_length--;

	match_positions[match_length++ % ARRAY_LENGTH (match_positions)] = buffer->out_len;
	DEBUG_MSG (APPLY, nullptr, "Set component at %u", buffer->out_len);
      }

      if (LigatureEntryT::performAction (entry))
      {
	DEBUG_MSG (APPLY, nullptr, "Perform action with %u", match_length);
	unsigned int end = buffer->out_len;

	if (unlikely (!match_length))
	  return;

	if (buffer->idx >= buffer->len)
	  return; /* TODO Work on previous instead? */

	unsigned int cursor = match_length;

	unsigned int action_idx = LigatureEntryT::ligActionIndex (entry);
	action_idx = Types::offsetToIndex (action_idx, table, ligAction.arrayZ);
	const HBUINT32 *actionData = &ligAction[action_idx];

	unsigned int ligature_idx = 0;
	unsigned int action;
	do
	{
	  if (unlikely (!cursor))
	  {
	    /* Stack underflow.  Clear the stack. */
	    DEBUG_MSG (APPLY, nullptr, "Stack underflow");
	    match_length = 0;
	    break;
	  }

	  DEBUG_MSG (APPLY, nullptr, "Moving to stack position %u", cursor - 1);
	  if (unlikely (!buffer->move_to (match_positions[--cursor % ARRAY_LENGTH (match_positions)]))) return;

	  if (unlikely (!actionData->sanitize (&c->sanitizer))) break;
	  hb_barrier ();
	  action = *actionData;

	  uint32_t uoffset = action & LigActionOffset;
	  if (uoffset & 0x20000000)
	    uoffset |= 0xC0000000; /* Sign-extend. */
	  int32_t offset = (int32_t) uoffset;
	  unsigned int component_idx = buffer->cur().codepoint + offset;
	  component_idx = Types::wordOffsetToIndex (component_idx, table, component.arrayZ);
	  const HBUINT16 &componentData = component[component_idx];
	  if (unlikely (!componentData.sanitize (&c->sanitizer))) break;
	  hb_barrier ();
	  ligature_idx += componentData;

	  DEBUG_MSG (APPLY, nullptr, "Action store %d last %d",
		     bool (action & LigActionStore),
		     bool (action & LigActionLast));
	  if (action & (LigActionStore | LigActionLast))
	  {
	    ligature_idx = Types::offsetToIndex (ligature_idx, table, ligature.arrayZ);
	    const HBGlyphID16 &ligatureData = ligature[ligature_idx];
	    if (unlikely (!ligatureData.sanitize (&c->sanitizer))) break;
	    hb_barrier ();
	    hb_codepoint_t lig = ligatureData;

	    DEBUG_MSG (APPLY, nullptr, "Produced ligature %u", lig);
	    if (unlikely (!buffer->replace_glyph (lig))) return;

	    unsigned int lig_end = match_positions[(match_length - 1u) % ARRAY_LENGTH (match_positions)] + 1u;
	    /* Now go and delete all subsequent components. */
	    while (match_length - 1u > cursor)
	    {
	      DEBUG_MSG (APPLY, nullptr, "Skipping ligature component");
	      if (unlikely (!buffer->move_to (match_positions[--match_length % ARRAY_LENGTH (match_positions)]))) return;
	      buffer->cur().unicode_props() |= UPROPS_MASK_IGNORABLE;
	      if (unlikely (!buffer->replace_glyph (DELETED_GLYPH))) return;
	    }

	    if (unlikely (!buffer->move_to (lig_end))) return;
	    buffer->merge_out_clusters (match_positions[cursor % ARRAY_LENGTH (match_positions)], buffer->out_len);
	  }

	  actionData++;
	}
	while (!(action & LigActionLast));
	if (unlikely (!buffer->move_to (end))) return;
      }
    }

    public:
    bool ret;
    private:
    hb_aat_apply_context_t *c;
    const LigatureSubtable *table;
    const UnsizedArrayOf<HBUINT32> &ligAction;
    const UnsizedArrayOf<HBUINT16> &component;
    const UnsizedArrayOf<HBGlyphID16> &ligature;
    unsigned int match_length;
    unsigned int match_positions[HB_MAX_CONTEXT_LENGTH];
  };

  bool apply (hb_aat_apply_context_t *c) const
  {
    TRACE_APPLY (this);

    driver_context_t dc (this, c);

    StateTableDriver<Types, EntryData> driver (machine, c->face);

    if (driver.is_idempotent_on_all_out_of_bounds (&dc, c) &&
	!c->buffer_digest.may_have (c->machine_glyph_set))
      return_trace (false);

    driver.drive (&dc, c);

    return_trace (dc.ret);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    /* The rest of array sanitizations are done at run-time. */
    return_trace (c->check_struct (this) && machine.sanitize (c) &&
		  hb_barrier () &&
		  ligAction && component && ligature);
  }

  public:
  StateTable<Types, EntryData>
		machine;
  protected:
  NNOffsetTo<UnsizedArrayOf<HBUINT32>, HBUINT>
		ligAction;	/* Offset to the ligature action table. */
  NNOffsetTo<UnsizedArrayOf<HBUINT16>, HBUINT>
		component;	/* Offset to the component table. */
  NNOffsetTo<UnsizedArrayOf<HBGlyphID16>, HBUINT>
		ligature;	/* Offset to the actual ligature lists. */
  public:
  DEFINE_SIZE_STATIC ((StateTable<Types, EntryData>::static_size + 3 * HBUINT::static_size));
};

template <typename Types>
struct NoncontextualSubtable
{
  bool apply (hb_aat_apply_context_t *c) const
  {
    TRACE_APPLY (this);

    const OT::GDEF &gdef (*c->gdef_table);
    bool has_glyph_classes = gdef.has_glyph_classes ();

    bool ret = false;
    unsigned int num_glyphs = c->face->get_num_glyphs ();

    hb_glyph_info_t *info = c->buffer->info;
    unsigned int count = c->buffer->len;
    // If there's only one range, we already checked the flag.
    auto *last_range = c->range_flags && (c->range_flags->length > 1) ? &(*c->range_flags)[0] : nullptr;
    for (unsigned int i = 0; i < count; i++)
    {
      /* This block copied from StateTableDriver::drive. Keep in sync. */
      if (last_range)
      {
	auto *range = last_range;
	{
	  unsigned cluster = info[i].cluster;
	  while (cluster < range->cluster_first)
	    range--;
	  while (cluster > range->cluster_last)
	    range++;

	  last_range = range;
	}
	if (!(range->flags & c->subtable_flags))
	  continue;
      }

      const HBGlyphID16 *replacement = substitute.get_value (info[i].codepoint, num_glyphs);
      if (replacement)
      {
	info[i].codepoint = *replacement;
	c->buffer_digest.add (*replacement);
	if (has_glyph_classes)
	  _hb_glyph_info_set_glyph_props (&info[i],
					  gdef.get_glyph_props (*replacement));
	ret = true;
      }
    }

    return_trace (ret);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (substitute.sanitize (c));
  }

  protected:
  Lookup<HBGlyphID16>	substitute;
  public:
  DEFINE_SIZE_MIN (2);
};

template <typename Types>
struct InsertionSubtable
{
  typedef typename Types::HBUINT HBUINT;

  struct EntryData
  {
    HBUINT16	currentInsertIndex;	/* Zero-based index into the insertion glyph table.
					 * The number of glyphs to be inserted is contained
					 * in the currentInsertCount field in the flags.
					 * A value of 0xFFFF indicates no insertion is to
					 * be done. */
    HBUINT16	markedInsertIndex;	/* Zero-based index into the insertion glyph table.
					 * The number of glyphs to be inserted is contained
					 * in the markedInsertCount field in the flags.
					 * A value of 0xFFFF indicates no insertion is to
					 * be done. */
    public:
    DEFINE_SIZE_STATIC (4);
  };

  struct driver_context_t
  {
    static constexpr bool in_place = false;
    enum Flags
    {
      SetMark		= 0x8000,	/* If set, mark the current glyph. */
      DontAdvance	= 0x4000,	/* If set, don't advance to the next glyph before
					 * going to the new state.  This does not mean
					 * that the glyph pointed to is the same one as
					 * before. If you've made insertions immediately
					 * downstream of the current glyph, the next glyph
					 * processed would in fact be the first one
					 * inserted. */
      CurrentIsKashidaLike= 0x2000,	/* If set, and the currentInsertList is nonzero,
					 * then the specified glyph list will be inserted
					 * as a kashida-like insertion, either before or
					 * after the current glyph (depending on the state
					 * of the currentInsertBefore flag). If clear, and
					 * the currentInsertList is nonzero, then the
					 * specified glyph list will be inserted as a
					 * split-vowel-like insertion, either before or
					 * after the current glyph (depending on the state
					 * of the currentInsertBefore flag). */
      MarkedIsKashidaLike= 0x1000,	/* If set, and the markedInsertList is nonzero,
					 * then the specified glyph list will be inserted
					 * as a kashida-like insertion, either before or
					 * after the marked glyph (depending on the state
					 * of the markedInsertBefore flag). If clear, and
					 * the markedInsertList is nonzero, then the
					 * specified glyph list will be inserted as a
					 * split-vowel-like insertion, either before or
					 * after the marked glyph (depending on the state
					 * of the markedInsertBefore flag). */
      CurrentInsertBefore= 0x0800,	/* If set, specifies that insertions are to be made
					 * to the left of the current glyph. If clear,
					 * they're made to the right of the current glyph. */
      MarkedInsertBefore= 0x0400,	/* If set, specifies that insertions are to be
					 * made to the left of the marked glyph. If clear,
					 * they're made to the right of the marked glyph. */
      CurrentInsertCount= 0x3E0,	/* This 5-bit field is treated as a count of the
					 * number of glyphs to insert at the current
					 * position. Since zero means no insertions, the
					 * largest number of insertions at any given
					 * current location is 31 glyphs. */
      MarkedInsertCount= 0x001F,	/* This 5-bit field is treated as a count of the
					 * number of glyphs to insert at the marked
					 * position. Since zero means no insertions, the
					 * largest number of insertions at any given
					 * marked location is 31 glyphs. */
    };

    driver_context_t (const InsertionSubtable *table,
		      hb_aat_apply_context_t *c_) :
	ret (false),
	c (c_),
	mark (0),
	insertionAction (table+table->insertionAction) {}

    bool is_actionable (hb_buffer_t *buffer HB_UNUSED,
			StateTableDriver<Types, EntryData> *driver HB_UNUSED,
			const Entry<EntryData> &entry) const
    {
      return (entry.flags & (CurrentInsertCount | MarkedInsertCount)) &&
	     (entry.data.currentInsertIndex != 0xFFFF ||entry.data.markedInsertIndex != 0xFFFF);
    }
    void transition (hb_buffer_t *buffer,
		     StateTableDriver<Types, EntryData> *driver,
		     const Entry<EntryData> &entry)
    {
      unsigned int flags = entry.flags;

      unsigned mark_loc = buffer->out_len;

      if (entry.data.markedInsertIndex != 0xFFFF)
      {
	unsigned int count = (flags & MarkedInsertCount);
	if (unlikely ((buffer->max_ops -= count) <= 0)) return;
	unsigned int start = entry.data.markedInsertIndex;
	const HBGlyphID16 *glyphs = &insertionAction[start];
	if (unlikely (!c->sanitizer.check_array (glyphs, count))) count = 0;
	hb_barrier ();

	bool before = flags & MarkedInsertBefore;

	unsigned int end = buffer->out_len;
	if (unlikely (!buffer->move_to (mark))) return;

	if (buffer->idx < buffer->len && !before)
	  if (unlikely (!buffer->copy_glyph ())) return;
	/* TODO We ignore KashidaLike setting. */
	if (unlikely (!buffer->replace_glyphs (0, count, glyphs))) return;
	for (unsigned int i = 0; i < count; i++)
	  c->buffer_digest.add (glyphs[i]);
	ret = true;
	if (buffer->idx < buffer->len && !before)
	  buffer->skip_glyph ();

	if (unlikely (!buffer->move_to (end + count))) return;

	buffer->unsafe_to_break_from_outbuffer (mark, hb_min (buffer->idx + 1, buffer->len));
      }

      if (flags & SetMark)
	mark = mark_loc;

      if (entry.data.currentInsertIndex != 0xFFFF)
      {
	unsigned int count = (flags & CurrentInsertCount) >> 5;
	if (unlikely ((buffer->max_ops -= count) <= 0)) return;
	unsigned int start = entry.data.currentInsertIndex;
	const HBGlyphID16 *glyphs = &insertionAction[start];
	if (unlikely (!c->sanitizer.check_array (glyphs, count))) count = 0;
	hb_barrier ();

	bool before = flags & CurrentInsertBefore;

	unsigned int end = buffer->out_len;

	if (buffer->idx < buffer->len && !before)
	  if (unlikely (!buffer->copy_glyph ())) return;
	/* TODO We ignore KashidaLike setting. */
	if (unlikely (!buffer->replace_glyphs (0, count, glyphs))) return;
	if (buffer->idx < buffer->len && !before)
	  buffer->skip_glyph ();

	/* Humm. Not sure where to move to.  There's this wording under
	 * DontAdvance flag:
	 *
	 * "If set, don't update the glyph index before going to the new state.
	 * This does not mean that the glyph pointed to is the same one as
	 * before. If you've made insertions immediately downstream of the
	 * current glyph, the next glyph processed would in fact be the first
	 * one inserted."
	 *
	 * This suggests that if DontAdvance is NOT set, we should move to
	 * end+count.  If it *was*, then move to end, such that newly inserted
	 * glyphs are now visible.
	 *
	 * https://github.com/harfbuzz/harfbuzz/issues/1224#issuecomment-427691417
	 */
	if (unlikely (!buffer->move_to ((flags & DontAdvance) ? end : end + count))) return;
      }
    }

    public:
    bool ret;
    private:
    hb_aat_apply_context_t *c;
    unsigned int mark;
    const UnsizedArrayOf<HBGlyphID16> &insertionAction;
  };

  bool apply (hb_aat_apply_context_t *c) const
  {
    TRACE_APPLY (this);

    driver_context_t dc (this, c);

    StateTableDriver<Types, EntryData> driver (machine, c->face);

    if (driver.is_idempotent_on_all_out_of_bounds (&dc, c) &&
	!c->buffer_digest.may_have (c->machine_glyph_set))
      return_trace (false);

    driver.drive (&dc, c);

    return_trace (dc.ret);
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    /* The rest of array sanitizations are done at run-time. */
    return_trace (c->check_struct (this) && machine.sanitize (c) &&
		  hb_barrier () &&
		  insertionAction);
  }

  public:
  StateTable<Types, EntryData>
		machine;
  protected:
  NNOffsetTo<UnsizedArrayOf<HBGlyphID16>, HBUINT>
		insertionAction;	/* Byte offset from stateHeader to the start of
					 * the insertion glyph table. */
  public:
  DEFINE_SIZE_STATIC ((StateTable<Types, EntryData>::static_size + HBUINT::static_size));
};


struct Feature
{
  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this));
  }

  public:
  HBUINT16	featureType;	/* The type of feature. */
  HBUINT16	featureSetting;	/* The feature's setting (aka selector). */
  HBUINT32	enableFlags;	/* Flags for the settings that this feature
				 * and setting enables. */
  HBUINT32	disableFlags;	/* Complement of flags for the settings that this
				 * feature and setting disable. */

  public:
  DEFINE_SIZE_STATIC (12);
};


struct hb_accelerate_subtables_context_t :
       hb_dispatch_context_t<hb_accelerate_subtables_context_t>
{
  struct hb_applicable_t
  {
    friend struct hb_accelerate_subtables_context_t;
    friend struct hb_aat_layout_lookup_accelerator_t;

    public:
    hb_set_digest_t digest;

    template <typename T>
    auto init_ (const T &obj_, unsigned num_glyphs, hb_priority<1>) HB_AUTO_RETURN
    (
      obj_.machine.collect_glyphs (this->digest, num_glyphs)
    )

    template <typename T>
    void init_ (const T &obj_, unsigned num_glyphs, hb_priority<0>)
    {
      digest = digest.full ();
    }

    template <typename T>
    void init (const T &obj_, unsigned num_glyphs)
    {
      init_ (obj_, num_glyphs, hb_prioritize);
    }
  };

  /* Dispatch interface. */
  template <typename T>
  return_t dispatch (const T &obj)
  {
    hb_applicable_t *entry = &array[i++];

    entry->init (obj, num_glyphs);

    return hb_empty_t ();
  }
  static return_t default_return_value () { return hb_empty_t (); }

  bool stop_sublookup_iteration (return_t r) const { return false; }

  hb_accelerate_subtables_context_t (hb_applicable_t *array_, unsigned num_glyphs_) :
				     hb_dispatch_context_t<hb_accelerate_subtables_context_t> (),
				     array (array_), num_glyphs (num_glyphs_) {}

  hb_applicable_t *array;
  unsigned num_glyphs;
  unsigned i = 0;
};

struct hb_aat_layout_chain_accelerator_t
{
  template <typename TChain>
  static hb_aat_layout_chain_accelerator_t *create (const TChain &chain, unsigned num_glyphs)
  {
    unsigned count = chain.get_subtable_count ();

    unsigned size = sizeof (hb_aat_layout_chain_accelerator_t) -
		    HB_VAR_ARRAY * sizeof (hb_accelerate_subtables_context_t::hb_applicable_t) +
		    count * sizeof (hb_accelerate_subtables_context_t::hb_applicable_t);

    /* The following is a calloc because when we are collecting subtables,
     * some of them might be invalid and hence not collect; as a result,
     * we might not fill in all the count entries of the subtables array.
     * Zeroing it allows the set digest to gatekeep it without having to
     * initialize it further. */
    auto *thiz = (hb_aat_layout_chain_accelerator_t *) hb_calloc (1, size);
    if (unlikely (!thiz))
      return nullptr;

    hb_accelerate_subtables_context_t c_accelerate_subtables (thiz->subtables, num_glyphs);
    chain.dispatch (&c_accelerate_subtables);

    return thiz;
  }

  hb_accelerate_subtables_context_t::hb_applicable_t subtables[HB_VAR_ARRAY];
};

template <typename Types>
struct ChainSubtable
{
  typedef typename Types::HBUINT HBUINT;

  template <typename T>
  friend struct Chain;

  unsigned int get_size () const     { return length; }
  unsigned int get_type () const     { return coverage & 0xFF; }
  unsigned int get_coverage () const { return coverage >> (sizeof (HBUINT) * 8 - 8); }

  enum Coverage
  {
    Vertical		= 0x80,	/* If set, this subtable will only be applied
				 * to vertical text. If clear, this subtable
				 * will only be applied to horizontal text. */
    Backwards		= 0x40,	/* If set, this subtable will process glyphs
				 * in descending order. If clear, it will
				 * process the glyphs in ascending order. */
    AllDirections	= 0x20,	/* If set, this subtable will be applied to
				 * both horizontal and vertical text (i.e.
				 * the state of bit 0x80000000 is ignored). */
    Logical		= 0x10,	/* If set, this subtable will process glyphs
				 * in logical order (or reverse logical order,
				 * depending on the value of bit 0x80000000). */
  };
  enum Type
  {
    Rearrangement	= 0,
    Contextual		= 1,
    Ligature		= 2,
    Noncontextual	= 4,
    Insertion		= 5
  };

  template <typename context_t, typename ...Ts>
  typename context_t::return_t dispatch (context_t *c, Ts&&... ds) const
  {
    unsigned int subtable_type = get_type ();
    TRACE_DISPATCH (this, subtable_type);
    switch (subtable_type) {
    case Rearrangement:		return_trace (c->dispatch (u.rearrangement, std::forward<Ts> (ds)...));
    case Contextual:		return_trace (c->dispatch (u.contextual, std::forward<Ts> (ds)...));
    case Ligature:		return_trace (c->dispatch (u.ligature, std::forward<Ts> (ds)...));
    case Noncontextual:		return_trace (c->dispatch (u.noncontextual, std::forward<Ts> (ds)...));
    case Insertion:		return_trace (c->dispatch (u.insertion, std::forward<Ts> (ds)...));
    default:			return_trace (c->default_return_value ());
    }
  }

  bool apply (hb_aat_apply_context_t *c) const
  {
    TRACE_APPLY (this);
    // Disabled for https://github.com/harfbuzz/harfbuzz/issues/4873
    //hb_sanitize_with_object_t with (&c->sanitizer, this);
    return_trace (dispatch (c));
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (!(length.sanitize (c) &&
	  hb_barrier () &&
	  length >= min_size &&
	  c->check_range (this, length)))
      return_trace (false);

    // Disabled for https://github.com/harfbuzz/harfbuzz/issues/4873
    //hb_sanitize_with_object_t with (c, this);
    return_trace (dispatch (c));
  }

  protected:
  HBUINT	length;		/* Total subtable length, including this header. */
  HBUINT	coverage;	/* Coverage flags and subtable type. */
  HBUINT32	subFeatureFlags;/* The 32-bit mask identifying which subtable this is. */
  union {
  RearrangementSubtable<Types>	rearrangement;
  ContextualSubtable<Types>	contextual;
  LigatureSubtable<Types>	ligature;
  NoncontextualSubtable<Types>	noncontextual;
  InsertionSubtable<Types>	insertion;
  } u;
  public:
  DEFINE_SIZE_MIN (2 * sizeof (HBUINT) + 4);
};

template <typename Types>
struct Chain
{
  typedef typename Types::HBUINT HBUINT;

  unsigned get_subtable_count () const { return subtableCount; }

  hb_mask_t compile_flags (const hb_aat_map_builder_t *map) const
  {
    hb_mask_t flags = defaultFlags;
    {
      unsigned int count = featureCount;
      for (unsigned i = 0; i < count; i++)
      {
	const Feature &feature = featureZ[i];
	hb_aat_layout_feature_type_t type = (hb_aat_layout_feature_type_t) (unsigned int) feature.featureType;
	hb_aat_layout_feature_selector_t setting = (hb_aat_layout_feature_selector_t) (unsigned int) feature.featureSetting;
      retry:
	// Check whether this type/setting pair was requested in the map, and if so, apply its flags.
	// (The search here only looks at the type and setting fields of feature_info_t.)
	hb_aat_map_builder_t::feature_info_t info = { type, setting, false, 0 };
	if (map->current_features.bsearch (info))
	{
	  flags &= feature.disableFlags;
	  flags |= feature.enableFlags;
	}
	else if (type == HB_AAT_LAYOUT_FEATURE_TYPE_LETTER_CASE && setting == HB_AAT_LAYOUT_FEATURE_SELECTOR_SMALL_CAPS)
	{
	  /* Deprecated. https://github.com/harfbuzz/harfbuzz/issues/1342 */
	  type = HB_AAT_LAYOUT_FEATURE_TYPE_LOWER_CASE;
	  setting = HB_AAT_LAYOUT_FEATURE_SELECTOR_LOWER_CASE_SMALL_CAPS;
	  goto retry;
	}
#ifndef HB_NO_AAT
	else if (type == HB_AAT_LAYOUT_FEATURE_TYPE_LANGUAGE_TAG_TYPE && setting &&
		 /* TODO: Rudimentary language matching. */
		 hb_language_matches (map->face->table.ltag->get_language (setting - 1), map->props.language))
	{
	  flags &= feature.disableFlags;
	  flags |= feature.enableFlags;
	}
#endif
      }
    }
    return flags;
  }

  void apply (hb_aat_apply_context_t *c,
	      const hb_aat_layout_chain_accelerator_t *accel) const
  {
    const ChainSubtable<Types> *subtable = &StructAfter<ChainSubtable<Types>> (featureZ.as_array (featureCount));
    unsigned int count = subtableCount;
    for (unsigned int i = 0; i < count; i++)
    {
      bool reverse;

      if (hb_none (hb_iter (c->range_flags) |
		   hb_map ([&subtable] (const hb_aat_map_t::range_flags_t _) -> bool { return subtable->subFeatureFlags & (_.flags); })))
	goto skip;
      c->subtable_flags = subtable->subFeatureFlags;
      c->machine_glyph_set = accel ? accel->subtables[i].digest : hb_set_digest_t::full ();

      if (!(subtable->get_coverage() & ChainSubtable<Types>::AllDirections) &&
	  HB_DIRECTION_IS_VERTICAL (c->buffer->props.direction) !=
	  bool (subtable->get_coverage() & ChainSubtable<Types>::Vertical))
	goto skip;

      /* Buffer contents is always in logical direction.  Determine if
       * we need to reverse before applying this subtable.  We reverse
       * back after if we did reverse indeed.
       *
       * Quoting the spac:
       * """
       * Bits 28 and 30 of the coverage field control the order in which
       * glyphs are processed when the subtable is run by the layout engine.
       * Bit 28 is used to indicate if the glyph processing direction is
       * the same as logical order or layout order. Bit 30 is used to
       * indicate whether glyphs are processed forwards or backwards within
       * that order.

		Bit 30	Bit 28	Interpretation for Horizontal Text
		0	0	The subtable is processed in layout order
				(the same order as the glyphs, which is
				always left-to-right).
		1	0	The subtable is processed in reverse layout order
				(the order opposite that of the glyphs, which is
				always right-to-left).
		0	1	The subtable is processed in logical order
				(the same order as the characters, which may be
				left-to-right or right-to-left).
		1	1	The subtable is processed in reverse logical order
				(the order opposite that of the characters, which
				may be right-to-left or left-to-right).
       */
      reverse = subtable->get_coverage () & ChainSubtable<Types>::Logical ?
		bool (subtable->get_coverage () & ChainSubtable<Types>::Backwards) :
		bool (subtable->get_coverage () & ChainSubtable<Types>::Backwards) !=
		HB_DIRECTION_IS_BACKWARD (c->buffer->props.direction);

      if (!c->buffer->message (c->font, "start chainsubtable %u", c->lookup_index))
	goto skip;

      if (reverse)
	c->buffer->reverse ();

      subtable->apply (c);

      if (reverse)
	c->buffer->reverse ();

      (void) c->buffer->message (c->font, "end chainsubtable %u", c->lookup_index);

      if (unlikely (!c->buffer->successful)) return;

    skip:
      subtable = &StructAfter<ChainSubtable<Types>> (*subtable);
      c->set_lookup_index (c->lookup_index + 1);
    }
  }

  unsigned int get_size () const { return length; }

  template <typename context_t, typename ...Ts>
  typename context_t::return_t dispatch (context_t *c, Ts&&... ds) const
  {
    const ChainSubtable<Types> *subtable = &StructAfter<ChainSubtable<Types>> (featureZ.as_array (featureCount));
    unsigned int count = subtableCount;
    for (unsigned int i = 0; i < count; i++)
    {
      typename context_t::return_t ret = subtable->dispatch (c, std::forward<Ts> (ds)...);
      if (c->stop_sublookup_iteration (ret))
	return ret;
      subtable = &StructAfter<ChainSubtable<Types>> (*subtable);
    }
    return c->default_return_value ();
  }

  bool sanitize (hb_sanitize_context_t *c, unsigned int version) const
  {
    TRACE_SANITIZE (this);
    if (!(length.sanitize (c) &&
	  hb_barrier () &&
	  length >= min_size &&
	  c->check_range (this, length)))
      return_trace (false);

    if (!c->check_array (featureZ.arrayZ, featureCount))
      return_trace (false);

    const ChainSubtable<Types> *subtable = &StructAfter<ChainSubtable<Types>> (featureZ.as_array (featureCount));
    unsigned int count = subtableCount;
    for (unsigned int i = 0; i < count; i++)
    {
      if (!subtable->sanitize (c))
	return_trace (false);
      hb_barrier ();
      subtable = &StructAfter<ChainSubtable<Types>> (*subtable);
    }

    if (version >= 3)
    {
      const SubtableGlyphCoverage *coverage = (const SubtableGlyphCoverage *) subtable;
      if (!coverage->sanitize (c, count))
        return_trace (false);
    }

    return_trace (true);
  }

  protected:
  HBUINT32	defaultFlags;	/* The default specification for subtables. */
  HBUINT32	length;		/* Total byte count, including this header. */
  HBUINT	featureCount;	/* Number of feature subtable entries. */
  HBUINT	subtableCount;	/* The number of subtables in the chain. */

  UnsizedArrayOf<Feature>	featureZ;	/* Features. */
/*ChainSubtable	firstSubtable;*//* Subtables. */
/*SubtableGlyphCoverage coverages*//* Only if version >= 3. */

  public:
  DEFINE_SIZE_MIN (8 + 2 * sizeof (HBUINT));
};


/*
 * The 'mort'/'morx' Table
 */

template <typename T, typename Types, hb_tag_t TAG>
struct mortmorx
{
  static constexpr hb_tag_t tableTag = TAG;

  bool has_data () const { return version != 0; }

  struct accelerator_t
  {
    accelerator_t (hb_face_t *face)
    {
      hb_sanitize_context_t sc;
      this->table = sc.reference_table<T> (face);

      this->chain_count = table->get_chain_count ();

      this->accels = (hb_atomic_ptr_t<hb_aat_layout_chain_accelerator_t> *) hb_calloc (this->chain_count, sizeof (*accels));
      if (unlikely (!this->accels))
      {
	this->chain_count = 0;
	this->table.destroy ();
	this->table = hb_blob_get_empty ();
      }
    }
    ~accelerator_t ()
    {
      for (unsigned int i = 0; i < this->chain_count; i++)
	hb_free (this->accels[i]);
      hb_free (this->accels);
      this->table.destroy ();
    }

    hb_blob_t *get_blob () const { return table.get_blob (); }

    template <typename Chain>
    hb_aat_layout_chain_accelerator_t *get_accel (unsigned chain_index, const Chain &chain, unsigned num_glyphs) const
    {
      if (unlikely (chain_index >= chain_count)) return nullptr;

    retry:
      auto *accel = accels[chain_index].get_acquire ();
      if (unlikely (!accel))
      {
	accel = hb_aat_layout_chain_accelerator_t::create (chain, num_glyphs);
	if (unlikely (!accel))
	  return nullptr;

	if (unlikely (!accels[chain_index].cmpexch (nullptr, accel)))
	{
	  hb_free (accel);
	  goto retry;
	}
      }

      return accel;
    }

    hb_blob_ptr_t<T> table;
    unsigned int chain_count;
    hb_atomic_ptr_t<hb_aat_layout_chain_accelerator_t> *accels;
  };


  void compile_flags (const hb_aat_map_builder_t *mapper,
		      hb_aat_map_t *map) const
  {
    const Chain<Types> *chain = &firstChain;
    unsigned int count = chainCount;
    if (unlikely (!map->chain_flags.resize (count)))
      return;
    for (unsigned int i = 0; i < count; i++)
    {
      map->chain_flags[i].push (hb_aat_map_t::range_flags_t {chain->compile_flags (mapper),
							     mapper->range_first,
							     mapper->range_last});
      chain = &StructAfter<Chain<Types>> (*chain);
    }
  }

  unsigned get_chain_count () const
  {
	  return chainCount;
  }

  void apply (hb_aat_apply_context_t *c,
	      const hb_aat_map_t &map,
	      const accelerator_t &accel) const
  {
    if (unlikely (!c->buffer->successful)) return;

    c->buffer->unsafe_to_concat ();

    if (c->buffer->len < HB_AAT_BUFFER_DIGEST_THRESHOLD)
      c->buffer_digest = c->buffer->digest ();
    else
      c->buffer_digest = hb_set_digest_t::full ();

    c->set_lookup_index (0);
    const Chain<Types> *chain = &firstChain;
    unsigned int count = chainCount;
    for (unsigned int i = 0; i < count; i++)
    {
      auto *chain_accel = accel.get_accel (i, *chain, c->face->get_num_glyphs ());
      c->range_flags = &map.chain_flags[i];
      chain->apply (c, chain_accel);
      if (unlikely (!c->buffer->successful)) return;
      chain = &StructAfter<Chain<Types>> (*chain);
    }
  }

  bool sanitize (hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    if (!(version.sanitize (c) &&
	  hb_barrier () &&
	  version &&
	  chainCount.sanitize (c)))
      return_trace (false);

    const Chain<Types> *chain = &firstChain;
    unsigned int count = chainCount;
    for (unsigned int i = 0; i < count; i++)
    {
      if (!chain->sanitize (c, version))
	return_trace (false);
      hb_barrier ();
      chain = &StructAfter<Chain<Types>> (*chain);
    }

    return_trace (true);
  }

  protected:
  HBUINT16	version;	/* Version number of the glyph metamorphosis table.
				 * 1, 2, or 3. */
  HBUINT16	unused;		/* Set to 0. */
  HBUINT32	chainCount;	/* Number of metamorphosis chains contained in this
				 * table. */
  Chain<Types>	firstChain;	/* Chains. */

  public:
  DEFINE_SIZE_MIN (8);
};

struct morx : mortmorx<morx, ExtendedTypes, HB_AAT_TAG_morx> {};
struct mort : mortmorx<mort, ObsoleteTypes, HB_AAT_TAG_mort> {};

struct morx_accelerator_t : morx::accelerator_t {
  morx_accelerator_t (hb_face_t *face) : morx::accelerator_t (face) {}
};
struct mort_accelerator_t : mort::accelerator_t {
  mort_accelerator_t (hb_face_t *face) : mort::accelerator_t (face) {}
};


} /* namespace AAT */


#endif /* HB_AAT_LAYOUT_MORX_TABLE_HH */
