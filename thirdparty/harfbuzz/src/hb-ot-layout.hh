/*
 * Copyright © 2007,2008,2009  Red Hat, Inc.
 * Copyright © 2012,2013  Google, Inc.
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

#ifndef HB_OT_LAYOUT_HH
#define HB_OT_LAYOUT_HH

#include "hb.hh"

#include "hb-font.hh"
#include "hb-buffer.hh"
#include "hb-open-type.hh"
#include "hb-ot-shape.hh"
#include "hb-set-digest.hh"


struct hb_ot_shape_plan_t;


/*
 * kern
 */

HB_INTERNAL bool
hb_ot_layout_has_kerning (hb_face_t *face);

HB_INTERNAL bool
hb_ot_layout_has_machine_kerning (hb_face_t *face);

HB_INTERNAL bool
hb_ot_layout_has_cross_kerning (hb_face_t *face);

HB_INTERNAL void
hb_ot_layout_kern (const hb_ot_shape_plan_t *plan,
		   hb_font_t *font,
		   hb_buffer_t  *buffer);


/* Private API corresponding to hb-ot-layout.h: */

HB_INTERNAL bool
hb_ot_layout_table_find_feature (hb_face_t    *face,
				 hb_tag_t      table_tag,
				 hb_tag_t      feature_tag,
				 unsigned int *feature_index);


/*
 * GDEF
 */

enum hb_ot_layout_glyph_props_flags_t
{
  /* The following three match LookupFlags::Ignore* numbers. */
  HB_OT_LAYOUT_GLYPH_PROPS_BASE_GLYPH	= 0x02u,
  HB_OT_LAYOUT_GLYPH_PROPS_LIGATURE	= 0x04u,
  HB_OT_LAYOUT_GLYPH_PROPS_MARK		= 0x08u,

  /* The following are used internally; not derived from GDEF. */
  HB_OT_LAYOUT_GLYPH_PROPS_SUBSTITUTED	= 0x10u,
  HB_OT_LAYOUT_GLYPH_PROPS_LIGATED	= 0x20u,
  HB_OT_LAYOUT_GLYPH_PROPS_MULTIPLIED	= 0x40u,

  HB_OT_LAYOUT_GLYPH_PROPS_PRESERVE     = HB_OT_LAYOUT_GLYPH_PROPS_SUBSTITUTED |
					  HB_OT_LAYOUT_GLYPH_PROPS_LIGATED |
					  HB_OT_LAYOUT_GLYPH_PROPS_MULTIPLIED
};
HB_MARK_AS_FLAG_T (hb_ot_layout_glyph_props_flags_t);


/*
 * GSUB/GPOS
 */


/* Should be called before all the substitute_lookup's are done. */
HB_INTERNAL void
hb_ot_layout_substitute_start (hb_font_t    *font,
			       hb_buffer_t  *buffer);

namespace OT {
  struct hb_ot_apply_context_t;
  struct hb_ot_layout_lookup_accelerator_t;
namespace Layout {
namespace GSUB_impl {
  struct SubstLookup;
}
}
}

HB_INTERNAL void
hb_ot_layout_substitute_lookup (OT::hb_ot_apply_context_t *c,
				const OT::Layout::GSUB_impl::SubstLookup &lookup,
				const OT::hb_ot_layout_lookup_accelerator_t &accel);


/* Should be called before all the position_lookup's are done. */
HB_INTERNAL void
hb_ot_layout_position_start (hb_font_t    *font,
			     hb_buffer_t  *buffer);

/* Should be called after all the position_lookup's are done, to fini advances. */
HB_INTERNAL void
hb_ot_layout_position_finish_advances (hb_font_t    *font,
				       hb_buffer_t  *buffer);

/* Should be called after hb_ot_layout_position_finish_advances, to fini offsets. */
HB_INTERNAL void
hb_ot_layout_position_finish_offsets (hb_font_t    *font,
				      hb_buffer_t  *buffer);


/*
 * Buffer var routines.
 */

/* buffer var allocations, used during the entire shaping process */
#define unicode_props()		var2.u16[0]

/* buffer var allocations, used during the GSUB/GPOS processing */
#define glyph_props()		var1.u16[0] /* GDEF glyph properties */
#define lig_props()		var1.u8[2] /* GSUB/GPOS ligature tracking */
#define syllable()		var1.u8[3] /* GSUB/GPOS shaping boundaries */


