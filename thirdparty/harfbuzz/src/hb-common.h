/*
 * Copyright © 2007,2008,2009  Red Hat, Inc.
 * Copyright © 2011,2012  Google, Inc.
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

#if !defined(HB_H_IN) && !defined(HB_NO_SINGLE_HEADER_ERROR)
#error "Include <hb.h> instead."
#endif

#ifndef HB_COMMON_H
#define HB_COMMON_H

#ifndef HB_EXTERN
#define HB_EXTERN extern
#endif

#ifndef HB_BEGIN_DECLS
# ifdef __cplusplus
#  define HB_BEGIN_DECLS	extern "C" {
#  define HB_END_DECLS		}
# else /* !__cplusplus */
#  define HB_BEGIN_DECLS
#  define HB_END_DECLS
# endif /* !__cplusplus */
#endif

#if defined (_AIX)
#  include <sys/inttypes.h>
#elif defined (_MSC_VER) && _MSC_VER < 1600
/* VS 2010 (_MSC_VER 1600) has stdint.h   */
typedef __int8 int8_t;
typedef unsigned __int8 uint8_t;
typedef __int16 int16_t;
typedef unsigned __int16 uint16_t;
typedef __int32 int32_t;
typedef unsigned __int32 uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
#elif defined (_MSC_VER) && _MSC_VER < 1800
/* VS 2013 (_MSC_VER 1800) has inttypes.h */
#  include <stdint.h>
#else
#  include <inttypes.h>
#endif

#if defined(__GNUC__) && ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ >= 1))
#define HB_DEPRECATED __attribute__((__deprecated__))
#elif defined(_MSC_VER) && (_MSC_VER >= 1300)
#define HB_DEPRECATED __declspec(deprecated)
#else
#define HB_DEPRECATED
#endif

#if defined(__GNUC__) && ((__GNUC__ > 4) || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5))
#define HB_DEPRECATED_FOR(f) __attribute__((__deprecated__("Use '" #f "' instead")))
#elif defined(_MSC_FULL_VER) && (_MSC_FULL_VER > 140050320)
#define HB_DEPRECATED_FOR(f) __declspec(deprecated("is deprecated. Use '" #f "' instead"))
#else
#define HB_DEPRECATED_FOR(f) HB_DEPRECATED
#endif


HB_BEGIN_DECLS

/**
 * hb_bool_t:
 * 
 * Data type for booleans.
 *
 **/
typedef int hb_bool_t;

/**
 * hb_codepoint_t:
 * 
 * Data type for holding Unicode codepoints. Also
 * used to hold glyph IDs.
 *
 **/
typedef uint32_t hb_codepoint_t;

/**
 * HB_CODEPOINT_INVALID:
 *
 * Unused #hb_codepoint_t value.
 *
 * Since: 8.0.0
 */
#define HB_CODEPOINT_INVALID ((hb_codepoint_t) -1)

/**
 * hb_position_t:
 * 
 * Data type for holding a single coordinate value.
 * Contour points and other multi-dimensional data are
 * stored as tuples of #hb_position_t's.
 *
 **/
typedef int32_t hb_position_t;
/**
 * hb_mask_t:
 * 
 * Data type for bitmasks.
 *
 **/
typedef uint32_t hb_mask_t;

typedef union _hb_var_int_t {
  uint32_t u32;
  int32_t i32;
  uint16_t u16[2];
  int16_t i16[2];
  uint8_t u8[4];
  int8_t i8[4];
} hb_var_int_t;

typedef union _hb_var_num_t {
  float f;
  uint32_t u32;
  int32_t i32;
  uint16_t u16[2];
  int16_t i16[2];
  uint8_t u8[4];
  int8_t i8[4];
} hb_var_num_t;


/* hb_tag_t */

/**
 * hb_tag_t:
 *
 * Data type for tag identifiers. Tags are four
 * byte integers, each byte representing a character.
 *
 * Tags are used to identify tables, design-variation axes,
 * scripts, languages, font features, and baselines with
 * human-readable names.
 *
 **/
typedef uint32_t hb_tag_t;

/**
 * HB_TAG:
 * @c1: 1st character of the tag
 * @c2: 2nd character of the tag
 * @c3: 3rd character of the tag
 * @c4: 4th character of the tag
 *
 * Constructs an #hb_tag_t from four character literals.
 *
 **/
#define HB_TAG(c1,c2,c3,c4) ((hb_tag_t)((((uint32_t)(c1)&0xFF)<<24)|(((uint32_t)(c2)&0xFF)<<16)|(((uint32_t)(c3)&0xFF)<<8)|((uint32_t)(c4)&0xFF)))

/**
 * HB_UNTAG:
 * @tag: an #hb_tag_t
 *
 * Extracts four character literals from an #hb_tag_t.
 *
 * Since: 0.6.0
 *
 **/
#define HB_UNTAG(tag)   (uint8_t)(((tag)>>24)&0xFF), (uint8_t)(((tag)>>16)&0xFF), (uint8_t)(((tag)>>8)&0xFF), (uint8_t)((tag)&0xFF)

/**
 * HB_TAG_NONE:
 *
 * Unset #hb_tag_t.
 */
#define HB_TAG_NONE HB_TAG(0,0,0,0)
/**
 * HB_TAG_MAX:
 *
 * Maximum possible unsigned #hb_tag_t.
 *
 * Since: 0.9.26
 */
#define HB_TAG_MAX HB_TAG(0xff,0xff,0xff,0xff)
/**
 * HB_TAG_MAX_SIGNED:
 *
 * Maximum possible signed #hb_tag_t.
 *
 * Since: 0.9.33
 */
#define HB_TAG_MAX_SIGNED HB_TAG(0x7f,0xff,0xff,0xff)

/* len=-1 means str is NUL-terminated. */
HB_EXTERN hb_tag_t
hb_tag_from_string (const char *str, int len);

/* buf should have 4 bytes. */
HB_EXTERN void
hb_tag_to_string (hb_tag_t tag, char *buf);


