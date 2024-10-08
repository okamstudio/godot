/*
 * Copyright © 2010,2011,2012  Google, Inc.
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

#ifndef HB_OT_SHAPER_HH
#define HB_OT_SHAPER_HH

#include "hb.hh"

#include "hb-ot-layout.hh"
#include "hb-ot-shape.hh"
#include "hb-ot-shape-normalize.hh"


/* buffer var allocations, used by all OT shapers */
#define ot_shaper_var_u8_category()	var2.u8[2]
#define ot_shaper_var_u8_auxiliary()	var2.u8[3]


#define HB_OT_SHAPE_MAX_COMBINING_MARKS 32

enum hb_ot_shape_zero_width_marks_type_t {
  HB_OT_SHAPE_ZERO_WIDTH_MARKS_NONE,
  HB_OT_SHAPE_ZERO_WIDTH_MARKS_BY_GDEF_EARLY,
  HB_OT_SHAPE_ZERO_WIDTH_MARKS_BY_GDEF_LATE
};


/* Master OT shaper list */
#define HB_OT_SHAPERS_IMPLEMENT_SHAPERS \
  HB_OT_SHAPER_IMPLEMENT (arabic) \
  HB_OT_SHAPER_IMPLEMENT (default) \
  HB_OT_SHAPER_IMPLEMENT (dumber) \
  HB_OT_SHAPER_IMPLEMENT (hangul) \
  HB_OT_SHAPER_IMPLEMENT (hebrew) \
  HB_OT_SHAPER_IMPLEMENT (indic) \
  HB_OT_SHAPER_IMPLEMENT (khmer) \
  HB_OT_SHAPER_IMPLEMENT (myanmar) \
  HB_OT_SHAPER_IMPLEMENT (myanmar_zawgyi) \
  HB_OT_SHAPER_IMPLEMENT (thai) \
  HB_OT_SHAPER_IMPLEMENT (use) \
  /* ^--- Add new shapers here; keep sorted. */


struct hb_ot_shaper_t
{
  /* collect_features()
   * Called during shape_plan().
   * Shapers should use plan->map to add their features and callbacks.
   * May be NULL.
   */
  void (*collect_features) (hb_ot_shape_planner_t *plan);

  /* override_features()
   * Called during shape_plan().
   * Shapers should use plan->map to override features and add callbacks after
   * common features are added.
   * May be NULL.
   */
  void (*override_features) (hb_ot_shape_planner_t *plan);


  /* data_create()
   * Called at the end of shape_plan().
   * Whatever shapers return will be accessible through plan->data later.
   * If nullptr is returned, means a plan failure.
   */
  void *(*data_create) (const hb_ot_shape_plan_t *plan);

  /* data_destroy()
   * Called when the shape_plan is being destroyed.
   * plan->data is passed here for destruction.
   * If nullptr is returned, means a plan failure.
   * May be NULL.
   */
  void (*data_destroy) (void *data);


  /* preprocess_text()
   * Called during shape().
   * Shapers can use to modify text before shaping starts.
   * May be NULL.
   */
  void (*preprocess_text) (const hb_ot_shape_plan_t *plan,
			   hb_buffer_t              *buffer,
			   hb_font_t                *font);

  /* postprocess_glyphs()
   * Called during shape().
   * Shapers can use to modify glyphs after shaping ends.
   * May be NULL.
   */
  void (*postprocess_glyphs) (const hb_ot_shape_plan_t *plan,
			      hb_buffer_t              *buffer,
			      hb_font_t                *font);


  /* decompose()
   * Called during shape()'s normalization.
   * May be NULL.
   */
  bool (*decompose) (const hb_ot_shape_normalize_context_t *c,
		     hb_codepoint_t  ab,
		     hb_codepoint_t *a,
		     hb_codepoint_t *b);

  /* compose()
   * Called during shape()'s normalization.
   * May be NULL.
   */
  bool (*compose) (const hb_ot_shape_normalize_context_t *c,
		   hb_codepoint_t  a,
		   hb_codepoint_t  b,
		   hb_codepoint_t *ab);

  /* setup_masks()
   * Called during shape().
   * Shapers should use map to get feature masks and set on buffer.
   * Shapers may NOT modify characters.
   * May be NULL.
   */
  void (*setup_masks) (const hb_ot_shape_plan_t *plan,
		       hb_buffer_t              *buffer,
		       hb_font_t                *font);

  /* reorder_marks()
   * Called during shape().
   * Shapers can use to modify ordering of combining marks.
   * May be NULL.
   */
  void (*reorder_marks) (const hb_ot_shape_plan_t *plan,
			 hb_buffer_t              *buffer,
			 unsigned int              start,
			 unsigned int              end);

  /* gpos_tag()
   * If not HB_TAG_NONE, then must match found GPOS script tag for
   * GPOS to be applied.  Otherwise, fallback positioning will be used.
   */
  hb_tag_t gpos_tag;

  hb_ot_shape_normalization_mode_t normalization_preference;

  hb_ot_shape_zero_width_marks_type_t zero_width_marks;