/* Loop over syllables. Based on foreach_cluster(). */
#define foreach_syllable(buffer, start, end) \
  for (unsigned int \
       _count = buffer->len, \
       start = 0, end = _count ? _hb_next_syllable (buffer, 0) : 0; \
       start < _count; \
       start = end, end = _hb_next_syllable (buffer, start))

static inline unsigned int
_hb_next_syllable (hb_buffer_t *buffer, unsigned int start)
{
  hb_glyph_info_t *info = buffer->info;
  unsigned int count = buffer->len;

  unsigned int syllable = info[start].syllable();
  while (++start < count && syllable == info[start].syllable())
    ;

  return start;
}


/* unicode_props */

/* Design:
 * unicode_props() is a two-byte number.  The low byte includes:
 * - Extended General_Category: 5 bits.
 * - A bit each for:
 *   * Is it Default_Ignorable(); we have a modified Default_Ignorable().
 *   * Whether it's one of the four Mongolian Free Variation Selectors,
 *     CGJ, or other characters that are hidden but should not be ignored
 *     like most other Default_Ignorable()s do during GSUB matching.
 *   * Whether it's a grapheme continuation.
 *
 * The high-byte has different meanings, switched by the Gen-Cat:
 * - For Mn,Mc,Me: the modified Combining_Class.
 * - For Cf: whether it's ZWJ, ZWNJ, or something else.
 * - For Ws: index of which space character this is, if space fallback
 *   is needed, ie. we don't set this by default, only if asked to.
 */

enum hb_unicode_props_flags_t {
  UPROPS_MASK_GEN_CAT	= 0x001Fu,
  UPROPS_MASK_IGNORABLE	= 0x0020u,
  UPROPS_MASK_HIDDEN	= 0x0040u, /* MONGOLIAN FREE VARIATION SELECTOR 1..4, or TAG characters */
  UPROPS_MASK_CONTINUATION=0x0080u,

  /* If GEN_CAT=FORMAT, top byte masks: */
  UPROPS_MASK_Cf_ZWJ	= 0x0100u,
  UPROPS_MASK_Cf_ZWNJ	= 0x0200u
};
HB_MARK_AS_FLAG_T (hb_unicode_props_flags_t);

static inline void
_hb_glyph_info_set_unicode_props (hb_glyph_info_t *info, hb_buffer_t *buffer)
{
  hb_unicode_funcs_t *unicode = buffer->unicode;
  unsigned int u = info->codepoint;
  unsigned int gen_cat = (unsigned int) unicode->general_category (u);
  unsigned int props = gen_cat;

  if (u >= 0x80u)
  {
    buffer->scratch_flags |= HB_BUFFER_SCRATCH_FLAG_HAS_NON_ASCII;

    if (unlikely (unicode->is_default_ignorable (u)))
    {
      buffer->scratch_flags |= HB_BUFFER_SCRATCH_FLAG_HAS_DEFAULT_IGNORABLES;
      props |=  UPROPS_MASK_IGNORABLE;
      if (u == 0x200Cu) props |= UPROPS_MASK_Cf_ZWNJ;
      else if (u == 0x200Du) props |= UPROPS_MASK_Cf_ZWJ;
      /* Mongolian Free Variation Selectors need to be remembered
       * because although we need to hide them like default-ignorables,
       * they need to non-ignorable during shaping.  This is similar to
       * what we do for joiners in Indic-like shapers, but since the
       * FVSes are GC=Mn, we have use a separate bit to remember them.
       * Fixes:
       * https://github.com/harfbuzz/harfbuzz/issues/234 */
      else if (unlikely (hb_in_ranges<hb_codepoint_t> (u, 0x180Bu, 0x180Du, 0x180Fu, 0x180Fu))) props |= UPROPS_MASK_HIDDEN;
      /* TAG characters need similar treatment. Fixes:
       * https://github.com/harfbuzz/harfbuzz/issues/463 */
      else if (unlikely (hb_in_range<hb_codepoint_t> (u, 0xE0020u, 0xE007Fu))) props |= UPROPS_MASK_HIDDEN;
      /* COMBINING GRAPHEME JOINER should not be skipped; at least some times.
       * https://github.com/harfbuzz/harfbuzz/issues/554 */
      else if (unlikely (u == 0x034Fu))
      {
	buffer->scratch_flags |= HB_BUFFER_SCRATCH_FLAG_HAS_CGJ;
	props |= UPROPS_MASK_HIDDEN;
      }
    }

    if (unlikely (HB_UNICODE_GENERAL_CATEGORY_IS_MARK (gen_cat)))
    {
      props |= UPROPS_MASK_CONTINUATION;
      props |= unicode->modified_combining_class (u)<<8;
    }
  }

  info->unicode_props() = props;
}