/**
 * hb_direction_t:
 * @HB_DIRECTION_INVALID: Initial, unset direction.
 * @HB_DIRECTION_LTR: Text is set horizontally from left to right.
 * @HB_DIRECTION_RTL: Text is set horizontally from right to left.
 * @HB_DIRECTION_TTB: Text is set vertically from top to bottom.
 * @HB_DIRECTION_BTT: Text is set vertically from bottom to top.
 *
 * The direction of a text segment or buffer.
 * 
 * A segment can also be tested for horizontal or vertical
 * orientation (irrespective of specific direction) with 
 * HB_DIRECTION_IS_HORIZONTAL() or HB_DIRECTION_IS_VERTICAL().
 *
 */
typedef enum {
  HB_DIRECTION_INVALID = 0,
  HB_DIRECTION_LTR = 4,
  HB_DIRECTION_RTL,
  HB_DIRECTION_TTB,
  HB_DIRECTION_BTT
} hb_direction_t;

/* len=-1 means str is NUL-terminated */
HB_EXTERN hb_direction_t
hb_direction_from_string (const char *str, int len);

HB_EXTERN const char *
hb_direction_to_string (hb_direction_t direction);

/**
 * HB_DIRECTION_IS_VALID:
 * @dir: #hb_direction_t to test
 *
 * Tests whether a text direction is valid.
 *
 **/
#define HB_DIRECTION_IS_VALID(dir)	((((unsigned int) (dir)) & ~3U) == 4)
/* Direction must be valid for the following */
/**
 * HB_DIRECTION_IS_HORIZONTAL:
 * @dir: #hb_direction_t to test
 *
 * Tests whether a text direction is horizontal. Requires
 * that the direction be valid.
 *
 **/
#define HB_DIRECTION_IS_HORIZONTAL(dir)	((((unsigned int) (dir)) & ~1U) == 4)
/**
 * HB_DIRECTION_IS_VERTICAL:
 * @dir: #hb_direction_t to test
 *
 * Tests whether a text direction is vertical. Requires
 * that the direction be valid.
 *
 **/
#define HB_DIRECTION_IS_VERTICAL(dir)	((((unsigned int) (dir)) & ~1U) == 6)
/**
 * HB_DIRECTION_IS_FORWARD:
 * @dir: #hb_direction_t to test
 *
 * Tests whether a text direction moves forward (from left to right, or from
 * top to bottom). Requires that the direction be valid.
 *
 **/
#define HB_DIRECTION_IS_FORWARD(dir)	((((unsigned int) (dir)) & ~2U) == 4)
/**
 * HB_DIRECTION_IS_BACKWARD:
 * @dir: #hb_direction_t to test
 *
 * Tests whether a text direction moves backward (from right to left, or from
 * bottom to top). Requires that the direction be valid.
 *
 **/
#define HB_DIRECTION_IS_BACKWARD(dir)	((((unsigned int) (dir)) & ~2U) == 5)
/**
 * HB_DIRECTION_REVERSE:
 * @dir: #hb_direction_t to reverse
 *
 * Reverses a text direction. Requires that the direction
 * be valid.
 *
 **/
#define HB_DIRECTION_REVERSE(dir)	((hb_direction_t) (((unsigned int) (dir)) ^ 1))


/* hb_language_t */

/**
 * hb_language_t:
 *
 * Data type for languages. Each #hb_language_t corresponds to a BCP 47
 * language tag.
 *
 */
typedef const struct hb_language_impl_t *hb_language_t;

HB_EXTERN hb_language_t
hb_language_from_string (const char *str, int len);

HB_EXTERN const char *
hb_language_to_string (hb_language_t language);

/**
 * HB_LANGUAGE_INVALID:
 *
 * An unset #hb_language_t.
 *
 * Since: 0.6.0
 */
#define HB_LANGUAGE_INVALID ((hb_language_t) 0)

HB_EXTERN hb_language_t
hb_language_get_default (void);

HB_EXTERN hb_bool_t
hb_language_matches (hb_language_t language,
		     hb_language_t specific);

