/*
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
 * Google Author(s): Behdad Esfahbod
 */

#include "hb.hh"

#ifndef HB_NO_OT_SHAPE

#include "hb-ot-shaper-arabic.hh"
#include "hb-ot-shape.hh"


/* buffer var allocations */
#define arabic_shaping_action() ot_shaper_var_u8_auxiliary() /* arabic shaping action */

#define HB_BUFFER_SCRATCH_FLAG_ARABIC_HAS_STCH HB_BUFFER_SCRATCH_FLAG_SHAPER0

/* See:
 * https://github.com/harfbuzz/harfbuzz/commit/6e6f82b6f3dde0fc6c3c7d991d9ec6cfff57823d#commitcomment-14248516 */
#define HB_ARABIC_GENERAL_CATEGORY_IS_WORD(gen_cat) \
	(FLAG_UNSAFE (gen_cat) & \
	 (FLAG (HB_UNICODE_GENERAL_CATEGORY_UNASSIGNED) | \
	  FLAG (HB_UNICODE_GENERAL_CATEGORY_PRIVATE_USE) | \
	  /*FLAG (HB_UNICODE_GENERAL_CATEGORY_LOWERCASE_LETTER) |*/ \
	  FLAG (HB_UNICODE_GENERAL_CATEGORY_MODIFIER_LETTER) | \
	  FLAG (HB_UNICODE_GENERAL_CATEGORY_OTHER_LETTER) | \
	  /*FLAG (HB_UNICODE_GENERAL_CATEGORY_TITLECASE_LETTER) |*/ \
	  /*FLAG (HB_UNICODE_GENERAL_CATEGORY_UPPERCASE_LETTER) |*/ \
	  FLAG (HB_UNICODE_GENERAL_CATEGORY_SPACING_MARK) | \
	  FLAG (HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK) | \
	  FLAG (HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK) | \
	  FLAG (HB_UNICODE_GENERAL_CATEGORY_DECIMAL_NUMBER) | \
	  FLAG (HB_UNICODE_GENERAL_CATEGORY_LETTER_NUMBER) | \
	  FLAG (HB_UNICODE_GENERAL_CATEGORY_OTHER_NUMBER) | \
	  FLAG (HB_UNICODE_GENERAL_CATEGORY_CURRENCY_SYMBOL) | \
	  FLAG (HB_UNICODE_GENERAL_CATEGORY_MODIFIER_SYMBOL) | \
	  FLAG (HB_UNICODE_GENERAL_CATEGORY_MATH_SYMBOL) | \
	  FLAG (HB_UNICODE_GENERAL_CATEGORY_OTHER_SYMBOL)))


/*
 * Joining types:
 */

/*
 * Bits used in the joining tables
 */
enum hb_arabic_joining_type_t {
  JOINING_TYPE_U		= 0,
  JOINING_TYPE_L		= 1,
  JOINING_TYPE_R		= 2,
  JOINING_TYPE_D		= 3,
  JOINING_TYPE_C		= JOINING_TYPE_D,
  JOINING_GROUP_ALAPH		= 4,
  JOINING_GROUP_DALATH_RISH	= 5,
  NUM_STATE_MACHINE_COLS	= 6,

  JOINING_TYPE_T = 7,
  JOINING_TYPE_X = 8  /* means: use general-category to choose between U or T. */
};

#include "hb-ot-shaper-arabic-table.hh"

static unsigned int get_joining_type (hb_codepoint_t u, hb_unicode_general_category_t gen_cat)
{
  unsigned int j_type = joining_type(u);
  if (likely (j_type != JOINING_TYPE_X))
    return j_type;

  return (FLAG_UNSAFE(gen_cat) &
	  (FLAG(HB_UNICODE_GENERAL_CATEGORY_NON_SPACING_MARK) |
	   FLAG(HB_UNICODE_GENERAL_CATEGORY_ENCLOSING_MARK) |
	   FLAG(HB_UNICODE_GENERAL_CATEGORY_FORMAT))
	 ) ?  JOINING_TYPE_T : JOINING_TYPE_U;
}

#define FEATURE_IS_SYRIAC(tag) hb_in_range<unsigned char> ((unsigned char) (tag), '2', '3')

static const hb_tag_t arabic_features[] =
{
  HB_TAG('i','s','o','l'),
  HB_TAG('f','i','n','a'),
  HB_TAG('f','i','n','2'),
  HB_TAG('f','i','n','3'),
  HB_TAG('m','e','d','i'),
  HB_TAG('m','e','d','2'),
  HB_TAG('i','n','i','t'),
  HB_TAG_NONE
};