static inline void
_hb_glyph_info_set_general_category (hb_glyph_info_t *info,
				     hb_unicode_general_category_t gen_cat)
{
  /* Clears top-byte. */
  info->unicode_props() = (unsigned int) gen_cat | (info->unicode_props() & (0xFF & ~UPROPS_MASK_GEN_CAT));
}

static inline hb_unicode_general_category_t
_hb_glyph_info_get_general_category (const hb_glyph_info_t *info)
{
  return (hb_unicode_general_category_t) (info->unicode_props() & UPROPS_MASK_GEN_CAT);
}

static inline bool
_hb_glyph_info_is_unicode_mark (const hb_glyph_info_t *info)
{
  return HB_UNICODE_GENERAL_CATEGORY_IS_MARK (info->unicode_props() & UPROPS_MASK_GEN_CAT);
}
static inline void
_hb_glyph_info_set_modified_combining_class (hb_glyph_info_t *info,
					     unsigned int modified_class)
{
  if (unlikely (!_hb_glyph_info_is_unicode_mark (info)))
    return;
  info->unicode_props() = (modified_class<<8) | (info->unicode_props() & 0xFF);
}
static inline unsigned int
_hb_glyph_info_get_modified_combining_class (const hb_glyph_info_t *info)
{
  return _hb_glyph_info_is_unicode_mark (info) ? info->unicode_props()>>8 : 0;
}
#define info_cc(info) (_hb_glyph_info_get_modified_combining_class (&(info)))

static inline bool
_hb_glyph_info_is_unicode_space (const hb_glyph_info_t *info)
{
  return _hb_glyph_info_get_general_category (info) ==
	 HB_UNICODE_GENERAL_CATEGORY_SPACE_SEPARATOR;
}
static inline void
_hb_glyph_info_set_unicode_space_fallback_type (hb_glyph_info_t *info, hb_unicode_funcs_t::space_t s)
{
  if (unlikely (!_hb_glyph_info_is_unicode_space (info)))
    return;
  info->unicode_props() = (((unsigned int) s)<<8) | (info->unicode_props() & 0xFF);
}
static inline hb_unicode_funcs_t::space_t
_hb_glyph_info_get_unicode_space_fallback_type (const hb_glyph_info_t *info)
{
  return _hb_glyph_info_is_unicode_space (info) ?
	 (hb_unicode_funcs_t::space_t) (info->unicode_props()>>8) :
	 hb_unicode_funcs_t::NOT_SPACE;
}

static inline bool _hb_glyph_info_substituted (const hb_glyph_info_t *info);

static inline bool
_hb_glyph_info_is_default_ignorable (const hb_glyph_info_t *info)
{
  return (info->unicode_props() & UPROPS_MASK_IGNORABLE) &&
	 !_hb_glyph_info_substituted (info);
}
static inline void
_hb_glyph_info_clear_default_ignorable (hb_glyph_info_t *info)
{
  info->unicode_props() &= ~ UPROPS_MASK_IGNORABLE;
}
static inline bool
_hb_glyph_info_is_hidden (const hb_glyph_info_t *info)
{
  return info->unicode_props() & UPROPS_MASK_HIDDEN;
}
static inline void
_hb_glyph_info_unhide (hb_glyph_info_t *info)
{
  info->unicode_props() &= ~ UPROPS_MASK_HIDDEN;
}

static inline void
_hb_glyph_info_set_continuation (hb_glyph_info_t *info)
{
  info->unicode_props() |= UPROPS_MASK_CONTINUATION;
}
static inline void
_hb_glyph_info_reset_continuation (hb_glyph_info_t *info)
{
  info->unicode_props() &= ~ UPROPS_MASK_CONTINUATION;
}
static inline bool
_hb_glyph_info_is_continuation (const hb_glyph_info_t *info)
{
  return info->unicode_props() & UPROPS_MASK_CONTINUATION;
}

static inline bool
_hb_grapheme_group_func (const hb_glyph_info_t& a HB_UNUSED,
			 const hb_glyph_info_t& b)
{ return _hb_glyph_info_is_continuation (&b); }

#define foreach_grapheme(buffer, start, end) \
	foreach_group (buffer, start, end, _hb_grapheme_group_func)

static inline void
_hb_ot_layout_reverse_graphemes (hb_buffer_t *buffer)
{
  buffer->reverse_groups (_hb_grapheme_group_func,
			  buffer->cluster_level == HB_BUFFER_CLUSTER_LEVEL_MONOTONE_CHARACTERS);
}