/**
 * hb_script_t:
 * @HB_SCRIPT_COMMON: `Zyyy`
 * @HB_SCRIPT_INHERITED: `Zinh`
 * @HB_SCRIPT_UNKNOWN: `Zzzz`
 * @HB_SCRIPT_ARABIC: `Arab`
 * @HB_SCRIPT_ARMENIAN: `Armn`
 * @HB_SCRIPT_BENGALI: `Beng`
 * @HB_SCRIPT_CYRILLIC: `Cyrl`
 * @HB_SCRIPT_DEVANAGARI: `Deva`
 * @HB_SCRIPT_GEORGIAN: `Geor`
 * @HB_SCRIPT_GREEK: `Grek`
 * @HB_SCRIPT_GUJARATI: `Gujr`
 * @HB_SCRIPT_GURMUKHI: `Guru`
 * @HB_SCRIPT_HANGUL: `Hang`
 * @HB_SCRIPT_HAN: `Hani`
 * @HB_SCRIPT_HEBREW: `Hebr`
 * @HB_SCRIPT_HIRAGANA: `Hira`
 * @HB_SCRIPT_KANNADA: `Knda`
 * @HB_SCRIPT_KATAKANA: `Kana`
 * @HB_SCRIPT_LAO: `Laoo`
 * @HB_SCRIPT_LATIN: `Latn`
 * @HB_SCRIPT_MALAYALAM: `Mlym`
 * @HB_SCRIPT_ORIYA: `Orya`
 * @HB_SCRIPT_TAMIL: `Taml`
 * @HB_SCRIPT_TELUGU: `Telu`
 * @HB_SCRIPT_THAI: `Thai`
 * @HB_SCRIPT_TIBETAN: `Tibt`
 * @HB_SCRIPT_BOPOMOFO: `Bopo`
 * @HB_SCRIPT_BRAILLE: `Brai`
 * @HB_SCRIPT_CANADIAN_SYLLABICS: `Cans`
 * @HB_SCRIPT_CHEROKEE: `Cher`
 * @HB_SCRIPT_ETHIOPIC: `Ethi`
 * @HB_SCRIPT_KHMER: `Khmr`
 * @HB_SCRIPT_MONGOLIAN: `Mong`
 * @HB_SCRIPT_MYANMAR: `Mymr`
 * @HB_SCRIPT_OGHAM: `Ogam`
 * @HB_SCRIPT_RUNIC: `Runr`
 * @HB_SCRIPT_SINHALA: `Sinh`
 * @HB_SCRIPT_SYRIAC: `Syrc`
 * @HB_SCRIPT_THAANA: `Thaa`
 * @HB_SCRIPT_YI: `Yiii`
 * @HB_SCRIPT_DESERET: `Dsrt`
 * @HB_SCRIPT_GOTHIC: `Goth`
 * @HB_SCRIPT_OLD_ITALIC: `Ital`
 * @HB_SCRIPT_BUHID: `Buhd`
 * @HB_SCRIPT_HANUNOO: `Hano`
 * @HB_SCRIPT_TAGALOG: `Tglg`
 * @HB_SCRIPT_TAGBANWA: `Tagb`
 * @HB_SCRIPT_CYPRIOT: `Cprt`
 * @HB_SCRIPT_LIMBU: `Limb`
 * @HB_SCRIPT_LINEAR_B: `Linb`
 * @HB_SCRIPT_OSMANYA: `Osma`
 * @HB_SCRIPT_SHAVIAN: `Shaw`
 * @HB_SCRIPT_TAI_LE: `Tale`
 * @HB_SCRIPT_UGARITIC: `Ugar`
 * @HB_SCRIPT_BUGINESE: `Bugi`
 * @HB_SCRIPT_COPTIC: `Copt`
 * @HB_SCRIPT_GLAGOLITIC: `Glag`
 * @HB_SCRIPT_KHAROSHTHI: `Khar`
 * @HB_SCRIPT_NEW_TAI_LUE: `Talu`
 * @HB_SCRIPT_OLD_PERSIAN: `Xpeo`
 * @HB_SCRIPT_SYLOTI_NAGRI: `Sylo`
 * @HB_SCRIPT_TIFINAGH: `Tfng`
 * @HB_SCRIPT_BALINESE: `Bali`
 * @HB_SCRIPT_CUNEIFORM: `Xsux`
 * @HB_SCRIPT_NKO: `Nkoo`
 * @HB_SCRIPT_PHAGS_PA: `Phag`
 * @HB_SCRIPT_PHOENICIAN: `Phnx`
 * @HB_SCRIPT_CARIAN: `Cari`
 * @HB_SCRIPT_CHAM: `Cham`
 * @HB_SCRIPT_KAYAH_LI: `Kali`
 * @HB_SCRIPT_LEPCHA: `Lepc`
 * @HB_SCRIPT_LYCIAN: `Lyci`
 * @HB_SCRIPT_LYDIAN: `Lydi`
 * @HB_SCRIPT_OL_CHIKI: `Olck`
 * @HB_SCRIPT_REJANG: `Rjng`
 * @HB_SCRIPT_SAURASHTRA: `Saur`
 * @HB_SCRIPT_SUNDANESE: `Sund`
 * @HB_SCRIPT_VAI: `Vaii`
 * @HB_SCRIPT_AVESTAN: `Avst`
 * @HB_SCRIPT_BAMUM: `Bamu`
 * @HB_SCRIPT_EGYPTIAN_HIEROGLYPHS: `Egyp`
 * @HB_SCRIPT_IMPERIAL_ARAMAIC: `Armi`
 * @HB_SCRIPT_INSCRIPTIONAL_PAHLAVI: `Phli`
 * @HB_SCRIPT_INSCRIPTIONAL_PARTHIAN: `Prti`
 * @HB_SCRIPT_JAVANESE: `Java`
 * @HB_SCRIPT_KAITHI: `Kthi`
 * @HB_SCRIPT_LISU: `Lisu`
 * @HB_SCRIPT_MEETEI_MAYEK: `Mtei`
 * @HB_SCRIPT_OLD_SOUTH_ARABIAN: `Sarb`
 * @HB_SCRIPT_OLD_TURKIC: `Orkh`
 * @HB_SCRIPT_SAMARITAN: `Samr`
 * @HB_SCRIPT_TAI_THAM: `Lana`
 * @HB_SCRIPT_TAI_VIET: `Tavt`
 * @HB_SCRIPT_BATAK: `Batk`
 * @HB_SCRIPT_BRAHMI: `Brah`
 * @HB_SCRIPT_MANDAIC: `Mand`
 * @HB_SCRIPT_CHAKMA: `Cakm`
 * @HB_SCRIPT_MEROITIC_CURSIVE: `Merc`
 * @HB_SCRIPT_MEROITIC_HIEROGLYPHS: `Mero`
 * @HB_SCRIPT_MIAO: `Plrd`
 * @HB_SCRIPT_SHARADA: `Shrd`
 * @HB_SCRIPT_SORA_SOMPENG: `Sora`
 * @HB_SCRIPT_TAKRI: `Takr`
 * @HB_SCRIPT_BASSA_VAH: `Bass`, Since: 0.9.30
 * @HB_SCRIPT_CAUCASIAN_ALBANIAN: `Aghb`, Since: 0.9.30
 * @HB_SCRIPT_DUPLOYAN: `Dupl`, Since: 0.9.30
 * @HB_SCRIPT_ELBASAN: `Elba`, Since: 0.9.30
 * @HB_SCRIPT_GRANTHA: `Gran`, Since: 0.9.30
 * @HB_SCRIPT_KHOJKI: `Khoj`, Since: 0.9.30
 * @HB_SCRIPT_KHUDAWADI: `Sind`, Since: 0.9.30
 * @HB_SCRIPT_LINEAR_A: `Lina`, Since: 0.9.30
 * @HB_SCRIPT_MAHAJANI: `Mahj`, Since: 0.9.30
 * @HB_SCRIPT_MANICHAEAN: `Mani`, Since: 0.9.30
 * @HB_SCRIPT_MENDE_KIKAKUI: `Mend`, Since: 0.9.30
 * @HB_SCRIPT_MODI: `Modi`, Since: 0.9.30
 * @HB_SCRIPT_MRO: `Mroo`, Since: 0.9.30
 * @HB_SCRIPT_NABATAEAN: `Nbat`, Since: 0.9.30
 * @HB_SCRIPT_OLD_NORTH_ARABIAN: `Narb`, Since: 0.9.30
 * @HB_SCRIPT_OLD_PERMIC: `Perm`, Since: 0.9.30
 * @HB_SCRIPT_PAHAWH_HMONG: `Hmng`, Since: 0.9.30
 * @HB_SCRIPT_PALMYRENE: `Palm`, Since: 0.9.30
 * @HB_SCRIPT_PAU_CIN_HAU: `Pauc`, Since: 0.9.30
 * @HB_SCRIPT_PSALTER_PAHLAVI: `Phlp`, Since: 0.9.30
 * @HB_SCRIPT_SIDDHAM: `Sidd`, Since: 0.9.30
 * @HB_SCRIPT_TIRHUTA: `Tirh`, Since: 0.9.30
 * @HB_SCRIPT_WARANG_CITI: `Wara`, Since: 0.9.30
 * @HB_SCRIPT_AHOM: `Ahom`, Since: 0.9.30
 * @HB_SCRIPT_ANATOLIAN_HIEROGLYPHS: `Hluw`, Since: 0.9.30
 * @HB_SCRIPT_HATRAN: `Hatr`, Since: 0.9.30
 * @HB_SCRIPT_MULTANI: `Mult`, Since: 0.9.30
 * @HB_SCRIPT_OLD_HUNGARIAN: `Hung`, Since: 0.9.30
 * @HB_SCRIPT_SIGNWRITING: `Sgnw`, Since: 0.9.30
 * @HB_SCRIPT_ADLAM: `Adlm`, Since: 1.3.0
 * @HB_SCRIPT_BHAIKSUKI: `Bhks`, Since: 1.3.0
 * @HB_SCRIPT_MARCHEN: `Marc`, Since: 1.3.0
 * @HB_SCRIPT_OSAGE: `Osge`, Since: 1.3.0
 * @HB_SCRIPT_TANGUT: `Tang`, Since: 1.3.0
 * @HB_SCRIPT_NEWA: `Newa`, Since: 1.3.0
 * @HB_SCRIPT_MASARAM_GONDI: `Gonm`, Since: 1.6.0
 * @HB_SCRIPT_NUSHU: `Nshu`, Since: 1.6.0
 * @HB_SCRIPT_SOYOMBO: `Soyo`, Since: 1.6.0
 * @HB_SCRIPT_ZANABAZAR_SQUARE: `Zanb`, Since: 1.6.0
 * @HB_SCRIPT_DOGRA: `Dogr`, Since: 1.8.0
 * @HB_SCRIPT_GUNJALA_GONDI: `Gong`, Since: 1.8.0
 * @HB_SCRIPT_HANIFI_ROHINGYA: `Rohg`, Since: 1.8.0
 * @HB_SCRIPT_MAKASAR: `Maka`, Since: 1.8.0
 * @HB_SCRIPT_MEDEFAIDRIN: `Medf`, Since: 1.8.0
 * @HB_SCRIPT_OLD_SOGDIAN: `Sogo`, Since: 1.8.0
 * @HB_SCRIPT_SOGDIAN: `Sogd`, Since: 1.8.0
 * @HB_SCRIPT_ELYMAIC: `Elym`, Since: 2.4.0
 * @HB_SCRIPT_NANDINAGARI: `Nand`, Since: 2.4.0
 * @HB_SCRIPT_NYIAKENG_PUACHUE_HMONG: `Hmnp`, Since: 2.4.0
 * @HB_SCRIPT_WANCHO: `Wcho`, Since: 2.4.0
 * @HB_SCRIPT_CHORASMIAN: `Chrs`, Since: 2.6.7
 * @HB_SCRIPT_DIVES_AKURU: `Diak`, Since: 2.6.7
 * @HB_SCRIPT_KHITAN_SMALL_SCRIPT: `Kits`, Since: 2.6.7
 * @HB_SCRIPT_YEZIDI: `Yezi`, Since: 2.6.7
 * @HB_SCRIPT_CYPRO_MINOAN: `Cpmn`, Since: 3.0.0
 * @HB_SCRIPT_OLD_UYGHUR: `Ougr`, Since: 3.0.0
 * @HB_SCRIPT_TANGSA: `Tnsa`, Since: 3.0.0
 * @HB_SCRIPT_TOTO: `Toto`, Since: 3.0.0
 * @HB_SCRIPT_VITHKUQI: `Vith`, Since: 3.0.0
 * @HB_SCRIPT_MATH: `Zmth`, Since: 3.4.0
 * @HB_SCRIPT_KAWI: `Kawi`, Since: 5.2.0
 * @HB_SCRIPT_NAG_MUNDARI: `Nagm`, Since: 5.2.0
 * @HB_SCRIPT_GARAY: `Gara`, Since: 10.0.0
 * @HB_SCRIPT_GURUNG_KHEMA: `Gukh`, Since: 10.0.0
 * @HB_SCRIPT_KIRAT_RAI: `Krai`, Since: 10.0.0
 * @HB_SCRIPT_OL_ONAL: `Onao`, Since: 10.0.0
 * @HB_SCRIPT_SUNUWAR: `Sunu`, Since: 10.0.0
 * @HB_SCRIPT_TODHRI: `Todr`, Since: 10.0.0
 * @HB_SCRIPT_TULU_TIGALARI: `Tutg`, Since: 10.0.0
 * @HB_SCRIPT_INVALID: No script set
 *
 * Data type for scripts. Each #hb_script_t's value is an #hb_tag_t corresponding
 * to the four-letter values defined by [ISO 15924](https://unicode.org/iso15924/).
 *
 * See also the Script (sc) property of the Unicode Character Database.
 *
 **/