/* Same order as the feature array */
enum arabic_action_t {
  ISOL,
  FINA,
  FIN2,
  FIN3,
  MEDI,
  MED2,
  INIT,

  NONE,

  ARABIC_NUM_FEATURES = NONE,

  /* We abuse the same byte for other things... */
  STCH_FIXED,
  STCH_REPEATING,
};

static const struct arabic_state_table_entry {
	uint8_t prev_action;
	uint8_t curr_action;
	uint16_t next_state;
} arabic_state_table[][NUM_STATE_MACHINE_COLS] =
{
  /*   jt_U,          jt_L,          jt_R,          jt_D,          jg_ALAPH,      jg_DALATH_RISH */

  /* State 0: prev was U, not willing to join. */
  { {NONE,NONE,0}, {NONE,ISOL,2}, {NONE,ISOL,1}, {NONE,ISOL,2}, {NONE,ISOL,1}, {NONE,ISOL,6}, },

  /* State 1: prev was R or ISOL/ALAPH, not willing to join. */
  { {NONE,NONE,0}, {NONE,ISOL,2}, {NONE,ISOL,1}, {NONE,ISOL,2}, {NONE,FIN2,5}, {NONE,ISOL,6}, },

  /* State 2: prev was D/L in ISOL form, willing to join. */
  { {NONE,NONE,0}, {NONE,ISOL,2}, {INIT,FINA,1}, {INIT,FINA,3}, {INIT,FINA,4}, {INIT,FINA,6}, },

  /* State 3: prev was D in FINA form, willing to join. */
  { {NONE,NONE,0}, {NONE,ISOL,2}, {MEDI,FINA,1}, {MEDI,FINA,3}, {MEDI,FINA,4}, {MEDI,FINA,6}, },

  /* State 4: prev was FINA ALAPH, not willing to join. */
  { {NONE,NONE,0}, {NONE,ISOL,2}, {MED2,ISOL,1}, {MED2,ISOL,2}, {MED2,FIN2,5}, {MED2,ISOL,6}, },

  /* State 5: prev was FIN2/FIN3 ALAPH, not willing to join. */
  { {NONE,NONE,0}, {NONE,ISOL,2}, {ISOL,ISOL,1}, {ISOL,ISOL,2}, {ISOL,FIN2,5}, {ISOL,ISOL,6}, },

  /* State 6: prev was DALATH/RISH, not willing to join. */
  { {NONE,NONE,0}, {NONE,ISOL,2}, {NONE,ISOL,1}, {NONE,ISOL,2}, {NONE,FIN3,5}, {NONE,ISOL,6}, }
};


static bool
arabic_fallback_shape (const hb_ot_shape_plan_t *plan,
		       hb_font_t *font,
		       hb_buffer_t *buffer);

static bool
record_stch (const hb_ot_shape_plan_t *plan,
	     hb_font_t *font,
	     hb_buffer_t *buffer);

static bool
deallocate_buffer_var (const hb_ot_shape_plan_t *plan,
		       hb_font_t *font,
		       hb_buffer_t *buffer)
{
  HB_BUFFER_DEALLOCATE_VAR (buffer, arabic_shaping_action);
  return false;
}