static inline bool
_hb_glyph_info_is_unicode_format (const hb_glyph_info_t *info)
{
  return _hb_glyph_info_get_general_category (info) ==
	 HB_UNICODE_GENERAL_CATEGORY_FORMAT;
}
static inline bool
_hb_glyph_info_is_zwnj (const hb_glyph_info_t *info)
{
  return _hb_glyph_info_is_unicode_format (info) && (info->unicode_props() & UPROPS_MASK_Cf_ZWNJ);
}
static inline bool
_hb_glyph_info_is_zwj (const hb_glyph_info_t *info)
{
  return _hb_glyph_info_is_unicode_format (info) && (info->unicode_props() & UPROPS_MASK_Cf_ZWJ);
}
static inline bool
_hb_glyph_info_is_joiner (const hb_glyph_info_t *info)
{
  return _hb_glyph_info_is_unicode_format (info) && (info->unicode_props() & (UPROPS_MASK_Cf_ZWNJ|UPROPS_MASK_Cf_ZWJ));
}
static inline void
_hb_glyph_info_flip_joiners (hb_glyph_info_t *info)
{
  if (!_hb_glyph_info_is_unicode_format (info))
    return;
  info->unicode_props() ^= UPROPS_MASK_Cf_ZWNJ | UPROPS_MASK_Cf_ZWJ;
}

/* lig_props: aka lig_id / lig_comp
 *
 * When a ligature is formed:
 *
 *   - The ligature glyph and any marks in between all the same newly allocated
 *     lig_id,
 *   - The ligature glyph will get lig_num_comps set to the number of components
 *   - The marks get lig_comp > 0, reflecting which component of the ligature
 *     they were applied to.
 *   - This is used in GPOS to attach marks to the right component of a ligature
 *     in MarkLigPos,
 *   - Note that when marks are ligated together, much of the above is skipped
 *     and the current lig_id reused.
 *
 * When a multiple-substitution is done:
 *
 *   - All resulting glyphs will have lig_id = 0,
 *   - The resulting glyphs will have lig_comp = 0, 1, 2, ... respectively.
 *   - This is used in GPOS to attach marks to the first component of a
 *     multiple substitution in MarkBasePos.
 *
 * The numbers are also used in GPOS to do mark-to-mark positioning only
 * to marks that belong to the same component of the same ligature.
 */

static inline void
_hb_glyph_info_clear_lig_props (hb_glyph_info_t *info)
{
  info->lig_props() = 0;
}

#define IS_LIG_BASE 0x10

static inline void
_hb_glyph_info_set_lig_props_for_ligature (hb_glyph_info_t *info,
					   unsigned int lig_id,
					   unsigned int lig_num_comps)
{
  info->lig_props() = (lig_id << 5) | IS_LIG_BASE | (lig_num_comps & 0x0F);
}

static inline void
_hb_glyph_info_set_lig_props_for_mark (hb_glyph_info_t *info,
				       unsigned int lig_id,
				       unsigned int lig_comp)
{
  info->lig_props() = (lig_id << 5) | (lig_comp & 0x0F);
}

static inline void
_hb_glyph_info_set_lig_props_for_component (hb_glyph_info_t *info, unsigned int comp)
{
  _hb_glyph_info_set_lig_props_for_mark (info, 0, comp);
}

static inline unsigned int
_hb_glyph_info_get_lig_id (const hb_glyph_info_t *info)
{
  return info->lig_props() >> 5;
}

static inline bool
_hb_glyph_info_ligated_internal (const hb_glyph_info_t *info)
{
  return info->lig_props() & IS_LIG_BASE;
}

static inline unsigned int
_hb_glyph_info_get_lig_comp (const hb_glyph_info_t *info)
{
  if (_hb_glyph_info_ligated_internal (info))
    return 0;
  else
    return info->lig_props() & 0x0F;
}

static inline unsigned int
_hb_glyph_info_get_lig_num_comps (const hb_glyph_info_t *info)
{
  if ((info->glyph_props() & HB_OT_LAYOUT_GLYPH_PROPS_LIGATURE) &&
      _hb_glyph_info_ligated_internal (info))
    return info->lig_props() & 0x0F;
  else
    return 1;
}

static inline uint8_t
_hb_allocate_lig_id (hb_buffer_t *buffer)
{
  uint8_t lig_id = buffer->next_serial () & 0x07;
  if (unlikely (!lig_id))
    lig_id = _hb_allocate_lig_id (buffer); /* in case of overflow */
  return lig_id;
}