/* https://docs.google.com/spreadsheets/d/1Y90M0Ie3MUJ6UVCRDOypOtijlMDLNNyyLk36T6iMu0o */
typedef enum
{
  HB_SCRIPT_COMMON			= HB_TAG ('Z','y','y','y'), /*1.1*/
  HB_SCRIPT_INHERITED			= HB_TAG ('Z','i','n','h'), /*1.1*/
  HB_SCRIPT_UNKNOWN			= HB_TAG ('Z','z','z','z'), /*5.0*/

  HB_SCRIPT_ARABIC			= HB_TAG ('A','r','a','b'), /*1.1*/
  HB_SCRIPT_ARMENIAN			= HB_TAG ('A','r','m','n'), /*1.1*/
  HB_SCRIPT_BENGALI			= HB_TAG ('B','e','n','g'), /*1.1*/
  HB_SCRIPT_CYRILLIC			= HB_TAG ('C','y','r','l'), /*1.1*/
  HB_SCRIPT_DEVANAGARI			= HB_TAG ('D','e','v','a'), /*1.1*/
  HB_SCRIPT_GEORGIAN			= HB_TAG ('G','e','o','r'), /*1.1*/
  HB_SCRIPT_GREEK			= HB_TAG ('G','r','e','k'), /*1.1*/
  HB_SCRIPT_GUJARATI			= HB_TAG ('G','u','j','r'), /*1.1*/
  HB_SCRIPT_GURMUKHI			= HB_TAG ('G','u','r','u'), /*1.1*/
  HB_SCRIPT_HANGUL			= HB_TAG ('H','a','n','g'), /*1.1*/
  HB_SCRIPT_HAN				= HB_TAG ('H','a','n','i'), /*1.1*/
  HB_SCRIPT_HEBREW			= HB_TAG ('H','e','b','r'), /*1.1*/
  HB_SCRIPT_HIRAGANA			= HB_TAG ('H','i','r','a'), /*1.1*/
  HB_SCRIPT_KANNADA			= HB_TAG ('K','n','d','a'), /*1.1*/
  HB_SCRIPT_KATAKANA			= HB_TAG ('K','a','n','a'), /*1.1*/
  HB_SCRIPT_LAO				= HB_TAG ('L','a','o','o'), /*1.1*/
  HB_SCRIPT_LATIN			= HB_TAG ('L','a','t','n'), /*1.1*/
  HB_SCRIPT_MALAYALAM			= HB_TAG ('M','l','y','m'), /*1.1*/
  HB_SCRIPT_ORIYA			= HB_TAG ('O','r','y','a'), /*1.1*/
  HB_SCRIPT_TAMIL			= HB_TAG ('T','a','m','l'), /*1.1*/
  HB_SCRIPT_TELUGU			= HB_TAG ('T','e','l','u'), /*1.1*/
  HB_SCRIPT_THAI			= HB_TAG ('T','h','a','i'), /*1.1*/

  HB_SCRIPT_TIBETAN			= HB_TAG ('T','i','b','t'), /*2.0*/

  HB_SCRIPT_BOPOMOFO			= HB_TAG ('B','o','p','o'), /*3.0*/
  HB_SCRIPT_BRAILLE			= HB_TAG ('B','r','a','i'), /*3.0*/
  HB_SCRIPT_CANADIAN_SYLLABICS		= HB_TAG ('C','a','n','s'), /*3.0*/
  HB_SCRIPT_CHEROKEE			= HB_TAG ('C','h','e','r'), /*3.0*/
  HB_SCRIPT_ETHIOPIC			= HB_TAG ('E','t','h','i'), /*3.0*/
  HB_SCRIPT_KHMER			= HB_TAG ('K','h','m','r'), /*3.0*/
  HB_SCRIPT_MONGOLIAN			= HB_TAG ('M','o','n','g'), /*3.0*/
  HB_SCRIPT_MYANMAR			= HB_TAG ('M','y','m','r'), /*3.0*/
  HB_SCRIPT_OGHAM			= HB_TAG ('O','g','a','m'), /*3.0*/
  HB_SCRIPT_RUNIC			= HB_TAG ('R','u','n','r'), /*3.0*/
  HB_SCRIPT_SINHALA			= HB_TAG ('S','i','n','h'), /*3.0*/
  HB_SCRIPT_SYRIAC			= HB_TAG ('S','y','r','c'), /*3.0*/
  HB_SCRIPT_THAANA			= HB_TAG ('T','h','a','a'), /*3.0*/
  HB_SCRIPT_YI				= HB_TAG ('Y','i','i','i'), /*3.0*/

  HB_SCRIPT_DESERET			= HB_TAG ('D','s','r','t'), /*3.1*/
  HB_SCRIPT_GOTHIC			= HB_TAG ('G','o','t','h'), /*3.1*/
  HB_SCRIPT_OLD_ITALIC			= HB_TAG ('I','t','a','l'), /*3.1*/

  HB_SCRIPT_BUHID			= HB_TAG ('B','u','h','d'), /*3.2*/
  HB_SCRIPT_HANUNOO			= HB_TAG ('H','a','n','o'), /*3.2*/
  HB_SCRIPT_TAGALOG			= HB_TAG ('T','g','l','g'), /*3.2*/
  HB_SCRIPT_TAGBANWA			= HB_TAG ('T','a','g','b'), /*3.2*/

  HB_SCRIPT_CYPRIOT			= HB_TAG ('C','p','r','t'), /*4.0*/
  HB_SCRIPT_LIMBU			= HB_TAG ('L','i','m','b'), /*4.0*/
  HB_SCRIPT_LINEAR_B			= HB_TAG ('L','i','n','b'), /*4.0*/
  HB_SCRIPT_OSMANYA			= HB_TAG ('O','s','m','a'), /*4.0*/
  HB_SCRIPT_SHAVIAN			= HB_TAG ('S','h','a','w'), /*4.0*/
  HB_SCRIPT_TAI_LE			= HB_TAG ('T','a','l','e'), /*4.0*/
  HB_SCRIPT_UGARITIC			= HB_TAG ('U','g','a','r'), /*4.0*/

  HB_SCRIPT_BUGINESE			= HB_TAG ('B','u','g','i'), /*4.1*/
  HB_SCRIPT_COPTIC			= HB_TAG ('C','o','p','t'), /*4.1*/
  HB_SCRIPT_GLAGOLITIC			= HB_TAG ('G','l','a','g'), /*4.1*/
  HB_SCRIPT_KHAROSHTHI			= HB_TAG ('K','h','a','r'), /*4.1*/
  HB_SCRIPT_NEW_TAI_LUE			= HB_TAG ('T','a','l','u'), /*4.1*/
  HB_SCRIPT_OLD_PERSIAN			= HB_TAG ('X','p','e','o'), /*4.1*/
  HB_SCRIPT_SYLOTI_NAGRI		= HB_TAG ('S','y','l','o'), /*4.1*/
  HB_SCRIPT_TIFINAGH			= HB_TAG ('T','f','n','g'), /*4.1*/

  HB_SCRIPT_BALINESE			= HB_TAG ('B','a','l','i'), /*5.0*/
  HB_SCRIPT_CUNEIFORM			= HB_TAG ('X','s','u','x'), /*5.0*/
  HB_SCRIPT_NKO				= HB_TAG ('N','k','o','o'), /*5.0*/
  HB_SCRIPT_PHAGS_PA			= HB_TAG ('P','h','a','g'), /*5.0*/
  HB_SCRIPT_PHOENICIAN			= HB_TAG ('P','h','n','x'), /*5.0*/

  HB_SCRIPT_CARIAN			= HB_TAG ('C','a','r','i'), /*5.1*/
  HB_SCRIPT_CHAM			= HB_TAG ('C','h','a','m'), /*5.1*/
  HB_SCRIPT_KAYAH_LI			= HB_TAG ('K','a','l','i'), /*5.1*/
  HB_SCRIPT_LEPCHA			= HB_TAG ('L','e','p','c'), /*5.1*/
  HB_SCRIPT_LYCIAN			= HB_TAG ('L','y','c','i'), /*5.1*/
  HB_SCRIPT_LYDIAN			= HB_TAG ('L','y','d','i'), /*5.1*/
  HB_SCRIPT_OL_CHIKI			= HB_TAG ('O','l','c','k'), /*5.1*/
  HB_SCRIPT_REJANG			= HB_TAG ('R','j','n','g'), /*5.1*/
  HB_SCRIPT_SAURASHTRA			= HB_TAG ('S','a','u','r'), /*5.1*/
  HB_SCRIPT_SUNDANESE			= HB_TAG ('S','u','n','d'), /*5.1*/
  HB_SCRIPT_VAI				= HB_TAG ('V','a','i','i'), /*5.1*/

  HB_SCRIPT_AVESTAN			= HB_TAG ('A','v','s','t'), /*5.2*/
  HB_SCRIPT_BAMUM			= HB_TAG ('B','a','m','u'), /*5.2*/
  HB_SCRIPT_EGYPTIAN_HIEROGLYPHS	= HB_TAG ('E','g','y','p'), /*5.2*/
  HB_SCRIPT_IMPERIAL_ARAMAIC		= HB_TAG ('A','r','m','i'), /*5.2*/
  HB_SCRIPT_INSCRIPTIONAL_PAHLAVI	= HB_TAG ('P','h','l','i'), /*5.2*/
  HB_SCRIPT_INSCRIPTIONAL_PARTHIAN	= HB_TAG ('P','r','t','i'), /*5.2*/
  HB_SCRIPT_JAVANESE			= HB_TAG ('J','a','v','a'), /*5.2*/
  HB_SCRIPT_KAITHI			= HB_TAG ('K','t','h','i'), /*5.2*/
  HB_SCRIPT_LISU			= HB_TAG ('L','i','s','u'), /*5.2*/
  HB_SCRIPT_MEETEI_MAYEK		= HB_TAG ('M','t','e','i'), /*5.2*/
  HB_SCRIPT_OLD_SOUTH_ARABIAN		= HB_TAG ('S','a','r','b'), /*5.2*/
  HB_SCRIPT_OLD_TURKIC			= HB_TAG ('O','r','k','h'), /*5.2*/
  HB_SCRIPT_SAMARITAN			= HB_TAG ('S','a','m','r'), /*5.2*/
  HB_SCRIPT_TAI_THAM			= HB_TAG ('L','a','n','a'), /*5.2*/
  HB_SCRIPT_TAI_VIET			= HB_TAG ('T','a','v','t'), /*5.2*/

  HB_SCRIPT_BATAK			= HB_TAG ('B','a','t','k'), /*6.0*/
  HB_SCRIPT_BRAHMI			= HB_TAG ('B','r','a','h'), /*6.0*/
  HB_SCRIPT_MANDAIC			= HB_TAG ('M','a','n','d'), /*6.0*/

  HB_SCRIPT_CHAKMA			= HB_TAG ('C','a','k','m'), /*6.1*/
  HB_SCRIPT_MEROITIC_CURSIVE		= HB_TAG ('M','e','r','c'), /*6.1*/
  HB_SCRIPT_MEROITIC_HIEROGLYPHS	= HB_TAG ('M','e','r','o'), /*6.1*/
  HB_SCRIPT_MIAO			= HB_TAG ('P','l','r','d'), /*6.1*/
  HB_SCRIPT_SHARADA			= HB_TAG ('S','h','r','d'), /*6.1*/
  HB_SCRIPT_SORA_SOMPENG		= HB_TAG ('S','o','r','a'), /*6.1*/
  HB_SCRIPT_TAKRI			= HB_TAG ('T','a','k','r'), /*6.1*/

  /*
   * Since: 0.9.30
   */
  HB_SCRIPT_BASSA_VAH			= HB_TAG ('B','a','s','s'), /*7.0*/
  HB_SCRIPT_CAUCASIAN_ALBANIAN		= HB_TAG ('A','g','h','b'), /*7.0*/
  HB_SCRIPT_DUPLOYAN			= HB_TAG ('D','u','p','l'), /*7.0*/
  HB_SCRIPT_ELBASAN			= HB_TAG ('E','l','b','a'), /*7.0*/
  HB_SCRIPT_GRANTHA			= HB_TAG ('G','r','a','n'), /*7.0*/
  HB_SCRIPT_KHOJKI			= HB_TAG ('K','h','o','j'), /*7.0*/
  HB_SCRIPT_KHUDAWADI			= HB_TAG ('S','i','n','d'), /*7.0*/
  HB_SCRIPT_LINEAR_A			= HB_TAG ('L','i','n','a'), /*7.0*/
  HB_SCRIPT_MAHAJANI			= HB_TAG ('M','a','h','j'), /*7.0*/
  HB_SCRIPT_MANICHAEAN			= HB_TAG ('M','a','n','i'), /*7.0*/
  HB_SCRIPT_MENDE_KIKAKUI		= HB_TAG ('M','e','n','d'), /*7.0*/
  HB_SCRIPT_MODI			= HB_TAG ('M','o','d','i'), /*7.0*/
  HB_SCRIPT_MRO				= HB_TAG ('M','r','o','o'), /*7.0*/
  HB_SCRIPT_NABATAEAN			= HB_TAG ('N','b','a','t'), /*7.0*/
  HB_SCRIPT_OLD_NORTH_ARABIAN		= HB_TAG ('N','a','r','b'), /*7.0*/
  HB_SCRIPT_OLD_PERMIC			= HB_TAG ('P','e','r','m'), /*7.0*/
  HB_SCRIPT_PAHAWH_HMONG		= HB_TAG ('H','m','n','g'), /*7.0*/
  HB_SCRIPT_PALMYRENE			= HB_TAG ('P','a','l','m'), /*7.0*/
  HB_SCRIPT_PAU_CIN_HAU			= HB_TAG ('P','a','u','c'), /*7.0*/
  HB_SCRIPT_PSALTER_PAHLAVI		= HB_TAG ('P','h','l','p'), /*7.0*/
  HB_SCRIPT_SIDDHAM			= HB_TAG ('S','i','d','d'), /*7.0*/
  HB_SCRIPT_TIRHUTA			= HB_TAG ('T','i','r','h'), /*7.0*/
  HB_SCRIPT_WARANG_CITI			= HB_TAG ('W','a','r','a'), /*7.0*/

  HB_SCRIPT_AHOM			= HB_TAG ('A','h','o','m'), /*8.0*/
  HB_SCRIPT_ANATOLIAN_HIEROGLYPHS	= HB_TAG ('H','l','u','w'), /*8.0*/
  HB_SCRIPT_HATRAN			= HB_TAG ('H','a','t','r'), /*8.0*/
  HB_SCRIPT_MULTANI			= HB_TAG ('M','u','l','t'), /*8.0*/
  HB_SCRIPT_OLD_HUNGARIAN		= HB_TAG ('H','u','n','g'), /*8.0*/
  HB_SCRIPT_SIGNWRITING			= HB_TAG ('S','g','n','w'), /*8.0*/

  /*
   * Since 1.3.0
   */
  HB_SCRIPT_ADLAM			= HB_TAG ('A','d','l','m'), /*9.0*/
  HB_SCRIPT_BHAIKSUKI			= HB_TAG ('B','h','k','s'), /*9.0*/
  HB_SCRIPT_MARCHEN			= HB_TAG ('M','a','r','c'), /*9.0*/
  HB_SCRIPT_OSAGE			= HB_TAG ('O','s','g','e'), /*9.0*/
  HB_SCRIPT_TANGUT			= HB_TAG ('T','a','n','g'), /*9.0*/
  HB_SCRIPT_NEWA			= HB_TAG ('N','e','w','a'), /*9.0*/

  /*
   * Since 1.6.0
   */
  HB_SCRIPT_MASARAM_GONDI		= HB_TAG ('G','o','n','m'), /*10.0*/
  HB_SCRIPT_NUSHU			= HB_TAG ('N','s','h','u'), /*10.0*/
  HB_SCRIPT_SOYOMBO			= HB_TAG ('S','o','y','o'), /*10.0*/
  HB_SCRIPT_ZANABAZAR_SQUARE		= HB_TAG ('Z','a','n','b'), /*10.0*/

  /*
   * Since 1.8.0
   */
  HB_SCRIPT_DOGRA			= HB_TAG ('D','o','g','r'), /*11.0*/
  HB_SCRIPT_GUNJALA_GONDI		= HB_TAG ('G','o','n','g'), /*11.0*/
  HB_SCRIPT_HANIFI_ROHINGYA		= HB_TAG ('R','o','h','g'), /*11.0*/
  HB_SCRIPT_MAKASAR			= HB_TAG ('M','a','k','a'), /*11.0*/
  HB_SCRIPT_MEDEFAIDRIN			= HB_TAG ('M','e','d','f'), /*11.0*/
  HB_SCRIPT_OLD_SOGDIAN			= HB_TAG ('S','o','g','o'), /*11.0*/
  HB_SCRIPT_SOGDIAN			= HB_TAG ('S','o','g','d'), /*11.0*/

  /*
   * Since 2.4.0
   */
  HB_SCRIPT_ELYMAIC			= HB_TAG ('E','l','y','m'), /*12.0*/
  HB_SCRIPT_NANDINAGARI			= HB_TAG ('N','a','n','d'), /*12.0*/
  HB_SCRIPT_NYIAKENG_PUACHUE_HMONG	= HB_TAG ('H','m','n','p'), /*12.0*/
  HB_SCRIPT_WANCHO			= HB_TAG ('W','c','h','o'), /*12.0*/

  /*
   * Since 2.6.7
   */
  HB_SCRIPT_CHORASMIAN			= HB_TAG ('C','h','r','s'), /*13.0*/
  HB_SCRIPT_DIVES_AKURU			= HB_TAG ('D','i','a','k'), /*13.0*/
  HB_SCRIPT_KHITAN_SMALL_SCRIPT		= HB_TAG ('K','i','t','s'), /*13.0*/
  HB_SCRIPT_YEZIDI			= HB_TAG ('Y','e','z','i'), /*13.0*/

  /*
   * Since 3.0.0
   */
  HB_SCRIPT_CYPRO_MINOAN		= HB_TAG ('C','p','m','n'), /*14.0*/
  HB_SCRIPT_OLD_UYGHUR			= HB_TAG ('O','u','g','r'), /*14.0*/
  HB_SCRIPT_TANGSA			= HB_TAG ('T','n','s','a'), /*14.0*/
  HB_SCRIPT_TOTO			= HB_TAG ('T','o','t','o'), /*14.0*/
  HB_SCRIPT_VITHKUQI			= HB_TAG ('V','i','t','h'), /*14.0*/

  /*
   * Since 3.4.0
   */
  HB_SCRIPT_MATH			= HB_TAG ('Z','m','t','h'),

  /*
   * Since 5.2.0
   */
  HB_SCRIPT_KAWI			= HB_TAG ('K','a','w','i'), /*15.0*/
  HB_SCRIPT_NAG_MUNDARI			= HB_TAG ('N','a','g','m'), /*15.0*/

  /*
   * Since 10.0.0
   */
  HB_SCRIPT_GARAY			= HB_TAG ('G','a','r','a'), /*16.0*/
  HB_SCRIPT_GURUNG_KHEMA		= HB_TAG ('G','u','k','h'), /*16.0*/
  HB_SCRIPT_KIRAT_RAI			= HB_TAG ('K','r','a','i'), /*16.0*/
  HB_SCRIPT_OL_ONAL			= HB_TAG ('O','n','a','o'), /*16.0*/
  HB_SCRIPT_SUNUWAR			= HB_TAG ('S','u','n','u'), /*16.0*/
  HB_SCRIPT_TODHRI			= HB_TAG ('T','o','d','r'), /*16.0*/
  HB_SCRIPT_TULU_TIGALARI		= HB_TAG ('T','u','t','g'), /*16.0*/

  /* No script set. */
  HB_SCRIPT_INVALID			= HB_TAG_NONE,

  /*< private >*/

  /* Dummy values to ensure any hb_tag_t value can be passed/stored as hb_script_t
   * without risking undefined behavior.  We have two, for historical reasons.
   * HB_TAG_MAX used to be unsigned, but that was invalid Ansi C, so was changed
   * to _HB_SCRIPT_MAX_VALUE to be equal to HB_TAG_MAX_SIGNED as well.
   *
   * See this thread for technicalities:
   *
   *   https://lists.freedesktop.org/archives/harfbuzz/2014-March/004150.html
   */
  _HB_SCRIPT_MAX_VALUE				= HB_TAG_MAX_SIGNED, /*< skip >*/
  _HB_SCRIPT_MAX_VALUE_SIGNED			= HB_TAG_MAX_SIGNED /*< skip >*/

} hb_script_t;