static void
collect_features_arabic (hb_ot_shape_planner_t *plan)
{
  hb_ot_map_builder_t *map = &plan->map;

  /* We apply features according to the Arabic spec, with pauses
   * in between most.
   *
   * The pause between init/medi/... and rlig is required.  See eg:
   * https://bugzilla.mozilla.org/show_bug.cgi?id=644184
   *
   * The pauses between init/medi/... themselves are not necessarily
   * needed as only one of those features is applied to any character.
   * The only difference it makes is when fonts have contextual
   * substitutions.  We now follow the order of the spec, which makes
   * for better experience if that's what Uniscribe is doing.
   *
   * At least for Arabic, looks like Uniscribe has a pause between
   * rlig and calt.  Otherwise the IranNastaliq's ALLAH ligature won't
   * work.  However, testing shows that rlig and calt are applied
   * together for Mongolian in Uniscribe.  As such, we only add a
   * pause for Arabic, not other scripts.
   */


  map->enable_feature (HB_TAG('s','t','c','h'));
  map->add_gsub_pause (record_stch);

  map->enable_feature (HB_TAG('c','c','m','p'), F_MANUAL_ZWJ);
  map->enable_feature (HB_TAG('l','o','c','l'), F_MANUAL_ZWJ);

  map->add_gsub_pause (nullptr);

  for (unsigned int i = 0; i < ARABIC_NUM_FEATURES; i++)
  {
    bool has_fallback = plan->props.script == HB_SCRIPT_ARABIC && !FEATURE_IS_SYRIAC (arabic_features[i]);
    map->add_feature (arabic_features[i], F_MANUAL_ZWJ | (has_fallback ? F_HAS_FALLBACK : F_NONE));
    map->add_gsub_pause (nullptr);
  }
   map->add_gsub_pause (deallocate_buffer_var);

  /* Normally, Unicode says a ZWNJ means "don't ligate".  In Arabic script
   * however, it says a ZWJ should also mean "don't ligate".  So we run
   * the main ligating features as MANUAL_ZWJ. */

  map->enable_feature (HB_TAG('r','l','i','g'), F_MANUAL_ZWJ | F_HAS_FALLBACK);

  if (plan->props.script == HB_SCRIPT_ARABIC)
    map->add_gsub_pause (arabic_fallback_shape);

   map->enable_feature (HB_TAG('c','a','l','t'), F_MANUAL_ZWJ);
   /* https://github.com/harfbuzz/harfbuzz/issues/1573 */
   if (!map->has_feature (HB_TAG('r','c','l','t')))
     map->add_gsub_pause (nullptr);

   map->enable_feature (HB_TAG('l','i','g','a'), F_MANUAL_ZWJ);
   map->enable_feature (HB_TAG('c','l','i','g'), F_MANUAL_ZWJ);

  /* The spec includes 'cswh'.  Earlier versions of Windows
   * used to enable this by default, but testing suggests
   * that Windows 8 and later do not enable it by default,
   * and spec now says 'Off by default'.
   * We disabled this in ae23c24c32.
   * Note that IranNastaliq uses this feature extensively
   * to fixup broken glyph sequences.  Oh well...
   * Test case: U+0643,U+0640,U+0631. */
  //map->enable_feature (HB_TAG('c','s','w','h'), F_MANUAL_ZWJ);
  map->enable_feature (HB_TAG('m','s','e','t'), F_MANUAL_ZWJ);
}

#include "hb-ot-shaper-arabic-fallback.hh"

struct arabic_shape_plan_t
{
  /* The "+ 1" in the next array is to accommodate for the "NONE" command,
   * which is not an OpenType feature, but this simplifies the code by not
   * having to do a "if (... < NONE) ..." and just rely on the fact that
   * mask_array[NONE] == 0. */
  hb_mask_t mask_array[ARABIC_NUM_FEATURES + 1];

  hb_atomic_ptr_t<arabic_fallback_plan_t> fallback_plan;

  unsigned int do_fallback : 1;
  unsigned int has_stch : 1;
};

void *
data_create_arabic (const hb_ot_shape_plan_t *plan)
{
  arabic_shape_plan_t *arabic_plan = (arabic_shape_plan_t *) hb_calloc (1, sizeof (arabic_shape_plan_t));
  if (unlikely (!arabic_plan))
    return nullptr;

  arabic_plan->do_fallback = plan->props.script == HB_SCRIPT_ARABIC;
  arabic_plan->has_stch = !!plan->map.get_1_mask (HB_TAG ('s','t','c','h'));
  for (unsigned int i = 0; i < ARABIC_NUM_FEATURES; i++) {
    arabic_plan->mask_array[i] = plan->map.get_1_mask (arabic_features[i]);
    arabic_plan->do_fallback = arabic_plan->do_fallback &&
			       (FEATURE_IS_SYRIAC (arabic_features[i]) ||
				plan->map.needs_fallback (arabic_features[i]));
  }

  return arabic_plan;
}

void
data_destroy_arabic (void *data)
{
  arabic_shape_plan_t *arabic_plan = (arabic_shape_plan_t *) data;

  arabic_fallback_plan_destroy (arabic_plan->fallback_plan);

  hb_free (data);
}