  bool fallback_position;
};

#define HB_OT_SHAPER_IMPLEMENT(name) extern HB_INTERNAL const hb_ot_shaper_t _hb_ot_shaper_##name;
HB_OT_SHAPERS_IMPLEMENT_SHAPERS
#undef HB_OT_SHAPER_IMPLEMENT


static inline const hb_ot_shaper_t *
hb_ot_shaper_categorize (hb_script_t script,
			 hb_direction_t direction,
			 hb_tag_t gsub_script)
{
  switch ((hb_tag_t) script)
  {
    default:
      return &_hb_ot_shaper_default;


    /* Unicode-1.1 additions */
    case HB_SCRIPT_ARABIC:

    /* Unicode-3.0 additions */
    case HB_SCRIPT_SYRIAC:

      /* For Arabic script, use the Arabic shaper even if no OT script tag was found.
       * This is because we do fallback shaping for Arabic script (and not others).
       * But note that Arabic shaping is applicable only to horizontal layout; for
       * vertical text, just use the generic shaper instead. */
      if ((gsub_script != HB_OT_TAG_DEFAULT_SCRIPT || script == HB_SCRIPT_ARABIC) &&
	  HB_DIRECTION_IS_HORIZONTAL (direction))
	return &_hb_ot_shaper_arabic;
      else
	return &_hb_ot_shaper_default;


    /* Unicode-1.1 additions */
    case HB_SCRIPT_THAI:
    case HB_SCRIPT_LAO:

      return &_hb_ot_shaper_thai;


    /* Unicode-1.1 additions */
    case HB_SCRIPT_HANGUL:

      return &_hb_ot_shaper_hangul;


    /* Unicode-1.1 additions */
    case HB_SCRIPT_HEBREW:

      return &_hb_ot_shaper_hebrew;


    /* Unicode-1.1 additions */
    case HB_SCRIPT_BENGALI:
    case HB_SCRIPT_DEVANAGARI:
    case HB_SCRIPT_GUJARATI:
    case HB_SCRIPT_GURMUKHI:
    case HB_SCRIPT_KANNADA:
    case HB_SCRIPT_MALAYALAM:
    case HB_SCRIPT_ORIYA:
    case HB_SCRIPT_TAMIL:
    case HB_SCRIPT_TELUGU:

      /* If the designer designed the font for the 'DFLT' script,
       * (or we ended up arbitrarily pick 'latn'), use the default shaper.
       * Otherwise, use the specific shaper.
       *
       * If it's indy3 tag, send to USE. */
      if (gsub_script == HB_TAG ('D','F','L','T') ||
	  gsub_script == HB_TAG ('l','a','t','n'))
	return &_hb_ot_shaper_default;
      else if ((gsub_script & 0x000000FF) == '3')
	return &_hb_ot_shaper_use;
      else
	return &_hb_ot_shaper_indic;

    case HB_SCRIPT_KHMER:
	return &_hb_ot_shaper_khmer;

    case HB_SCRIPT_MYANMAR:
      /* If the designer designed the font for the 'DFLT' script,
       * (or we ended up arbitrarily pick 'latn'), use the default shaper.
       * Otherwise, use the specific shaper.
       *
       * If designer designed for 'mymr' tag, also send to default
       * shaper.  That's tag used from before Myanmar shaping spec
       * was developed.  The shaping spec uses 'mym2' tag. */
      if (gsub_script == HB_TAG ('D','F','L','T') ||
	  gsub_script == HB_TAG ('l','a','t','n') ||
	  gsub_script == HB_TAG ('m','y','m','r'))
	return &_hb_ot_shaper_default;
      else
	return &_hb_ot_shaper_myanmar;


#ifndef HB_NO_OT_SHAPER_MYANMAR_ZAWGYI
#define HB_SCRIPT_MYANMAR_ZAWGYI	((hb_script_t) HB_TAG ('Q','a','a','g'))
    case HB_SCRIPT_MYANMAR_ZAWGYI:
    /* https://github.com/harfbuzz/harfbuzz/issues/1162 */

      return &_hb_ot_shaper_myanmar_zawgyi;
#endif


    /* Unicode-2.0 additions */
    case HB_SCRIPT_TIBETAN:

    /* Unicode-3.0 additions */
    case HB_SCRIPT_MONGOLIAN:
    case HB_SCRIPT_SINHALA:

    /* Unicode-3.2 additions */
    case HB_SCRIPT_BUHID:
    case HB_SCRIPT_HANUNOO:
    case HB_SCRIPT_TAGALOG:
    case HB_SCRIPT_TAGBANWA:

    /* Unicode-4.0 additions */
    case HB_SCRIPT_LIMBU:
    case HB_SCRIPT_TAI_LE:

    /* Unicode-4.1 additions */
    case HB_SCRIPT_BUGINESE:
    case HB_SCRIPT_KHAROSHTHI:
    case HB_SCRIPT_SYLOTI_NAGRI:
    case HB_SCRIPT_TIFINAGH:

    /* Unicode-5.0 additions */
    case HB_SCRIPT_BALINESE:
    case HB_SCRIPT_NKO:
    case HB_SCRIPT_PHAGS_PA:

    /* Unicode-5.1 additions */
    case HB_SCRIPT_CHAM:
    case HB_SCRIPT_KAYAH_LI:
    case HB_SCRIPT_LEPCHA:
    case HB_SCRIPT_REJANG:
    case HB_SCRIPT_SAURASHTRA:
    case HB_SCRIPT_SUNDANESE:

    /* Unicode-5.2 additions */
    case HB_SCRIPT_EGYPTIAN_HIEROGLYPHS:
    case HB_SCRIPT_JAVANESE:
    case HB_SCRIPT_KAITHI:
    case HB_SCRIPT_MEETEI_MAYEK:
    case HB_SCRIPT_TAI_THAM:
    case HB_SCRIPT_TAI_VIET:

    /* Unicode-6.0 additions */
    case HB_SCRIPT_BATAK:
    case HB_SCRIPT_BRAHMI:
    case HB_SCRIPT_MANDAIC:

    /* Unicode-6.1 additions */
    case HB_SCRIPT_CHAKMA:
    case HB_SCRIPT_MIAO:
    case HB_SCRIPT_SHARADA:
    case HB_SCRIPT_TAKRI:

    /* Unicode-7.0 additions */
    case HB_SCRIPT_DUPLOYAN:
    case HB_SCRIPT_GRANTHA:
    case HB_SCRIPT_KHOJKI:
    case HB_SCRIPT_KHUDAWADI:
    case HB_SCRIPT_MAHAJANI:
    case HB_SCRIPT_MANICHAEAN:
    case HB_SCRIPT_MODI:
    case HB_SCRIPT_PAHAWH_HMONG:
    case HB_SCRIPT_PSALTER_PAHLAVI:
    case HB_SCRIPT_SIDDHAM:
    case HB_SCRIPT_TIRHUTA:

    /* Unicode-8.0 additions */
    case HB_SCRIPT_AHOM:
    case HB_SCRIPT_MULTANI:

    /* Unicode-9.0 additions */
    case HB_SCRIPT_ADLAM:
    case HB_SCRIPT_BHAIKSUKI:
    case HB_SCRIPT_MARCHEN:
    case HB_SCRIPT_NEWA:

    /* Unicode-10.0 additions */
    case HB_SCRIPT_MASARAM_GONDI:
    case HB_SCRIPT_SOYOMBO:
    case HB_SCRIPT_ZANABAZAR_SQUARE:

    /* Unicode-11.0 additions */
    case HB_SCRIPT_DOGRA:
    case HB_SCRIPT_GUNJALA_GONDI:
    case HB_SCRIPT_HANIFI_ROHINGYA:
    case HB_SCRIPT_MAKASAR:
    case HB_SCRIPT_MEDEFAIDRIN:
    case HB_SCRIPT_OLD_SOGDIAN:
    case HB_SCRIPT_SOGDIAN:

    /* Unicode-12.0 additions */
    case HB_SCRIPT_ELYMAIC:
    case HB_SCRIPT_NANDINAGARI:
    case HB_SCRIPT_NYIAKENG_PUACHUE_HMONG:
    case HB_SCRIPT_WANCHO:

    /* Unicode-13.0 additions */
    case HB_SCRIPT_CHORASMIAN:
    case HB_SCRIPT_DIVES_AKURU:
    case HB_SCRIPT_KHITAN_SMALL_SCRIPT:
    case HB_SCRIPT_YEZIDI:

    /* Unicode-14.0 additions */
    case HB_SCRIPT_CYPRO_MINOAN:
    case HB_SCRIPT_OLD_UYGHUR:
    case HB_SCRIPT_TANGSA:
    case HB_SCRIPT_TOTO:
    case HB_SCRIPT_VITHKUQI:

    /* Unicode-15.0 additions */
    case HB_SCRIPT_KAWI:
    case HB_SCRIPT_NAG_MUNDARI:

    /* Unicode-16.0 additions */
    case HB_SCRIPT_GARAY:
    case HB_SCRIPT_GURUNG_KHEMA:
    case HB_SCRIPT_KIRAT_RAI:
    case HB_SCRIPT_OL_ONAL:
    case HB_SCRIPT_SUNUWAR:
    case HB_SCRIPT_TODHRI:
    case HB_SCRIPT_TULU_TIGALARI:

      /* If the designer designed the font for the 'DFLT' script,
       * (or we ended up arbitrarily pick 'latn'), use the default shaper.
       * Otherwise, use the specific shaper.
       * Note that for some simple scripts, there may not be *any*
       * GSUB/GPOS needed, so there may be no scripts found! */
      if (gsub_script == HB_TAG ('D','F','L','T') ||
	  gsub_script == HB_TAG ('l','a','t','n'))
	return &_hb_ot_shaper_default;
      else
	return &_hb_ot_shaper_use;
  }
}


#endif /* HB_OT_SHAPER_HH */