/* Script functions */

HB_EXTERN hb_script_t
hb_script_from_iso15924_tag (hb_tag_t tag);

HB_EXTERN hb_script_t
hb_script_from_string (const char *str, int len);

HB_EXTERN hb_tag_t
hb_script_to_iso15924_tag (hb_script_t script);

HB_EXTERN hb_direction_t
hb_script_get_horizontal_direction (hb_script_t script);


/* User data */

/**
 * hb_user_data_key_t:
 *
 * Data structure for holding user-data keys.
 *
 **/
typedef struct hb_user_data_key_t {
  /*< private >*/
  char unused;
} hb_user_data_key_t;

/**
 * hb_destroy_func_t:
 * @user_data: the data to be destroyed
 *
 * A virtual method for destroy user-data callbacks.
 *
 */
typedef void (*hb_destroy_func_t) (void *user_data);


/* Font features and variations. */

/**
 * HB_FEATURE_GLOBAL_START:
 *
 * Special setting for #hb_feature_t.start to apply the feature from the start
 * of the buffer.
 *
 * Since: 2.0.0
 */
#define HB_FEATURE_GLOBAL_START	0

/**
 * HB_FEATURE_GLOBAL_END:
 *
 * Special setting for #hb_feature_t.end to apply the feature from to the end
 * of the buffer.
 *
 * Since: 2.0.0
 */