static void
arabic_joining (hb_buffer_t *buffer)
{
  unsigned int count = buffer->len;
  hb_glyph_info_t *info = buffer->info;
  unsigned int prev = UINT_MAX, state = 0;

  /* Check pre-context */
  for (unsigned int i = 0; i < buffer->context_len[0]; i++)
  {
    unsigned int this_type = get_joining_type (buffer->context[0][i], buffer->unicode->general_category (buffer->context[0][i]));

    if (unlikely (this_type == JOINING_TYPE_T))
      continue;

    const arabic_state_table_entry *entry = &arabic_state_table[state][this_type];
    state = entry->next_state;
    break;
  }

  for (unsigned int i = 0; i < count; i++)
  {
    unsigned int this_type = get_joining_type (info[i].codepoint, _hb_glyph_info_get_general_category (&info[i]));

    if (unlikely (this_type == JOINING_TYPE_T)) {
      info[i].arabic_shaping_action() = NONE;
      continue;
    }

    const arabic_state_table_entry *entry = &arabic_state_table[state][this_type];

    if (entry->prev_action != NONE && prev != UINT_MAX)
    {
      info[prev].arabic_shaping_action() = entry->prev_action;
      buffer->safe_to_insert_tatweel (prev, i + 1);
    }
    else
    {
      if (prev == UINT_MAX)
      {
        if (this_type >= JOINING_TYPE_R)
	  buffer->unsafe_to_concat_from_outbuffer (0, i + 1);
      }
      else
      {
	if (this_type >= JOINING_TYPE_R ||
	    (2 <= state && state <= 5) /* States that have a possible prev_action. */)
	  buffer->unsafe_to_concat (prev, i + 1);
      }
    }

    info[i].arabic_shaping_action() = entry->curr_action;

    prev = i;
    state = entry->next_state;
  }

  for (unsigned int i = 0; i < buffer->context_len[1]; i++)
  {
    unsigned int this_type = get_joining_type (buffer->context[1][i], buffer->unicode->general_category (buffer->context[1][i]));

    if (unlikely (this_type == JOINING_TYPE_T))
      continue;

    const arabic_state_table_entry *entry = &arabic_state_table[state][this_type];
    if (entry->prev_action != NONE && prev != UINT_MAX)
    {
      info[prev].arabic_shaping_action() = entry->prev_action;
      buffer->safe_to_insert_tatweel (prev, buffer->len);
    }
    else if (2 <= state && state <= 5) /* States that have a possible prev_action. */
    {
      buffer->unsafe_to_concat (prev, buffer->len);
    }
    break;
  }
}

static void
mongolian_variation_selectors (hb_buffer_t *buffer)
{
  /* Copy arabic_shaping_action() from base to Mongolian variation selectors. */
  unsigned int count = buffer->len;
  hb_glyph_info_t *info = buffer->info;
  for (unsigned int i = 1; i < count; i++)
    if (unlikely (hb_in_ranges<hb_codepoint_t> (info[i].codepoint, 0x180Bu, 0x180Du, 0x180Fu, 0x180Fu)))
      info[i].arabic_shaping_action() = info[i - 1].arabic_shaping_action();
}

void
setup_masks_arabic_plan (const arabic_shape_plan_t *arabic_plan,
			 hb_buffer_t               *buffer,
			 hb_script_t                script)
{
  HB_BUFFER_ALLOCATE_VAR (buffer, arabic_shaping_action);

  arabic_joining (buffer);
  if (script == HB_SCRIPT_MONGOLIAN)
    mongolian_variation_selectors (buffer);

  unsigned int count = buffer->len;
  hb_glyph_info_t *info = buffer->info;
  for (unsigned int i = 0; i < count; i++)
    info[i].mask |= arabic_plan->mask_array[info[i].arabic_shaping_action()];
}

static void
setup_masks_arabic (const hb_ot_shape_plan_t *plan,
		    hb_buffer_t              *buffer,
		    hb_font_t                *font HB_UNUSED)
{
  const arabic_shape_plan_t *arabic_plan = (const arabic_shape_plan_t *) plan->data;
  setup_masks_arabic_plan (arabic_plan, buffer, plan->props.script);
}