/* glyph_props: */

static inline void
_hb_glyph_info_set_glyph_props (hb_glyph_info_t *info, unsigned int props)
{
  info->glyph_props() = props;
}

static inline unsigned int
_hb_glyph_info_get_glyph_props (const hb_glyph_info_t *info)
{
  return info->glyph_props();
}

static inline bool
_hb_glyph_info_is_base_glyph (const hb_glyph_info_t *info)
{
  return info->glyph_props() & HB_OT_LAYOUT_GLYPH_PROPS_BASE_GLYPH;
}

static inline bool
_hb_glyph_info_is_ligature (const hb_glyph_info_t *info)
{
  return info->glyph_props() & HB_OT_LAYOUT_GLYPH_PROPS_LIGATURE;
}

static inline bool
_hb_glyph_info_is_mark (const hb_glyph_info_t *info)
{
  return info->glyph_props() & HB_OT_LAYOUT_GLYPH_PROPS_MARK;
}

static inline bool
_hb_glyph_info_substituted (const hb_glyph_info_t *info)
{
  return info->glyph_props() & HB_OT_LAYOUT_GLYPH_PROPS_SUBSTITUTED;
}

static inline bool
_hb_glyph_info_ligated (const hb_glyph_info_t *info)
{
  return info->glyph_props() & HB_OT_LAYOUT_GLYPH_PROPS_LIGATED;
}

static inline bool
_hb_glyph_info_multiplied (const hb_glyph_info_t *info)
{
  return info->glyph_props() & HB_OT_LAYOUT_GLYPH_PROPS_MULTIPLIED;
}

static inline bool
_hb_glyph_info_ligated_and_didnt_multiply (const hb_glyph_info_t *info)
{
  return _hb_glyph_info_ligated (info) && !_hb_glyph_info_multiplied (info);
}

static inline void
_hb_glyph_info_clear_ligated_and_multiplied (hb_glyph_info_t *info)
{
  info->glyph_props() &= ~(HB_OT_LAYOUT_GLYPH_PROPS_LIGATED |
			   HB_OT_LAYOUT_GLYPH_PROPS_MULTIPLIED);
}

static inline void
_hb_glyph_info_clear_substituted (hb_glyph_info_t *info)
{
  info->glyph_props() &= ~(HB_OT_LAYOUT_GLYPH_PROPS_SUBSTITUTED);
}

static inline bool
_hb_clear_substitution_flags (const hb_ot_shape_plan_t *plan HB_UNUSED,
			      hb_font_t *font HB_UNUSED,
			      hb_buffer_t *buffer)
{
  hb_glyph_info_t *info = buffer->info;
  unsigned int count = buffer->len;
  for (unsigned int i = 0; i < count; i++)
    _hb_glyph_info_clear_substituted (&info[i]);
  return false;
}


/* Allocation / deallocation. */

static inline void
_hb_buffer_allocate_unicode_vars (hb_buffer_t *buffer)
{
  HB_BUFFER_ALLOCATE_VAR (buffer, unicode_props);
}

static inline void
_hb_buffer_deallocate_unicode_vars (hb_buffer_t *buffer)
{
  HB_BUFFER_DEALLOCATE_VAR (buffer, unicode_props);
}

static inline void
_hb_buffer_assert_unicode_vars (hb_buffer_t *buffer)
{
  HB_BUFFER_ASSERT_VAR (buffer, unicode_props);
}

static inline void
_hb_buffer_allocate_gsubgpos_vars (hb_buffer_t *buffer)
{
  HB_BUFFER_ALLOCATE_VAR (buffer, glyph_props);
  HB_BUFFER_ALLOCATE_VAR (buffer, lig_props);
}

static inline void
_hb_buffer_deallocate_gsubgpos_vars (hb_buffer_t *buffer)
{
  HB_BUFFER_DEALLOCATE_VAR (buffer, lig_props);
  HB_BUFFER_DEALLOCATE_VAR (buffer, glyph_props);
}

static inline void
_hb_buffer_assert_gsubgpos_vars (hb_buffer_t *buffer)
{
  HB_BUFFER_ASSERT_VAR (buffer, glyph_props);
  HB_BUFFER_ASSERT_VAR (buffer, lig_props);
}

/* Make sure no one directly touches our props... */
#undef unicode_props0
#undef unicode_props1
#undef lig_props
#undef glyph_props

#endif /* HB_OT_LAYOUT_HH */