#define HB_FEATURE_GLOBAL_END	((unsigned int) -1)

/**
 * hb_feature_t:
 * @tag: The #hb_tag_t tag of the feature
 * @value: The value of the feature. 0 disables the feature, non-zero (usually
 * 1) enables the feature.  For features implemented as lookup type 3 (like
 * 'salt') the @value is a one based index into the alternates.
 * @start: the cluster to start applying this feature setting (inclusive).
 * @end: the cluster to end applying this feature setting (exclusive).
 *
 * The #hb_feature_t is the structure that holds information about requested
 * feature application. The feature will be applied with the given value to all
 * glyphs which are in clusters between @start (inclusive) and @end (exclusive).
 * Setting start to #HB_FEATURE_GLOBAL_START and end to #HB_FEATURE_GLOBAL_END
 * specifies that the feature always applies to the entire buffer.
 */
typedef struct hb_feature_t {
  hb_tag_t      tag;
  uint32_t      value;
  unsigned int  start;
  unsigned int  end;
} hb_feature_t;

HB_EXTERN hb_bool_t
hb_feature_from_string (const char *str, int len,
			hb_feature_t *feature);

HB_EXTERN void
hb_feature_to_string (hb_feature_t *feature,
		      char *buf, unsigned int size);

/**
 * hb_variation_t:
 * @tag: The #hb_tag_t tag of the variation-axis name
 * @value: The value of the variation axis
 *
 * Data type for holding variation data. Registered OpenType
 * variation-axis tags are listed in
 * [OpenType Axis Tag Registry](https://docs.microsoft.com/en-us/typography/opentype/spec/dvaraxisreg).
 * 
 * Since: 1.4.2
 */