static bool
arabic_fallback_shape (const hb_ot_shape_plan_t *plan,
		       hb_font_t *font,
		       hb_buffer_t *buffer)
{
#ifdef HB_NO_OT_SHAPER_ARABIC_FALLBACK
  return false;
#endif

  const arabic_shape_plan_t *arabic_plan = (const arabic_shape_plan_t *) plan->data;

  if (!arabic_plan->do_fallback)
    return false;

retry:
  arabic_fallback_plan_t *fallback_plan = arabic_plan->fallback_plan;
  if (unlikely (!fallback_plan))
  {
    /* This sucks.  We need a font to build the fallback plan... */
    fallback_plan = arabic_fallback_plan_create (plan, font);
    if (unlikely (!arabic_plan->fallback_plan.cmpexch (nullptr, fallback_plan)))
    {
      arabic_fallback_plan_destroy (fallback_plan);
      goto retry;
    }
  }

  arabic_fallback_plan_shape (fallback_plan, font, buffer);
  return true;
}

/*
 * Stretch feature: "stch".
 * See example here:
 * https://docs.microsoft.com/en-us/typography/script-development/syriac
 * We implement this in a generic way, such that the Arabic subtending
 * marks can use it as well.
 */

static bool
record_stch (const hb_ot_shape_plan_t *plan,
	     hb_font_t *font HB_UNUSED,
	     hb_buffer_t *buffer)
{
  const arabic_shape_plan_t *arabic_plan = (const arabic_shape_plan_t *) plan->data;
  if (!arabic_plan->has_stch)
    return false;

  /* 'stch' feature was just applied.  Look for anything that multiplied,
   * and record it for stch treatment later.  Note that rtlm, frac, etc
   * are applied before stch, but we assume that they didn't result in
   * anything multiplying into 5 pieces, so it's safe-ish... */

  unsigned int count = buffer->len;
  hb_glyph_info_t *info = buffer->info;
  for (unsigned int i = 0; i < count; i++)
    if (unlikely (_hb_glyph_info_multiplied (&info[i])))
    {
      unsigned int comp = _hb_glyph_info_get_lig_comp (&info[i]);
      info[i].arabic_shaping_action() = comp % 2 ? STCH_REPEATING : STCH_FIXED;
      buffer->scratch_flags |= HB_BUFFER_SCRATCH_FLAG_ARABIC_HAS_STCH;
    }
  return false;
}

