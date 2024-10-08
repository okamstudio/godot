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

#include "hb-ot-shaper.hh"


static bool
compose_hebrew (const hb_ot_shape_normalize_context_t *c,
		hb_codepoint_t  a,
		hb_codepoint_t  b,
		hb_codepoint_t *ab)
{
  /* Hebrew presentation-form shaping.
   * https://bugzilla.mozilla.org/show_bug.cgi?id=728866
   * Hebrew presentation forms with dagesh, for characters U+05D0..05EA;
   * Note that some letters do not have a dagesh presForm encoded.
   */
  static const hb_codepoint_t sDageshForms[0x05EAu - 0x05D0u + 1] = {
    0xFB30u, /* ALEF */
    0xFB31u, /* BET */
    0xFB32u, /* GIMEL */
    0xFB33u, /* DALET */
    0xFB34u, /* HE */
    0xFB35u, /* VAV */
    0xFB36u, /* ZAYIN */
    0x0000u, /* HET */
    0xFB38u, /* TET */
    0xFB39u, /* YOD */
    0xFB3Au, /* FINAL KAF */
    0xFB3Bu, /* KAF */
    0xFB3Cu, /* LAMED */
    0x0000u, /* FINAL MEM */
    0xFB3Eu, /* MEM */
    0x0000u, /* FINAL NUN */
    0xFB40u, /* NUN */
    0xFB41u, /* SAMEKH */
    0x0000u, /* AYIN */
    0xFB43u, /* FINAL PE */
    0xFB44u, /* PE */
    0x0000u, /* FINAL TSADI */
    0xFB46u, /* TSADI */
    0xFB47u, /* QOF */
    0xFB48u, /* RESH */
    0xFB49u, /* SHIN */
    0xFB4Au /* TAV */
  };

  bool found = (bool) c->unicode->compose (a, b, ab);

#ifdef HB_NO_OT_SHAPER_HEBREW_FALLBACK
  return found;
#endif

  if (!found && (c->plan && !c->plan->has_gpos_mark))
  {
      /* Special-case Hebrew presentation forms that are excluded from
       * standard normalization, but wanted for old fonts. */
      switch (b) {
      case 0x05B4u: /* HIRIQ */
	  if (a == 0x05D9u) { /* YOD */
	      *ab = 0xFB1Du;
	      found = true;
	  }
	  break;
      case 0x05B7u: /* PATAH */
	  if (a == 0x05F2u) { /* YIDDISH YOD YOD */
	      *ab = 0xFB1Fu;
	      found = true;
	  } else if (a == 0x05D0u) { /* ALEF */
	      *ab = 0xFB2Eu;
	      found = true;
	  }
	  break;
      case 0x05B8u: /* QAMATS */
	  if (a == 0x05D0u) { /* ALEF */
	      *ab = 0xFB2Fu;
	      found = true;
	  }
	  break;
      case 0x05B9u: /* HOLAM */
	  if (a == 0x05D5u) { /* VAV */
	      *ab = 0xFB4Bu;
	      found = true;
	  }
	  break;
      case 0x05BCu: /* DAGESH */
	  if (a >= 0x05D0u && a <= 0x05EAu) {
	      *ab = sDageshForms[a - 0x05D0u];
	      found = (*ab != 0);
	  } else if (a == 0xFB2Au) { /* SHIN WITH SHIN DOT */
	      *ab = 0xFB2Cu;
	      found = true;
	  } else if (a == 0xFB2Bu) { /* SHIN WITH SIN DOT */
	      *ab = 0xFB2Du;
	      found = true;
	  }
	  break;
      case 0x05BFu: /* RAFE */
	  switch (a) {
	  case 0x05D1u: /* BET */
	      *ab = 0xFB4Cu;
	      found = true;
	      break;
	  case 0x05DBu: /* KAF */
	      *ab = 0xFB4Du;
	      found = true;
	      break;
	  case 0x05E4u: /* PE */
	      *ab = 0xFB4Eu;
	      found = true;
	      break;
	  }
	  break;
      case 0x05C1u: /* SHIN DOT */
	  if (a == 0x05E9u) { /* SHIN */
	      *ab = 0xFB2Au;
	      found = true;
	  } else if (a == 0xFB49u) { /* SHIN WITH DAGESH */
	      *ab = 0xFB2Cu;
	      found = true;
	  }
	  break;
      case 0x05C2u: /* SIN DOT */
	  if (a == 0x05E9u) { /* SHIN */
	      *ab = 0xFB2Bu;
	      found = true;
	  } else if (a == 0xFB49u) { /* SHIN WITH DAGESH */
	      *ab = 0xFB2Du;
	      found = true;
	  }
	  break;
      }
  }

  return found;
}

static void
reorder_marks_hebrew (const hb_ot_shape_plan_t *plan HB_UNUSED,
		      hb_buffer_t              *buffer,
		      unsigned int              start,
		      unsigned int              end)
{
  hb_glyph_info_t *info = buffer->info;

  for (unsigned i = start + 2; i < end; i++)
  {
    unsigned c0 = info_cc (info[i - 2]);
    unsigned c1 = info_cc (info[i - 1]);
    unsigned c2 = info_cc (info[i - 0]);

    if ((c0 == HB_MODIFIED_COMBINING_CLASS_CCC17 || c0 == HB_MODIFIED_COMBINING_CLASS_CCC18) /* patach or qamats */ &&
	(c1 == HB_MODIFIED_COMBINING_CLASS_CCC10 || c1 == HB_MODIFIED_COMBINING_CLASS_CCC14) /* sheva or hiriq */ &&
	(c2 == HB_MODIFIED_COMBINING_CLASS_CCC22 || c2 == HB_UNICODE_COMBINING_CLASS_BELOW) /* meteg or below */)
    {
      buffer->merge_clusters (i - 1, i + 1);
      hb_swap (info[i - 1], info[i]);
      break;
    }
  }


}

const hb_ot_shaper_t _hb_ot_shaper_hebrew =
{
  nullptr, /* collect_features */
  nullptr, /* override_features */
  nullptr, /* data_create */
  nullptr, /* data_destroy */
  nullptr, /* preprocess_text */
  nullptr, /* postprocess_glyphs */
  nullptr, /* decompose */
  compose_hebrew,
  nullptr, /* setup_masks */
  reorder_marks_hebrew,
  HB_TAG ('h','e','b','r'), /* gpos_tag. https://github.com/harfbuzz/harfbuzz/issues/347#issuecomment-267838368 */
  HB_OT_SHAPE_NORMALIZATION_MODE_DEFAULT,
  HB_OT_SHAPE_ZERO_WIDTH_MARKS_BY_GDEF_LATE,
  true, /* fallback_position */
};


#endif