typedef struct hb_variation_t {
  hb_tag_t tag;
  float    value;
} hb_variation_t;

HB_EXTERN hb_bool_t
hb_variation_from_string (const char *str, int len,
			  hb_variation_t *variation);

HB_EXTERN void
hb_variation_to_string (hb_variation_t *variation,
			char *buf, unsigned int size);

/**
 * hb_color_t:
 *
 * Data type for holding color values. Colors are eight bits per
 * channel RGB plus alpha transparency.
 *
 * Since: 2.1.0
 */
typedef uint32_t hb_color_t;

/**
 * HB_COLOR:
 * @b: blue channel value
 * @g: green channel value
 * @r: red channel value
 * @a: alpha channel value
 *
 * Constructs an #hb_color_t from four integers.
 *
 * Since: 2.1.0
 */
#define HB_COLOR(b,g,r,a) ((hb_color_t) HB_TAG ((b),(g),(r),(a)))

HB_EXTERN uint8_t
hb_color_get_alpha (hb_color_t color);
#define hb_color_get_alpha(color)	((color) & 0xFF)

HB_EXTERN uint8_t
hb_color_get_red (hb_color_t color);
#define hb_color_get_red(color)		(((color) >> 8) & 0xFF)

HB_EXTERN uint8_t
hb_color_get_green (hb_color_t color);
#define hb_color_get_green(color)	(((color) >> 16) & 0xFF)

HB_EXTERN uint8_t
hb_color_get_blue (hb_color_t color);
#define hb_color_get_blue(color)	(((color) >> 24) & 0xFF)

/**
 * hb_glyph_extents_t:
 * @x_bearing: Distance from the x-origin to the left extremum of the glyph.
 * @y_bearing: Distance from the top extremum of the glyph to the y-origin.
 * @width: Distance from the left extremum of the glyph to the right extremum.
 * @height: Distance from the top extremum of the glyph to the bottom extremum.
 *
 * Glyph extent values, measured in font units.
 *
 * Note that @height is negative, in coordinate systems that grow up.
 **/
typedef struct hb_glyph_extents_t {
  hb_position_t x_bearing;
  hb_position_t y_bearing;
  hb_position_t width;
  hb_position_t height;
} hb_glyph_extents_t;

/**
 * hb_font_t:
 *
 * Data type for holding fonts.
 *
 */
typedef struct hb_font_t hb_font_t;

HB_END_DECLS

#endif /* HB_COMMON_H */