static void
apply_stch (const hb_ot_shape_plan_t *plan HB_UNUSED,
	    hb_buffer_t              *buffer,
	    hb_font_t                *font)
{
  if (likely (!(buffer->scratch_flags & HB_BUFFER_SCRATCH_FLAG_ARABIC_HAS_STCH)))
    return;

  bool rtl = buffer->props.direction == HB_DIRECTION_RTL;

  if (!rtl)
    buffer->reverse ();

  /* We do a two pass implementation:
   * First pass calculates the exact number of extra glyphs we need,
   * We then enlarge buffer to have that much room,
   * Second pass applies the stretch, copying things to the end of buffer.
   */

  int sign = font->x_scale < 0 ? -1 : +1;
  unsigned int extra_glyphs_needed = 0; // Set during MEASURE, used during CUT
  enum { MEASURE, CUT } /* step_t */;

  for (unsigned int step = MEASURE; step <= CUT; step = step + 1)
  {
    unsigned int count = buffer->len;
    hb_glyph_info_t *info = buffer->info;
    hb_glyph_position_t *pos = buffer->pos;
    unsigned int new_len = count + extra_glyphs_needed; // write head during CUT
    unsigned int j = new_len;
    for (unsigned int i = count; i; i--)
    {
      if (!hb_in_range<uint8_t> (info[i - 1].arabic_shaping_action(), STCH_FIXED, STCH_REPEATING))
      {
	if (step == CUT)
	{
	  --j;
	  info[j] = info[i - 1];
	  pos[j] = pos[i - 1];
	}
	continue;
      }

      /* Yay, justification! */

      hb_position_t w_total = 0; // Total to be filled
      hb_position_t w_fixed = 0; // Sum of fixed tiles
      hb_position_t w_repeating = 0; // Sum of repeating tiles
      int n_fixed = 0;
      int n_repeating = 0;

      unsigned int end = i;
      while (i &&
	     hb_in_range<uint8_t> (info[i - 1].arabic_shaping_action(), STCH_FIXED, STCH_REPEATING))
      {
	i--;
	hb_position_t width = font->get_glyph_h_advance (info[i].codepoint);
	if (info[i].arabic_shaping_action() == STCH_FIXED)
	{
	  w_fixed += width;
	  n_fixed++;
	}
	else
	{
	  w_repeating += width;
	  n_repeating++;
	}
      }
      unsigned int start = i;
      unsigned int context = i;
      while (context &&
	     !hb_in_range<uint8_t> (info[context - 1].arabic_shaping_action(), STCH_FIXED, STCH_REPEATING) &&
	     (_hb_glyph_info_is_default_ignorable (&info[context - 1]) ||
	      HB_ARABIC_GENERAL_CATEGORY_IS_WORD (_hb_glyph_info_get_general_category (&info[context - 1]))))
      {
	context--;
	w_total += pos[context].x_advance;
      }
      i++; // Don't touch i again.

      DEBUG_MSG (ARABIC, nullptr, "%s stretch at (%u,%u,%u)",
		 step == MEASURE ? "measuring" : "cutting", context, start, end);
      DEBUG_MSG (ARABIC, nullptr, "rest of word:    count=%u width %" PRId32, start - context, w_total);
      DEBUG_MSG (ARABIC, nullptr, "fixed tiles:     count=%d width=%" PRId32, n_fixed, w_fixed);
      DEBUG_MSG (ARABIC, nullptr, "repeating tiles: count=%d width=%" PRId32, n_repeating, w_repeating);

      /* Number of additional times to repeat each repeating tile. */
      int n_copies = 0;

      hb_position_t w_remaining = w_total - w_fixed;
      if (sign * w_remaining > sign * w_repeating && sign * w_repeating > 0)
	n_copies = (sign * w_remaining) / (sign * w_repeating) - 1;

      /* See if we can improve the fit by adding an extra repeat and squeezing them together a bit. */
      hb_position_t extra_repeat_overlap = 0;
      hb_position_t shortfall = sign * w_remaining - sign * w_repeating * (n_copies + 1);
      if (shortfall > 0 && n_repeating > 0)
      {
	++n_copies;
	hb_position_t excess = (n_copies + 1) * sign * w_repeating - sign * w_remaining;
	if (excess > 0)
	{
	  extra_repeat_overlap = excess / (n_copies * n_repeating);
	  w_remaining = 0;
	}
      }

      if (step == MEASURE)
      {
	extra_glyphs_needed += n_copies * n_repeating;
	DEBUG_MSG (ARABIC, nullptr, "will add extra %d copies of repeating tiles", n_copies);
      }
      else
      {
	buffer->unsafe_to_break (context, end);
	hb_position_t x_offset = w_remaining / 2;
	for (unsigned int k = end; k > start; k--)
	{
	  hb_position_t width = font->get_glyph_h_advance (info[k - 1].codepoint);

	  unsigned int repeat = 1;
	  if (info[k - 1].arabic_shaping_action() == STCH_REPEATING)
	    repeat += n_copies;

	  DEBUG_MSG (ARABIC, nullptr, "appending %u copies of glyph %" PRIu32 "; j=%u",
		     repeat, info[k - 1].codepoint, j);
	  pos[k - 1].x_advance = 0;
	  for (unsigned int n = 0; n < repeat; n++)
	  {
	    if (rtl)
	    {
	      x_offset -= width;
	      if (n > 0)
		x_offset += extra_repeat_overlap;
	    }
	    pos[k - 1].x_offset = x_offset;
	    /* Append copy. */
	    --j;
	    info[j] = info[k - 1];
	    pos[j] = pos[k - 1];

	    if (!rtl)
	    {
	      x_offset += width;
	      if (n > 0)
		x_offset -= extra_repeat_overlap;
	    }
	  }
	}
      }
    }

    if (step == MEASURE)
    {
      if (unlikely (!buffer->ensure (count + extra_glyphs_needed)))
	break;
    }
    else
    {
      assert (j == 0);
      buffer->len = new_len;
    }
  }

  if (!rtl)
    buffer->reverse ();
}


static void
postprocess_glyphs_arabic (const hb_ot_shape_plan_t *plan,
			   hb_buffer_t              *buffer,
			   hb_font_t                *font)
{
  apply_stch (plan, buffer, font);
}

/* https://www.unicode.org/reports/tr53/ */

static hb_codepoint_t
modifier_combining_marks[] =
{
  0x0654u, /* ARABIC HAMZA ABOVE */
  0x0655u, /* ARABIC HAMZA BELOW */
  0x0658u, /* ARABIC MARK NOON GHUNNA */
  0x06DCu, /* ARABIC SMALL HIGH SEEN */
  0x06E3u, /* ARABIC SMALL LOW SEEN */
  0x06E7u, /* ARABIC SMALL HIGH YEH */
  0x06E8u, /* ARABIC SMALL HIGH NOON */
  0x08CAu, /* ARABIC SMALL HIGH FARSI YEH */
  0x08CBu, /* ARABIC SMALL HIGH YEH BARREE WITH TWO DOTS BELOW */
  0x08CDu, /* ARABIC SMALL HIGH ZAH */
  0x08CEu, /* ARABIC LARGE ROUND DOT ABOVE */
  0x08CFu, /* ARABIC LARGE ROUND DOT BELOW */
  0x08D3u, /* ARABIC SMALL LOW WAW */
  0x08F3u, /* ARABIC SMALL HIGH WAW */
};

static inline bool
info_is_mcm (const hb_glyph_info_t &info)
{
  hb_codepoint_t u = info.codepoint;
  for (unsigned int i = 0; i < ARRAY_LENGTH (modifier_combining_marks); i++)
    if (u == modifier_combining_marks[i])
      return true;
  return false;
}

static void
reorder_marks_arabic (const hb_ot_shape_plan_t *plan HB_UNUSED,
		      hb_buffer_t              *buffer,
		      unsigned int              start,
		      unsigned int              end)
{
  hb_glyph_info_t *info = buffer->info;

  DEBUG_MSG (ARABIC, buffer, "Reordering marks from %u to %u", start, end);

  unsigned int i = start;
  for (unsigned int cc = 220; cc <= 230; cc += 10)
  {
    DEBUG_MSG (ARABIC, buffer, "Looking for %u's starting at %u", cc, i);
    while (i < end && info_cc(info[i]) < cc)
      i++;
    DEBUG_MSG (ARABIC, buffer, "Looking for %u's stopped at %u", cc, i);

    if (i == end)
      break;

    if (info_cc(info[i]) > cc)
      continue;

    unsigned int j = i;
    while (j < end && info_cc(info[j]) == cc && info_is_mcm (info[j]))
      j++;

    if (i == j)
      continue;

    DEBUG_MSG (ARABIC, buffer, "Found %u's from %u to %u", cc, i, j);

    /* Shift it! */
    DEBUG_MSG (ARABIC, buffer, "Shifting %u's: %u %u", cc, i, j);
    hb_glyph_info_t temp[HB_OT_SHAPE_MAX_COMBINING_MARKS];
    assert (j - i <= ARRAY_LENGTH (temp));
    buffer->merge_clusters (start, j);
    memmove (temp, &info[i], (j - i) * sizeof (hb_glyph_info_t));
    memmove (&info[start + j - i], &info[start], (i - start) * sizeof (hb_glyph_info_t));
    memmove (&info[start], temp, (j - i) * sizeof (hb_glyph_info_t));

    /* Renumber CC such that the reordered sequence is still sorted.
     * 22 and 26 are chosen because they are smaller than all Arabic categories,
     * and are folded back to 220/230 respectively during fallback mark positioning.
     *
     * We do this because the CGJ-handling logic in the normalizer relies on
     * mark sequences having an increasing order even after this reordering.
     * https://github.com/harfbuzz/harfbuzz/issues/554
     * This, however, does break some obscure sequences, where the normalizer
     * might compose a sequence that it should not.  For example, in the seequence
     * ALEF, HAMZAH, MADDAH, we should NOT try to compose ALEF+MADDAH, but with this
     * renumbering, we will.
     */
    unsigned int new_start = start + j - i;
    unsigned int new_cc = cc == 220 ? HB_MODIFIED_COMBINING_CLASS_CCC22 : HB_MODIFIED_COMBINING_CLASS_CCC26;
    while (start < new_start)
    {
      _hb_glyph_info_set_modified_combining_class (&info[start], new_cc);
      start++;
    }

    i = j;
  }
}

const hb_ot_shaper_t _hb_ot_shaper_arabic =
{
  collect_features_arabic,
  nullptr, /* override_features */
  data_create_arabic,
  data_destroy_arabic,
  nullptr, /* preprocess_text */
  postprocess_glyphs_arabic,
  nullptr, /* decompose */
  nullptr, /* compose */
  setup_masks_arabic,
  reorder_marks_arabic,
  HB_TAG_NONE, /* gpos_tag */
  HB_OT_SHAPE_NORMALIZATION_MODE_DEFAULT,
  HB_OT_SHAPE_ZERO_WIDTH_MARKS_BY_GDEF_LATE,
  true, /* fallback_position */
};


#endif
